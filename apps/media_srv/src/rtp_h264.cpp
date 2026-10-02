#include "cockpit/media/rtp_h264.hpp"

#include <algorithm>
#include <stdexcept>

namespace cockpit::media {
namespace {

void write_header(std::vector<std::uint8_t>& bytes, std::uint8_t payload_type,
                  bool marker, std::uint16_t sequence, std::uint32_t timestamp,
                  std::uint32_t ssrc) {
    bytes.resize(12U);
    bytes[0] = 0x80U;
    bytes[1] = static_cast<std::uint8_t>((marker ? 0x80U : 0U) | payload_type);
    bytes[2] = static_cast<std::uint8_t>(sequence >> 8U);
    bytes[3] = static_cast<std::uint8_t>(sequence);
    bytes[4] = static_cast<std::uint8_t>(timestamp >> 24U);
    bytes[5] = static_cast<std::uint8_t>(timestamp >> 16U);
    bytes[6] = static_cast<std::uint8_t>(timestamp >> 8U);
    bytes[7] = static_cast<std::uint8_t>(timestamp);
    bytes[8] = static_cast<std::uint8_t>(ssrc >> 24U);
    bytes[9] = static_cast<std::uint8_t>(ssrc >> 16U);
    bytes[10] = static_cast<std::uint8_t>(ssrc >> 8U);
    bytes[11] = static_cast<std::uint8_t>(ssrc);
}

}  // namespace

H264RtpPacketizer::H264RtpPacketizer(std::size_t max_payload,
                                     std::uint8_t payload_type,
                                     std::uint32_t ssrc)
    : max_payload_(max_payload), payload_type_(payload_type), ssrc_(ssrc) {
    if (max_payload_ < 3U) throw std::invalid_argument("RTP payload too small");
}

std::vector<RtpPacket> H264RtpPacketizer::packetize(
    const std::vector<H264Nal>& nals, std::uint32_t timestamp,
    std::uint16_t& next_sequence) const {
    std::vector<RtpPacket> packets;
    for (const auto& nal : nals) {
        if (nal.empty()) continue;
        if (nal.size() <= max_payload_) {
            RtpPacket packet;
            packet.sequence = next_sequence++;
            packet.timestamp = timestamp;
            write_header(packet.bytes, payload_type_, false, packet.sequence,
                         timestamp, ssrc_);
            packet.bytes.insert(packet.bytes.end(), nal.begin(), nal.end());
            packets.push_back(std::move(packet));
            continue;
        }
        const std::uint8_t header = nal.front();
        const std::uint8_t fu_indicator = static_cast<std::uint8_t>((header & 0xE0U) | 28U);
        const std::uint8_t nal_type = static_cast<std::uint8_t>(header & 0x1FU);
        const std::size_t chunk_capacity = max_payload_ - 2U;
        std::size_t offset = 1U;
        bool first = true;
        while (offset < nal.size()) {
            const auto length = std::min(chunk_capacity, nal.size() - offset);
            const bool last = offset + length == nal.size();
            RtpPacket packet;
            packet.sequence = next_sequence++;
            packet.timestamp = timestamp;
            write_header(packet.bytes, payload_type_, false, packet.sequence,
                         timestamp, ssrc_);
            packet.bytes.push_back(fu_indicator);
            packet.bytes.push_back(static_cast<std::uint8_t>(nal_type |
                (first ? 0x80U : 0U) | (last ? 0x40U : 0U)));
            packet.bytes.insert(packet.bytes.end(),
                                nal.begin() + static_cast<std::ptrdiff_t>(offset),
                                nal.begin() + static_cast<std::ptrdiff_t>(offset + length));
            packets.push_back(std::move(packet));
            first = false;
            offset += length;
        }
    }
    if (!packets.empty()) {
        packets.back().marker = true;
        packets.back().bytes[1] = static_cast<std::uint8_t>(packets.back().bytes[1] | 0x80U);
    }
    return packets;
}

std::vector<RtpPacket> H264RtpPacketizer::packetize(
    const EncodedPacket& access_unit, std::uint16_t& next_sequence) const {
    return packetize(split_annex_b(access_unit.annex_b),
                     access_unit.rtp_timestamp, next_sequence);
}

}  // namespace cockpit::media
