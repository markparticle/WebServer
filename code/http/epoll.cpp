/*
 * @Author       : mark
 * @Date         : 2020-06-19
 * @copyleft GPL 2.0
 */

#include "epoll.h"

Epoll::Epoll(int eventSize, int timeout):
        eventSize_(eventSize), timeout_(timeout) {
    epollFd_ = epoll_create(5);
    ev_ = new epoll_event[eventSize];
}

Epoll::~Epoll() {
    close(epollFd_);
    delete[] ev_;
}

int Epoll::Wait() {
    return epoll_wait(epollFd_, ev_, eventSize_, timeout_);
}

void Epoll::AddFd(int fd, bool enableET, bool enableOneShot) {
    assert(fd > 0);
    epoll_event ev = { 0 };
    ev.data.fd = fd;
    if (enableET)
    {
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        ev.events = EPOLLIN | EPOLLRDHUP;
    }
    if (enableOneShot)
    {
        ev.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
    SetNonblock(fd);
}

void Epoll::RemoveFd(int fd) {
    assert(fd > 0);
    epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void Epoll::Modify(int fd, uint32_t mode, bool enableET, bool enableOneShot) {
    assert(fd > 0);
    epoll_event ev;
    ev.data.fd = fd;
    if (enableET)
    {
        ev.events = mode | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        ev.events = mode | EPOLLRDHUP;
    }
    if (enableOneShot)
    {
        ev.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

int Epoll::SetNonblock(int fd) {
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

int Epoll::GetFd() const {
    return epollFd_;
}

int Epoll::GetEventFd(int i) const {
    assert(i >= 0);
    return ev_[i].data.fd;
}

uint32_t Epoll::GetEvent(int i) const {
    assert(i >= 0);
    return ev_[i].events;
}
