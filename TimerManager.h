// TimerManager.h
#ifndef TIMERMANAGER_H
#define TIMERMANAGER_H

#include <iostream>
#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include <algorithm>

typedef std::chrono::duration<int, std::milli> milliseconds;

class JTimer {
public:
    JTimer() {
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        m_JTimerStart = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch();
    }

    int GetTicks() {
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        milliseconds ticks = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch();
        return ticks.count() - m_JTimerStart.count();
    }

private:
    milliseconds m_JTimerStart;
};

class TimerManager {
public:
    void setInterval(std::function<void()> callback, int intervalMs, std::atomic_bool& cancelToken) {
        std::thread([callback, intervalMs, &cancelToken]() {
            JTimer timer;
            while (cancelToken.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
                if (cancelToken.load()) {
                    callback();
                }
            }
        }).detach();
    }
};

#endif // TIMERMANAGER_H