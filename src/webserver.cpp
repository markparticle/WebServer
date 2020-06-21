/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */ 

#include "webserver.h"

int WebServer::pipFds_[2];

WebServer::WebServer(int port, int sqlPort, std::string sqlUser,
    std::string sqlPwd, std::string dbName, int connPoolNum, int threadNum,
    int trigMode, bool isReactor,bool OptLinger ,bool isCloseLog, int logQueSize) {
    
    getcwd(resPath_, 265);
    strcat(resPath_, "/www");

    port_ = port;
    isClose_ = false;
    isCloseLog_ = isCloseLog;
    isOptLinger_ = OptLinger;
    listenFd_ = -1;
    isReactor_ = isReactor;
    threadNum_ = threadNum;
    trigMode_ = trigMode;
    sqlConfig_ = {"localhost", 3306, sqlUser, sqlPwd, dbName, connPoolNum, isCloseLog};
    logConfig_ = {"./log", ".log", 1024, 5000000, logQueSize};
}

WebServer::~WebServer() {
    Close();
    delete timer_;
    delete threadpool_;
    for(auto &item: users_) {
        delete item.second;
    }
    delete epoll_;
}

void WebServer::InitTrigMode_() {
    switch (trigMode_)
    {
    case 0:
        isEtListen_ = false;
        isEtConn_ = false;
        break;
    case 1:
        isEtListen_ = false;
        isEtConn_ = true;
    case 2:
        isEtListen_ = true;
        isEtConn_ = false;
    case 3:
        isEtListen_ = true;
        isEtConn_ = true;
    default:
        isEtListen_ = true;
        isEtConn_ = true;
        break;
    }
    LOG_INFO("OpenListenET:%d, OpenConnET:%d", isEtListen_, isEtConn_);
}

void WebServer::Init(){
    InitLog_();
    InitTrigMode_();
    InitSocket_();
    InitSqlPool_();
    InitThreadPool_();
    InitHttpConn_();
}

void WebServer::InitThreadPool_(){
    threadpool_ = new ThreadPool(threadNum_);
    assert(threadpool_ != nullptr);
    LOG_INFO("threadpool num: %d", threadNum_);
}

void WebServer::InitHttpConn_() {
    users_[0] = new HttpConn;
    HttpConn::resPath = resPath_;
    LOG_DEBUG("path%s %d %d", HttpConn::resPath, sizeof(HttpConn::resPath), strlen(HttpConn::resPath));
    HttpConn::isCloseLog = isCloseLog_;
    HttpConn::userCount = 0;
    HttpConn::isET = isEtConn_;
    HttpConn::epollPtr = epoll_;
    HttpConn::connPool = connPool_;
}

void WebServer::InitLog_() {
    if(!isCloseLog_) {
        Log::GetInstance()->init(
            logConfig_.path.c_str(), logConfig_.suffix.c_str(), 
            logConfig_.maxLines, logConfig_.maxQueueSize);
        LOG_INFO("========== Server init ==========");
        LOG_INFO("Log file: %s/xxx%s", logConfig_.path.c_str(), logConfig_.suffix.c_str());
    }

}

void WebServer::InitSqlPool_() {
    connPool_ = SqlConnPool::GetInstance();
    connPool_->Init(sqlConfig_.host, 
        sqlConfig_.user, sqlConfig_.pwd,
        sqlConfig_.dbName, sqlConfig_.port, 
        sqlConfig_.connNum, sqlConfig_.isCloseLog);
    LOG_INFO("SqlConnPool num: %d", sqlConfig_.connNum);
}

void WebServer::InitSocket_() {
    int ret;
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr_.sin_port = htons(port_);

    /* 延迟关闭 */
    struct linger tmp =  {0, 1};
    if(isOptLinger_) {
        tmp = {1, 1};
    }
    setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        exit(-1);
    }
    ret = bind(listenFd_, (struct sockaddr *)&serverAddr_, sizeof(serverAddr_));
    if(ret < 0) {
        LOG_ERROR("Bind socket error!");
        exit(-1);
    }
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        exit(-1);
    }
    LOG_INFO("Server port:%d", port_);

    epoll_ = new Epoll(MAX_EVENT_SIZE);
    if(epoll_->GetFd() < 0) {
        LOG_ERROR("Create epoll:%d error!");
        exit(-1);
    }
    epoll_->AddFd(listenFd_, isEtListen_);
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipFds_);
    if(ret < 0) {
        LOG_ERROR("Create pipFds:%d error!");
    }
    epoll_->SetNonblock(pipFds_[1]);
    epoll_->AddFd(pipFds_[0], false , false);

    /* SIG_IGN 忽略信号的处理程序 */
    SetSignal(SIGPIPE, SIG_IGN);      //管道损坏信号
    SetSignal(SIGALRM, sigHandle);    //定时器定时信号
    SetSignal(SIGTERM, sigHandle);    //进程终止信号
    /* 定时器 */
    alarm(TIME_SLOT);
    timer_ = new HeapTimer();
}

void WebServer::Start() {
    LOG_INFO("========== Server start ==========");
    bool isTimeOut = false;
    while(!isClose_) {
        int eventCnt = epoll_->Wait();
        for(int i = 0; i < eventCnt; i++) {
            int fd = epoll_->GetEventFd(i);
            uint32_t events = epoll_->GetEvent(i);
            if(fd == listenFd_) {
                DealListen_();
            }
            else if(events & ((EPOLLRDHUP | EPOLLHUP | EPOLLERR))){
                timer_->action(users_[fd]);
            }
            else if(fd == pipFds_[0] && (events & EPOLLIN)) {
                if(!DealSignal_(isTimeOut)) {
                    LOG_ERROR("Deal client data failure");
                }
            }
            else if(events & EPOLLIN) {
                DealRead_(fd);
            }
            else if(events & EPOLLOUT) {
                DealWrite_(fd);
            }
        }
        if(isTimeOut) {
            DealTimeOut_();
            isTimeOut = false;
        }
    }
}

void WebServer::Close() {
    if(isClose_ == false) {
        isClose_ = true;
        LOG_INFO("========== Server quit ==========");
    }
}

void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    assert(ret > 0);
    close(fd);
}

void WebServer::DealTimeOut_() {
    timer_->tick();
    alarm(TIME_SLOT);
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    if(!users_[fd]) {
        users_[fd] = new HttpConn;
    }
    users_[fd]->init(fd, addr);
    timer_->add(users_[fd],  TIME_SLOT);
}

void WebServer::DealListen_() {
    LOG_INFO("Deal Listen");
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if(isEtListen_ == false) {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd < 0) {
            LOG_WARN("Failed accept Client(%s:%d).", inet_ntoa(addr.sin_addr), addr.sin_port);
        }
        if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
        }
        AddClient_(fd, addr);
    } 
    else {
        while(!isClose_) {
            int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
            if(fd < 0) { break; }
            if(HttpConn::userCount >= MAX_FD) {
                SendError_(fd, "Server busy!");
                LOG_WARN("Clients is full!");
                break;
            }
            AddClient_(fd, addr);
        }
    }
}

bool WebServer::ExtentTime_(HttpConn* client) {
    return timer_->adjust(client, time(nullptr) + 10 * TIME_SLOT);
}

void WebServer::DealRead_(int fd) {
    LOG_INFO("Deal client[%d] read", fd);
    ExtentTime_(users_[fd]);
    if(isReactor_) {
        ExtentTime_(users_[fd]);
        threadpool_->addTask(ReadCallback, users_[fd]);
    } else {
        users_[fd]->process();
        ExtentTime_(users_[fd]);
    }
}


void WebServer::DealWrite_(int fd) {
    LOG_INFO("Deal client[%d] write", fd);
    if(isReactor_) {
        ExtentTime_(users_[fd]);
        threadpool_->addTask(WriteCallback, users_[fd]);
            
    } else {
        users_[fd]->write();
        ExtentTime_(users_[fd]);
    }
}

void WebServer::ReadCallback(HttpConn* client) {
    LOG_DEBUG("Client[%d]: read callback", client->GetFd());
    client->process();
}

void WebServer::WriteCallback(HttpConn* client) {
    LOG_DEBUG("Client[%d]: write callback", client->GetFd());
    if(!client->write()) {
        client->CloseConn();
    }
}

bool WebServer::DealSignal_(bool &isTimeOut) {
    char signals[1024];
    int len = recv(pipFds_[0], signals, sizeof(signals), 0);
    if(len <= 0 ) { return false; }
    for(int i = 0; i < len; i++) {
        switch (signals[i])
        {
        case SIGALRM:
            isTimeOut = true;
            break;
        case SIGTERM:
            isClose_ = true;
        case SIGINT:
            isClose_ = true;
        case SIGKILL:
            isClose_ = true;
        default:
            break;
        }
    }
    return true;
}

void WebServer::SetSignal(int sig, void(handler)(int), bool enableRestart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    if(enableRestart) {
        sa.sa_flags |= SA_RESTART;
    }
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask); //初始化一个信号集合，包含所有的信号。
    sigaction(sig, &sa, nullptr);
}

void WebServer::sigHandle(int sig) {
    int tmpErrno = errno;
    send(pipFds_[1], (char*)&sig, 1, 0);
    errno = tmpErrno;
}