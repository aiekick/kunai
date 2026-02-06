#pragma once

#include <app/headers/defs.hpp>

#include <string>

namespace kunai {

// Forward declarations
namespace cmake {
    struct CMakeTarget;
}

/**
 * Interface for BuildParser to write build links to the database during parsing.
 */
class IBuildLinkWriter {
public:
    virtual ~IBuildLinkWriter() = default;

    // Insert a build link (from ninja build.ninja)
    virtual void insertBuildLink(const datas::BuildLink& link) = 0;
};

/**
 * Interface for DepsParser to write dependency entries to the database during parsing.
 */
class IDepsEntryWriter {
public:
    virtual ~IDepsEntryWriter() = default;

    // Insert a dependency entry (from ninja .ninja_deps)
    virtual void insertDepsEntry(const datas::DepsEntry& deps) = 0;
};

/**
 * Interface for CMakeReplyParser to write CMake targets to the database during parsing.
 */
class ICMakeTargetWriter {
public:
    virtual ~ICMakeTargetWriter() = default;

    // Insert a CMake target (from CMake reply files)
    virtual void insertCMakeTarget(const cmake::CMakeTarget& target) = 0;

    // File extension management
    virtual void addFileExtension(const std::string& ext, datas::TargetType type) = 0;
    virtual datas::TargetType getFileExtensionType(const std::string& ext) const = 0;
};

}  // namespace kunai
