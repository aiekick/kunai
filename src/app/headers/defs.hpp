#pragma once

#include <set>
#include <vector>
#include <string>
#include <cstdint>

namespace kunai {
namespace datas {

inline std::string KUNAI_DB_NAME{"kunai.db"};

inline std::set<std::string> SOURCE_FILE_EXTS{
    ".c",     // C source
    ".cc",    // C++ source (GNU)
    ".cpp",   // C++ source (standard)
    ".cxx",   // C++ source (alternative)
    ".c++",   // C++ source (alternative)
    ".C",     // C++ source (case-sensitive systems)
    ".inl",   // Inline implementations
    ".ixx",   // C++20 module interface
    ".cppm",  // C++20 module (MSVC)
    ".ccm",   // C++20 module
    ".cxxm",  // C++20 module
    ".c++m"   // C++20 module
};

inline std::set<std::string> HEADER_FILE_EXTS{
    ".h",    // C/C++ header
    ".hh",   // C++ header (GNU)
    ".hpp",  // C++ header (standard)
    ".hxx",  // C++ header (alternative)
    ".h++",  // C++ header (alternative)
    ".H",    // C++ header (case-sensitive systems)
    ".tpp",  // Template implementations
    ".tcc",  // Template implementations (alternative)
    ".ipp",  // Inline/implementation header
    ".inl",  // Inline header (parfois utilisé aussi comme header)
    ".inc"   // Include file (utilisé dans certains projets)
};

inline std::set<std::string> LIBRARY_FILE_EXTS{
    ".a",         // Static library (Unix/Linux/macOS)
    ".so",        // Shared library (Linux/Unix)
    ".dylib",     // Dynamic library (macOS)
    ".lib",       // Static library (Windows) ou import library
    ".dll",       // Dynamic-link library (Windows)
    ".dll.a",     // Import library (MinGW)
    ".framework"  // Framework bundle (macOS) - techniquement un répertoire
};

enum class TargetType {
    NOT_SUPPORTED = 0,  //
    SOURCE,
    HEADER,
    OBJECT,
    LIBRARY,
    BINARY
};

struct BuildLink {
    std::string rule;                        // rule name
    std::string target;                      // first target
    std::vector<std::string> targets;        // all targets
    std::vector<std::string> explicit_deps;  // explicit deps
    std::vector<std::string> implicit_deps;  // After | in build rule
    std::vector<std::string> order_only;     // After || in build rule
};

struct DepsEntry {
    uint64_t mtime{};
    std::string target;
    std::vector<std::string> deps;
};

}  // namespace datas
}  // namespace kunai
