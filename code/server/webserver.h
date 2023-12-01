/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */ 
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h" // 定时器处理非活动连接，基于升序链表的定时器，超时断开连接
#include "../pool/sqlconnpool.h" // 数据库连接池
#include "../pool/threadpool.h" // 线程池
#include "../pool/sqlconnRAII.h" // 数据库连接池RAII，RAII：资源获取即初始化，自动释放资源
#include "../http/httpconn.h" // http连接处理类

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    ~WebServer();
    void Start();

private:
    bool InitSocket_(); // 初始化套接字
    void InitEventMode_(int trigMode); // 初始化触发模式，有LT和ET两种，LT是默认的，ET是高速模式
    void AddClient_(int fd, sockaddr_in addr); // 添加客户端
  
    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char*info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int MAX_FD = 65536;

    static int SetFdNonblock(int fd);

    int port_;
    bool openLinger_; // 是否优雅关闭
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;
    int listenFd_;
    char* srcDir_; // 资源路径
    
    uint32_t listenEvent_; // 设置监听socket的事件类型，有读和写两种
    uint32_t connEvent_; // 设置连接socket的事件类型，有读和写两种
   
    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;
};


#endif //WEBSERVER_H