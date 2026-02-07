#include "deps_parser.h"

namespace kunai {
namespace ninja {

std::pair<std::unique_ptr<DepsParser>, std::string> DepsParser::create(
    const std::string& aFilePathName,
    IDepsWriter& arDbWriter) {
    auto pRet = std::make_unique<DepsParser>(arDbWriter);
    std::string error;
    if (!pRet->m_parse(aFilePathName)) {
        error = pRet->getError();
        pRet.reset();
    }
    return std::make_pair(std::move(pRet), error);
}

DepsParser::DepsParser(IDepsWriter& arDbWriter) : mr_dbWriter(arDbWriter) {
}

std::string DepsParser::getError() const {  //
    return m_error.str();
}

bool DepsParser::m_parse(const std::string& aFilePathName) {
    const auto bytes = ez::file::loadFileToBin(aFilePathName);
    if (bytes.empty()) {
        return false;
    }

    m_binBuf.setDatas(bytes);

    size_t pos = 0;

    // Magic number
    char signature[13] = {0};
    m_binBuf.readArrayLE<char>(pos, signature, 12);
    if (strncmp(signature, "# ninjadeps\n", 12)) {
        m_error << "Invalid signature";
        return false;
    }

    // Version (u32 LE)
    uint32_t version = m_binBuf.readValueLE<uint32_t>(pos);
    if (version != 3 && version != 4) {
        m_error << "Unsupported version: " << std::to_string(version);
        return false;
    }

    // Records
    uint32_t nextPathId = 0;
    while (pos < m_binBuf.size()) {
        if (pos + 4 > m_binBuf.size()) {
            break;
        }

        uint32_t header = m_binBuf.readValueLE<uint32_t>(pos);
        bool isDeps = (header & 0x80000000) != 0;
        uint32_t payloadSize = header & 0x7FFFFFFF;

        if (payloadSize == 0) {
            continue;
        }
        if (pos + payloadSize > m_binBuf.size()) {
            m_error << "Truncated record at offset " << std::to_string(pos);
            return false;
        }

        size_t recordEnd = pos + payloadSize;

        if (!isDeps) {
            // PathRecord: path string (null-terminated, padded) + checksum
            // Le checksum est dans les 4 derniers bytes
            uint32_t stringSize = payloadSize - 4;

            std::string path;
            path.reserve(stringSize);
            for (uint32_t i = 0; i < stringSize; ++i) {
                char c = static_cast<char>(m_binBuf[pos + i]);
                if (c == '\0') {
                    break;
                }
                path += c;
            }

            m_paths.push_back(path);
            m_pathIndex[nextPathId] = m_paths.size() - 1;
            nextPathId++;

            pos = recordEnd;
        } else {
            // DepsRecord: output_id (u32) + mtime (u64 v4 / u32 v3) + dep_ids[]
            IDepsWriter::DepsEntry entry;

            uint32_t outputId = m_binBuf.readValueLE<uint32_t>(pos);

            uint64_t mtime;
            if (version == 4) {
                mtime = m_binBuf.readValueLE<uint64_t>(pos);
            } else {
                mtime = m_binBuf.readValueLE<uint32_t>(pos);
            }
            entry.mtime = mtime;

            // Output path
            auto it = m_pathIndex.find(outputId);
            if (it != m_pathIndex.end()) {
                entry.target = m_paths[it->second];
            } else {
                entry.target = "<unknown:" + std::to_string(outputId) + ">";
            }

            // deps
            while (pos < recordEnd) {
                uint32_t depId = m_binBuf.readValueLE<uint32_t>(pos);
                auto depIt = m_pathIndex.find(depId);
                if (depIt != m_pathIndex.end()) {
                    entry.deps.push_back(m_paths[depIt->second]);
                }
            }

            // Insert directly to database during parsing
            mr_dbWriter.insertNinjaDepsEntry(entry);
        }
    }

    return true;
}

}  // namespace ninja
}  // namespace kunai
