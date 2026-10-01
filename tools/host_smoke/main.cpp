#include "cockpit/common/build_info.hpp"
#include <iostream>
#include <string_view>
int main(int argc, char** argv) {
    if (argc > 2 || (argc == 2 && std::string_view(argv[1]) != "--self-test"
                    && std::string_view(argv[1]) != "--version")) {
        std::cerr << "Usage: cockpit_host_smoke [--self-test|--version]\n";
        return 2;
    }
    std::cout << cockpit::project_name() << ' ' << cockpit::starter_version() << '\n'
              << "build_architecture=" << cockpit::build_architecture() << '\n'
              << "mode=scaffold; hardware_access=none\n"
              << "camera/audio/ui/inference/rpmsg=NOT_IMPLEMENTED\n";
    return 0;
}
