#include "loader.h"

#include <chrono>

#include <ezlibs/ezTime.hpp>

#include <app/headers/defs.hpp>

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

// Check if database needs rebuild based on SHA1 changes
Loader::Status Loader::checkStatus(const fs::path& buildDir) {
    Status status;

    // Get file paths
    fs::path buildNinjaPath = buildDir / "build.ninja";
    fs::path ninjaDepsPath = buildDir / ".ninja_deps";

    // Get current file modification times
    std::error_code ec;
    if (fs::exists(buildNinjaPath, ec)) {
        status.buildNinjaTime = fs::last_write_time(buildNinjaPath, ec);
    }
    if (fs::exists(ninjaDepsPath, ec)) {
        status.ninjaDepsTime = fs::last_write_time(ninjaDepsPath, ec);
    }

    // Get stored timestamps (stored as nanoseconds since epoch)
    std::string storedBuildTime = m_db.getMetadata("build_ninja_time");
    std::string storedDepsTime = m_db.getMetadata("ninja_deps_time");

    // Convert current file times to nanoseconds for comparison
    auto buildTimeNanos = status.buildNinjaTime.time_since_epoch().count();
    auto depsTimeNanos = status.ninjaDepsTime.time_since_epoch().count();

    // Check if timestamps have changed
    bool buildTimeChanged = (storedBuildTime.empty() || storedBuildTime != ez::str::toStr(buildTimeNanos));
    bool depsTimeChanged = (storedDepsTime.empty() || storedDepsTime != ez::str::toStr(depsTimeNanos));

    // Only compute SHA1 if timestamps have changed
    if (buildTimeChanged) {
        status.buildNinjaSha1 = m_computeSha1(buildNinjaPath);
        std::string storedBuildSha1 = m_db.getMetadata("build_ninja_sha1");
        status.buildNinjaChanged = (status.buildNinjaSha1 != storedBuildSha1);
    } else {
        status.buildNinjaChanged = false;
    }

    if (depsTimeChanged) {
        status.ninjaDepsSha1 = m_computeSha1(ninjaDepsPath);
        std::string storedDepsSha1 = m_db.getMetadata("ninja_deps_sha1");
        status.ninjaDepsChanged = (status.ninjaDepsSha1 != storedDepsSha1);
    } else {
        status.ninjaDepsChanged = false;
    }

    status.needsRebuild = (status.buildNinjaChanged || status.ninjaDepsChanged);

    return status;
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
        m_db.setMetadata("perf_query_ms", ez::str::toStr(query_timing));
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
        m_db.setMetadata("perf_query_ms", ez::str::toStr(query_timing));
    }
    return ret;
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
bool Loader::m_load(const fs::path& buildDir, bool force) {
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
    m_db.setMetadata("perf_db_loading_ms", ez::str::toStr(db_loading_timing));

    fs::path buildNinjaPath = buildDir / "build.ninja";
    fs::path ninjaDepsPath = buildDir / ".ninja_deps";

    // Check if rebuild needed
    Status status = checkStatus(buildDir);

    // If timestamps changed but SHA1s match, update timestamps without rebuild
    if (!force && !status.needsRebuild) {
        // Update timestamps if they were checked and SHA1s were calculated but matched
        if (!status.buildNinjaSha1.empty()) {
            m_db.setMetadata("build_ninja_time", ez::str::toStr(status.buildNinjaTime.time_since_epoch().count()));
        }
        if (!status.ninjaDepsSha1.empty()) {
            m_db.setMetadata("ninja_deps_time", ez::str::toStr(status.ninjaDepsTime.time_since_epoch().count()));
        }
        return true;  // Nothing to do
    }

    double db_filling_timing{};
    {
        ez::time::ScopedTimer t(db_filling_timing);

       
    // Parse build.ninja
       std::unique_ptr<ninja::BuildParser> pBuildParser;
       if (fs::exists(buildNinjaPath)) {
           auto tmp_pBuildParser = ninja::BuildParser::create(buildNinjaPath.string());
           if (tmp_pBuildParser.first == nullptr) {
               m_error << "Failed to parse build.ninja: " << tmp_pBuildParser.second;
               return false;
           }
           pBuildParser = std::move(tmp_pBuildParser.first);
       } else {
           m_error << "build.ninja is not existing";
           return false;
       }

       // Parse .ninja_deps (optional)
       std::unique_ptr<ninja::DepsParser> pDepsParser;
       if (fs::exists(ninjaDepsPath)) {
           auto tmp_pDepsParser = ninja::DepsParser::create(ninjaDepsPath.string());
           if (tmp_pDepsParser.first == nullptr) {
               m_error << "Failed to parse .ninja_deps: " << tmp_pDepsParser.second;
               return false;
           }
           pDepsParser = std::move(tmp_pDepsParser.first);
       }

       if (pBuildParser->empty() && pDepsParser->empty()) {
           return true;  // Nothing to do
       }

       // Begin transaction
       if (!m_db.beginTransaction()) {
           m_error << "Failed to begin transaction: " << m_db.getError();
           return false;
       }

       // Clear and reload
       m_db.clear();

       // Insert build links
       for (const auto& link : pBuildParser->getLinks()) {
           m_db.insertBuildLink(link);
       }

       // Insert discovered deps
       if (pDepsParser != nullptr) {
           for (const auto& entry : pDepsParser->getEntries()) {
               m_db.insertDepsEntry(entry);
           }
       }

       // Store SHA1s and timestamps
       m_db.setMetadata("build_ninja_sha1", status.buildNinjaSha1);
       m_db.setMetadata("ninja_deps_sha1", status.ninjaDepsSha1);
       m_db.setMetadata("build_ninja_time", ez::str::toStr(status.buildNinjaTime.time_since_epoch().count()));
       m_db.setMetadata("ninja_deps_time", ez::str::toStr(status.ninjaDepsTime.time_since_epoch().count()));
       m_db.setMetadata("build_dir", buildDir.string());

       // Commit
       if (!m_db.commit()) {
           m_db.rollback();
           m_error << "Failed to commit: " << m_db.getError();
           return false;
       }
    }

    m_db.setMetadata("perf_db_filling_ms", ez::str::toStr(db_filling_timing));

    return true;
}

}  // namespace kunai
