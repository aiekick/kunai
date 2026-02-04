#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace kunai {
namespace datas {

inline std::vector<std::string> SOURCE_FILE_EXTS{".c", ".cc", ".cpp", ".inl", ".cxx"};
inline std::vector<std::string> HEADER_FILE_EXTS{".h", ".hpp", ".tpp", ".hxx"};
inline std::vector<std::string> LIBRARY_FILE_EXTS{".so", ".a"};

enum class TargetType {
    NOT_SUPPORTED = 0,  //
    SOURCE,
    HEADER,
    OBJECT,
    LIBRARY,
    MODULE,
    BINARY
};

struct BuildLink {
    std::string target;                      // frist target
    std::vector<std::string> targets;        // all targets
    std::string rule;                        // rule name
    std::vector<std::string> explicit_deps;  // explicit deps
    std::vector<std::string> implicit_deps;  // After |
    std::vector<std::string> order_only;     // After ||
};

struct DepsEntry {
    std::string target;
    std::vector<std::string> deps;
    uint64_t mtime{};
};

inline std::string KUNAI_DB_NAME{"kunai.db"};

}  // namespace datas
}  // namespace kunai
