#ifndef SIMPLE_EVENTS_HPP
#define SIMPLE_EVENTS_HPP

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

class Event
{
public:
    Event() : flag(false) {}

    void set()
    {
        std::unique_lock<std::mutex> lock(mtx);
        flag = true;
        cv.notify_all(); // 通知所有等待的线程
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]
                { return flag; }); // 等待标志变为true
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    bool flag;
};

#endif