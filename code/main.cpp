/*
 * @Author       : mark
 * @Date         : 2020-06-18
 * @copyleft GPL 2.0
 */ 
#include <unistd.h>
#include "server/webserver.h"

int main() {
    //daemon(1, 0);
    WebServer server(1315, 
            3306, "root", "root", "webserver", 
            10, 16, 
            1, true, true,
            true, 2, 1000);
    server.Init();
    server.Start();
}







