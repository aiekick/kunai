#include <app/app.h>

#include <cstdint>
#include <iostream>

int main(int argc, char** argv) {
    int32_t ret = EXIT_FAILURE;
    try {
        kunai::App app;
        if (app.init(argc, argv)) {
            ret = app.run();
        }
        app.unit();
    } catch (const std::exception& e) {
        std::cerr << "Exception " << e.what() << std::endl;
    }
    return ret;
}
