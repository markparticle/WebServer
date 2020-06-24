/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft GPL 2.0
 */ 
#include "httpconn.h"
using namespace std;

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
unordered_map<string, int> HttpConn::htmlMap {
            {"/index.html", 0}, {"/register.html", 1}, {"/login.html", 2},  {"/welcome.html", 3},
            {"/video.html", 4}, {"/picture.html", 5}, {"/file.html", 6} };
unordered_set<string> HttpConn::htmlSet{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture",
             "/file"};

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

void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    Init_();
    userCount++;
    isClose_ = false;
    addr_ = addr;
    fd_ = fd;
    epollPtr->AddFd(fd);
    //LOG_INFO("Client[%d](%s:%d) in", fd_, GetIP(), GetPort());
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

    request_.body = request_.method = request_.method = request_.version = "";
    request_.header.clear();
    
    bytesToSend_ = 0;
    bytesHaveSend_ = 0;
}

void HttpConn::CloseConn() {
    if(isClose_ == false){
        isClose_ = true;
        epollPtr->RemoveFd(fd_);
        userCount--;
        //LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), userCount);
    }
}

bool HttpConn::IsClose() const {
    return isClose_;
};

int HttpConn::GetFd() const {
    return fd_;
};

bool HttpConn::read() {
    if(readIdx_ >= READ_BUFF_SIZE) { return false; };
    int len = 0;
    if(!isET) {
        len = recv(fd_, readBuff_ + readIdx_, READ_BUFF_SIZE - readIdx_, 0);
        if(len > 0) {
            readIdx_ += len;
            return true;
        }
    }
    else {
        while (!isClose_) {
            len = recv(fd_, readBuff_ + readIdx_, READ_BUFF_SIZE - readIdx_, 0);
            if(len > 0) {
                readIdx_ += len;
            }
            else {
                /* 读写完毕或不需要重新读或者写 */ 
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    return true;
                } 
                break;
            } 
        }
    }
    return false;
}

bool HttpConn::write() {
    LOG_DEBUG("Send to client[%d] response", fd_);
    /* 无可读数据 */
    if(bytesToSend_ == 0) {
        LOG_DEBUG("Not Data to client[%d] response", fd_);
        epollPtr->Modify(fd_, EPOLLIN, isET);
        Init_();
        return true;
    }
    size_t offset = 0;
    /* 直接从iov中发送数据 */
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
                    iov_[1].iov_len = bytesToSend_;
                } else {
                    iov_[0].iov_base = writeBuff_ + bytesToSend_;
                    iov_[0].iov_len -=  bytesHaveSend_;
                }
                epollPtr->Modify(fd_, EPOLLOUT, isET);
                return true;
            }
            /* 发送失败 */
            LOG_ERROR("Send client[%d] respond error", fd_);
            Unmap_();
            break;
        }
        bytesToSend_ -= len;
        if(bytesToSend_ <= 0) {
            Unmap_();
            if(request_.header["keep-alive"] == "true") {
                epollPtr->Modify(fd_, EPOLLIN, isET);
                LOG_DEBUG("client[%d] keep-alive", fd_);
                Init_();
                return true;
            }
            break;
        }
    }
    return false;
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

void HttpConn::Unmap_() {
    if(fileAddr_) {
        munmap(fileAddr_, fileStat_.st_size);
        fileAddr_ = nullptr;
    }
}

void HttpConn::GetRequestLine_() {
    for(; checkIdx_ < readIdx_; checkIdx_++) {
        char ch;
        ch = readBuff_[checkIdx_];
        if(ch == '\r') {
            if(readBuff_[checkIdx_ + 1 ] == '\n') {
                readBuff_[checkIdx_++] = '\0';
                readBuff_[checkIdx_++] = '\0';
                break;
            }
        }
        // else if(ch == '\n') {
        //      if(checkIdx_ > 1 && readBuff_[checkIdx_ - 1] == '\r') {
        //         LOG_DEBUG("down!")
        //         readBuff_[checkIdx_ - 1] = '\0';
        //         readBuff_[checkIdx_++] = '\0';
        //         break;
        //     }
        // }
    }
}

HttpConn::PARSE_STATE HttpConn::ParseRequestLine_(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten)) {   
        request_.method = subMatch[1];
        request_.path = subMatch[2];
        request_.version = subMatch[3];
        LOG_DEBUG("Client[%d]: [%s], [%s], [%s]", fd_,
                request_.method.c_str(), 
                request_.path.c_str(), 
                request_.version.c_str());
        return OK;
    } 
    return ERROR;
}

HttpConn::PARSE_STATE HttpConn::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        request_.header[subMatch[1]] = subMatch[2];
        return CONTINUE;
    } 
    return OK;
}

HttpConn::PARSE_STATE HttpConn::ParseContent_(const string& line) {
    request_.body = line;
    LOG_DEBUG("Client[%d] Body:%s, len:%d", fd_, line.c_str(),line.size());
    return OK;
}

HttpConn::HTTP_CODE HttpConn::ParseRequest_()
{
    PARSE_STATE ret = OK;
    int next = 0;
    int startLine = 0;
    while(true) {
        GetRequestLine_();
        char* line = readBuff_ + startLine;
        startLine = checkIdx_;
        switch (next)
        {
        case 0:
            ret = ParseRequestLine_(line);
            break;
        case 1:
            ret = ParseHeader_(line);
            break;
        case 2:
            ret = ParseContent_(line);
            break;
        default:
            break;
        }
        if(ret == OK) {
            next++;
        } 
        else if(ret == ERROR) {
            return BAD_REQUEST; 
        }
        if(next == 3) {
            return DoRequest_();
        }
    }
    return NO_REQUEST;
}

bool HttpConn::UserVerify(const string &name, const string &pwd, bool isLogin) {
    LOG_TEST(1, "Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql = connPool->GetConn();
    unsigned int j = 0;
    bool flag = false;
    char order[256];
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    sprintf(order, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_TEST(1, "%s", order);

    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_TEST(0, "MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_TEST(0,"pwd error!");
            }
        } 
        else { 
            flag = false; 
            LOG_TEST(0, "user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_TEST(0, "regirster!");
        sprintf(order, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_TEST(1, "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_TEST(1, "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    connPool->FreeConn(sql);
    LOG_TEST(0, "UserVerify success!!");
    return flag;
}

int converHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void HttpConn::ParseFromUrlencoded_() {
    if(request_.header["Content-Type"] == "application/x-www-form-urlencoded") {
        int n = request_.body.size();
        string key, value;
        int num = 0;
        for(int i = 0, j = 0; i <= n; i++) {
            char ch = request_.body[i];
            switch (ch) {
            case '=':
                key = request_.body.substr(j, i - j);
                j = i + 1;
                break;
            case '+':
                request_.body[i] = ' ';
                break;
            case '%':
                num = converHex(request_.body[i + 1]) * 16 + converHex(request_.body[i + 2]);
                request_.body[i + 2] = num % 10 + '0';
                request_.body[i + 1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                value = request_.body.substr(j, i - j);
                j = i + 1;
                request_.post[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            case '\0':
                value = request_.body.substr(j, i - j);
                request_.post[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            default:
                break;
            }
        }
    }
}

/* todo 
void HttpConn::ParsePost() {
}
void HttpConn::ParseFormData() {
}
void HttpConn::ParseJson() {
}
*/

void StrToLow(string& str) {
    size_t n = str.size();
    for(size_t i = 0; i < n; i++) {
        str[i] = tolower(str[i]);
    }
}

HttpConn::HTTP_CODE HttpConn::DoRequest_() {
    char filePath[256];
    strcpy(filePath, resPath);
    StrToLow(request_.path);
    LOG_DEBUG("%s %d", request_.path.c_str() ,htmlSet.size());
    if(request_.path == "/") {
        request_.path = "/index.html"; 
    } else {
        for(auto &item: htmlSet) {
            if(item == request_.path) {
                request_.path += ".html";
                break;
            }
        }
    }

    if(request_.method == "POST") {
        if(htmlMap[request_.path] == 1 || htmlMap[request_.path] == 2) {
            ParseFromUrlencoded_();
            const string& name = request_.post["username"];
            const string& pwd = request_.post["password"];
            LOG_DEBUG("Form: user:%s pwd:%s", name.c_str(), pwd.c_str());
            if(!name.empty() && !pwd.empty()) {
                bool isLogin = (htmlMap[request_.path] == 2);
                if(UserVerify(name, pwd, isLogin)) {
                    request_.path = "/welcome.html";
                     LOG_ERROR("TO welcome");
                } 
                else if(isLogin){
                    request_.path = "/logError.html";
                    
                    LOG_ERROR("username:%s, password error", name.c_str());
                } else {
                    request_.path = "/registerError.html";
                    LOG_ERROR("username:%s is used", name.c_str());
                }
            }
            else {
                LOG_ERROR("Parse content:%s, error", request_.body.c_str());
            }
        }
    }
    strcat(filePath, request_.path.c_str()); 
    LOG_DEBUG("FilePath:%s", filePath);
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

bool HttpConn::GenerateResponse_(HTTP_CODE ret) {
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

            bytesToSend_ = writeIdx_ + fileStat_.st_size;
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
    bytesToSend_ = writeIdx_;
    return true;
}

void HttpConn::process() {
    if(read()) {
        /* 读取成功，解析请求头 */
        auto ret = ParseRequest_();
        if(ret == NO_REQUEST) {
            LOG_DEBUG("Generated client[%d] request read!", fd_);
            epollPtr->Modify(fd_, EPOLLIN, isET);
        }
        else if(GenerateResponse_(ret)) {
            /* 生成响应头 */
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