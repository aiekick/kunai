#pragma once

#include <sqlite3/sqlite3.hpp>

#include <app/headers/defs.hpp>
#include <app/model/database_writer.h>

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <filesystem>
#include <type_traits>

namespace kunai {
namespace cmake {
    struct CMakeTarget;  // Forward declaration
}

class DataBase : public IBuildLinkWriter, public IDepsEntryWriter, public ICMakeTargetWriter {
public:
    struct Stats {
        struct Counter {
            int64_t deps{};
            int64_t sources{};
            int64_t headers{};
            int64_t objects{};
            int64_t libraries{};
            int64_t binaries{};
            int64_t inputs{};
        } counters;
        struct Timing {
            double dbFilling{};
            double dbLoading{};
            double query{};
        } timings; // Ms
    };

private:
    struct SqliteDeleter {
        void operator()(sqlite3* apDB) {
            if (apDB) {
                sqlite3_close(apDB);
            }
        }
    };
    std::unique_ptr<sqlite3, SqliteDeleter> mp_db;
    mutable std::stringstream m_error;

public:
    DataBase();
    ~DataBase();

    DataBase(const DataBase&) = delete;
    DataBase& operator=(const DataBase&) = delete;

    // Open/create database
    bool open(const std::filesystem::path& dbPath);
    void close();

    // Transaction
    bool beginTransaction();
    bool commit();
    bool rollback();

    // Insertions
    void clear();
    void insertBuildLink(const datas::BuildLink& link) override;
    void insertDepsEntry(const datas::DepsEntry& deps) override;
    void insertCMakeTarget(const cmake::CMakeTarget& target) override;

    // File extension management
    void addFileExtension(const std::string& ext, datas::TargetType type) override;
    datas::TargetType getFileExtensionType(const std::string& ext) const override;
    void initializeDefaultExtensions();

    // Metadata
    void setMetadata(const std::string& key, const std::string& value);

    // Template version for automatic conversion of numeric types
    template<typename T>
    typename std::enable_if<std::is_arithmetic<T>::value, void>::type
    setMetadata(const std::string& key, T value) {
        setMetadata(key, std::to_string(value));
    }

    std::string getMetadata(const std::string& key);

    // Queries
    Stats getStats() const;
    std::vector<std::string> getAllTargetsByType(datas::TargetType aTargetType) const;
    std::vector<std::string> getPointedTargetsByType(const std::vector<std::string>& sourcePaths, datas::TargetType aTargetType) const;

    // Infos
    std::string getError() const;

private:
    bool m_exec(const char* sql);
    bool m_createSchema();

    // Check if a rule looks like an executable linker
    datas::TargetType m_getTargetType(const std::string& aRule, const std::string& aTarget) const;

    int64_t m_getOrCreateNode(const std::string& path, datas::TargetType type);
    void m_insertLink(int64_t fromId, int64_t toId);
};

}  // namespace kunai
