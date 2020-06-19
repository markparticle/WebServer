/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */ 
#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 

typedef std::function<void()> TimerCb;

class HeapTimer {
public:
    HeapTimer() {
        heap_.reserve(64);
    }

    ~HeapTimer() {
        clear();
    }

    struct TimerNode {
        time_t expires;
        TimerCb cbfunc;
        size_t index;
        int id;
        bool operator <(const TimerNode& t) {
            return expires < t.expires;
        }
    };
    
    bool action(int id);
    
    bool adjust(int id, time_t newExpires);

    int add(time_t timeSlot, TimerCb cbfunc);

    bool del(int id);

    void clear();

    void tick();

private:
    void siftup(size_t i);

    void siftdown(size_t i, size_t n);

    void del(TimerNode* node);

    void action(TimerNode* node);

    int nextCount();

    std::vector<TimerNode*> heap_;

    std::unordered_map<int, TimerNode*> ref_;

    int idCount_;
};

#endif //HEAP_TIMER_H