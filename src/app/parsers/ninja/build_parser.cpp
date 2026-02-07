#include "build_parser.h"

#include <ezlibs/ezStr.hpp>

namespace kunai {
namespace ninja {

std::pair<std::unique_ptr<BuildParser>, std::string> BuildParser::create(
    const std::string& aFilePathName,
    IBuildWriter& arDbWriter) {
    auto pRet = std::make_unique<BuildParser>(arDbWriter);
    std::string error;
    if (!pRet->m_parse(aFilePathName)) {
        error = pRet->getError();
        pRet.reset();
    }
    return std::make_pair(std::move(pRet), error);
}

BuildParser::BuildParser(IBuildWriter& arDbWriter) : mr_dbWriter(arDbWriter) {

}

std::string BuildParser::getError() const {  //
    return m_error.str();
}

std::string BuildParser::m_getDirectory(const std::string& aFilePathName) {
    size_t pos = aFilePathName.find_last_of("/\\");
    if (pos == std::string::npos) {
        return ".";
    }
    return aFilePathName.substr(0, pos);
}

std::string BuildParser::m_resolvePath(const std::string& aPath) {
    if (aPath.empty()          //
        || (aPath[0] == '/')   // Unix
        || (aPath[0] == '\\')  // Win32
        || m_baseDir.empty()   //
        || (m_baseDir == ".")) {
        return aPath;
    }
    return m_baseDir + "/" + aPath;
}

bool BuildParser::m_parse(const std::string& aFilePathName) {
    m_baseDir = m_getDirectory(aFilePathName);
    // the first parsing file cant be optional
    return m_parseFile(aFilePathName, false);
}

bool BuildParser::m_parseFile(const std::string& aFilePathName, bool aOpeningOptional) {
    // Avoid circular includes
    if (m_parsedFiles.count(aFilePathName)) {
        return true;
    }
    m_parsedFiles.insert(aFilePathName);

    std::ifstream file(aFilePathName);
    if (!file.is_open()) {
        if (aOpeningOptional) {
            return true;
        }
        m_error << "Cannot open file: " << aFilePathName;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Handle line continuations ($)
        while (!line.empty() && line.back() == '$') {
            line.pop_back();
            std::string next;
            if (std::getline(file, next)) {
                m_trim(next);
                line += next;
            }
        }

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Include directive
        if (m_startsWith(line, "include ")) {
            std::string includePath = line.substr(8);
            m_trim(includePath);
            includePath = m_expandVars(includePath, m_globalVars);
            if (!m_parseFile(m_resolvePath(includePath), true)) {
                return false;
            }
            continue;
        }

        // Subninja directive (same as include for our purposes)
        if (m_startsWith(line, "subninja ")) {
            std::string includePath = line.substr(9);
            m_trim(includePath);
            includePath = m_expandVars(includePath, m_globalVars);
            if (!m_parseFile(m_resolvePath(includePath), true)) {
                return false;
            }
            continue;
        }

        // Global variable: name = value
        if ((line.find('=') != std::string::npos)  //
            && (!m_startsWith(line, "build "))     //
            && (!m_startsWith(line, "rule "))      //
            && (!m_startsWith(line, "  "))) {
            m_parseVariable(line, m_globalVars);
            continue;
        }

        // Build statement
        if (m_startsWith(line, "build ")) {
            m_parseBuildStatement(line, file);
            continue;
        }

        // Rule (skip, we don't need rule details for DAG)
        if (m_startsWith(line, "rule ")) {
            // Skip indented lines
            while (file.peek() == ' ') {
                std::getline(file, line);
            }
            continue;
        }
    }

    return true;
}

void BuildParser::m_trim(std::string& arStr) {
    size_t start = arStr.find_first_not_of(" \t\r\n");
    size_t end = arStr.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) {
        arStr.clear();
    } else {
        arStr = arStr.substr(start, end - start + 1);
    }
}

bool BuildParser::m_startsWith(const std::string& s, const std::string& prefix) {
    return (s.size() >= prefix.size())  //
        && (s.compare(0, prefix.size(), prefix) == 0);
}

void BuildParser::m_parseVariable(const std::string& aLine, std::unordered_map<std::string, std::string>& aVars) {
    size_t eq = aLine.find('=');
    if (eq == std::string::npos) {
        return;
    }
    std::string name = aLine.substr(0, eq);
    std::string value = aLine.substr(eq + 1);
    m_trim(name);
    m_trim(value);
    aVars[name] = m_expandVars(value, aVars);
}

std::string BuildParser::m_expandVars(const std::string& aInput, const std::unordered_map<std::string, std::string>& aVars) {
    std::string result;
    result.reserve(aInput.size());
    for (size_t i = 0; i < aInput.size(); ++i) {
        if (aInput[i] == '$') {
            if (i + 1 < aInput.size()) {
                if (aInput[i + 1] == '$') {
                    result += '$';
                    ++i;
                } else if (aInput[i + 1] == '{') {
                    size_t end = aInput.find('}', i + 2);
                    if (end != std::string::npos) {
                        std::string var = aInput.substr(i + 2, end - i - 2);
                        auto it = aVars.find(var);
                        if (it != aVars.end()) {
                            result += it->second;
                        }
                        i = end;
                    }
                } else {
                    size_t start = i + 1;
                    size_t end = start;
                    while (end < aInput.size() && (std::isalnum(aInput[end]) || aInput[end] == '_')) {
                        ++end;
                    }
                    std::string var = aInput.substr(start, end - start);
                    auto it = aVars.find(var);
                    if (it != aVars.end()) {
                        result += it->second;
                    }
                    i = end - 1;
                }
            }
        } else {
            result += aInput[i];
        }
    }
    return result;
}

// Format: build targets: rule inputs | implicit || order_only
void BuildParser::m_parseBuildStatement(const std::string& aLine, std::ifstream& arFile) {
    std::string stmt = aLine.substr(6);  // Skip "build "

    ez::str::replaceString(stmt, "\\", "/");

    // Read indented local variables
    std::unordered_map<std::string, std::string> localVars = m_globalVars;
    std::streampos savedPos = arFile.tellg();
    std::string varLine;

    while (arFile.peek() == ' ') {
        savedPos = arFile.tellg();
        if (!std::getline(arFile, varLine)) {
            break;
        }
        size_t eq = varLine.find('=');
        if (eq != std::string::npos) {
            m_parseVariable(varLine, localVars);
        }
    }

    // Parse targets : rule inputs
    size_t colon = stmt.find(':');
    if (colon == std::string::npos) {
        return;
    }

    std::string outputsStr = stmt.substr(0, colon);
    std::string rest = stmt.substr(colon + 1);
    m_trim(outputsStr);
    m_trim(rest);

    // First token after : is the rule
    size_t space = rest.find(' ');
    std::string rule;
    std::string inputsStr;
    if (space == std::string::npos) {
        rule = rest;
    } else {
        rule = rest.substr(0, space);
        inputsStr = rest.substr(space + 1);
    }
    m_trim(rule);

    // Parse targets
    IBuildWriter::BuildLink link;
    link.rule = rule;

    std::istringstream outputsStream(outputsStr);
    std::string token;
    while (outputsStream >> token) {
        std::string expanded = m_expandVars(token, localVars);
        link.targets.push_back(expanded);
    }
    if (!link.targets.empty()) {
        link.target = link.targets[0];
    }

    // Parse inputs: explicit | implicit || order_only
    size_t pipeIdx = inputsStr.find(" | ");
    size_t pipePipeIdx = inputsStr.find(" || ");

    std::string explicitStr, implicitStr, orderOnlyStr;

    if (pipePipeIdx != std::string::npos) {
        orderOnlyStr = inputsStr.substr(pipePipeIdx + 4);
        inputsStr = inputsStr.substr(0, pipePipeIdx);
    }
    if (pipeIdx != std::string::npos) {
        implicitStr = inputsStr.substr(pipeIdx + 3);
        inputsStr = inputsStr.substr(0, pipeIdx);
    }
    explicitStr = inputsStr;
    
    // Parse each section
    auto parsePaths = [&](const std::string& s, std::vector<std::string>& out) {
        std::istringstream iss(s);
        std::string t;
        while (iss >> t) {
            std::string expanded = m_expandVars(t, localVars);
            if (!expanded.empty()) {
                out.push_back(expanded);
            }
        }
    };

    parsePaths(explicitStr, link.explicit_deps);
    parsePaths(implicitStr, link.implicit_deps);
    parsePaths(orderOnlyStr, link.order_only);

    // Insert directly to database during parsing
    mr_dbWriter.insertNinjaBuildLink(link);
}

}  // namespace ninja
}  // namespace kunai
