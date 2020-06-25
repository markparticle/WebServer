/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */ 
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "signal.h"
#include <unordered_map>

#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/epoll.h"
#include "../http/httpconn.h"

class WebServer {
public:
    WebServer(int port, int sqlPort, const char* sqlUser,
        const char* sqlPwd, const char* dbName, int connPoolNum, int threadNum,
        int trigMode, bool isReactor, bool OptLinger ,bool openLog, int logLevel, int logQueSize);

    ~WebServer();

    void Init();
    void Start();
    void Close();
    bool OpenLog() { return openLog_; }

    static const int MAX_FD = 65536;
    static const int MAX_EVENT_SIZE = 10000;
    static const time_t TIME_SLOT = 5;
    static int pipFds_[2];
    static const int MAX_PATH = 256;

    struct LogConfig {
        int level;
        char path[128];
        char suffix[24];
        int maxLines;
        int maxQueueSize;
    };
    struct SqlConfig{
        char host[24];
        int port;
        char user[48];
        char pwd[48];
        char dbName[48];
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
    static void SetSignal(int sig, void(handler)(int), bool restart = true);
    static void sigHandle(int sig) ;

    /* server config */
    bool isClose_;
    bool openLog_;

    int port_;
    char* resPath_;

    int trigMode_;
    bool isReactor_;
    bool isOptLinger_;
    bool isEtListen_;
    bool isEtConn_;

    LogConfig logConfig_;
    SqlConfig sqlConfig_;
    int threadNum_;

    int listenFd_;
    struct sockaddr_in serverAddr_;

    Epoll* epoll_;
    SqlConnPool* connPool_;
    ThreadPool* threadpool_;
    HeapTimer* timer_;
    HttpConn* users_;
};



#endif //WEBSERVER_H