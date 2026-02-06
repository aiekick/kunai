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
    ".inl",   // Inline implementations
};

inline std::set<std::string> HEADER_FILE_EXTS{
    ".h",    // C/C++ header
    ".hh",   // C++ header (GNU)
    ".hpp",  // C++ header (standard)
    ".hxx",  // C++ header (alternative)
    ".tpp",  // Template implementations
    ".inc"   // Include file (utilis� dans certains projets)
};

inline std::set<std::string> INPUTS_FILE_EXTS{
    ".ini",  // text params file
    ".log",  // log file
    ".txt",  // text params file
    ".xml",  // xml params file
    ".csv",  // csv params
    ".bin",  // binary params file
};

inline std::set<std::string> LIBRARY_FILE_EXTS{
    ".a",         // Static library (Unix/Linux/macOS)
    ".so",        // Shared library (Linux/Unix)
    ".dylib",     // Dynamic library (macOS)
    ".lib",       // Static library (Windows) ou import library
    ".dll",       // Dynamic-link library (Windows)
    ".dll.a",     // Import library (MinGW)
    ".framework"  // Framework bundle (macOS) - techniquement un r�pertoire
};

enum class TargetType {
    NOT_SUPPORTED = 0,  //
    SOURCE,
    HEADER,
    OBJECT,
    LIBRARY,
    BINARY,
    INPUT
};

}  // namespace datas
}  // namespace kunai
