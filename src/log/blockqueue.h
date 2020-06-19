/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft GPL 2.0
 */ 
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

template<class T>
class BlockDeque
{
public:
    explicit BlockDeque(size_t MaxCapacity = 1000)
        :capacity_(MaxCapacity) {
        if(MaxCapacity < 0) exit(-1);
    }

    ~BlockDeque() {
        {
            std::lock_guard<std::mutex> locker(mtx_);
            isClose_ = true;
        } 
        condProducer_.notify_all();
        condConsumer_.notify_all();
    };
    void clear() {
        {
            std::lock_guard<std::mutex> locker(mtx_);
            deq_.clear();
        }
    }
    bool empty() {
        {
            std::lock_guard<std::mutex> locker(mtx_);
            return deq_.empty();
        }
    }
    bool full(){
        {
            std::lock_guard<std::mutex> locker(mtx_);
            return deq_.size() >= capacity_;
        }
    }
    T front() {
        {
            std::lock_guard<std::mutex> locker(mtx_);
            return deq_.front();
        }
    }
    T back() {
        {
            std::lock_guard<std::mutex> locker(mtx_);
            return deq_.back();
        }
    }
    size_t size() {
        {
            std::lock_guard<std::mutex> locker(mtx_);
            return deq_.size();
        }
    }
    size_t capacity() 
    {
        return capacity_;
    }

    void push_back(const T &item) {
        {
            std::unique_lock<std::mutex> locker(mtx_);
            while(deq_.size() >= capacity_) {
                condProducer_.wait(locker);
            }
            deq_.push_back(item);
        }
        condConsumer_.notify_all();
    }

    void push_front(const T &item) {
        {
            std::unique_lock<std::mutex> locker(mtx_);
            while(deq_.size() >= capacity_) {
                condProducer_.wait(locker);
            }
            deq_.push_front(item);
        }
        condConsumer_.notify_one();
    }

    bool pop(T &item) {
        {
            std::unique_lock<std::mutex> locker(mtx_);
            while(deq_.empty()){
                condConsumer_.wait(locker);
                if(isClose_ == true){
                    return false;
                }
            }
            item = deq_.front();
            deq_.pop_front();
        }
        condProducer_.notify_one();
        return true;
    }

    bool pop(T &item, int timeout) {
        {   
            std::unique_lock<std::mutex> locker(mtx_);
            while(deq_.empty()){
                if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                    == std::cv_status::timeout)
                {
                    item = nullptr;
                    return false;
                }
                if(isClose_ == true){
                    return false;
                }
            }
            item = deq_.front();
            deq_.pop_front();
        }
        condProducer_.notify_one();
        return true;
    }

private:
    std::deque<T> deq_;
    size_t capacity_;
    std::mutex mtx_;
    bool isClose_;
    std::condition_variable condConsumer_;
    std::condition_variable condProducer_;
};

#endif // BLOCKQUEUE_H