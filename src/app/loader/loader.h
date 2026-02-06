#pragma once

/*
 * NinjaLoader - Loads ninja files into NinjaDb
 * 
 * Handles:
 *   - Parsing build.ninja and .ninja_deps
 *   - SHA1 checksums to detect changes
 *   - Deciding whether to rebuild the database
 */

#include <app/model/model.h>

#include <ezlibs/ezSha.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>

namespace kunai {

class Loader {
public:
    struct Status {
        bool needsRebuild = false;
        bool buildNinjaChanged = false;
        bool ninjaDepsChanged = false;
        std::string buildNinjaSha1;
        std::string ninjaDepsSha1;
        std::filesystem::file_time_type buildNinjaTime;
        std::filesystem::file_time_type ninjaDepsTime;
    };

    static std::pair<std::unique_ptr<Loader>, std::string> create(  //
        const std::filesystem::path& buildDir,
        bool aRebuild = false);

private:
    DataBase m_db;
    std::stringstream m_error;

public:
    Loader() = default;
    Loader(const Loader&) = delete;
    Loader& operator=(const Loader&) = delete;

    // get the last error
    std::string getError() const;

    // database getters
    DataBase::Stats getStats() const;
    std::vector<std::string> getAllTargetsByType(datas::TargetType aTargetType) ;
    std::vector<std::string> getPointedTargetsByType(const std::vector<std::string>& sourcePaths, datas::TargetType aTargetType) ;

private:
    // Check if database needs rebuild based on file date and SHA1 changes
    void m_checkStatus(const std::filesystem::path& buildDir, bool aForceRebuild, Loader::Status& aoStatus);

    // compute the sah1 of a file
    std::string m_computeSha1(const std::filesystem::path& filepath);

    // load ninja file in database
    bool m_load(const std::filesystem::path& buildDir, bool aForceRebuild);
};

}  // namespace ninja
