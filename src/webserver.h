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
#include "timer/heaptimer.h"
#include "pool/sqlconnpool.h"
#include "pool/threadpool.h"
#include "pool/sqlconnRAII.h"
#include "http/epoll.h"
#include "http/httpconn.h"

class WebServer {
public:
    static const int MAX_FD = 50;
    static const int MAX_EVENT_SIZE = 10000;
    static const time_t TIME_SLOT = 1;
    static int pipFds_[2];
    static const int MAX_PATH = 256;

    WebServer(int port, int sqlPort, std::string sqlUser,
        std::string sqlPwd, std::string dbName, int connPoolNum, int threadNum,
        int trigMode, bool isReactor, bool OptLinger ,bool openLog, int logLevel, int logQueSize);

    ~WebServer();

    typedef std::function<void()> CallbackFunc;

    void Init();
    void Start();
    void Close();
    bool OpenLog() { return openLog_; }

    enum class ActorMode { PROACTOR = 0, REACTOR };
    struct LogConfig {
        int level;
        std::string path;
        std::string suffix;
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
    };

private:
    void InitLog_(); 
    void InitSqlPool_();
    void InitSocket_();
    void InitTrigMode_();
    void InitHttpConn_();
    void InitThreadPool_();

    void AddClient_(int fd, sockaddr_in addr);
  
    void DealListen_();
    bool DealSignal_(bool &isTimeOut);
    void DealTimeOut_();
    void DealWrite_(int fd);
    void DealRead_(int fd);

    void SendError_(int fd, const char*info);
    bool ExtentTime_(HttpConn* client);


    static void ReadCallback(HttpConn* client);

    static void WriteCallback(HttpConn* client);

    static void SetSignal(int sig, void(handler)(int), bool enableRestart = true);
    static void sigHandle(int sig) ;

    bool isClose_;
    bool openLog_;
    int listenFd_;
    int port_;
    char resPath_[MAX_PATH];
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

    SqlConnPool* connPool_;

    ThreadPool* threadpool_;

    HeapTimer* timer_;

    std::unordered_map<int, HttpConn*> users_;
};



#endif //WEBSERVER_H