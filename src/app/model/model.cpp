#include "model.h"

#include <sqlite3/sqlite3.h>

#include <ezlibs/ezStr.hpp>
#include <ezlibs/ezTime.hpp>

#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace kunai {

using namespace datas;

void DataBase::SqliteDeleter::operator()(sqlite3* apDB) {
    if (apDB) {
        sqlite3_close(apDB);
    }
}

DataBase::DataBase() = default;
DataBase::~DataBase() {
    close();
}

bool DataBase::open(const fs::path& aDbPath) {
    close();

    sqlite3* ptr = nullptr;
    int rc = sqlite3_open(aDbPath.string().c_str(), &ptr);
    if (rc != SQLITE_OK) {
        m_error << sqlite3_errmsg(ptr);
        sqlite3_close(ptr);
        return false;
    }
    mp_db.reset(ptr);

    return m_createSchema();
}

void DataBase::close() {
    mp_db.reset();
}

std::string DataBase::getError() const {
    return m_error.str();
}

///////////////////////////////////////////////////////////////////////////////
// Transaction control

bool DataBase::beginTransaction() {
    return m_exec("BEGIN TRANSACTION;");
}

bool DataBase::commit() {
    return m_exec("COMMIT;");
}

bool DataBase::rollback() {
    return m_exec("ROLLBACK;");
}

///////////////////////////////////////////////////////////////////////////////
// Data insertion

void DataBase::clear() {
    m_exec("DELETE FROM links;");
    m_exec("DELETE FROM targets;");
    m_exec("DELETE FROM metadata;");
}

void DataBase::insertNinjaBuildLink(const ninja::IBuildWriter::BuildLink& link) {
    const auto type = m_getTargetType(link.rule, link.target);
    if (type != TargetType::NOT_SUPPORTED) {
        const int64_t targetId = m_getOrCreateNode(link.target, type);
        // les .o (.cpp.o) sont en explicits
        for (const auto& dep : link.explicit_deps) {
            const auto depType = m_getTargetType("", dep);
            if (depType != TargetType::NOT_SUPPORTED) {
                const int64_t depId = m_getOrCreateNode(dep, depType);
                m_insertLink(targetId, depId);
            }
        }
        // les .so sont en implcities
        for (const auto& dep : link.implicit_deps) {
            const auto depType = m_getTargetType("", dep);
            if (depType != TargetType::NOT_SUPPORTED) {
                const int64_t depId = m_getOrCreateNode(dep, depType);
                m_insertLink(targetId, depId);
            }
        }
        for (const auto& dep : link.order_only) {
            const auto depType = m_getTargetType("", dep);
            if (depType != TargetType::NOT_SUPPORTED) {
                const int64_t depId = m_getOrCreateNode(dep, depType);
                m_insertLink(targetId, depId);
            }
        }
    }
}

void DataBase::insertNinjaDepsEntry(const ninja::IDepsWriter::DepsEntry& deps) {
    const auto type = m_getTargetType("", deps.target);
    if (type != TargetType::NOT_SUPPORTED) {
        const int64_t targetId = m_getOrCreateNode(deps.target, type);
        for (const auto& dep : deps.deps) {
            const auto depType = m_getTargetType("", dep);
            if (depType != TargetType::NOT_SUPPORTED) {
                const int64_t depId = m_getOrCreateNode(dep, depType);
                m_insertLink(targetId, depId);
            }
        }
    }
}

void DataBase::insertCMakeTarget(const cmake::ITargetWriter::Target& target) {
    // Use target name as the target node
    const int64_t targetId = m_getOrCreateNode(target.name, target.type);

    // Link all source files to this target
    for (const auto& source : target.sources) {
        // Determine source type from extension or from database
        TargetType sourceType = getFileExtensionType(source);
        if (sourceType == TargetType::NOT_SUPPORTED) {
            sourceType = m_getTargetType("", source);
        }

        if (sourceType != TargetType::NOT_SUPPORTED) {
            const int64_t sourceId = m_getOrCreateNode(source, sourceType);
            m_insertLink(targetId, sourceId);
        }
    }
}

void DataBase::addFileExtension(const std::string& ext, TargetType type) {
    if (type == TargetType::NOT_SUPPORTED || type == TargetType::OBJECT || type == TargetType::BINARY) {
        return;  // Only SOURCE, HEADER, INPUT, LIBRARY are valid for extensions
    }

    sqlite3_stmt* stmt{nullptr};
    sqlite3_prepare_v2(mp_db.get(), "INSERT OR IGNORE INTO file_extensions (ext, type) VALUES (?, ?)", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, ext.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, static_cast<int32_t>(type));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

datas::TargetType DataBase::getFileExtensionType(const std::string& path) const {
    // Extract extension from path
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) {
        return TargetType::NOT_SUPPORTED;
    }

    std::string ext = path.substr(dotPos);

    sqlite3_stmt* stmt{nullptr};
    if (sqlite3_prepare_v2(mp_db.get(), "SELECT type FROM file_extensions WHERE ext = ? LIMIT 1", -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ext.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int type = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            return static_cast<TargetType>(type);
        }
        sqlite3_finalize(stmt);
    }

    return TargetType::NOT_SUPPORTED;
}

void DataBase::initializeDefaultExtensions() {
    // Initialize with default extensions from defs.hpp
    for (const auto& ext : SOURCE_FILE_EXTS) {
        addFileExtension(ext, TargetType::SOURCE);
    }
    for (const auto& ext : HEADER_FILE_EXTS) {
        addFileExtension(ext, TargetType::HEADER);
    }
    for (const auto& ext : LIBRARY_FILE_EXTS) {
        addFileExtension(ext, TargetType::LIBRARY);
    }
    for (const auto& ext : INPUTS_FILE_EXTS) {
        addFileExtension(ext, TargetType::INPUT);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Metadata

void DataBase::setMetadata(const std::string& key, const std::string& value) {
    sqlite3_stmt* stmt{nullptr};
    sqlite3_prepare_v2(mp_db.get(), "INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?)", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::string DataBase::getMetadata(const std::string& key) {
    std::string ret;
    sqlite3_stmt* stmt{nullptr};
    if (sqlite3_prepare_v2(mp_db.get(), "SELECT value FROM metadata WHERE key = ?", -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (val) ret = val;
        }
        sqlite3_finalize(stmt);
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Queries

DataBase::Stats DataBase::getStats() const {
    Stats stats;

    const char* sql = R"(
        SELECT
            (SELECT COUNT(*) FROM links) AS links,
            (SELECT COUNT(*) FROM targets WHERE type = 1) AS sources,
            (SELECT COUNT(*) FROM targets WHERE type = 2) AS headers,
            (SELECT COUNT(*) FROM targets WHERE type = 3) AS objects,
            (SELECT COUNT(*) FROM targets WHERE type = 4) AS libraries,
            (SELECT COUNT(*) FROM targets WHERE type = 5) AS binaries,
            (SELECT COUNT(*) FROM targets WHERE type = 6) AS inputs,
            (SELECT CAST(value AS REAL) FROM metadata WHERE key = "perf_db_filling_ms"),
            (SELECT CAST(value AS REAL) FROM metadata WHERE key = "perf_db_loading_ms"),
            (SELECT CAST(value AS REAL) FROM metadata WHERE key = "perf_query_ms")
    )";

    sqlite3_stmt* stmt{nullptr};
    if (sqlite3_prepare_v2(mp_db.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.counters.deps = sqlite3_column_int64(stmt, 0);
            stats.counters.sources = sqlite3_column_int64(stmt, 1);
            stats.counters.headers = sqlite3_column_int64(stmt, 2);
            stats.counters.objects = sqlite3_column_int64(stmt, 3);
            stats.counters.libraries = sqlite3_column_int64(stmt, 4);
            stats.counters.binaries = sqlite3_column_int64(stmt, 5);
            stats.counters.inputs = sqlite3_column_int64(stmt, 6);
            stats.timings.dbFilling = sqlite3_column_double(stmt, 7);
            stats.timings.dbLoading = sqlite3_column_double(stmt, 8);
            stats.timings.query = sqlite3_column_double(stmt, 9);
        }
        sqlite3_finalize(stmt);
    }

    return stats;
}

std::vector<std::string> DataBase::getAllTargetsByType(TargetType aTargetType) const {
    std::vector<std::string> ret;
    sqlite3_stmt* stmt{nullptr};
    if (sqlite3_prepare_v2(mp_db.get(), "SELECT path FROM targets WHERE type = ?", -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, static_cast<int32_t>(aTargetType));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ret.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        sqlite3_finalize(stmt);
    }
    return ret;
}

std::vector<std::string> DataBase::getPointedTargetsByType(const std::vector<std::string>& sourcePaths, TargetType aTargetType) const {
    std::vector<std::string> ret;

    if (sourcePaths.empty()) {
        return ret;
    }

    std::string sql = R"(
        WITH RECURSIVE pointed(id) AS (
            SELECT id FROM targets WHERE )";

    // Add conditions for each source path
    for (size_t i = 0; i < sourcePaths.size(); ++i) {
        if (i > 0) sql += " OR ";
        sql += "path = ? OR path LIKE ?";
    }

    sql += R"(
            UNION
            SELECT l.from_id 
            FROM links l
            JOIN pointed a ON l.to_id = a.id
        )
        SELECT DISTINCT path FROM targets 
        WHERE id IN (SELECT id FROM pointed) 
          AND type = ?
    )";

    sqlite3_stmt* stmt{nullptr};
    if (sqlite3_prepare_v2(mp_db.get(), sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        int paramIdx = 1;
        for (const auto& path : sourcePaths) {
            sqlite3_bind_text(stmt, paramIdx++, path.c_str(), -1, SQLITE_TRANSIENT);
            std::string pattern = "%" + path + "%";
            sqlite3_bind_text(stmt, paramIdx++, pattern.c_str(), -1, SQLITE_TRANSIENT);
        }
        sqlite3_bind_int(stmt, paramIdx, static_cast<int32_t>(aTargetType));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (val) {
                ret.push_back(val);
            }
        }
        sqlite3_finalize(stmt);
    }

    return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Private

bool DataBase::m_exec(const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(mp_db.get(), sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        m_error << (err ? err : "Unknown error");
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool DataBase::m_createSchema() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS targets (
            id INTEGER PRIMARY KEY,
            path TEXT UNIQUE NOT NULL,
            type INTEGER DEFAULT 0 -- 0 is not supported, its bug if there is some values to 0
        );

        CREATE TABLE IF NOT EXISTS links (
            from_id INTEGER NOT NULL,
            to_id INTEGER NOT NULL,
            PRIMARY KEY (from_id, to_id),
            FOREIGN KEY (from_id) REFERENCES targets(id),
            FOREIGN KEY (to_id) REFERENCES targets(id)
        );

        CREATE TABLE IF NOT EXISTS metadata (
            key TEXT PRIMARY KEY,
            value TEXT
        );

        CREATE TABLE IF NOT EXISTS file_extensions (
            id INTEGER PRIMARY KEY,
            ext TEXT NOT NULL,
            type INTEGER NOT NULL, -- 1=SOURCE, 2=HEADER, 6=INPUT, 4=LIBRARY
            UNIQUE(ext, type)
        );

        CREATE INDEX IF NOT EXISTS idx_links_to ON links(to_id);
        CREATE INDEX IF NOT EXISTS idx_links_from ON links(from_id);
        CREATE INDEX IF NOT EXISTS idx_targets_source ON targets(type) WHERE type = 1; -- the source type
        CREATE INDEX IF NOT EXISTS idx_targets_header ON targets(type) WHERE type = 2; -- the header type
        CREATE INDEX IF NOT EXISTS idx_targets_object ON targets(type) WHERE type = 3; -- the object type
        CREATE INDEX IF NOT EXISTS idx_targets_library ON targets(type) WHERE type = 4; -- the library type
        CREATE INDEX IF NOT EXISTS idx_targets_binary ON targets(type) WHERE type = 5; -- the binary type
        CREATE INDEX IF NOT EXISTS idx_targets_input ON targets(type) WHERE type = 6; -- the input type
    )";
    return m_exec(schema);
}

TargetType DataBase::m_getTargetType(const std::string& aRule, const std::string& aTarget) const {
    if (!aRule.empty() && aRule != "CUSTOM_COMMAND") {
        if (aRule.find("MODULE") != std::string::npos) {
            return TargetType::LIBRARY;
        } else if (aRule.find("LIBRARY") != std::string::npos) {
            return TargetType::LIBRARY;
        } else if (aRule.find("EXECUTABLE") != std::string::npos) {
            return TargetType::BINARY;
        }
    } else {
        const auto p = aTarget.find_last_of('.');
        if (p != std::string::npos) {
            const auto ext = aTarget.substr(p);
            if (ext == ".o") {
                return TargetType::OBJECT;
            } else if (LIBRARY_FILE_EXTS.find(ext) != LIBRARY_FILE_EXTS.end()) {
                return TargetType::LIBRARY;
            } else if (SOURCE_FILE_EXTS.find(ext) != SOURCE_FILE_EXTS.end()) {
                return TargetType::SOURCE;
            } else if (HEADER_FILE_EXTS.find(ext) != HEADER_FILE_EXTS.end()) {
                return TargetType::HEADER;
            }             
        }
    }
    return TargetType::NOT_SUPPORTED;
}

int64_t DataBase::m_getOrCreateNode(const std::string& path, TargetType type) {
    sqlite3_stmt* stmt{nullptr};

    // Try to find existing
    sqlite3_prepare_v2(mp_db.get(), "SELECT id FROM targets WHERE path = ?", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);

    int64_t id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (id >= 0) {
        // Update type if provided
        if (type != TargetType::NOT_SUPPORTED) {
            sqlite3_prepare_v2(mp_db.get(), "UPDATE targets SET type = ? WHERE id = ?", -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, static_cast<int32_t>(type));
            sqlite3_bind_int64(stmt, 2, id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        return id;
    }

    // Insert new
    sqlite3_prepare_v2(mp_db.get(), "INSERT INTO targets (path, type) VALUES (?, ?)", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, static_cast<int32_t>(type));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return sqlite3_last_insert_rowid(mp_db.get());
}

void DataBase::m_insertLink(int64_t fromId, int64_t toId) {
    sqlite3_stmt* stmt{nullptr};
    sqlite3_prepare_v2(mp_db.get(), "INSERT OR IGNORE INTO links (from_id, to_id) VALUES (?, ?)", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, fromId);
    sqlite3_bind_int64(stmt, 2, toId);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

}  // namespace kunai
