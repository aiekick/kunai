#pragma once

#include <app/headers/defs.hpp>

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
    static std::pair<std::unique_ptr<CMakeReplyParser>, std::string> create(const std::string& aBuildDir);

private:
    std::stringstream m_error;
    std::string m_buildDir;
    std::vector<CMakeTarget> m_targets;

public:
    CMakeReplyParser() = default;
    CMakeReplyParser(const CMakeReplyParser&) = delete;
    CMakeReplyParser& operator=(const CMakeReplyParser&) = delete;

    const std::vector<CMakeTarget>& getTargets() const;
    std::string getError() const;

    // Is Empty
    bool empty() const;

private:
    bool m_parse(const std::string& aBuildDir);
    bool m_parseIndexFile(const std::string& aIndexPath);
    bool m_parseCodeModel(const std::string& aCodeModelPath);
    bool m_parseTarget(const std::string& aTargetPath, CMakeTarget& target);
    std::string m_findLatestFile(const std::string& aDir, const std::string& aPrefix);
};

}  // namespace cmake
}  // namespace kunai
