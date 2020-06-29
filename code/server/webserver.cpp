/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */ 

#include "webserver.h"

using namespace std;

WebServer::WebServer(
    int port, int trigMode, bool isReactor, bool OptLinger,
    int sqlPort, const char* sqlUser, const  char* sqlPwd, 
    const char* dbName, int connPoolNum, int threadNum,
    bool openLog, int logLevel, int logQueSize):
    port_(port), openLinger_(OptLinger), isReactor_(isReactor), isClose_(false), 
    timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/html", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    
    InitEventMode_(trigMode);
    if(!InitSocket_()){isClose_ = true;}
    if(openLog) { 
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize); 
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s", 
                    (listenEvent_ & EPOLLET ? "ET": "LT"), 
                    (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("Port:%d, OpenLinger: %s, IO Mode: %s", 
                    port_, OptLinger? "true":"false", isReactor_?"Reactor":"Proctor");
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
}

void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP; 
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::Start() {
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {
        int timeMS = timer_->GetNextTick();
        int eventCnt = epoller_->Wait(timeMS);
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd == listenFd_) {
                DealListen_();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            }
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    epoller_->DelFd(client->GetFd());
    client->Close();
    LOG_INFO("Client[%d] quit!", client->GetFd());
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    timer_->add(fd, 3 * TIME_SLOT, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) {
            if(fd == -1 && errno == EAGAIN){
                /* 完成所有accept任务 */
                return;
            }
            LOG_ERROR("Failed accept Client(%s:%d).", inet_ntoa(addr.sin_addr), addr.sin_port);
            return;
        }
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);
}

void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    if(isReactor_) {
        threadpool_->addTask(std::bind(&WebServer::OnRead_, this, client));
    } else {
        OnRead_(client);
    }
}

void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    if(isReactor_) {
        threadpool_->addTask(std::bind(&WebServer::OnWrite_, this, client));
    } else {
        OnWrite_(client);
    }
}

void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    timer_->adjust(client->GetFd(), 3 * TIME_SLOT);
}

void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
 
    if(ret <= 0 && readErrno != EAGAIN && readErrno != EWOULDBLOCK) {
        CloseConn_(client);
        return;
    }
    client->process();
    epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
}

void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    
    ret = client->write(&writeErrno);
    LOG_DEBUG("To Write:%d", client->ToWriteBytes());
    if(client->ToWriteBytes() == 0){
        LOG_DEBUG("Write Finish!!");
        if(client->IsKeepAlive()) {
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
            LOG_DEBUG("KEEPLIVE!!");
            return;
        }
    }
    if(ret < 0) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
        return;
    }
    CloseConn_(client);
}

bool WebServer::InitSocket_() {            
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 && port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = { 0 }; 
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时*/
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }
    
    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


