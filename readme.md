# TinyWebServer

## 环境要求
* C++14
* Linux
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
│   └── html
├── bin            可执行文件
│   └── server
├── log            日志
│   └── xxx.log
├── webbench-1.5   压力测试
├── build          
│   └── Makefile
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
* 大文件接收
* 注册登录

## 项目启动
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
* HTTP请求体数据解析 Json/form-data
* Config(Json格式)
* 完善单元测试
* 完善循环缓冲区
* HTTPS加密(Cryto++库)
* 文件压缩
* 文件传输
* 登录 cookie/session


