#include "cockpit/common/build_info.hpp"
#include <iostream>
int main() {
    // Explicit checks remain active in Release builds (unlike assert()).
    if (cockpit::project_name() != "rk3576-ai-cockpit") {
        std::cerr << "Unexpected project name\n";
        return 1;
    }
    if (cockpit::starter_version() != "0.1.0-scaffold" || cockpit::build_architecture().empty()) {
        std::cerr << "Invalid build metadata\n";
        return 1;
    }
    return 0;
}
