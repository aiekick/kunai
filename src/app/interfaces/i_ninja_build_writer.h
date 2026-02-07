#pragma once

#include <string>
#include <vector>

namespace kunai {
namespace ninja {

class IBuildWriter {
public:
    struct BuildLink {
        std::string rule;                        // rule name
        std::string target;                      // first target
        std::vector<std::string> targets;        // all targets
        std::vector<std::string> explicit_deps;  // explicit deps
        std::vector<std::string> implicit_deps;  // After | in build rule
        std::vector<std::string> order_only;     // After || in build rule
    };

public:
    virtual ~IBuildWriter() = default;

    // Insert a build link (from ninja build.ninja)
    virtual void insertNinjaBuildLink(const BuildLink& link) = 0;
};

}  // namespace ninja
}  // namespace kunai
