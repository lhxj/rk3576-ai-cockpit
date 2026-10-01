#include "cockpit/infer/infer.hpp"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return 1; } } while (false)

int main() {
    using namespace cockpit;
    using namespace std::chrono_literals;
    infer::SerialInferenceScheduler scheduler;
    infer::MockLanguageModelBackend model(scheduler, {"one", "two", "three"}, 20ms);
    CHECK(model.state() == infer::ModelState::Unloaded);
    CHECK(model.load().ok());
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::string> chunks;
    bool terminal = false;
    infer::LanguageRequest request{1, 100, 1, "hello"};
    CHECK(model.generate(request, [&](const infer::LanguageEvent& event) {
        std::lock_guard<std::mutex> lock(mutex);
        if (event.terminal) terminal = true;
        else chunks.push_back(event.text);
        cv.notify_all();
    }).ok());
    CHECK(scheduler.try_acquire(infer::ModelKind::Vision).code == protocol::StatusCode::UNAVAILABLE);
    {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK(cv.wait_for(lock, 1s, [&] { return terminal; }));
    }
    CHECK(chunks.size() == 3);
    for (int i = 0; i < 100 && model.state() != infer::ModelState::Ready; ++i)
        std::this_thread::sleep_for(1ms);
    CHECK(model.state() == infer::ModelState::Ready);

    request.session_id = 101;
    request.request_id = 2;
    terminal = false;
    chunks.clear();
    CHECK(model.generate(request, [&](const infer::LanguageEvent& event) {
        std::lock_guard<std::mutex> lock(mutex);
        if (event.terminal) terminal = true;
        else chunks.push_back(event.text);
        cv.notify_all();
    }).ok());
    {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK(cv.wait_for(lock, 1s, [&] { return !chunks.empty(); }));
    }
    CHECK(model.cancel(101).ok());
    const auto count_after_cancel = chunks.size();
    std::this_thread::sleep_for(80ms);
    CHECK(chunks.size() == count_after_cancel && !terminal);
    CHECK(model.state() == infer::ModelState::Ready);
    CHECK(scheduler.try_acquire(infer::ModelKind::Vision).ok());
    scheduler.release(infer::ModelKind::Vision);
    model.unload();
    CHECK(model.state() == infer::ModelState::Unloaded);
    return 0;
}
