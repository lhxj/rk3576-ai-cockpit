#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace cockpit::ui {

enum class PageId : std::size_t {
    Home = 0,
    Camera,
    Media,
    Vehicle,
    Ai,
    Monitor,
    Settings,
    Count,
};

constexpr std::array<PageId, static_cast<std::size_t>(PageId::Count)> kPageOrder{
    PageId::Home,
    PageId::Camera,
    PageId::Media,
    PageId::Vehicle,
    PageId::Ai,
    PageId::Monitor,
    PageId::Settings,
};

constexpr std::size_t pageIndex(PageId page) noexcept {
    return static_cast<std::size_t>(page);
}

constexpr std::string_view pageName(PageId page) noexcept {
    switch (page) {
    case PageId::Home:
        return "Home";
    case PageId::Camera:
        return "Camera";
    case PageId::Media:
        return "Media";
    case PageId::Vehicle:
        return "Vehicle";
    case PageId::Ai:
        return "AI";
    case PageId::Monitor:
        return "Monitor";
    case PageId::Settings:
        return "Settings";
    case PageId::Count:
        break;
    }
    return "Unknown";
}

}  // namespace cockpit::ui
