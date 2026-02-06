#pragma once

#include <app/headers/defs.hpp>
#include <app/interfaces/i_ninja_build_writer.h>

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace kunai {
namespace ninja {

class BuildParser {
public:
    static std::pair<std::unique_ptr<BuildParser>, std::string> create(const std::string& aFilePathName, IBuildWriter& arDbWriter);

private:
    std::stringstream m_error;
    std::string m_baseDir;
    IBuildWriter& mr_dbWriter;
    std::unordered_map<std::string, std::string> m_globalVars;
    std::unordered_set<std::string> m_parsedFiles;  // Avoid parsing same file twice

public:
    BuildParser(IBuildWriter& arDbWriter);
    BuildParser(const BuildParser&) = delete;
    BuildParser& operator=(const BuildParser&) = delete;
    std::string getError() const;

private:
    std::string m_getDirectory(const std::string& aFilePathName);
    std::string m_resolvePath(const std::string& aPath);
    bool m_parse(const std::string& aFilePathName);
    // aOpeningOptional by ex if its a non existing include we not want to stop the parsing
    bool m_parseFile(const std::string& aFilePathName, bool aOpeningOptional);
    void m_trim(std::string& arStr);
    bool m_startsWith(const std::string& aStr, const std::string& aPrefix);
    void m_parseVariable(const std::string& aLine, std::unordered_map<std::string, std::string>& aVars);
    std::string m_expandVars(const std::string& aInput, const std::unordered_map<std::string, std::string>& aVars);
    void m_parseBuildStatement(const std::string& aLine, std::ifstream& arFile);
};

}  // namespace ninja
}  // namespace kunai
