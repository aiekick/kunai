#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace kunai {
namespace ninja {

class IDepsWriter {
public:
    struct DepsEntry {
        uint64_t mtime{};
        std::string target;
        std::vector<std::string> deps;
    };

public:
    virtual ~IDepsWriter() = default;

    // Insert a dependency entry (from ninja .ninja_deps)
    virtual void insertNinjaDepsEntry(const DepsEntry& deps) = 0;
};

}  // namespace ninja
}  // namespace kunai
