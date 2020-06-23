/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft GPL 2.0
 */ 

#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>  // mmap()
#include <sys/stat.h>   // stat()
#include <sys/uio.h>   // readv/writev

#include <arpa/inet.h> // sockaddr_in

#include <stdio.h>  // vsnparintf()
#include <stdlib.h> //atoi()
#include <string.h>   //strcpy()
#include <stdarg.h>    // va_list

#include <errno.h>     // errno
#include <mysql/mysql.h> //mysql

#include <unordered_map>
#include <string>
#include <regex>

#include "epoll.h"
#include "../log/log.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
//#include "../timer/heaptimer.h"

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
    
    struct Request {
        std::string method, path, version;
        std::string content;
        int contentLen;
        std::unordered_map<std::string, std::string> header;
        //std::smatch pathMatch;
    };

public:
    HttpConn();

    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);

    bool read();
    bool write();
    void process();
    void CloseConn();
    bool IsClose() const;
    // void InitMySQLResult();
    // void InitResultFile();
    static bool OpenLog() { return openLog; };

    static Epoll* epollPtr;
    static SqlConnPool* connPool;

    static int userCount;
    static char* resPath;
    static bool openLog;
    static bool isET;

    int GetFd() const;
    struct sockaddr_in GetAddr() const;
    const char* GetIP() const;
    int GetPort() const;
    time_t GetExpires() const;
    MYSQL* GetSql() const;

    void SetExpires(time_t expires);
    void BindSql(MYSQL * mysql);
    
private:
    static const int PATH_LEN = 200;
    static const int READ_BUFF_SIZE = 5000;
    static const int WRITE_BUFF_SIZE = 1024;

    void Init_();
    void Unmap_();

    LINE_STATUS ParseLine_();

    HTTP_CODE ParseRequestLine_(std::string line);
    HTTP_CODE ParseHeader_(std::string line);
    HTTP_CODE ParseContent_(std::string line);
    HTTP_CODE DoRequest_();

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

    int fd_;
    struct  sockaddr_in addr_;
    time_t expires_;
    Request request_;

    char* readBuff_;
    size_t readIdx_;
    size_t checkIdx_;
    size_t startLine_;

    char* writeBuff_;
    size_t writeIdx_;

    CHECK_STATE checkState_;

    bool isClose_;

    char* fileAddr_;
    int iovCount_;
    struct stat fileStat_;
    struct iovec iov_[2];

    size_t bytesTosSend_;
    size_t bytesHaveSend_;

    MYSQL *mysql_;
};


#endif //HTTPCONN_H