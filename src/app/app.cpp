#include "app.h"

#include <app/headers/kunaiBuild.h>
#include <app/model/model.h>

#include <ezlibs/ezApp.hpp>
#include <ezlibs/ezArgs.hpp>
#include <ezlibs/ezFmt.hpp>

#include <cstring>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace kunai {

bool App::init(int32_t argc, char** argv) {
    bool set_current_dir{false};
#ifdef _MSC_VER
    set_current_dir = true;
#endif
    ez::App app(argc, argv, set_current_dir);
    m_args = ez::Args(kunai_Label, "--help");
    m_args.addHeader("parse Ninja files and Find which executables to rebuild for changed file(s)");

    m_args.addPositional("build_dir").help("The build directory", "<build_dir>");
    m_args.addOptional("--rebuild").help("Force the kunia database rebuild", {});

    // command stats
    m_args.addCommand("stats").help("Get stats of the kunai database", {});

    // command all
    auto& cmd_all = m_args.addCommand("all").help("Get all targets by type", {});
    cmd_all.addOptional("-b/--bins").help("Get binaries targets", {});
    cmd_all.addOptional("-l/--libs").help("Get libraries targets", {});
    cmd_all.addOptional("-s/--sources").help("Get sources targets", {});
    cmd_all.addOptional("-h/--headers").help("Get headers targets", {});
    cmd_all.addOptional("--match").delimiter(' ').help("match pattern for filtering targets (ex : --match test_*). not case sensitive", "<pattern>");

    // command pointed
    auto& cmd_pointed = m_args.addCommand("pointed").help("Get targets pointed by modified files", {});
    cmd_pointed.addOptional("-b/--bins").help("Get binaries targets", {});
    cmd_pointed.addOptional("-l/--libs").help("Get libraries targets", {});
    cmd_pointed.addOptional("-s/--sources").help("Get sources targets", {});
    cmd_pointed.addOptional("-h/--headers").help("Get headers targets", {});
    cmd_pointed.addOptional("--match").delimiter(' ').help("match pattern for filtering targets (ex : --match test_*). not case sensitive", "<pattern>");
    cmd_pointed.addPositional("source_files")
        .help("The source file non case sensitive pattern. Can be a sub-string without wildcards", "<source_files>")
        .arrayUnlimited();

    if (m_args.parse(argc, argv)) {
        // build dir
        auto build_dir = m_args.getValue<std::string>("build_dir");
        if (build_dir == ".") {
            build_dir = fs::current_path().string();
        }
        if (build_dir.back() == '/') {
            build_dir = build_dir.substr(0U, build_dir.size() - 1U);
        }
        m_buildDir = build_dir;

        {  // loader
            auto tmp_pLoader = Loader::create(m_buildDir, m_args.isPresent("rebuild"));
            if (tmp_pLoader.first == nullptr) {
                std::cerr << "Error loading build dir " << m_buildDir << " : " << tmp_pLoader.second << std::endl;
                return false;
            }
            mp_loader = std::move(tmp_pLoader.first);
        }

        return true;
    } else {
        m_args.printErrors(" - ");
        std::cout << "--------------\nHelp : \n\n";
        std::cout << kunai_Label << " v" << kunai_BuildId << std::endl;
        m_args.printHelp();
    }
    return false;
}

int32_t App::run() {
    int32_t ret{EXIT_FAILURE};
    try {
        if (m_args.isCommand("stats")) {
            ret = m_cmdStats();
        } else if (m_args.isCommand("all")) {
            ret = m_cmdAllTargetsByType();
        } else if (m_args.isCommand("pointed")) {
            ret = m_cmdPointedTargetsByType();
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Err : " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown err" << std::endl;
    }
    return ret;
}

void App::unit() {
}

int32_t App::m_cmdStats() const {
    const auto stats = mp_loader->getStats();
    {
        ez::TableFormatter tbl({"Stats", ""});
        tbl.addRow({"Database", (m_buildDir / datas::KUNAI_DB_NAME).string()});
        tbl.addRow({"Dependencies", ez::str::toStr(stats.counters.deps)});
        tbl.addRow({"Sources", ez::str::toStr(stats.counters.sources)});
        tbl.addRow({"Headers", ez::str::toStr(stats.counters.headers)});
        tbl.addRow({"Objects", ez::str::toStr(stats.counters.objects)});
        tbl.addRow({"Libraries", ez::str::toStr(stats.counters.libraries)});
        tbl.addRow({"Binaries", ez::str::toStr(stats.counters.binaries)});
        tbl.print("", std::cout);
    }
    {
        ez::TableFormatter tbl({"Perfos", "Last measures"});
        tbl.addRow({"db filling", ez::str::toStr(stats.timings.dbFilling) + " ms"});
        tbl.addRow({"db loading", ez::str::toStr(stats.timings.dbLoading) + " ms"});
        tbl.addRow({"last query", ez::str::toStr(stats.timings.query) + " ms"});
        tbl.print("", std::cout);
    }
    return EXIT_SUCCESS;
}

int32_t App::m_cmdAllTargetsByType() const {
    std::set<std::string> targets;
    if (m_args.isPresent("sources")) {
        const auto sources = mp_loader->getAllTargetsByType(datas::TargetType::SOURCE);
        targets.insert(sources.begin(), sources.end());
    }
    if (m_args.isPresent("headers")) {
        const auto headers = mp_loader->getAllTargetsByType(datas::TargetType::HEADER);
        targets.insert(headers.begin(), headers.end());
    }
    if (m_args.isPresent("libs")) {
        const auto libs = mp_loader->getAllTargetsByType(datas::TargetType::LIBRARY);
        targets.insert(libs.begin(), libs.end());
    }
    if (m_args.isPresent("bins")) {
        const auto bins = mp_loader->getAllTargetsByType(datas::TargetType::BINARY);
        targets.insert(bins.begin(), bins.end());
    }
    return m_printTargets(targets);
}

int32_t App::m_cmdPointedTargetsByType() const {
    const auto files = m_args.getArrayValues("source_files");
    std::set<std::string> targets;
    if (m_args.isPresent("sources")) {
        const auto sources = mp_loader->getPointedTargetsByType(files, datas::TargetType::SOURCE);
        targets.insert(sources.begin(), sources.end());
    }
    if (m_args.isPresent("headers")) {
        const auto headers = mp_loader->getPointedTargetsByType(files, datas::TargetType::HEADER);
        targets.insert(headers.begin(), headers.end());
    }
    if (m_args.isPresent("libs")) {
        const auto libs = mp_loader->getPointedTargetsByType(files, datas::TargetType::LIBRARY);
        targets.insert(libs.begin(), libs.end());
    }
    if (m_args.isPresent("bins")) {
        const auto bins = mp_loader->getPointedTargetsByType(files, datas::TargetType::BINARY);
        targets.insert(bins.begin(), bins.end());
    }
    return m_printTargets(targets);
}

int32_t App::m_printTargets(const std::set<std::string>& aTargets) const {
    if (aTargets.empty()) {
        return EXIT_FAILURE;
    }
    auto pattern = ez::str::toLower(m_args.getValue<std::string>("match"));
    for (const auto& target : aTargets) {
        if ((pattern.empty()) ||  //
            (!ez::str::searchForPatternWithWildcards(ez::str::toLower(target), pattern).empty())) {
            std::cout << target << "\n";
        }
    }
    return EXIT_SUCCESS;
}

}  // namespace kunai
