#include "loader.h"

#include <chrono>

#include <ezlibs/ezTime.hpp>

#include <app/headers/defs.hpp>
#include <app/parsers/ninja/build_parser.h>
#include <app/parsers/ninja/deps_parser.h>
#include <app/parsers/cmake/reply_parser.h>

namespace fs = std::filesystem;

namespace kunai {

std::pair<std::unique_ptr<Loader>, std::string> Loader::create(const std::filesystem::path& buildDir, bool aRebuild) {
    auto pRet = std::make_unique<Loader>();
    std::string error;
    if (!pRet->m_load(buildDir, aRebuild)) {
        error = pRet->getError();
        pRet.reset();
    }
    return std::make_pair(std::move(pRet), error);
}

std::string Loader::getError() const {
    return m_error.str();
}

DataBase::Stats Loader::getStats() const {
    return m_db.getStats();
}

std::vector<std::string> Loader::getAllTargetsByType(datas::TargetType aTargetType) {
    std::vector<std::string> ret;
    double query_timing{};
    {
        ez::time::ScopedTimer t(query_timing);
        ret = m_db.getAllTargetsByType(aTargetType);
    }
    if (!ret.empty()) {
        m_db.setMetadata("perf_query_ms", query_timing);
    }
    return ret;
}

std::vector<std::string> Loader::getPointedTargetsByType(const std::vector<std::string>& sourcePaths, datas::TargetType aTargetType) {
    std::vector<std::string> ret;
    double query_timing{};
    {
        ez::time::ScopedTimer t(query_timing);
        ret = m_db.getPointedTargetsByType(sourcePaths, aTargetType);
    }
    if (!ret.empty()) {
        m_db.setMetadata("perf_query_ms", query_timing);
    }
    return ret;
}

// Check if database needs rebuild based on SHA1 changes
void Loader::m_checkStatus(const fs::path& buildDir, bool aForceRebuild, Loader::Status& aoStatus) {
    // Get file paths
    fs::path buildNinjaPath = buildDir / "build.ninja";
    fs::path ninjaDepsPath = buildDir / ".ninja_deps";

    // Get current file modification times
    std::error_code ec;
    if (fs::exists(buildNinjaPath, ec)) {
        aoStatus.buildNinjaTime = fs::last_write_time(buildNinjaPath, ec);
    }
    if (fs::exists(ninjaDepsPath, ec)) {
        aoStatus.ninjaDepsTime = fs::last_write_time(ninjaDepsPath, ec);
    }

    // Get stored timestamps (stored as nanoseconds since epoch)
    std::string storedBuildTime = m_db.getMetadata("build_ninja_time");
    std::string storedDepsTime = m_db.getMetadata("ninja_deps_time");

    // Convert current file times to nanoseconds for comparison
    auto buildTimeNanos = aoStatus.buildNinjaTime.time_since_epoch().count();
    auto depsTimeNanos = aoStatus.ninjaDepsTime.time_since_epoch().count();

    // Check if timestamps have changed
    bool buildTimeChanged = (storedBuildTime.empty() || storedBuildTime != std::to_string(buildTimeNanos));
    bool depsTimeChanged = (storedDepsTime.empty() || storedDepsTime != std::to_string(depsTimeNanos));

    // Only compute SHA1 if timestamps have changed
    if (buildTimeChanged || aForceRebuild) {
        aoStatus.buildNinjaSha1 = m_computeSha1(buildNinjaPath);
        std::string storedBuildSha1 = m_db.getMetadata("build_ninja_sha1");
        aoStatus.buildNinjaChanged = (aoStatus.buildNinjaSha1 != storedBuildSha1);
    } else {
        aoStatus.buildNinjaChanged = false;
    }

    if (depsTimeChanged || aForceRebuild) {
        aoStatus.ninjaDepsSha1 = m_computeSha1(ninjaDepsPath);
        std::string storedDepsSha1 = m_db.getMetadata("ninja_deps_sha1");
        aoStatus.ninjaDepsChanged = (aoStatus.ninjaDepsSha1 != storedDepsSha1);
    } else {
        aoStatus.ninjaDepsChanged = false;
    }

    aoStatus.needsRebuild = aForceRebuild || (aoStatus.buildNinjaChanged || aoStatus.ninjaDepsChanged);
}

std::string Loader::m_computeSha1(const fs::path& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    ez::sha1 sha;
    sha.add(content);
    sha.finalize();
    return sha.getHex();
}

// Load ninja files into database
bool Loader::m_load(const fs::path& buildDir, bool aForceRebuild) {
    if (!fs::exists(buildDir)) {
        return false;
    }

    double db_loading_timing{};
    {
        ez::time::ScopedTimer t(db_loading_timing);

        if (!m_db.open(buildDir / datas::KUNAI_DB_NAME)) {
            m_error << "Error: " << m_db.getError() << "\n";
            return false;
        }
    }
    m_db.setMetadata("perf_db_loading_ms", db_loading_timing);

    fs::path buildNinjaPath = buildDir / "build.ninja";
    fs::path ninjaDepsPath = buildDir / ".ninja_deps";

    // Check if rebuild needed
    Status status;

    m_checkStatus(buildDir, aForceRebuild, status);

    if (status.needsRebuild) {
        if (!status.buildNinjaSha1.empty()) {
            m_db.setMetadata("build_ninja_time", status.buildNinjaTime.time_since_epoch().count());
        }
        if (!status.ninjaDepsSha1.empty()) {
            m_db.setMetadata("ninja_deps_time", status.ninjaDepsTime.time_since_epoch().count());
        }
    } else {
        return true;  // Nothing to do
    }

    double db_filling_timing{};
    {
        ez::time::ScopedTimer t(db_filling_timing);

        // Begin transaction
        if (!m_db.beginTransaction()) {
            m_error << "Failed to begin transaction: " << m_db.getError();
            return false;
        }

        // Clear and reload
        m_db.clear();

        // Initialize default file extensions
        m_db.initializeDefaultExtensions();

        // Parse build.ninja - data is inserted directly to DB during parsing
        if (fs::exists(buildNinjaPath)) {
            auto tmp_pBuildParser = ninja::BuildParser::create(buildNinjaPath.string(), m_db);
            if (tmp_pBuildParser.first == nullptr) {
                m_db.rollback();
                m_error << "Failed to parse build.ninja: " << tmp_pBuildParser.second;
                return false;
            }
        } else {
            m_db.rollback();
            m_error << "build.ninja is not existing";
            return false;
        }

        // Parse .ninja_deps (optional) - data is inserted directly to DB during parsing
        if (fs::exists(ninjaDepsPath)) {
            auto tmp_pDepsParser = ninja::DepsParser::create(ninjaDepsPath.string(), m_db);
            if (tmp_pDepsParser.first == nullptr) {
                m_db.rollback();
                m_error << "Failed to parse .ninja_deps: " << tmp_pDepsParser.second;
                return false;
            }
        }

        // Parse CMake reply files (optional) - data is inserted directly to DB during parsing
        auto tmp_pCMakeParser = cmake::ReplyParser::create(buildDir.string(), m_db);
        // Note: CMake reply parsing failures are not fatal - it's an optional enhancement

        // Store SHA1s and timestamps
        m_db.setMetadata("build_ninja_sha1", status.buildNinjaSha1);
        m_db.setMetadata("ninja_deps_sha1", status.ninjaDepsSha1);
        m_db.setMetadata("build_ninja_time", status.buildNinjaTime.time_since_epoch().count());
        m_db.setMetadata("ninja_deps_time", status.ninjaDepsTime.time_since_epoch().count());
        m_db.setMetadata("build_dir", buildDir.string());

        // Commit
        if (!m_db.commit()) {
            m_db.rollback();
            m_error << "Failed to commit: " << m_db.getError();
            return false;
        }
    }

    m_db.setMetadata("perf_db_filling_ms", db_filling_timing);

    return true;
}

}  // namespace kunai
