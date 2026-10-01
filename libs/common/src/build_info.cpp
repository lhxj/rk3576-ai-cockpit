#include "cockpit/common/build_info.hpp"
namespace cockpit {
std::string_view project_name() noexcept { return "rk3576-ai-cockpit"; }
std::string_view starter_version() noexcept { return "0.1.0-scaffold"; }
std::string_view build_architecture() noexcept {
#if defined(__aarch64__)
    return "aarch64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#else
    return "other";
#endif
}
}  // namespace cockpit
