/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft GPL 2.0
 */ 
#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool
{
public:
    static SqlConnPool *GetInstance();
    MYSQL *GetConn();
    void FreeConn(MYSQL * conn);
    int GetFreeConnCount();

    void Init(std::string host, std::string user, 
        std::string pwd, std::string dbName,  int port, 
        int connSize);
    
    void ClosePool();
    static bool OpenLog() { return openLog; };

    struct SqlConfig {
        std::string host;
        std::string user;
        std::string pwd;
        std::string dbName;
        int port;
    };
    static bool openLog;

private:
    SqlConnPool();
    ~SqlConnPool();

    SqlConfig sqlConfig_;

    int MAX_CONN_;
    int useCount_;
    int freeCount_;

    std::queue<MYSQL *> connQue_;
    std::mutex mtx_;
    sem_t semId_;
};


#endif // SQLCONNPOOL_H