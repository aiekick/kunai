#pragma once

#include <app/headers/defs.hpp>
#include <app/interfaces/i_cmake_entry_wirter.h>

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

namespace kunai {
namespace cmake {

class ReplyParser {
public:
    static std::pair<std::unique_ptr<ReplyParser>, std::string> create(
        const std::string& aBuildDir,
        ITargetWriter& arDbWriter);

private:
    std::stringstream m_error;
    std::string m_buildDir;
    ITargetWriter& mr_dbWriter;

public:
    ReplyParser(ITargetWriter& arDbWriter);
    ReplyParser(const ReplyParser&) = delete;
    ReplyParser& operator=(const ReplyParser&) = delete;

    std::string getError() const;

private:
    bool m_parse(const std::string& aBuildDir);
    bool m_parseIndexFile(const std::string& aIndexPath);
    bool m_parseCodeModel(const std::string& aCodeModelPath);
    bool m_parseTarget(const std::string& aTargetPath, ITargetWriter::Target& target);
    std::string m_findLatestFile(const std::string& aDir, const std::string& aPrefix);
};

}  // namespace cmake
}  // namespace kunai
