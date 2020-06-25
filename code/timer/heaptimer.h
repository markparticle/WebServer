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
#include "../http/httpconn.h"
#include "../log/log.h"

class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }

    ~HeapTimer() { clear(); }
    
    void adjust(HttpConn* node, time_t newExpires);

    void add(HttpConn* node, time_t timeSlot);

    void del(HttpConn* node);

    void clear();

    void tick();

    void pop();

    HttpConn* top() const;

    bool OpenLog() { return true; }

private:
    void siftup_(int i);

    bool siftdown_(int i, int n);

    void SwapNode_(int i, int j);

    std::vector<HttpConn*> heap_;
    
    std::unordered_map<HttpConn*, int> hash_;
};

#endif //HEAP_TIMER_H