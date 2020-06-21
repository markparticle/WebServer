/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft GPL 2.0
 */ 

#include "httpconn.h"

const char *OK_200_TITLE= "OK";
const char *ERROR_400_TITLE = "Bad Request";
const char *ERROR_400_FORM = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *ERROR_403_TITLE = "Forbidden";
const char *ERROR_403_FORM = "You do not have permission to get file form this server.\n";
const char *ERROR_404_TITLE = "Not Found";
const char *ERROR_404_FORM = "The requested file was not found on this server.\n";
const char *ERROR_500_TITLE = "Internal Error";
const char *ERROR_500_FORM = "There was an unusual problem serving the request file.\n";

Epoll* HttpConn::epollPtr;
char* HttpConn::resPath;
int HttpConn::userCount;
bool HttpConn::isCloseLog;
bool HttpConn::isET;
SqlConnPool* HttpConn::connPool;

void HttpConn::init(int fd, const struct sockaddr_in& addr) {
    assert(fd > 0);
    Init_();
    isClose_ = false;
    addr_ = addr;
    fd_ = fd;
    epollPtr->AddFd(fd);
    LOG_INFO("Client[%d](%s:%d) in.", fd_, GetIP(), GetPort());
}

void HttpConn::Init_() {
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
    readIdx_ = 0;
    checkIdx_ = 0;
    startLine_ = 0;
    writeIdx_ = 0;
    memset(readBuff_, 0, READ_BUFF_SIZE);
    memset(writeBuff_, 0, WRITE_BUFF_SIZE);

    checkState_ = REQUESTLINE;
    requestMsg_ = {GET, 0, nullptr, nullptr, nullptr, nullptr, false ,false};
    
    bytesTosSend_ = 0;
    bytesHaveSend_ = 0;

    time_t expires_ = -1;
    int index_ = -1;
}

void HttpConn::CloseConn() {
    if(isClose_ == false && fd_ > 0){
        isClose_ = true;
        epollPtr->RemoveFd(fd_);
        LOG_INFO("Client[%d](%s:%d) quit.", fd_, GetIP(), GetPort());
        userCount--;
        fd_ = -1;
    }
}

int HttpConn::GetFd() const {
    assert(isClose_ == false);
    return fd_;
};

bool HttpConn::read() {
    if(readIdx_ >= READ_BUFF_SIZE) return false;
    int len = 0;
    if(!isET) {
        len = recv(fd_, readBuff_ + readIdx_, READ_BUFF_SIZE - readIdx_, 0);
        if(len > 0) {
            readIdx_ += len;
            return true;
        }
    }
    else {
        while (!isClose_)
        {
            len = recv(fd_, readBuff_ + readIdx_, READ_BUFF_SIZE - readIdx_, 0);
            if(len == -1) {
                //如果读完了就退出
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    return true;
                };
            } 
            else if(len == 0) {
                break;
            }
            readIdx_ += len;
        }
    }
    return false;
}

bool HttpConn::write() {
    //无数据
    if(bytesTosSend_ == 0) {
        epollPtr->Modify(fd_, EPOLLIN, isET);
        Init_();
        return true;
    }
    size_t offset = 0;
    while(!isClose_) {
        int len = writev(fd_, iov_, iovCount_);
        if(len > 0) {
            bytesHaveSend_ += len;
            offset = bytesHaveSend_ - writeIdx_;
        } else {
            //如果写入结束
            if(errno == EAGAIN) {
                LOG_DEBUG("Respone to client[%d]", fd_);
                if(bytesHaveSend_ >= iov_[0].iov_len) {
                    iov_[0].iov_len = 0;
                    iov_[1].iov_base = fileAddr_ + offset;
                    iov_[1].iov_len = bytesTosSend_;
                } else {
                    iov_[0].iov_base = writeBuff_ + bytesTosSend_;
                    iov_[0].iov_len -=  bytesHaveSend_;
                }
                epollPtr->Modify(fd_, EPOLLOUT, isET);
                return true;
            }
            //写入失败
            Unmap_();
            return false;
        }
        bytesTosSend_ -= len;
        if(bytesTosSend_ <= 0) {
            Unmap_();
            epollPtr->Modify(fd_, EPOLLIN, isET);
            if(requestMsg_.isKeepAlive) {
                Init_();
                return true;
            } else {
                return false;
            }
        }
    }
    return true;
}

void HttpConn::Unmap_() {
    if(fileAddr_) {
        munmap(fileAddr_, fileStat_.st_size);
        fileAddr_ = nullptr;
    }
}

HttpConn::LINE_STATUS HttpConn::ParseLine_() {
    for(; checkIdx_ < readIdx_; checkIdx_++) {
        char ch;
        ch = readBuff_[checkIdx_];
        if(ch == '\r') {
            if((checkIdx_ + 1) == readIdx_) {
                return LINE_OPEN;
            } 
            else if(readBuff_[checkIdx_ + 1] == '\n') {
                readBuff_[checkIdx_++] = '\0';
                readBuff_[checkIdx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(ch == '\n') {
            if(checkIdx_ > 1 && readBuff_[checkIdx_ - 1] == '\r') {
                readBuff_[checkIdx_++] = '\0';
                readBuff_[checkIdx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HttpConn::HTTP_CODE HttpConn::ParseRequestLine_(char* text) {
    //解析 method URL vision
    char* url = strpbrk(text, " \t");
    if(!url) {
        return BAD_REQUEST;
    }
    *url++ = '\0';

    char *method = text;
    if(strcasecmp(method, "GET") == 0) {
        requestMsg_.method = GET;
    } 
    else if(strcasecmp(method, "POST") == 0) {
        requestMsg_.method = POST;
        requestMsg_.cgi = true;
    } else {
        return BAD_REQUEST;
    }

    //跳过多余的空格或\t
    url += strspn(url, " \t");

    requestMsg_.version = strpbrk(url, " \t");
    if(requestMsg_.version) {
        return BAD_REQUEST;
    }
    *requestMsg_.version++ = '\0';
    requestMsg_.version = strpbrk( requestMsg_.version, " \t");
    if(strcasecmp(requestMsg_.version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }

    //跳过请求中的http://.../
    if(strncasecmp(url, "http://", 7) == 0) {
        url += 7;
        requestMsg_.url = strchr(url, '/');
    }
    else if(strncasecmp(url, "https://", 8) == 0) {
        url += 8;
        requestMsg_.url = strchr(url, '/');
    }

    if(!url || url[0] != '/') {
        return BAD_REQUEST;
    }
    if(strlen(url) == 1) {
        strcat(requestMsg_.url, "judge.html");
    }
    checkState_ = HEADER;
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseHeader_(char *text) {
    if(text[0] == '\0') {
        if(requestMsg_.contextLen > 0) {
            checkState_ = CONTENT;
        }
    } else if(strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0) {
            requestMsg_.isKeepAlive = true;
        }
    } 
    else if(strncasecmp(text, "Context-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        requestMsg_.contextLen = atoi(text);
    }
    else if(strncasecmp(text, "Host", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        requestMsg_.host = text;
    } else {
        LOG_INFO("unknow header: %s", text);
    }
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseContent_(char *text) {
    if(readIdx_ >= (checkIdx_ + requestMsg_.contextLen)) {
        text[requestMsg_.contextLen] = '\0';
        requestMsg_.context = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

char* HttpConn::GetLine_() {
    return readBuff_ + startLine_;
}

HttpConn::HTTP_CODE HttpConn::ParseHttpMsg_()
{
    LINE_STATUS lineStatue = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text;

    while(((lineStatue = ParseLine_()) == LINE_OK)
        || (checkState_ == CONTENT && lineStatue == LINE_OK)) {
        text = GetLine_();
        startLine_ = checkIdx_;
        switch (checkState_)
        {
        case REQUESTLINE:
            ret = ParseRequestLine_(text);
            if(ret == BAD_REQUEST) {
                return ret;
            }
            break;
        case HEADER:
            ret = ParseHeader_(text);
            if(ret == BAD_REQUEST) {
                return ret;
            }
            break;
        case CONTENT:
            ret = ParseContent_(text);
            lineStatue = LINE_OPEN;
            break;
        default:
            return INTERNAL_ERROR;
            break;
        } 
        if(ret == GET_REQUEST) {
            return WriteParseMsg_();
        }
    }
    return ret;
}

HttpConn::HTTP_CODE HttpConn::WriteParseMsg_() {
    char filePath[PATH_LEN];
    strcpy(filePath, resPath);
    int len = strlen(resPath);
    const char* p = strrchr(requestMsg_.url, '/');
    char fileName[PATH_LEN];
    switch (p[1])
    {
    case '0':
        strcpy(fileName, "/register.html");
        break;
    case '1':
        strcpy(fileName, "/log.html");
        break;
    default:
        strcpy(fileName, requestMsg_.url);
        break;
    }
    strncpy(filePath + len, fileName, PATH_LEN - len - 1);

    if(stat(filePath, &fileStat_) < 0) {
        return NO_RESOURSE;
    }
    if(!(fileStat_.st_mode & S_IROTH)) {
        return FORBIDDENT_REQUEST;
    }
    if(S_ISDIR(fileStat_.st_mode)) {
        return BAD_REQUEST;
    }
    int fd = open(filePath, O_RDONLY);
    fileAddr_ = (char*)mmap(0, fileStat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

bool HttpConn::AddResponse_(const char* format,...) {
    if(writeIdx_ >= WRITE_BUFF_SIZE) { return false; }
    va_list args;
    va_start(args, format);
    size_t len = vsnprintf(writeBuff_ + writeIdx_, WRITE_BUFF_SIZE - 1 - writeIdx_, format, args);
    if(len >= (WRITE_BUFF_SIZE - 1 - writeIdx_)) {
        va_end(args);
        return false;
    }
    writeIdx_ += len;
    va_end(args);
    return true;
}

bool HttpConn::AddContextType_() {
    return AddResponse_("Context-Type:%s\r\n", "text/html");
}

bool HttpConn::AddLinger_() {
    return AddResponse_("Connection:%s\r\n", (requestMsg_.isKeepAlive == true) ? "keep-alive" : "close");
}

bool HttpConn::AddBlinkLine_() {
    return AddResponse_("%s", "\r\n");
}
    
bool HttpConn::AddStatusLine_(int status, const char* title) {
    return AddResponse_("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConn::AddContentLength_(int len) {
    return AddResponse_("Context-type:%s\r\n", "text/html");
}

bool HttpConn::AddHeader_(int len) {
    return AddContentLength_(len) && AddLinger_() && AddBlinkLine_();
}

bool HttpConn::AddContent_(const char* content) {
    return AddResponse_("%s", content);
}

bool HttpConn::GenerateHttpMsg_(HTTP_CODE ret) {
    bool flag;
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        flag = (AddStatusLine_(500, ERROR_500_TITLE)
            && AddHeader_(strlen(ERROR_500_FORM))
            && AddContent_(ERROR_500_FORM));
        if(!flag) { return false;}
    }
    break;
    case BAD_REQUEST:
    {
        flag = (AddStatusLine_(404, ERROR_404_TITLE)
            && AddHeader_(strlen(ERROR_404_TITLE)) 
            && AddContent_(ERROR_404_TITLE));
        if(!flag) { return false;}
    }
    break;
    case FORBIDDENT_REQUEST:
    {
        flag = (AddStatusLine_(403, ERROR_403_TITLE)
            && AddHeader_(strlen(ERROR_403_TITLE))
            && AddContent_(ERROR_403_TITLE));
        if(!flag) { return false;}
    }
    break;
    case FILE_REQUEST:{
        if(fileStat_.st_size > 0) {
            AddHeader_(fileStat_.st_size);
            iov_[0].iov_base = writeBuff_;
            iov_[0].iov_len = writeIdx_;

            iov_[1].iov_base = fileAddr_;
            iov_[1].iov_len = fileStat_.st_size;
            iovCount_ = 2;

            bytesTosSend_ = writeIdx_ + fileStat_.st_size;
            return true;
        }
    }
    default:
        return false;
        break;
    }

    iov_[0].iov_base = writeBuff_;
    iov_[0].iov_len = writeIdx_;
    iovCount_ = 1;
    bytesTosSend_ = writeIdx_;
    return true;
}

void HttpConn::process() {
    if(read()) {
        //读取成功，开始解析数据
        auto ret = ParseHttpMsg_();
        LOG_DEBUG("parse ret %d", ret);
        if(ret == HttpConn::HTTP_CODE::NO_REQUEST) {
            //解析不完整，请求读
            LOG_DEBUG("Generated client[%d] request read!", fd_);
            epollPtr->Modify(fd_, EPOLLIN, isET);
        }
        else if(GenerateHttpMsg_(ret)) {
            //已生成报文
            LOG_DEBUG("Generated client[%d] success!", fd_);
            epollPtr->Modify(fd_, EPOLLOUT, isET);
        } else {
            LOG_DEBUG("Generated client[%d] error!", fd_);
            CloseConn();
        }
    } else {
        LOG_WARN("Read client[%d] msg error!", fd_);
        CloseConn();
    }
}