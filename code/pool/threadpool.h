/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft GPL 2.0
 */ 

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include "../http/httpconn.h"

class  HttpConn;
class ThreadPool {

public:
    explicit ThreadPool(size_t threadCount): pool_(std::make_shared<Pool>()) {
            for(size_t i = 0; i < threadCount; i++) {
                std::thread([pool = pool_] {
                    std::unique_lock<std::mutex> locker(pool->mtx);
                    while(true) {
                        if(!pool->tasks.empty()) {
                            auto task = std::move(pool->tasks.front().callback);
                            auto arg = pool->tasks.front().arg;
                            pool->tasks.pop();
                            locker.unlock();
                            task(arg);
                            locker.lock();
                        } 
                        else if(pool->isClosed) break;
                        else pool->cond.wait(locker);
                    }
                }).detach();
            }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            pool_->cond.notify_all();
        }
    }

    template<class F>
    void addTask(F&& task, HttpConn* arg) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(Node{std::forward<F>(task), arg});
        }
        pool_->cond.notify_one();
    }

    bool OpenLog() { return OpenLog_; };
    
private:
    struct Node {
        std::function<void(HttpConn*)> callback;
        HttpConn* arg;
    };
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed;
        std::queue<Node> tasks;
    };

    static bool OpenLog_;

    std::shared_ptr<Pool> pool_;
};


#endif //THREADPOOL_H