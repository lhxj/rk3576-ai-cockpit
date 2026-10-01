#pragma once
#include <string_view>
namespace cockpit {
// Build metadata only. This does not detect or test physical hardware.
std::string_view project_name() noexcept;
std::string_view starter_version() noexcept;
std::string_view build_architecture() noexcept;
}  // namespace cockpit
