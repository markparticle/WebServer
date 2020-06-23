/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */ 
#include "heaptimer.h"

void HeapTimer::siftup_(int i) {
    assert(i >= 0);
    int j = (i - 1) / 2;
    while(j >= 0 && heap_[i]->GetExpires() < heap_[j]->GetExpires()) {
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::SwapNode_(int i, int j) {
    std::swap(heap_[i], heap_[j]);
    hash_[heap_[i]] = i;
    hash_[heap_[j]] = j;
} 

bool HeapTimer::siftdown_(int x, int n) {
    assert(x >= 0 && n >= 0);
    int i = x;
    int j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1]->GetExpires() < heap_[j]->GetExpires()) j++;
        if(heap_[i]->GetExpires() < heap_[j]->GetExpires()) break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > x;
}

void HeapTimer::add(HttpConn* node, time_t timeSlot) {
    assert(node != NULL);
    int i;
    node->SetExpires(time(nullptr) + timeSlot);
    if(hash_.count(node) == 0) {
        i = heap_.size();
        hash_[node] = i;
        heap_.push_back(node);
        siftup_(i);
    } else {
        i = hash_[node];
        if(!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }
    LOG_DEBUG("Add Client[%d]:index = %d TotalTimer = %d", node->GetFd(), hash_[node], heap_.size());
}

void HeapTimer::del(HttpConn* node) {
    assert(node != nullptr);
    if(heap_.empty() || hash_.count(node) == 0) {
        LOG_ERROR("Del Client[%d]:index = %d NotFound", node->GetFd());
        return;
    }
    int i = hash_[node];
    int n = heap_.size() - 1;
    assert(i <= n);
    if(i != n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    node->CloseConn();
    heap_.pop_back();
    hash_.erase(node);
}

bool HeapTimer::adjust(HttpConn* node, time_t newExpires) {
    assert(hash_.count(node) > 0);
    node->SetExpires(newExpires);
    siftdown_(hash_[node], heap_.size());
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
        LOG_DEBUG("Tick Pop Client[%d]:index = %d timerTotal = %d", node->GetFd(), hash_[node], heap_.size() - 1);
        pop();
    }
}

void HeapTimer::pop() {
    if(!heap_.empty()) {
        del(heap_.front());
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
