/*
 * @Author       : mark
 * @Date         : 2020-06-25
 * @copyleft GPL 2.0
 */ 
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     // errno
#include <mysql/mysql.h> //mysql

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"

class HttpRequest {
public:
    HttpRequest() {
        Init();
    }
    
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,        
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
    
    bool parse(Buffer& buff);

    std::string path() const{
        return path_;
    }

    std::string& path(){
        return path_;
    }
    std::string method() const {
        return method_;
    }
    
    std::string version() const {
        return method_;
    }

    std::string GetPost(const std::string& key) const {
        assert(key != "");
        if(post_.count(key) == 1) {
            return post_.find(key)->second;
        }
        return "";
    }

    bool IsKeepAlive() const {
        if(post_.count("Connection") == 1) {
            return post_.find("Connection")->second == "Keep-Alive" && version_ == "1.1";
        }
        return false;
    }

    void Init() {
        method_ = path_ = version_ = body_ = "";
        state_ = REQUEST_LINE;
        header_.clear();
        post_.clear();
    }

    /* 
    todo 
    void HttpConn::ParseFormData() {}
    void HttpConn::ParseJson() {}
    */

private:
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseBody_(const std::string& line);

    void ParsePath_();
    void ParsePost_();
    void ParseFromUrlencoded_();

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);
};


#endif //HTTP_REQUEST_H