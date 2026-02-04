#pragma once

#include <ezlibs/ezArgs.hpp>

#include <app/loader/loader.h>

#include <cstdint>
#include <filesystem>

namespace kunai {

class Loader;
class App {
private:
    ez::Args m_args;
    std::filesystem::path m_buildDir;
    std::unique_ptr<Loader> mp_loader;

public:
    bool init(int32_t argc, char** argv);
    void unit();
    int32_t run();

private:
    int32_t m_cmdStats() const;
    int32_t m_cmdAllTargetsByType() const;
    int32_t m_cmdPointedTargetsByType() const;
    int32_t m_printTargets(const std::set<std::string>& aTargets) const;
};

}  // namespace kunai
