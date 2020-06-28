/*
 * @Author       : mark
 * @Date         : 2020-06-18
 * @copyleft GPL 2.0
 */ 
#include <unistd.h>
#include "server/webserver.h"

int main() {
    //daemon(1, 0);

    WebServer server(
        1314, 0, true, true, 
        3306, "root", "root", "webserver", 
        30, 16, true, 0, 0);
    server.Start();
} 







