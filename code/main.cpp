/*
 * @Author       : mark
 * @Date         : 2020-06-18
 * @copyleft GPL 2.0
 */ 
#include <unistd.h>
#include "server/webserver.h"

int main() {
    /* 守护进程 后台运行 */
    //daemon(1, 0); 
    WebServer server(
        1315, 3, true, false,               /* 端口 ET模式 Proactor/Reactor 优雅退出  */
        3306, "root", "root", "webserver",   /* sql配置 */
        1, 4, false, 2, 5000);               /* 连接池 线程池 日志开关 日志等级 日志异步队列最大 */
    server.Start();
} 







