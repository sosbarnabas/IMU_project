#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>

template<typename T>
class TSQueue {
public:
    void enqueue(const T &item) { {
            std::lock_guard<std::mutex> lk(mtx);
            queue.push_back(item);
        }
        cv.notify_all();
    }

    bool try_dequeue(T &item) {
        std::lock_guard<std::mutex> lk(mtx);
        if (queue.empty()) { return false; }
        int item = queue.front();
        queue.pop_front();
        return true;
    }

    T wait_dequeue() {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] {
                    return !queue.empty();
                }
        );
        T item = queue.front();
        queue.pop_front();
        return item;
    }

    size_t size() {
        std::lock_guard<std::mutex> lk(mtx);
        return queue.size();
    }

private:
    mutable std::mutex mtx;
    std::condition_variable cv;
    std::deque<T> queue;
};
