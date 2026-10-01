#include "cockpit/vehicle/clock.hpp"

#include <chrono>

namespace cockpit::vehicle {

protocol::Deadline SystemClock::now_ms() const {
    return static_cast<protocol::Deadline>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

}  // namespace cockpit::vehicle
