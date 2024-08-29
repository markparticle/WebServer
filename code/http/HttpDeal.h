#ifndef HTTP_DEAL_H
#define HTTP_DEAL_H
#include <unordered_map>
#include <functional>
#include <fcntl.h>
#include <fstream>
#include "../MD5/MD5.h"
#include "../webserver/DEBUGUTIL.h"
#include "HttpUtil.h"
#include "../buffer/buffer.h"
#include "../mysql/sqlconnpool.h"
#include "../mysql/mysqlUtil.h"
typedef enum{
    NONE,GET,POST,DELETE,PUT,
} HTTP_ENUM_E;
typedef struct{
    HTTP_ENUM_E enumMethod = NONE;
    std::string strPath;
    std::string strVersion;
    bool isKeepAlive = false;
    ssize_t iHeaderLen = 0;
    ssize_t iContentLen = 0;
    std::unordered_map<std::string,std::string> mapHeader;
} HTTP_HEADER_T;
typedef struct{
    HTTP_ENUM_E enumMethod;
    std::string strUrl;
    std::function<void()> funCallBack;

} HTTP_REQUEST_T;

typedef struct
{
    std::string strRealm;
    std::string strUser;
    std::string strUri;
    std::string strCnonce;
    std::string strNonce;
    std::string strResponse;
    std::string strQop = "auth";
    std::string strNc = "00000001";

} HTTP_DIGEST_AUTH_T;
class HttpDeal
{
private:
    Buffer* pWriteBuff_;
    Buffer* pReadBuff_;
    HTTP_HEADER_T struHttpHead_;
    HTTP_DIGEST_AUTH_T struHttpDigest_;
    /* data */
public:
    static std::unordered_map<std::string,HTTP_REQUEST_T> httpUrlMap;
    static std::unordered_map<int,std::string> mapReturnCode;
    static std::unordered_map<HTTP_ENUM_E,std::string> mapHttpMethod;
    std::string genDigestAuthenticate(const std::string& realm,std::string& nonce);
    std::string genHeadLine(const std::string& strKey,const std::string& strValue);
    std::string genHeadFirstLine(int iCode);
    int httpPraseAuthorization(std::string& strAuth,HTTP_DIGEST_AUTH_T& struDigest);
    int getPage();
    int getHomePage();
    int PraseHttpHeader();
    int doDigestAuth();
    HttpDeal(/* args */);
    ~HttpDeal();
    void init(Buffer* pReadBuff,Buffer* pWriteBuff);
    bool IsKeepAlive() const;
    void initHeaderStruct();
    int processHttp();
    int processCommonPage();
};


#endif
