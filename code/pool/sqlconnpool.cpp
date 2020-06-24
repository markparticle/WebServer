/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */ 

#include "sqlconnpool.h"
using namespace std;


bool SqlConnPool::openLog;

SqlConnPool::SqlConnPool() {
    useCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool* SqlConnPool::GetInstance() {
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(char* host, char* user,
                       char* pwd, char* dbName, int port,
                       int connSize) {
    memcpy(sqlConfig_.host, host, strlen(host));
    memcpy(sqlConfig_.user, host, strlen(user));
    memcpy(sqlConfig_.pwd, host, strlen(pwd));
    memcpy(sqlConfig_.dbName, host, strlen(dbName));
    sqlConfig_.port = port;
    for (int i = 0; i < connSize; i++)
    {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("MySql init error!");
            exit(1);
        }
        sql = mysql_real_connect(sql, sqlConfig_.host,
                                 sqlConfig_.user, sqlConfig_.pwd,
                                 sqlConfig_.dbName, sqlConfig_.port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
            exit(1);
        }
        connQue_.push(sql);
    }
    MAX_CONN_ = connSize;
    sem_init(&semId_, 0, MAX_CONN_);
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);
    {
        lock_guard<mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    if(sql) 
    {
        lock_guard<mutex> locker(mtx_);
        connQue_.push(sql);
        sem_post(&semId_);
    }
}

void SqlConnPool::ClosePool() {
    {
        lock_guard<mutex> locker(mtx_);
        while(!connQue_.empty()) {
            auto item = connQue_.front();
            connQue_.pop();
            mysql_close(item);
        }
        mysql_library_end();        
    }
}

int SqlConnPool::GetFreeConnCount() {
    {
        lock_guard<mutex> locker(mtx_);
        return connQue_.size();
    }
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}
