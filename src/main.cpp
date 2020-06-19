/*
 * @Author       : mark
 * @Date         : 2020-06-18
 * @copyleft GPL 2.0
 */ 
#include "webserver.h"
//#include "log/log.h"
//#include "http/epoll.h"
//#include <stdio.h>

int main() {

    WebServer server(1314, 3306, "root", "root", "webserver", 2, 8, 3, true, true, false, 800);
    server.start();
    // ThreadPool(4);
    // Log::GetInstance()->init();
    // SqlConnPool::GetInstance();
    // sockaddr_in addr;
    // HttpConn S(0, addr);
    getchar();
}