/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft GPL 2.0
 */ 

#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <sys/socket.h>
#include <arpa/inet.h> // sockaddr_in
#include <stdio.h>  // vsnparintf()
#include <stdlib.h> //atoi()
#include <string.h>   //strcpy()
#include <sys/mman.h>  // mmap()
#include <sys/stat.h>   // stat()
#include <stdarg.h>    // va_list
#include <sys/uio.h>   // readv/writev
#include <errno.h>     // errno
#include <mysql/mysql.h> //mysql
#include <arpa/inet.h> //sockaddr_in
#include <sys/types.h>
#include <sys/socket.h>
#include "epoll.h"
#include "../log/log.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"


class HttpConn {
public:
    enum METHOD {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNEXT,
        PATH,
    };

    enum CHECK_STATE {
        REQUESTLINE = 0,
        HEADER,
        CONTENT,
    };
    
    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };
    
    enum LINE_STATUS {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN,
    };

    struct RequestMsg {
        HttpConn::METHOD method;
        int contextLen;
        char* context;
        char* host;
        char* version;
        char* url;
        bool cgi;
        bool isKeepAlive;
    };

public:
    HttpConn() {
        Init_();
    };
    ~HttpConn() { CloseConn(); };

    void init(int sockFd, const sockaddr_in& addr);
   // bool read();
    bool read();
    bool write();
    void process();
    void CloseConn();

    // void InitMySQLResult();
    // void InitResultFile();
    static bool IsCloseLog() { return isCloseLog; };

    static Epoll* epollPtr;
    static int userCount;
    static char* resPath;
    static bool isCloseLog;
    static bool isET;
    static SqlConnPool* connPool;


    int GetFd() const;

    struct sockaddr_in GetAddr() const {
        return addr_;
    }

    const char* GetIP() const {
        return inet_ntoa(addr_.sin_addr);
    }

    int GetPort() const {
        return addr_.sin_port;
    }
    time_t GetExpires() const {
        return expires_;
    }
    void SetExpires(time_t expires)  {
        expires_ = expires;
    }
    int GetIndex() const {
        return index_;
    }
    void SetIndex(int idx) {
        index_ = idx;
    }
    int GetFd(int idx) const {
        return index_;
    }

    void BindSql(MYSQL * mysql) {
        mysql_ = mysql;
    }
    MYSQL *  GetSql() const {
        return mysql_;
    }
    
private:
    static const int PATH_LEN = 200;
    static const int READ_BUFF_SIZE = 2048;
    static const int WRITE_BUFF_SIZE = 1024;

    void Init_();
    void Unmap_();

    LINE_STATUS ParseLine_();
    char* GetLine_();

    HTTP_CODE ParseRequestLine_(char *text);
    HTTP_CODE ParseHeader_(char *text);
    HTTP_CODE ParseContent_(char *text);
    HTTP_CODE WriteParseMsg_();

    bool AddResponse_(const char* format,...);
    bool AddStatusLine_(int status, const char* title);
    bool AddContentLength_(int len);
    bool AddContent_(const char* content);
    bool AddHeader_(int len);
    bool AddContextType_();
    bool AddLinger_();
    bool AddBlinkLine_();

    HTTP_CODE ParseHttpMsg_();
    bool GenerateHttpMsg_(HTTP_CODE ret);

    time_t expires_;
    int index_;
    int fd_;
    struct  sockaddr_in addr_;

    char readBuff_[READ_BUFF_SIZE];
    size_t readIdx_;
    size_t checkIdx_;
    size_t startLine_;

    char writeBuff_[WRITE_BUFF_SIZE];
    size_t writeIdx_;

    CHECK_STATE checkState_;

    bool isClose_;

    RequestMsg requestMsg_;

    char* fileAddr_;
    int iovCount_;
    struct stat fileStat_;
    struct iovec iov_[2];

    size_t bytesTosSend_;
    size_t bytesHaveSend_;

    MYSQL *mysql_;
};


#endif //HTTPCONN_H