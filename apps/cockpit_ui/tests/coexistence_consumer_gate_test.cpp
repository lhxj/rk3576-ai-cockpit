#include "cockpit_ui/coexistence_consumer_gate.h"
#include "cockpit/media/media_service.hpp"
#include <array>
#include <sstream>
#include <stdexcept>

struct FakeService {
    std::array<bool, 4> active{};
    bool preview_active() const { return active[0]; }
    bool recording_active() const { return active[1]; }
    bool rtsp_active() const { return active[2]; }
    bool vision_active() const { return active[3]; }
    cockpit::media::CaptureStats capture_stats() const { return {}; }
    cockpit::media::RecorderStats recorder_stats() const {
        cockpit::media::RecorderStats result;
        result.last_error = std::string(300, '\n');
        result.overflow_count = 1;
        return result;
    }
    cockpit::media::EncoderStats encoder_stats() const { return {}; }
    cockpit::media::RtspStats rtsp_stats() const { return {}; }
    cockpit::media::MediaServiceStats service_stats() const { return {}; }
};
int main() {
    for (unsigned bits = 0; bits < 16; ++bits) {
        FakeService service;
        for (unsigned i = 0; i < 4; ++i) service.active[i] = (bits & (1u << i)) != 0;
        std::ostringstream evidence;
        bool stopped = false;
        try { cockpit::ui::require_coexistence_consumers(service, evidence); }
        catch (const std::runtime_error& error) {
            stopped = true;
            if (std::string(error.what()) != "coexistence consumer stopped") return 1;
        }
        if (stopped != (bits != 15)) return 2;
        const auto line = evidence.str();
        if (bits == 15 && !line.empty()) return 3;
        if (bits != 15) {
            if (line.find("record_overflow=1") == std::string::npos || line.size() > 2048) return 4;
            if (line.find('\n') != line.size() - 1) return 5;
            for (unsigned i = 0; i < 4; ++i) {
                const std::array<std::string,4> names{"preview", "recording", "rtsp", "vision"};
                if (line.find(names[i]+"="+(service.active[i]?"1":"0")) == std::string::npos) return 6;
            }
        }
    }
    if (cockpit::ui::bounded_error_hex(std::string(300, '\n')).size() != 512) return 7;
    return 0;
}
