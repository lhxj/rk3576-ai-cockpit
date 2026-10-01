#include "cockpit/media/v4l2_mplane_camera_capture.hpp"

#ifdef COCKPIT_ENABLE_V4L2_CAMERA

#include <linux/videodev2.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

namespace cockpit::media {
namespace {

int xioctl(int fd, unsigned long request, void* argument) {
    int result;
    do {
        result = ::ioctl(fd, request, argument);
    } while (result < 0 && errno == EINTR);
    return result;
}

std::int64_t steady_now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string fourcc_name(std::uint32_t value) {
    std::string name(4, '\0');
    name[0] = static_cast<char>(value & 0xFFU);
    name[1] = static_cast<char>((value >> 8U) & 0xFFU);
    name[2] = static_cast<char>((value >> 16U) & 0xFFU);
    name[3] = static_cast<char>((value >> 24U) & 0xFFU);
    return name;
}

}  // namespace

V4l2MplaneCameraCapture::~V4l2MplaneCameraCapture() { close_device(); }

MediaStatus V4l2MplaneCameraCapture::errno_status(const char* operation) const {
    std::ostringstream detail;
    detail << operation << ": " << std::strerror(errno) << " (errno=" << errno << ')';
    return {MediaStatusCode::IoError, detail.str()};
}

MediaStatus V4l2MplaneCameraCapture::open_device(const std::string& device) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fd_ >= 0) return MediaStatus::Ok("already open");
    if (device.empty()) return {MediaStatusCode::InvalidArgument, "empty camera device"};
    fd_ = ::open(device.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) return errno_status("open camera");
    config_.device = device;
    return MediaStatus::Ok();
}

MediaStatus V4l2MplaneCameraCapture::configure(const CameraCaptureConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fd_ < 0) return {MediaStatusCode::InvalidState, "camera is not open"};
    if (stream_on_) return {MediaStatusCode::InvalidState, "camera is streaming"};
    if (config.pixel_format != "NV12" || config.width != 1632 || config.height != 1224)
        return {MediaStatusCode::UnsupportedCameraFormat,
                "CAM0 v1 requires 1632x1224 NV12"};
    if (config.buffer_count < 3 || config.poll_timeout_ms <= 0)
        return {MediaStatusCode::InvalidArgument, "camera capture configuration"};

    v4l2_capability capability{};
    if (xioctl(fd_, VIDIOC_QUERYCAP, &capability) < 0) return errno_status("VIDIOC_QUERYCAP");
    const auto caps = capability.capabilities & V4L2_CAP_DEVICE_CAPS
                          ? capability.device_caps
                          : capability.capabilities;
    if ((caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) == 0 || (caps & V4L2_CAP_STREAMING) == 0)
        return {MediaStatusCode::UnsupportedCameraFormat,
                "device is not MPLANE streaming capture"};

    v4l2_format format{};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    format.fmt.pix_mp.width = config.width;
    format.fmt.pix_mp.height = config.height;
    format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    format.fmt.pix_mp.field = V4L2_FIELD_ANY;
    format.fmt.pix_mp.num_planes = 1;
    if (xioctl(fd_, VIDIOC_S_FMT, &format) < 0) return errno_status("VIDIOC_S_FMT");
    if (xioctl(fd_, VIDIOC_G_FMT, &format) < 0) return errno_status("VIDIOC_G_FMT");

    const auto actual_fourcc = fourcc_name(format.fmt.pix_mp.pixelformat);
    if (format.fmt.pix_mp.width != config.width || format.fmt.pix_mp.height != config.height ||
        format.fmt.pix_mp.pixelformat != V4L2_PIX_FMT_NV12)
        return {MediaStatusCode::UnsupportedCameraFormat,
                "actual format " + std::to_string(format.fmt.pix_mp.width) + "x" +
                    std::to_string(format.fmt.pix_mp.height) + " " + actual_fourcc};
    if (format.fmt.pix_mp.num_planes != 1)
        return {MediaStatusCode::UnsupportedPlaneLayout,
                "actual num_planes=" + std::to_string(format.fmt.pix_mp.num_planes)};

    config_ = config;
    actual_ = {config.camera_id,
               format.fmt.pix_mp.width,
               format.fmt.pix_mp.height,
               actual_fourcc,
               format.fmt.pix_mp.num_planes,
               format.fmt.pix_mp.plane_fmt[0].bytesperline,
               format.fmt.pix_mp.plane_fmt[0].sizeimage,
               config.buffer_count,
               0,
               0,
               0,
               false,
               ""};
    actual_.driver = reinterpret_cast<const char*>(capability.driver);

    v4l2_streamparm parameters{};
    parameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    parameters.parm.capture.timeperframe.numerator = 1;
    parameters.parm.capture.timeperframe.denominator = config.fps;
    // Some Rockchip mainpath drivers do not implement S/G_PARM. Format negotiation
    // remains valid; measured stream rate is recorded independently.
    (void)xioctl(fd_, VIDIOC_S_PARM, &parameters);
    parameters = {};
    parameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(fd_, VIDIOC_G_PARM, &parameters) == 0 &&
        parameters.parm.capture.timeperframe.numerator != 0 &&
        parameters.parm.capture.timeperframe.denominator != 0) {
        actual_.fps_numerator = parameters.parm.capture.timeperframe.numerator;
        actual_.fps_denominator = parameters.parm.capture.timeperframe.denominator;
        actual_.frame_interval_supported = true;
    }

    release_buffers();
    const auto buffer_status = request_and_map_buffers();
    if (!buffer_status.ok()) {
        release_buffers();
        return buffer_status;
    }
    configured_ = true;
    return MediaStatus::Ok();
}

MediaStatus V4l2MplaneCameraCapture::request_and_map_buffers() {
    v4l2_requestbuffers request{};
    request.count = config_.buffer_count;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    request.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd_, VIDIOC_REQBUFS, &request) < 0) return errno_status("VIDIOC_REQBUFS");
    actual_.actual_buffers = request.count;
    if (request.count < 3)
        return {MediaStatusCode::Unavailable,
                "driver allocated fewer than three MMAP buffers"};

    mappings_.reserve(request.count);
    for (std::uint32_t index = 0; index < request.count; ++index) {
        v4l2_plane planes[VIDEO_MAX_PLANES]{};
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = index;
        buffer.length = 1;
        buffer.m.planes = planes;
        if (xioctl(fd_, VIDIOC_QUERYBUF, &buffer) < 0) return errno_status("VIDIOC_QUERYBUF");
        if (buffer.length != 1 || planes[0].length < actual_.size_image)
            return {MediaStatusCode::UnsupportedPlaneLayout, "MMAP plane length/layout"};
        void* address = ::mmap(nullptr, planes[0].length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd_, planes[0].m.mem_offset);
        if (address == MAP_FAILED) return errno_status("mmap");
        mappings_.push_back({address, planes[0].length});
        planes[0].length = static_cast<__u32>(mappings_.back().length);
        if (xioctl(fd_, VIDIOC_QBUF, &buffer) < 0) return errno_status("VIDIOC_QBUF initial");
    }
    return MediaStatus::Ok();
}

MediaStatus V4l2MplaneCameraCapture::start(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_on_) return MediaStatus::Ok("already streaming");
    if (fd_ < 0 || !configured_ || mappings_.size() < 3)
        return {MediaStatusCode::InvalidState, "camera is not configured"};
    if (!callback) return {MediaStatusCode::InvalidArgument, "frame callback"};
    auto type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(fd_, VIDIOC_STREAMON, &type) < 0) return errno_status("VIDIOC_STREAMON");
    stream_on_ = true;
    streaming_ = true;
    stop_requested_.store(false);
    callback_ = std::move(callback);
    stats_ = {};
    stats_.bytes_used_min = std::numeric_limits<std::uint32_t>::max();
    stats_.stream_epoch = ++next_epoch_;
    try {
        worker_ = std::thread(&V4l2MplaneCameraCapture::capture_loop, this);
    } catch (...) {
        xioctl(fd_, VIDIOC_STREAMOFF, &type);
        stream_on_ = false;
        streaming_ = false;
        callback_ = {};
        return {MediaStatusCode::Unavailable, "capture worker start failed"};
    }
    return MediaStatus::Ok();
}

MediaStatus V4l2MplaneCameraCapture::stop() {
    stop_requested_.store(true);
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    MediaStatus status = MediaStatus::Ok(stream_on_ ? "stream stopped" : "already stopped");
    if (stream_on_) {
        auto type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl(fd_, VIDIOC_STREAMOFF, &type) < 0) status = errno_status("VIDIOC_STREAMOFF");
    }
    stream_on_ = false;
    streaming_ = false;
    callback_ = {};
    return status;
}

void V4l2MplaneCameraCapture::release_buffers() {
    for (auto& mapping : mappings_) {
        if (mapping.address != nullptr && mapping.address != MAP_FAILED)
            ::munmap(mapping.address, mapping.length);
    }
    mappings_.clear();
    if (fd_ >= 0) {
        v4l2_requestbuffers request{};
        request.count = 0;
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        request.memory = V4L2_MEMORY_MMAP;
        (void)xioctl(fd_, VIDIOC_REQBUFS, &request);
    }
    actual_.actual_buffers = 0;
}

void V4l2MplaneCameraCapture::close_device() {
    stop();
    std::lock_guard<std::mutex> lock(mutex_);
    release_buffers();
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    configured_ = false;
}

CameraFormat V4l2MplaneCameraCapture::actual_format() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return actual_;
}

CaptureStats V4l2MplaneCameraCapture::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

bool V4l2MplaneCameraCapture::streaming() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return streaming_;
}

void V4l2MplaneCameraCapture::capture_loop() {
    bool have_sequence = false;
    std::uint32_t last_sequence = 0;
    while (!stop_requested_.load()) {
        pollfd descriptor{fd_, POLLIN | POLLERR, 0};
        const int poll_result = ::poll(&descriptor, 1, config_.poll_timeout_ms);
        if (poll_result == 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.poll_timeouts;
            continue;
        }
        if (poll_result < 0) {
            if (errno == EINTR) continue;
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.dequeue_errors;
            break;
        }
        if ((descriptor.revents & POLLIN) == 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.dequeue_errors;
            break;
        }

        v4l2_plane planes[VIDEO_MAX_PLANES]{};
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.length = 1;
        buffer.m.planes = planes;
        if (xioctl(fd_, VIDIOC_DQBUF, &buffer) < 0) {
            if (errno == EAGAIN) continue;
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.dequeue_errors;
            break;
        }
        if (buffer.index >= mappings_.size() || buffer.length != 1 ||
            planes[0].bytesused > mappings_[buffer.index].length) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.dequeue_errors;
            break;
        }

        const auto dequeue_ns = steady_now_ns();
        CapturedFrame frame;
        frame.camera_id = actual_.camera_id;
        frame.width = actual_.width;
        frame.height = actual_.height;
        frame.pixel_format = actual_.pixel_format;
        frame.bytes_per_line = actual_.bytes_per_line;
        frame.size_image = actual_.size_image;
        const auto bytes_used = planes[0].bytesused;
        frame.bytes_used = bytes_used;
        frame.sequence = buffer.sequence;
        frame.stream_epoch = stats_.stream_epoch;
        frame.capture_timestamp_ns = static_cast<std::int64_t>(buffer.timestamp.tv_sec) *
                                         1000000000LL +
                                     static_cast<std::int64_t>(buffer.timestamp.tv_usec) * 1000LL;
        frame.dequeue_steady_timestamp_ns = dequeue_ns;
        frame.timestamp_clock = (buffer.flags & V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC)
                                    ? TimestampClock::Monotonic
                                    : TimestampClock::Unknown;
        const auto* begin = static_cast<const std::uint8_t*>(mappings_[buffer.index].address);
        frame.payload.assign(begin, begin + bytes_used);

        planes[0].length = static_cast<__u32>(mappings_[buffer.index].length);
        if (xioctl(fd_, VIDIOC_QBUF, &buffer) < 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.queue_errors;
            break;
        }

        FrameCallback callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (have_sequence && buffer.sequence > last_sequence + 1U)
                stats_.sequence_gap_count += buffer.sequence - last_sequence - 1U;
            have_sequence = true;
            last_sequence = buffer.sequence;
            ++stats_.frames;
            stats_.bytes_used_min = std::min(stats_.bytes_used_min, bytes_used);
            stats_.bytes_used_max = std::max(stats_.bytes_used_max, bytes_used);
            if (stats_.first_dequeue_steady_ns == 0)
                stats_.first_dequeue_steady_ns = dequeue_ns;
            stats_.last_dequeue_steady_ns = dequeue_ns;
            callback = callback_;
        }
        if (callback) {
            try {
                callback(std::move(frame));
            } catch (...) {
            }
        }
    }
}

}  // namespace cockpit::media

#endif
