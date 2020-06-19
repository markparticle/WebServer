/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft GPL 2.0
 */ 
#ifndef EPOLL_H
#define EPOLL_H

#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()

class Epoll {
public:
    Epoll(int eventSize = 10000, int timeout = -1);

    ~Epoll();

    int Wait();

    void AddFd(int fd, bool enableET = true, bool enableOneShot = false);

    void RemoveFd(int fd);

    void Modify(int fd, uint32_t mode, bool enableET = true, bool enableOneShot = true);

    int SetNonblock(int fd);

    int GetFd() const;

    int GetEventFd(int i) const;

    uint32_t GetEvent(int i) const;

private:
    int epollFd_;

    int eventSize_;

    int timeout_;

    epoll_event *ev_;
};
#endif //EPOLL_H