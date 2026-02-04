#pragma once

// see the file format 'doc/ninja_deps.hexpat' for ImHex

#include <app/headers/defs.hpp>

#include <ezlibs/ezFile.hpp>
#include <ezlibs/ezBinBuf.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <unordered_map>

namespace kunai {
namespace ninja {

class DepsParser {
public:

    static std::pair<std::unique_ptr<DepsParser>, std::string> create(const std::string& aFilePathName);

private:
    ez::BinBuf m_binBuf;
    std::stringstream m_error;
    std::vector<datas::DepsEntry> m_entries;
    std::vector<std::string> m_paths;                  // ID -> path
    std::unordered_map<uint32_t, size_t> m_pathIndex;  // ID -> index in m_paths

public:
    DepsParser() = default;
    DepsParser(const DepsParser&) = delete;
    DepsParser& operator=(const DepsParser&) = delete;

    const std::vector<datas::DepsEntry>& getEntries() const;
    std::string getError() const;

    // Utility: get all paths that depend on a given source
    std::vector<std::string> getDepsForFile(const std::string& aSourceFile) const;

    // Is Empty
    bool empty() const;

private:
    bool m_parse(const std::string& aFilePathName);
};

}  // namespace ninja
}  // namespace kunai
