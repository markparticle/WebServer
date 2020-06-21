/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */ 

#include "heaptimer.h"
#

void HeapTimer::siftup(size_t i) {
    assert(i >= 0);
    size_t j = (i - 1) / 2;
    while(i > 0 && heap_[i] < heap_[j]) {
        std::swap(heap_[i], heap_[j]);
        heap_[i]->SetIndex(i);
        heap_[j]->SetIndex(j);
        i = j;
        j = (i - 1) / 2;
    }
}

bool HeapTimer::siftdown(size_t i, size_t n) {
    bool flag = false;
    assert(i >= 0 && n >= 0);
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break;
        flag = true;
        std::swap(heap_[i], heap_[j]);
        heap_[i]->SetIndex(i);
        heap_[j]->SetIndex(j);
        i = j;
        j = i * 2 + 1;
    }
    return flag;
}

void HeapTimer::action(HttpConn* node) {
    node->CloseConn();
    del(node);
}

void HeapTimer::add(HttpConn* node, time_t timeSlot) {
    assert(node != NULL);
    heap_.push_back(node);
    siftup(heap_.size() - 1);
}

void HeapTimer::del(HttpConn* node) {
    assert(node != nullptr && !heap_.empty());
    size_t i = node->GetIndex();
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i != n) {
        std::swap(heap_[i], heap_[n]);
        heap_[i]->SetIndex(i);
        if(!siftdown(i, n)) {
            siftup(i);
        }
        siftup(i);
    }
    heap_.pop_back();
}

bool HeapTimer::adjust(HttpConn* node, time_t newExpires) {
    node->SetExpires(newExpires);
    siftdown(node->GetIndex(), heap_.size());
    return true;
}

void HeapTimer::tick() {
    if(heap_.empty()) {
        return;
    }
    time_t now = time(nullptr);
    while(!heap_.empty()) {
        auto node = heap_.front();
        if(now < node->GetExpires()) { break; }
        pop();
    }
}

void HeapTimer::pop() {
    if(!heap_.empty()) {
        action(heap_.front());
    }
}

HttpConn* HeapTimer::top() const{
    if(!heap_.empty()) {
        return heap_.front();
    }
}

void HeapTimer::clear() {
    heap_.clear();
}


