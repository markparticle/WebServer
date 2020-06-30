# WebServer
用C++实现的高性能WEB服务器，经过webbenchh压力测试可以实现上万的QPS

## 功能
* 使用正则解析HTTP请求报文，可以处理静态资源请求
* 用最小堆实现的定时器，支持HTTP长连接以及超时断开
* 采用epoll ET边沿触发模式作为IO复用技术
* 实现多线程的Reactor IO模式，主线程响应事件，线程池处理事件
* 实现自动增长缓冲区，作为HTTP连接的接收与发送的缓冲区
* 实现数据库连接池，提高对数据库操作的性能
* 通过访问数据库操作实现用户注册、登录功能
* 通过阻塞队列实现异步日志系统，记录服务器运行状态
* 增加logsys,threadpool测试单元(todo: timer, sqlconnpool, httprequest, httpresponse) 

## 环境要求
* Linux
* C++14
* MySql

## 目录树
```
.
├── code           源代码
│   ├── buffer
│   ├── config
│   ├── http
│   ├── log
│   ├── timer
│   ├── pool
│   ├── server
│   └── main.cpp
├── test           单元测试
│   ├── Makefile
│   └── test.cpp
├── resources      静态资源
│   ├── index.html
│   ├── image
│   ├── video
│   ├── js
│   └── css
├── bin            可执行文件
│   └── server
├── log            日志文件
├── webbench-1.5   压力测试
├── build          
│   └── Makefile
├── Makefile
├── LICENSE
└── readme.md
```


## 项目启动
需要先配置好对应的数据库
```bash
// 建立yourdb库
create database yourdb;

// 创建user表
USE yourdb;e
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, passwd) VALUES('name', 'passwd');
```

```bash
make
./bin/server
```

## 单元测试
```bash
cd test
make
./test
```

## 压力测试
```bash
./webbench-1.5/webbench -c 10000 -t 10 http://ip:port/
```
* 测试环境: Ubuntu:19.10 cpu:i5-8400 内存:8G 
* QPS 9000~10000

## TODO
* config配置
* 完善单元测试
* 实现循环缓冲区
* HTTPS加密(Cryto++库)
* 登录 cookie/session


