### SqlConnPool busy!
`/home/kelin/WebServer/code/pool/sqlconnpool.cpp:46`

1. 没有打出日志
2. assert(sql),sql地址为空，导致程序崩溃

```cpp
MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_); // 信号量-1
    {
        lock_guard<mutex> locker(mtx_); // 加锁
        sql = connQue_.front();
        connQue_.pop(); // 从queue中移除第一个元素
    }
    return sql;
}
```