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
    char rootPath[200];
    getcwd(rootPath, 200);
    resPath_ = new char[strlen(rootPath) + 10];
    strcpy(resPath_, rootPath);
    strcat(resPath_, "/resourse");

    port_ = port;
    isClose_ = false;
    isCloseLog_ = isCloseLog;
    isOptLinger_ = OptLinger;
    listenFd_ = -1;
    isReactor_ = isReactor;
    threadNum_ = threadNum;
    trigMode_ = trigMode;
    sqlConfig_ = {"localhost", 3306, sqlUser, sqlPwd, dbName, connPoolNum, isCloseLog};
    logConfig_ = {"./log", ".log", 8192, 5000000, logQueSize};
    init();
}

WebServer::~WebServer() {
    for(auto &item: clients_) {
        if(item.second.client) {
            delete item.second.client;
        }
    }
    delete epoll_;
    delete threadpool_;
    delete[] resPath_;
    delete[] HttpConn::resPath;
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
}

void WebServer::init(){
    InitTrigMode_();
    InitLog_();
    InitSqlPool_();
    InitSocket_();
    InitHttpConn_();
    InitThreadPool_();
    LOG_INFO("Init success!");
}

void WebServer::InitThreadPool_(){
    threadpool_ = new ThreadPool(threadNum_);
    assert(threadpool_ != nullptr);
}

void WebServer::InitHttpConn_() {
    assert(epoll_ != nullptr && resPath_ != nullptr);
    HttpConn::epollPtr = epoll_;
    HttpConn::resPath = new char[sizeof(resPath_)];
    strcpy(HttpConn::resPath, resPath_);
    HttpConn::isCloseLog = isCloseLog_;
    HttpConn::userCount = 0;
    HttpConn::isET = isEtConn_;
}

void WebServer::InitLog_() {
    if(!isCloseLog_) {
        Log::GetInstance()->init(
            logConfig_.path.c_str(), logConfig_.suffix.c_str(), logConfig_.buffSize, 
            logConfig_.maxLines, logConfig_.maxQueueSize);
    }
}

void WebServer::InitSqlPool_() {
    connPool_ = SqlConnPool::GetInstance();
    connPool_->Init(sqlConfig_.host, 
        sqlConfig_.user, sqlConfig_.pwd,
        sqlConfig_.dbName, sqlConfig_.port, 
        sqlConfig_.connNum, sqlConfig_.isCloseLog);
}

void WebServer::InitSocket_() {
    int ret;
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr_.sin_port = htons(port_);

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        perror("Create socket error");
        exit(-1);
    }
    ret = bind(listenFd_, (struct sockaddr *)&serverAddr_, sizeof(serverAddr_));
    if(ret < 0) {
        perror("Bind socket error");
        exit(-1);
    }
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        perror("Listen socket error");
        exit(-1);
    }

    epoll_ = new Epoll(MAX_EVENT_SIZE);
    if(epoll_->GetFd() < 0) {
        perror("Create epoll error");
        exit(-1);
    }

    epoll_->AddFd(listenFd_, isEtListen_);
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipFds_);
    if(ret < 0) {
        perror("create pipFds error");
        exit(-1);
    }
    epoll_->SetNonblock(pipFds_[1]);
    epoll_->AddFd(pipFds_[0], false , false);

    //SIG_IGN 忽略信号的处理程序
    SetSignal(SIGPIPE, SIG_IGN); //管道损坏信号
    SetSignal(SIGALRM, sigHandle); //定时器定时信号
    SetSignal(SIGTERM, sigHandle); //进程终止信号
    //定时器
    alarm(TIME_SLOT);
}

void WebServer::start() {
    LOG_INFO("Start server!");
    bool isTimeOut = false;
    while(!isClose_) {
        int eventCnt = epoll_->Wait();
        for(int i = 0; i < eventCnt; i++) {
            int socketFd = epoll_->GetEventFd(i);
            uint32_t events = epoll_->GetEvent(i);
            if(socketFd == listenFd_) {
                DealListen_();
            }
            else if(events & ((EPOLLRDHUP | EPOLLHUP | EPOLLERR))){
                timer_.action(clients_[socketFd].timerId);
                clients_[socketFd].timerId = -1;
            }
            else if(socketFd == pipFds_[0] && (events & EPOLLIN)) {
                DealSignal_(isTimeOut, isClose_);
            }
            else if(events & EPOLLIN) {
                DealRead_(socketFd);
            }
            else if(events & EPOLLOUT) {
                DealWrite_(socketFd);
            }
        }
        if(isTimeOut) {
            DealTimeOut_();
            isTimeOut = false;
        }
    }
}

void WebServer::DealTimeOut_() {
    timer_.tick();
    alarm(TIME_SLOT);
}

void WebServer::addClient_(int clientFd, sockaddr_in addr) {
    assert(clientFd > 0);
    HttpConn* client = new HttpConn(clientFd, addr);
   // TimerCb cbfunc = std::bind(timerCbFunc,  clientFd);
   auto task = std::bind(timerCbFunc, this, clientFd);
    TimerCb cbfunc = std::forward<decltype(task)>(task);
    int timerId = timer_.add(TIME_SLOT * 3, cbfunc);
    clients_[clientFd] = {client, timerId};
}

bool WebServer::DealListen_() {
    struct sockaddr_in Addr;
    socklen_t len = sizeof(Addr);
    if(isEtListen_ == false) {
        int fd = accept(listenFd_, (struct sockaddr *)&Addr, &len);
        addClient_(fd, Addr);
    } else {
        while(true) {
            int fd = accept(listenFd_, (struct sockaddr *)&Addr, &len);
            addClient_(fd, Addr);
        }
    }
    return true;
}

bool WebServer::ExtentTime_(int fd) {
    return timer_.adjust(clients_[fd].timerId, time(nullptr) + 3 * TIME_SLOT);
}

void WebServer::DealWrite_(int fd) {
    if(isReactor_) {
        ExtentTime_(fd);
        auto func = std::bind(taskWrite, clients_[fd].client, connPool_, true);
        threadpool_->addTask(func);
            
    } else {
        clients_[fd].client->write();
        ExtentTime_(fd);
    }
}

void WebServer::DealRead_(int fd) {
    if(isReactor_) {
        ExtentTime_(fd);
        CallbackFunc func = std::bind(taskRead, clients_[fd].client);
    } else {
        clients_[fd].client->read();
        CallbackFunc func = std::bind(taskRead, clients_[fd].client);
        ExtentTime_(fd);
    }
}

bool WebServer::DealSignal_(bool &isTimeOut, bool &isClose) {
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
            isClose = true;
        default:
            break;
        }
    }
    return true;
}

// void WebServer::timerCbFunc(HttpConn* client, Epoll *epoll, int fd) {
//     delete client;
//     epoll->RemoveFd(fd);
//     HttpConn::userCount--;
// }

void WebServer::timerCbFunc(WebServer* ins, int fd) {
    ins->epoll_->RemoveFd(fd);
    delete ins->clients_[fd].client;
    ins->clients_.erase(fd);
    HttpConn::userCount--;
}

void WebServer::taskWrite(HttpConn* client, SqlConnPool* connPool, bool isReactor) {
    if(isReactor) {
        if(client->read()) {
            SqlConnRAII sqlConn(&client->mysql_, connPool);
            client->process();
        }
    } else {
        SqlConnRAII sqlConn(&client->mysql_, connPool);
        client->process();
    }
}

void WebServer::taskRead(HttpConn* client) {
    client->write();
}