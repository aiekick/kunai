#pragma once

#include <app/headers/defs.hpp>

#include <string>
#include <vector>

namespace kunai {
namespace cmake {

class ITargetWriter {
public:
    struct Target {
        std::string id;
        std::string name;
        datas::TargetType type{datas::TargetType::NOT_SUPPORTED};
        std::vector<std::string> sources;
    };

public:
    virtual ~ITargetWriter() = default;

    // Insert a CMake target (from CMake reply files)
    virtual void insertCMakeTarget(const Target& target) = 0;

    // File extension management
    virtual void addFileExtension(const std::string& ext, datas::TargetType type) = 0;
    virtual datas::TargetType getFileExtensionType(const std::string& ext) const = 0;
};

}  // namespace cmake
}  // namespace kunai
