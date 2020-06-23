/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft GPL 2.0
 */ 
#include "httpconn.h"
#include <iostream>

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
SqlConnPool* HttpConn::connPool;
//eapTimer* timer;
char* HttpConn::resPath;
int HttpConn::userCount;
bool HttpConn::openLog;
bool HttpConn::isET;

HttpConn::HttpConn() { 
    readBuff_ = new char[READ_BUFF_SIZE];
    writeBuff_ = new char[WRITE_BUFF_SIZE];
    expires_ = 0;
    Init_(); 
};

HttpConn::~HttpConn() { 
    delete[] readBuff_;
    delete[] writeBuff_;
    CloseConn(); 
};

void HttpConn::init(int fd, const struct sockaddr_in& addr) {
    assert(fd > 0);
    Init_();
    userCount++;
    isClose_ = false;
    addr_ = addr;
    fd_ = fd;
    epollPtr->AddFd(fd);
    LOG_INFO("Client[%d](%s:%d) in", fd_, GetIP(), GetPort());
}

void HttpConn::Init_() {
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
    readIdx_ = 0;
    checkIdx_ = 0;
    startLine_ = 0;
    writeIdx_ = 0;

    memset(readBuff_, 0, READ_BUFF_SIZE);
    memset(writeBuff_, 0, WRITE_BUFF_SIZE);

    checkState_ = REQUESTLINE;

    request_.content = request_.method = request_.method = request_.version = "";
    request_.contentLen = 0;
    request_.header.clear();
    
    bytesTosSend_ = 0;
    bytesHaveSend_ = 0;
}

void HttpConn::CloseConn() {
    if(isClose_ == false){
        isClose_ = true;
        epollPtr->RemoveFd(fd_);
        userCount--;
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), userCount);
    }
}

bool HttpConn::IsClose() const {
    return isClose_;
};

int HttpConn::GetFd() const {
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
        return false;
    }
    else {
        while (!isClose_)
        {
            len = recv(fd_, readBuff_ + readIdx_, READ_BUFF_SIZE - readIdx_, 0);
            if(len > 0) {
                readIdx_ += len;
            }
            else {
                /* 读写完毕或不需要重新读或者写 */ 
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    return true;
                } 
                else {
                    return false;
                }
            } 
        }
    }
}

bool HttpConn::write() {
    //无数据
    LOG_DEBUG("Send to client[%d] response", fd_);
    if(bytesTosSend_ == 0) {
        LOG_DEBUG("Not Data to client[%d] response", fd_);
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
        } 
        else {
            /* 发送完成 */
            if(errno == EAGAIN) {
                LOG_DEBUG("Send response to client[%d] success!", fd_);
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
            /* 发送失败 */
            LOG_ERROR("Send client[%d] respond error", fd_);
            Unmap_();
            return false;
        }
        bytesTosSend_ -= len;
        if(bytesTosSend_ <= 0) {
            Unmap_();
            epollPtr->Modify(fd_, EPOLLIN, isET);
            if(request_.header["keep-alive"] == "true") {
                LOG_DEBUG("client[%d] keep-alive", fd_);
                Init_();
                return true;
            } else {
                return false;
            }
        }
    }
}

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

time_t HttpConn::GetExpires() const {
    return expires_;
}

void HttpConn::SetExpires(time_t expires)  {
    expires_ = expires;
}

void HttpConn::BindSql(MYSQL * mysql) {
    mysql_ = mysql;
}

MYSQL* HttpConn::GetSql() const {
    return mysql_;
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
            /* 行尾 */
            if(checkIdx_ > 1 && readBuff_[checkIdx_ - 1] == '\r') {
                readBuff_[checkIdx_ - 1] = '\0';
                readBuff_[checkIdx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HttpConn::HTTP_CODE HttpConn::ParseRequestLine_(std::string line) {
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;

    if (std::regex_match(line, subMatch, patten))
    {   
        request_.method = subMatch[1];
        request_.path = subMatch[2];
        request_.version = subMatch[3];
        LOG_DEBUG("client[%d]: [%s], [%s], [%s]", fd_,
                request_.method.c_str(), 
                request_.path.c_str(), 
                request_.version.c_str());
        checkState_ = HEADER;
        return NO_REQUEST;
    } 
    return BAD_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseHeader_(std::string line) {
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if(line[0] == '\0') {
        return GET_REQUEST;
    }
    if(std::regex_match(line, subMatch, patten)) {
        request_.header[subMatch[1]] = subMatch[2];
    }
    if(request_.header.count("Content-Length") > 0){
        request_.contentLen = stoi(request_.header["Content-Length"]);
        if(request_.contentLen >= 0) {
            checkState_ = CONTENT;
            return NO_REQUEST;
        }
    }
    return NO_REQUEST;
}
HttpConn::HTTP_CODE HttpConn::ParseContent_(std::string line) {
    if(readIdx_ >= (checkIdx_ + request_.contentLen)) {
        line[request_.contentLen] = '\0';
        request_.content = line;
        LOG_DEBUG("client[%d] context:%s", fd_, line);
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseHttpMsg_()
{
    LINE_STATUS lineStatue = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text;

    while(((lineStatue = ParseLine_()) == LINE_OK)
        || (checkState_ == CONTENT && lineStatue == LINE_OK)) {
        text = readBuff_ + startLine_;
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
            return DoRequest_();
        }
    }
    return ret;
}

HttpConn::HTTP_CODE HttpConn::DoRequest_() {
    std::string filePath = resPath;
    int len = strlen(resPath);
    if(request_.path == "/") {
        request_.path += "index.html";
    }
    filePath += request_.path;
    if(stat(filePath.c_str(), &fileStat_) < 0) {
        return NO_RESOURSE;
    }
    if(!(fileStat_.st_mode & S_IROTH)) {
        return FORBIDDENT_REQUEST;
    }
    if(S_ISDIR(fileStat_.st_mode)) {
        return BAD_REQUEST;
    }
    
    int fd = open(filePath.c_str(), O_RDONLY);
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
    return AddResponse_("Connection:%s\r\n", request_.header["Connection"]);
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
    case NO_RESOURSE:
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
        if(!flag) { return false; }
    }
    break;
    case FILE_REQUEST:{
        flag = AddStatusLine_(200, OK_200_TITLE);
        if(fileStat_.st_size > 0) {
            flag |= AddHeader_(fileStat_.st_size);
            iov_[0].iov_base = writeBuff_;
            iov_[0].iov_len = writeIdx_;

            iov_[1].iov_base = fileAddr_;
            iov_[1].iov_len = fileStat_.st_size;
            iovCount_ = 2;

            bytesTosSend_ = writeIdx_ + fileStat_.st_size;
            return true;
        } else {
            const char *t = "<html><body></body></html>";
            flag |= AddHeader_(strlen(t)) && AddContent_(t);
        }
        if(!flag) { return false; }
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
        LOG_DEBUG("parse client[%d] ret %d", fd_, ret);
        if(ret == HttpConn::HTTP_CODE::NO_REQUEST) {
            //解析不完整，继续读
            LOG_DEBUG("Generated client[%d] request read!", fd_);
            epollPtr->Modify(fd_, EPOLLIN, isET);
        }
        else if(GenerateHttpMsg_(ret)) {
            //已生成报文
            LOG_DEBUG("Generated respons to client[%d] success!", fd_);
            epollPtr->Modify(fd_, EPOLLOUT, isET);
        } 
        else {
            LOG_WARN("Generated respons client[%d] error!", fd_);
            CloseConn();
        }
    }
    else {
        LOG_WARN("Read client[%d] msg error!", fd_);
        CloseConn();
    }
}