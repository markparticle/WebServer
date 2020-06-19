/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */ 
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "signal.h"
#include <unordered_map>

#include "log/log.h"
#include "utils/heaptimer.h"
#include "pool/sqlconnpool.h"
#include "pool/threadpool.h"
#include "pool/sqlconnRAII.h"
#include "http/epoll.h"
#include "http/httpconn.h"

class WebServer {
public:
    WebServer(int port, int sqlPort, std::string sqlUser,
        std::string sqlPwd, std::string dbName, int connPoolNum, int threadNum,
        int trigMode, bool isReactor, bool OptLinger ,bool isCloseLog, int logQueSize);

    ~WebServer();

    typedef std::function<void()> CallbackFunc;

    void init();

    void start();

    bool IsCloseLog() { return isCloseLog_; }

    enum class ActorMode { PROACTOR = 0, REACTOR };

    struct ClientInfo {
        HttpConn *client;
        int timerId;
    };
    
    struct LogConfig {
        std::string path;
        std::string suffix;
        int buffSize;
        int maxLines;
        int maxQueueSize;
    };
    struct SqlConfig{
        std::string host;
        int port;
        std::string user;
        std::string pwd;
        std::string dbName;
        int connNum;
        bool isCloseLog;
    };

    static const int MAX_FD = 65536;
    static const int MAX_EVENT_SIZE = 10000;
    static const time_t TIME_SLOT = 5;
    static int pipFds_[2];

private:
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

    static void timerCbFunc(WebServer* ins, int fd);

    static void taskWrite(HttpConn* client, SqlConnPool* connPool_, bool isReactor);
    static void taskRead(HttpConn* client);

    static void SetSignal(int sig, void(handler)(int), bool enableRestart = true);
    static void sigHandle(int sig) ;

    bool isClose_;
    bool isCloseLog_;
    int listenFd_;
    int port_;
    char* resPath_;
    int threadNum_;

    int trigMode_;
    bool isReactor_;
    bool isOptLinger_;
    bool isEtListen_;
    bool isEtConn_;

    struct sockaddr_in serverAddr_;

    Epoll* epoll_;

    LogConfig logConfig_;

    SqlConfig sqlConfig_;

    SqlConnPool *connPool_;

    ThreadPool* threadpool_;

    HeapTimer timer_;

    std::unordered_map<int, ClientInfo> clients_;
};



#endif //WEBSERVER_H