/*
 * @Author       : mark
 * @Date         : 2020-06-19
 * @copyleft GPL 2.0
 */ 

#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

class SqlConnRAII {
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    ~SqlConnRAII() {
        if(sql_) {
            connpool_->FreeConn(sql_);
        }
        connpool_->ClosePool();
    }
    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif //SQLCONNRAII_H