/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */ 
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "threadpool.h"
#include "httpconn.h"
#include "sqlconnpool.h"
#include "threadpool.h"
#include "signal.h"
#include "heaptimer.h"
#include "log/log.h"
#include <unordered_map>

class WebServer {

public:
    WebServer(int port, int sqlPort, std::string sqlUser,
        std::string sqlPwd, std::string dbName, int connPoolNum, int threadNum,
        int trigMode, bool isReactor, bool OptLinger ,bool isCloseLog, int logQueSize);
    ~WebServer();

    typedef std::function<void()> CallbackFunc;
    enum class ActorMode { PROACTOR = 0, REACTOR };

    void init();
    void start();

    bool IsCloseLog() { return isCloseLog_; }

private:
    static const int MAX_FD = 65536;
    static const int MAX_EVENT_SIZE = 10000;
    static const time_t TIME_SLOT = 5;

    struct ClientInfo {
        HttpConn *client;
        int timerId;
    };

    void InitLog_(); 
    void InitSqlPool_();
    void InitSocket_();
    void InitTrigMode_();
    void InitHttpConn_();
    void InitThreadPool_();
    
    bool DealListen_();
    bool DealSignal_(bool &isTimeOut, bool &isClose);
    void DealTimeOut_();
    void DealWrite_(int fd);
    void DealRead_(int fd);

    void addClient_(int clientFd, sockaddr_in addr);
    bool ExtentTime_(int timerId);

    bool isClose_;
    bool isCloseLog_;
    int listenFd_;
    int port_;
    char* resPath_;
    
    bool isReactor_;
    bool isOptLinger_;
    int trigMode_;
    bool isEtListen_;
    bool isEtConn_;

    struct sockaddr_in serverAddr_;
    Epoll* epoll_;

    struct LogConfig {
        std::string path;
        std::string suffix;
        int buffSize;
        int maxLines;
        int maxQueueSize;
    } logConfig_;

    struct SqlConfig{
        std::string host;
        int port;
        std::string user;
        std::string pwd;
        std::string dbName;
        int connNum;
        bool isCloseLog;
    } sqlConfig_;
    SqlConnPool *connPool_;

    int threadNum_;
    ThreadPool* threadpool_;

    HeapTimer timer_;

    std::unordered_map<int, ClientInfo> clients_;

    static void timerCbFunc(WebServer* ins, int fd);
    static void taskWrite(HttpConn* client, SqlConnPool* connPool_, bool isReactor);
    static void taskRead(HttpConn* client);
    //?
    static int pipFds_[2];
    // 函数指针指向的函数是VOID型并接受一个（int）型的参数
    static void SetSignal(int sig, void(handler)(int), bool enableRestart = true) {
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        if(enableRestart) {
            sa.sa_flags |= SA_RESTART;
        }
        sa.sa_handler = handler;
        sigfillset(&sa.sa_mask); //初始化一个信号集合，包含所有的信号。
        sigaction(sig, &sa, nullptr);
    }
    static void sigHandle(int sig) {
        int tmpErrno = errno;
        send(pipFds_[1], (char*)&sig, 1, 0);
        errno = tmpErrno;
    }
};



#endif //WEBSERVER_H