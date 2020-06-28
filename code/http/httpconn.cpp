/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft GPL 2.0
 */ 
#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;


HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

HttpConn::~HttpConn() { 
    Close(); 
};

void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;
    isClose_ = false;
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

bool HttpConn::IsClose() const {
    return isClose_;
};

int HttpConn::GetFd() const {
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET);
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    size_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);
        LOG_DEBUG("write len: %d", len);
        if(len <= 0) {
            if(errno == EAGAIN) {
                *saveErrno = errno;
                return len;
            }
            response_.UnmapFile();
            return len;
        }
 
        if(len > iov_[0].iov_len && iovCnt_ == 2) {
            iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);

            writeBuff_.RetrieveAll();
            iov_[0].iov_len = 0;
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
        /* 发送完毕 */
        if(iov_[0].iov_len + iov_[1].iov_len == 0) {
            response_.UnmapFile();
            break;
        }
    } while(isET || ToWriteBytes() > 10240);
    return len;
}

void HttpConn::process() {
    request_.Init();
    if(request_.parse(readBuff_)) {
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive());
    } else {
        response_.Init(srcDir, request_.path(), false, 400);
    }
    LOG_DEBUG("Parse code: %d, %s", response_.Code(), request_.path().c_str());
    response_.MakeResponse(writeBuff_);

    iov_[0].iov_base = writeBuff_.Peek();
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    // LOG_DEBUG("\n%s\n", writeBuff_.Peek());
    iovCnt_ = 1;
    if(response_.FileLen() > 0  && response_.File()) {
        //LOG_DEBUG("\n%s\n", response_.File());
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    } 
    LOG_DEBUG("filesize:%d, %d", response_.FileLen() , iovCnt_);
}
