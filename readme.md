# TinyWebServer

## 环境需求
* C++14
* Linux

## 目录树
```
.
├── code
│   ├── buffer
│   ├── config
│   ├── http
│   ├── log
│   ├── timer
│   ├── pool
│   ├── server
│   ├── main.cpp
│   └── readme.md
├── test
│   ├── Makefile
│   └── test.cpp
├── resources
│   └── html
├── bin
│   └── server
├── build
│   └── Makefile
├── log
│   └── xxx.log
├── webbench-1.5
├── Makefile
├── LICENSE
└── readme.md
```

## 功能
* IO模型：Reactor/Proactor
* 实现 ET/LT(Epoll触发模式)的socket听/读/写
* HTTP请求报文解析
* 动态缓冲区
* 线程池、数据库连接池
* 定时器
* 日志系统
* 大文件传输
* 注册登录

## 项目启动
```bash
make
./bin/server
```

## 单元测试
```bash
make
./bin/server
```

## 压力测试
```bash
cd ./webbench-1.5/webbench -c 10000 -t 10 http://ip:port/
```
* 测试环境: Ubuntu:19.10 cpu:i5-8400 内存:8G 
* QPS 9000~10000

## TODO
* HTTP请求体数据解析 Json/form-data
* Config(Json格式)
* 单元测试
* 循环缓冲区
* HTTPS加密(Cryto++库)
* 文件压缩
* 登录 cookie/session


