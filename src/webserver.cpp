/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */ 

#include "webserver.h"

int WebServer::pipFds_[2];

WebServer::WebServer(int port, int sqlPort, std::string sqlUser,
    std::string sqlPwd, std::string dbName, int connPoolNum, int threadNum,
    int trigMode, bool isReactor,bool OptLinger ,bool openLog, int logLevel, int logQueSize) {
    
    getcwd(resPath_, 265);
    strcat(resPath_, "/www");

    port_ = port;
    isClose_ = false;
    openLog_ = openLog;
    isOptLinger_ = OptLinger;
    listenFd_ = -1;
    isReactor_ = isReactor;
    threadNum_ = threadNum;
    trigMode_ = trigMode;
    
    sqlConfig_ = { "localhost", 3306, sqlUser, sqlPwd, dbName, connPoolNum};
    logConfig_ = { logLevel, "./log", ".log", 5000, logQueSize };
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
    LOG_INFO("OpenListenET:%s, OpenConnET:%s", 
            (isEtListen_ == true ? "true": "false"), 
            (isEtConn_ == true ? "true": "false"));
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
    LOG_INFO("Threadpool num: %d", threadNum_);
}

void WebServer::InitHttpConn_() {
    //users_[0] = new HttpConn;
    
    HttpConn::resPath = resPath_;
    HttpConn::openLog = openLog_;
    HttpConn::userCount = 0;
    HttpConn::isET = isEtConn_;
    HttpConn::epollPtr = epoll_;
    HttpConn::connPool = connPool_;
    LOG_INFO("SrcPath: %s/", HttpConn::resPath);
}

void WebServer::InitLog_() {
    if(openLog_) {
        Log::GetInstance()->init(
            logConfig_.level, logConfig_.path.c_str(), logConfig_.suffix.c_str(), 
            logConfig_.maxLines, logConfig_.maxQueueSize);
        LOG_INFO("========== Server init ==========");
        LOG_INFO("Log file: %s/xxx%s, level: %d", 
            logConfig_.path.c_str(), logConfig_.suffix.c_str(), logConfig_.level);
    }

}

void WebServer::InitSqlPool_() {
    connPool_ = SqlConnPool::GetInstance();
    connPool_->Init(sqlConfig_.host, 
        sqlConfig_.user, sqlConfig_.pwd,
        sqlConfig_.dbName, sqlConfig_.port, 
        sqlConfig_.connNum);
    SqlConnPool::openLog = openLog_;
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
                timer_->del(users_[fd]);
            }
            /* 检查信号以及定时器事件 */
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
        /* 处理定时器事件 */
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
    timer_->add(users_[fd], TIME_SLOT);
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
    return timer_->adjust(client, time(nullptr) + 5 * TIME_SLOT);
}

void WebServer::DealRead_(int fd) {
    ExtentTime_(users_[fd]);
    if(isReactor_) {
        ExtentTime_(users_[fd]);
        threadpool_->addTask(ReadCallback, users_[fd]);
    } else {
        ExtentTime_(users_[fd]);
        users_[fd]->process();
    }
}

void WebServer::DealWrite_(int fd) {
    if(isReactor_) {
        ExtentTime_(users_[fd]);
        threadpool_->addTask(WriteCallback, users_[fd]);
            
    } else {
        ExtentTime_(users_[fd]);
        users_[fd]->write();
    }
}

void WebServer::ReadCallback(HttpConn* client) {
    client->process();
}

void WebServer::WriteCallback(HttpConn* client) {
    if(!client->write()) {
        client->CloseConn();
    }
}

bool WebServer::DealSignal_(bool &isTimeOut) {
    /* 从管道读取信号 */
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
    /* 初始化一个信号集合，包含所有的信号。 */
    sigfillset(&sa.sa_mask); 
    sigaction(sig, &sa, nullptr);
}

void WebServer::sigHandle(int sig) {
    /* 信号回调函数：往管道发送信号 */
    int tmpErrno = errno;
    send(pipFds_[1], (char*)&sig, 1, 0);
    errno = tmpErrno;
}