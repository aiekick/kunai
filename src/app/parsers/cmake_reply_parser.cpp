#include "cmake_reply_parser.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace kunai {
namespace cmake {

std::pair<std::unique_ptr<CMakeReplyParser>, std::string> CMakeReplyParser::create(
    const std::string& aBuildDir,
    ICMakeTargetWriter* apDbWriter) {
    auto pRet = std::make_unique<CMakeReplyParser>();
    pRet->mp_dbWriter = apDbWriter;
    std::string error;
    if (!pRet->m_parse(aBuildDir)) {
        error = pRet->getError();
        pRet.reset();
    }
    return std::make_pair(std::move(pRet), error);
}

std::string CMakeReplyParser::getError() const {
    return m_error.str();
}

bool CMakeReplyParser::m_parse(const std::string& aBuildDir) {
    m_buildDir = aBuildDir;

    // Check for .cmake/api/v1/reply directory
    fs::path replyDir = fs::path(aBuildDir) / ".cmake" / "api" / "v1" / "reply";
    if (!fs::exists(replyDir)) {
        // Not an error - CMake file API might not be enabled
        return true;
    }

    // Find the latest index file
    std::string indexFile = m_findLatestFile(replyDir.string(), "index-");
    if (indexFile.empty()) {
        m_error << "No index file found in CMake reply directory";
        return false;
    }

    return m_parseIndexFile(indexFile);
}

std::string CMakeReplyParser::m_findLatestFile(const std::string& aDir, const std::string& aPrefix) {
    std::vector<std::string> files;

    try {
        for (const auto& entry : fs::directory_iterator(aDir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.find(aPrefix) == 0 && filename.find(".json") != std::string::npos) {
                    files.push_back(entry.path().string());
                }
            }
        }
    } catch (...) {
        return {};
    }

    if (files.empty()) {
        return {};
    }

    // Sort and return the latest (they're timestamped)
    std::sort(files.begin(), files.end());
    return files.back();
}

// Simple JSON string value extractor
static std::string extractJsonString(const std::string& line, const std::string& key) {
    size_t keyPos = line.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return {};
    }

    size_t colonPos = line.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return {};
    }

    size_t firstQuote = line.find('"', colonPos);
    if (firstQuote == std::string::npos) {
        return {};
    }

    size_t secondQuote = line.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) {
        return {};
    }

    return line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

bool CMakeReplyParser::m_parseIndexFile(const std::string& aIndexPath) {
    std::ifstream file(aIndexPath);
    if (!file.is_open()) {
        m_error << "Cannot open index file: " << aIndexPath;
        return false;
    }

    std::string line;
    std::string codeModelFile;

    // Find the codemodel-v2 jsonFile reference
    while (std::getline(file, line)) {
        if (line.find("codemodel-v2") != std::string::npos) {
            // Look for jsonFile in the next few lines
            for (int i = 0; i < 10 && std::getline(file, line); ++i) {
                std::string jsonFile = extractJsonString(line, "jsonFile");
                if (!jsonFile.empty() && jsonFile.find("codemodel-v2") != std::string::npos) {
                    codeModelFile = jsonFile;
                    break;
                }
            }
            if (!codeModelFile.empty()) {
                break;
            }
        }
    }

    if (codeModelFile.empty()) {
        // No codemodel found, not an error
        return true;
    }

    fs::path replyDir = fs::path(aIndexPath).parent_path();
    fs::path codeModelPath = replyDir / codeModelFile;

    return m_parseCodeModel(codeModelPath.string());
}

bool CMakeReplyParser::m_parseCodeModel(const std::string& aCodeModelPath) {
    std::ifstream file(aCodeModelPath);
    if (!file.is_open()) {
        m_error << "Cannot open codemodel file: " << aCodeModelPath;
        return false;
    }

    std::string line;
    std::vector<std::string> targetFiles;
    bool inTargetsArray = false;

    // Parse to find all target jsonFile references
    while (std::getline(file, line)) {
        if (line.find("\"targets\"") != std::string::npos) {
            inTargetsArray = true;
            continue;
        }

        if (inTargetsArray) {
            if (line.find(']') != std::string::npos) {
                break;
            }

            std::string jsonFile = extractJsonString(line, "jsonFile");
            if (!jsonFile.empty()) {
                targetFiles.push_back(jsonFile);
            }
        }
    }

    // Parse each target file
    fs::path replyDir = fs::path(aCodeModelPath).parent_path();
    for (const auto& targetFile : targetFiles) {
        CMakeTarget target;
        fs::path targetPath = replyDir / targetFile;
        if (m_parseTarget(targetPath.string(), target)) {
            // Insert directly to database during parsing
            if (mp_dbWriter) {
                mp_dbWriter->insertCMakeTarget(target);
            }
        }
    }

    return true;
}

bool CMakeReplyParser::m_parseTarget(const std::string& aTargetPath, CMakeTarget& target) {
    std::ifstream file(aTargetPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    bool inSourcesArray = false;

    while (std::getline(file, line)) {
        // Extract target id
        if (target.id.empty()) {
            std::string id = extractJsonString(line, "id");
            if (!id.empty()) {
                target.id = id;
            }
        }

        // Extract target name
        if (target.name.empty()) {
            std::string name = extractJsonString(line, "name");
            if (!name.empty()) {
                target.name = name;
            }
        }

        // Extract target type
        if (target.type.empty()) {
            std::string type = extractJsonString(line, "type");
            if (!type.empty()) {
                target.type = type;
            }
        }

        // Parse sources array
        if (line.find("\"sources\"") != std::string::npos) {
            inSourcesArray = true;
            continue;
        }

        if (inSourcesArray) {
            if (line.find(']') != std::string::npos) {
                inSourcesArray = false;
                continue;
            }

            std::string path = extractJsonString(line, "path");
            if (!path.empty()) {
                // Make path absolute if relative
                if (!path.empty() && path[0] != '/') {
                    // Relative to build directory
                    fs::path absPath = fs::path(m_buildDir) / path;
                    try {
                        if (fs::exists(absPath)) {
                            target.sources.push_back(fs::canonical(absPath).string());
                        } else {
                            target.sources.push_back(absPath.string());
                        }
                    } catch (...) {
                        target.sources.push_back(absPath.string());
                    }
                } else {
                    target.sources.push_back(path);
                }
            }
        }
    }

    return !target.id.empty();
}

}  // namespace cmake
}  // namespace kunai
