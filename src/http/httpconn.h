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
#include "epoll.h"
#include "../log/log.h"

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

public:
    HttpConn(int sockFd, sockaddr_in addr)
        :fd_(sockFd), addr_(addr)
    { 
        epollPtr->AddFd(sockFd);
        Init_(); 
    }
    ~HttpConn() = default;

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
    MYSQL *mysql_;
    
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
    HTTP_CODE DoRequest_();

    bool AddResponse_(const char* format,...);
    bool AddStatusLine_(int status, const char* title);
    bool AddContentLength_(int len);
    bool AddContent_(const char* content);
    bool AddHeader_(int len);
    bool AddContextType_();
    bool AddLinger_();
    bool AddBlinkLine_();

    HTTP_CODE ProcessRead_();
    bool ProcessWrite_(HTTP_CODE ret);

    
    int fd_;
    sockaddr_in addr_;

    char readBuff_[READ_BUFF_SIZE];
    size_t readIdx_;
    size_t checkIdx_;
    size_t startLine_;

    char writeBuff_[WRITE_BUFF_SIZE];
    size_t writeIdx_;

    CHECK_STATE checkState_;

    struct RequestMsg {
        HttpConn::METHOD method;
        int contextLen;
        char* context;
        char* host;
        char* version;
        char* url;
        bool cgi;
        bool isKeepAlive;
    } requestMsg_;

    char* fileAddr_;
    int iovCount_;
    struct stat fileStat_;
    struct iovec iov_[2];

    size_t bytesTosSend_;
    size_t bytesHaveSend_;
};


#endif //HTTPCONN_H