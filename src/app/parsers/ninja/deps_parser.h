#pragma once

// see the file format 'doc/ninja_deps.hexpat' for ImHex

#include <app/headers/defs.hpp>
#include <app/interfaces/i_ninja_deps_writer.h>

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

    static std::pair<std::unique_ptr<DepsParser>, std::string> create(
        const std::string& aFilePathName, IDepsWriter& arDbWriter);

private:
    ez::BinBuf m_binBuf;
    std::stringstream m_error;
    IDepsWriter& mr_dbWriter;
    std::vector<std::string> m_paths;                  // ID -> path
    std::unordered_map<uint32_t, size_t> m_pathIndex;  // ID -> index in m_paths

public:
    DepsParser(IDepsWriter& arDbWriter);
    DepsParser(const DepsParser&) = delete;
    DepsParser& operator=(const DepsParser&) = delete;
    std::string getError() const;

private:
    bool m_parse(const std::string& aFilePathName);
};

}  // namespace ninja
}  // namespace kunai
