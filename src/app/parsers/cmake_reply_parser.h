#pragma once

#include <app/headers/defs.hpp>
#include <app/model/database_writer.h>

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

namespace kunai {
namespace cmake {

struct CMakeTarget {
    std::string id;
    std::string name;
    std::string type;  // EXECUTABLE, STATIC_LIBRARY, SHARED_LIBRARY, etc.
    std::vector<std::string> sources;
};

class CMakeReplyParser {
public:
    static std::pair<std::unique_ptr<CMakeReplyParser>, std::string> create(
        const std::string& aBuildDir,
        IDataBaseWriter* apDbWriter);

private:
    std::stringstream m_error;
    std::string m_buildDir;
    IDataBaseWriter* mp_dbWriter;

public:
    CMakeReplyParser() = default;
    CMakeReplyParser(const CMakeReplyParser&) = delete;
    CMakeReplyParser& operator=(const CMakeReplyParser&) = delete;

    std::string getError() const;

private:
    bool m_parse(const std::string& aBuildDir);
    bool m_parseIndexFile(const std::string& aIndexPath);
    bool m_parseCodeModel(const std::string& aCodeModelPath);
    bool m_parseTarget(const std::string& aTargetPath, CMakeTarget& target);
    std::string m_findLatestFile(const std::string& aDir, const std::string& aPrefix);
};

}  // namespace cmake
}  // namespace kunai
