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
    WebServer server(1314, 3306, "root", "root", "webserver", 10, 8, 3, true, false, false, 100);
    server.Init();
    server.Start();
}







