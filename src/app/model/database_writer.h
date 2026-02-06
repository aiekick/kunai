#pragma once

#include <app/headers/defs.hpp>

#include <string>

namespace kunai {

// Forward declarations
namespace cmake {
    struct CMakeTarget;
}

/**
 * Interface for parsers to write data to the database during parsing.
 * This allows parsers to decide what data to store, instead of storing
 * everything in memory first and then filtering during insertion.
 */
class IDataBaseWriter {
public:
    virtual ~IDataBaseWriter() = default;

    // Insert a build link (from ninja build.ninja)
    virtual void insertBuildLink(const datas::BuildLink& link) = 0;

    // Insert a dependency entry (from ninja .ninja_deps)
    virtual void insertDepsEntry(const datas::DepsEntry& deps) = 0;

    // Insert a CMake target (from CMake reply files)
    virtual void insertCMakeTarget(const cmake::CMakeTarget& target) = 0;

    // File extension management
    virtual void addFileExtension(const std::string& ext, datas::TargetType type) = 0;
    virtual datas::TargetType getFileExtensionType(const std::string& ext) const = 0;
};

}  // namespace kunai
