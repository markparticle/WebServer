/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */ 

#include "heaptimer.h"

void HeapTimer::siftup(size_t i) {
    size_t j = (i - 1) / 2;
    while(i > 0 && heap_[i] < heap_[j]) {
        std::swap(heap_[i], heap_[j]);
        heap_[i]->index = i;
        heap_[j]->index = j;
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::siftdown(size_t i, size_t n) {
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break;
        std::swap(heap_[i], heap_[j]);
        heap_[i]->index = i;
        heap_[j]->index = j;
        i = j;
        j = i * 2 + 1;
    }
}

bool HeapTimer::action(int id) {
    if(ref_.count(id) != 1) return false;
    auto cb = std::move(ref_[id]->cbfunc);
    if(cb) { cb(); };
    return del(id);
}

void HeapTimer::action(TimerNode* node) {
    auto cb = std::move(node->cbfunc);
    if(cb) { cb(); };
    del(node);
}

int HeapTimer::add(time_t timeSlot, TimerCb cbfunc) {
    time_t expire = time(nullptr) + timeSlot;
    size_t index = heap_.size();
    int id = nextCount();
    TimerNode* node = new TimerNode({expire, cbfunc, index, id});
    heap_.push_back(node);
    siftup(heap_.size() - 1);
    ref_[id] = node;
    return id;
}

bool HeapTimer::del(int id) {
    if(ref_.count(id) == 0) {
        return false;
    }
    del(ref_[id]);
    return true;
}

void HeapTimer::del(TimerNode* node) {
    assert(node != nullptr && !heap_.empty());
    size_t i = node->index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i != n) {
        std::swap(heap_[i], heap_[n]);
        heap_[i]->index = i;
        siftdown(i, n);
        siftup(i);
    }
    heap_.pop_back();
    ref_.erase(node->id);
    delete node;
}

bool HeapTimer::adjust(int id, time_t newExpires) {
    if(ref_.count(id) != 1) return false;
    ref_[id]->expires = newExpires;
    siftdown(ref_[id]->index, heap_.size());
    return true;
}

void HeapTimer::tick() {
    if(heap_.empty()) {
        return;
    }
    time_t now = time(nullptr);
    while(!heap_.empty()) {
        auto node = heap_.front();
        if(now < node->expires) { break; }
        action(node);
    }
}

void HeapTimer::clear() {
    ref_.clear();
    for(auto &item: heap_) {
        delete item;
    }
    heap_.clear();
}

int HeapTimer::nextCount() {
    return ++idCount_;
}