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
    explicit BlockDeque(int MaxCapacity = 1000)
        :capacity_(MaxCapacity) {
        if(MaxCapacity < 0) exit(-1);
    }

    ~BlockDeque() = default;

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
    int size() {
        {
            std::lock_guard<std::mutex> locker(mtx_);
            return deq_.size();
        }
    }
    int capacity() 
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

    T pop() {
        T tmp;
        {
            std::unique_lock<std::mutex> locker(mtx_);
            while(deq_.empty()){
                condConsumer_.wait(locker);
            }
            tmp = deq_.front();
            deq_.pop_front();
        }
        condProducer_.notify_one();
        return tmp;
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
            }
            item = deq_.front();
            deq_.pop_front();
        }
        condProducer_.notify_one();
        return true;
    }

private:
    std::deque<T> deq_;
    int capacity_;
    std::mutex mtx_;
    std::condition_variable condConsumer_;
    std::condition_variable condProducer_;
};

#endif // BLOCKQUEUE_H