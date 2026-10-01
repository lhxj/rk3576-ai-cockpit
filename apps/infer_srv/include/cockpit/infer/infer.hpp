#pragma once

#include "cockpit/protocol/message.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cockpit::infer {

enum class ModelState { Unloaded, Loading, Ready, Busy, Error };
enum class ModelKind { Language, Vision };

class IInferenceScheduler {
public:
    virtual ~IInferenceScheduler() = default;
    virtual protocol::Status try_acquire(ModelKind kind) = 0;
    virtual void release(ModelKind kind) = 0;
};

// One shared slot for future RKLLM and RKNN owners; no NPU preemption assumed.
class SerialInferenceScheduler final : public IInferenceScheduler {
public:
    protocol::Status try_acquire(ModelKind kind) override;
    void release(ModelKind kind) override;
private:
    std::mutex mutex_;
    bool occupied_{false};
    ModelKind owner_{ModelKind::Language};
};

struct LanguageRequest {
    protocol::RequestId request_id{0};
    protocol::SessionId session_id{0};
    std::uint64_t generation{0};
    std::string prompt;
};

struct LanguageEvent {
    protocol::RequestId request_id{0};
    protocol::SessionId session_id{0};
    std::uint64_t generation{0};
    std::string text;
    bool terminal{false};
    protocol::Status status;
};
using LanguageCallback = std::function<void(const LanguageEvent&)>;

class ILanguageModelBackend {
public:
    virtual ~ILanguageModelBackend() = default;
    virtual protocol::Status load() = 0;
    virtual void unload() = 0;
    virtual protocol::Status generate(LanguageRequest request, LanguageCallback callback) = 0;
    virtual protocol::Status cancel(protocol::SessionId session_id) = 0;
    virtual ModelState state() const = 0;
};

class IVisionBackend {
public:
    virtual ~IVisionBackend() = default;
    virtual protocol::Status load() = 0;
    virtual void unload() = 0;
    virtual ModelState state() const = 0;
};

// Host fixture with a joinable worker; callback must be quick and non-reentrant.
class MockLanguageModelBackend final : public ILanguageModelBackend {
public:
    MockLanguageModelBackend(IInferenceScheduler& scheduler,
                             std::vector<std::string> chunks,
                             std::chrono::milliseconds delay);
    ~MockLanguageModelBackend() override;
    protocol::Status load() override;
    void unload() override;
    protocol::Status generate(LanguageRequest request, LanguageCallback callback) override;
    protocol::Status cancel(protocol::SessionId session_id) override;
    ModelState state() const override;

private:
    void run(LanguageRequest request, LanguageCallback callback);
    IInferenceScheduler& scheduler_;
    const std::vector<std::string> chunks_;
    const std::chrono::milliseconds delay_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    ModelState state_{ModelState::Unloaded};
    protocol::SessionId active_session_{0};
    bool cancelled_{false};
    std::thread worker_;
};

}  // namespace cockpit::infer
