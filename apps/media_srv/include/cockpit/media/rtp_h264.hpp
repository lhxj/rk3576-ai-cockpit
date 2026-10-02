#pragma once

#include "cockpit/media/h264_annex_b.hpp"
#include "cockpit/media/h264_encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cockpit::media {

struct RtpPacket {
    std::vector<std::uint8_t> bytes;
    std::uint16_t sequence{0};
    std::uint32_t timestamp{0};
    bool marker{false};
};

class H264RtpPacketizer {
public:
    explicit H264RtpPacketizer(std::size_t max_payload = 1200,
                               std::uint8_t payload_type = 96,
                               std::uint32_t ssrc = 0x35760001U);
    std::vector<RtpPacket> packetize(const std::vector<H264Nal>& nals,
                                     std::uint32_t timestamp,
                                     std::uint16_t& next_sequence) const;
    std::vector<RtpPacket> packetize(const EncodedPacket& access_unit,
                                     std::uint16_t& next_sequence) const;

private:
    std::size_t max_payload_;
    std::uint8_t payload_type_;
    std::uint32_t ssrc_;
};

}  // namespace cockpit::media
