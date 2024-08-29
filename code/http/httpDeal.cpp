#include "HttpDeal.h"
static const std::string HTTP_VERSON_HEAD = "HTTP/1.1 ";
static const std::string HTTP_SPLIE = "\r\n";
static const std::string HTTP_HEAD_END = "\r\n\r\n";
static const std::string CONTENT_LENGTH = "Content-Length";
static const std::string Authenticate = "WWW-Authenticate";
std::unordered_map<int, std::string> HttpDeal::mapReturnCode = {
    {200, "200 OK"},
    {401, "401 Unauthorized"},
    {404, "404 Not Found"},
    {405, "405 Not Allowed"},
};

std::unordered_map<std::string, HTTP_REQUEST_T> HttpDeal::httpUrlMap;
std::vector<std::string> vecHttpMethod = {
    "GET", "POST", "DELETE", "PUT"};
std::unordered_map<HTTP_ENUM_E, std::string> HttpDeal::mapHttpMethod = {
    {GET, "GET"},
    {POST, "POST"},
    {DELETE, "DELETE"},
    {PUT, "PUT"},
};

std::string HttpDeal::genHeadLine(const std::string &strKey, const std::string &strValue)
{
    std::string strRe = strKey + ": " + strValue + HTTP_SPLIE;
    return strRe;
}

std::string HttpDeal::genDigestAuthenticate(const std::string &realm, std::string &nonce)
{
    std::string strDigest = "Digest realm=\"" + realm + "\","
                                                        "qop=\"auth\","
                                                        "nonce=\"" +
                            nonce + "\"";
    std::string strRe = genHeadLine(Authenticate, strDigest);
    return strRe;
}

HttpDeal::HttpDeal()
{
    httpUrlMap["/home"] = {GET, "/home", std::bind(&HttpDeal::getPage, this)};
    httpUrlMap["/"] = {GET, "/", std::bind(&HttpDeal::getHomePage, this)};
}

HttpDeal::~HttpDeal()
{
}
std::string HttpDeal::genHeadFirstLine(int iCode)
{
    std::string strRe = HTTP_VERSON_HEAD + mapReturnCode[iCode] + HTTP_SPLIE;
    return strRe;
}

int HttpDeal::getHomePage(){
    std::string strHome = "hello home";
    pWriteBuff_->Append(genHeadFirstLine(200));
    pWriteBuff_->Append(genHeadLine(CONTENT_LENGTH, std::to_string(strHome.length())));
    pWriteBuff_->Append(HTTP_SPLIE);
    pWriteBuff_->Append(strHome);
}

int HttpDeal::getPage()
{
    DEBUG_D(pReadBuff_->Peek());
    int iRet = -1;
    do
    {
        if (0 == doDigestAuth())
        {
            pWriteBuff_->Append(genHeadFirstLine(200));
            pWriteBuff_->Append(genHeadLine(CONTENT_LENGTH, "0"));
            pWriteBuff_->Append(HTTP_SPLIE);
        }
        else
        {
            pWriteBuff_->Append(genHeadFirstLine(401));
            pWriteBuff_->Append(genHeadLine(CONTENT_LENGTH, "0"));
            pWriteBuff_->Append(HTTP_SPLIE);
        }

    } while (0);
}
int HttpDeal::doDigestAuth()
{
    int iRet = -1;
    do
    {
        auto pairAuth = struHttpHead_.mapHeader.find("Authorization");
        if (pairAuth == struHttpHead_.mapHeader.end())
        {
            DEBUG_D("WWW-Authenticate" << "no");
            time_t timep;
            time(&timep); // 获取从1970至今过了多少秒，存入time_t类型的timep
            // printf("%s", ctime(&timep));//用ctime将秒数转化成字符串格式，输出：Thu Feb 28 14:14:17 2019
            std::string strTime = ctime(&timep);
            strTime += "abc";
            MD5 MD5Test1;
            struHttpDigest_.strNonce = MD5Test1.encode(strTime);
            struHttpDigest_.strRealm = "test";
            pWriteBuff_->Append(genHeadFirstLine(401));
            pWriteBuff_->Append(genDigestAuthenticate(struHttpDigest_.strRealm, struHttpDigest_.strNonce));
            pWriteBuff_->Append(genHeadLine(CONTENT_LENGTH, "0"));
            pWriteBuff_->Append(HTTP_SPLIE);
            iRet = -1;
            break;
        }
        else
        {
            HTTP_DIGEST_AUTH_T struDigest;
            // std::string strUser = "john";
            // std::string strPass = "hello";
            // std::string strHttpAction = "GET";
            std::string strDigest = struHttpHead_.mapHeader["Authorization"];
            httpPraseAuthorization(strDigest, struDigest);
            MYSQL *sql;
            SqlConnRAII(&sql, SqlConnPool::Instance());
            USER_INFO_T struUserInfo;
            sqlQueryUserInfo(sql, struDigest.strUser, struUserInfo);
            DEBUG_D(struUserInfo.userName);
            DEBUG_D(struUserInfo.passWord);
            DEBUG_D("reNonce:" << struDigest.strNonce);
            DEBUG_D("Nonce:" << struHttpDigest_.strNonce);
            if (0 == struDigest.strNonce.compare(struHttpDigest_.strNonce) && 0 < struHttpDigest_.strNonce.size())
            {
                // MD5加密HA1
                MD5 MD5Test1;
                std::string strA1 = struUserInfo.userName + ":" + struHttpDigest_.strRealm + ":" + struUserInfo.passWord;
                std::string strHA1 = MD5Test1.encode(strA1);
                // MD5加密HA2
                MD5 MD5Test2;
                std::string strA2 = mapHttpMethod[struHttpHead_.enumMethod] + ":" + struDigest.strUri;
                std::string strHA2 = MD5Test2.encode(strA2);
                // MD5生成随机数
                time_t timep;
                time(&timep);
                MD5 MD5Test4;
                std::string strRandom = MD5Test4.encode(std::to_string(timep));
                // MD5加密response
                MD5 MD5Test3;
                std::string strPreResponse = strHA1 + ":" + struHttpDigest_.strNonce + ":" + struDigest.strNc + ":" + struDigest.strCnonce + ":" + struDigest.strQop + ":" + strHA2;
                std::string strResponse = MD5Test3.encode(strPreResponse);
                DEBUG_D("httpRes:" << strResponse);
                DEBUG_D("prehttpRes:" << struDigest.strResponse);
                if (0 == strResponse.compare(struDigest.strResponse))
                {
                    iRet = 0;
                    break;
                }
                else
                {
                    iRet = -2;
                    break;
                }
                // pWriteBuff_->Append(genHeadFirstLine(200));
                // pWriteBuff_->Append(genHeadLine(CONTENT_LENGTH, "0"));
                // pWriteBuff_->Append(HTTP_SPLIE);
            }
        }

    } while (0);
    return iRet;
}

bool HttpDeal::IsKeepAlive() const
{
    return struHttpHead_.isKeepAlive;
}
int HttpDeal::processHttp()
{
    if (0 == PraseHttpHeader())
    {
        pReadBuff_->Retrieve(struHttpHead_.iHeaderLen);
        auto findUrl = httpUrlMap.find(struHttpHead_.strPath);
        if (findUrl != httpUrlMap.end())
        {
            if (findUrl->second.enumMethod == struHttpHead_.enumMethod)
            {
                findUrl->second.funCallBack();
            }else{
                pWriteBuff_->Append(genHeadFirstLine(405));
                pWriteBuff_->Append(genHeadLine(CONTENT_LENGTH, "0"));
                pWriteBuff_->Append(HTTP_SPLIE);
            }
        }
        else
        {
            if (GET == struHttpHead_.enumMethod){
                processCommonPage();
            }else{
                pWriteBuff_->Append(genHeadFirstLine(405));
                pWriteBuff_->Append(genHeadLine(CONTENT_LENGTH, "0"));
                pWriteBuff_->Append(HTTP_SPLIE);
            }
            
        }

        pReadBuff_->RetrieveAll();
    }
}

int HttpDeal::processCommonPage()
{
    std::string strSrc = "./www";
    strSrc += struHttpHead_.strPath;
    DEBUG_D(strSrc);
    int iFileRet = access(strSrc.c_str(), F_OK);
    if (-1 == iFileRet)
    {
        pWriteBuff_->Append(genHeadFirstLine(404));
        pWriteBuff_->Append(genHeadLine(CONTENT_LENGTH, "0"));
        pWriteBuff_->Append(HTTP_SPLIE);
    }
    else
    {
        pWriteBuff_->Append(genHeadFirstLine(200));
        ssize_t iFileSize = getFileSize(strSrc.c_str());
        DEBUG_D(iFileSize);
        pWriteBuff_->Append(genHeadLine(CONTENT_LENGTH, std::to_string(iFileSize)));
        pWriteBuff_->Append(HTTP_SPLIE);
        int fp = open(strSrc.c_str(), O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR);
        char buf[1024];
        ssize_t llen = 0;
        do
        {
            memset(buf, 0, 1024);
            llen = read(fp, buf, 1024);
            DEBUG_D(llen);
            pWriteBuff_->Append(buf, llen);
        } while (llen > 0);
    }
}

int HttpDeal::PraseHttpHeader()
{
    int iRet = -1;
    do
    {

        initHeaderStruct();
        std::string strMsg(pReadBuff_->Peek());
        std::regex pattern("(\\w+)\\s+([^\\s]+)\\s+HTTP/([^\\s]+)");
        std::smatch matches; // 用于存储匹配结果
        if (std::regex_search(strMsg, matches, pattern))
        {
            if (matches.size() > 2)
            {
                std::string strMethod = matches[1]; // 请求方法
                std::string strUrl = matches[2];    // 请求的URL
                std::string strVersion = matches[3];

                struHttpHead_.strVersion = strVersion;
                struHttpHead_.strPath = strUrl;
                DEBUG_D("ver:" << struHttpHead_.strVersion);
                DEBUG_I("path" << struHttpHead_.strPath);
                for (auto &item : mapHttpMethod)
                {

                    int index = strMethod.find(item.second);
                    if (index != std::string::npos)
                    {
                        struHttpHead_.enumMethod = item.first;
                        break;
                    }
                }
            }
        }
        else
        {
            iRet = -1;
            break;
        }
        std::regex pattern2("([^\\s:]+):\\s*(.*)\r\n");
        std::smatch matches2; // 用于存储匹配结果
        // 使用sregex_iterator来找到所有匹配的子串
        std::sregex_iterator begin = std::sregex_iterator(strMsg.begin(), strMsg.end(), pattern2);
        std::sregex_iterator end = std::sregex_iterator();
        // 存储所有匹配的结果
        // std::vector<std::string> matches;
        for (std::sregex_iterator it = begin; it != end; ++it)
        {

            std::smatch match = *it;
            std::string key = match[1];   // 第一个子匹配（ID）
            std::string value = match[2]; // 第二个子匹配（Name）
            struHttpHead_.mapHeader[key] = value;
        }
        for (auto &map : struHttpHead_.mapHeader)
        {
            DEBUG_I(map.first << " " << map.second);
        }
        if (struHttpHead_.mapHeader.find("Content-Length") != struHttpHead_.mapHeader.end())
        {
            struHttpHead_.iContentLen = atol(struHttpHead_.mapHeader["Content-Length"].c_str());
            DEBUG_I(struHttpHead_.iContentLen);
        }
        if (struHttpHead_.mapHeader.find("Connection") != struHttpHead_.mapHeader.end())
        {
            if (struHttpHead_.mapHeader["Connection"].compare("keep-alive") == 0)
            {
                struHttpHead_.isKeepAlive = true;
            }
            else
            {
                struHttpHead_.isKeepAlive = false;
            }
            DEBUG_I("keep-alive" << struHttpHead_.isKeepAlive);
        }
        int iIndex = strMsg.find("\r\n\r\n");
        if (std::string::npos != iIndex)
        {
            iIndex += 4;
            struHttpHead_.iHeaderLen = iIndex;
        }
        else
        {
            break;
        }

        iRet = 0;
    } while (0);
    return iRet;
}

void HttpDeal::init(Buffer *pReadBuff, Buffer *pWriteBuff)
{
    pReadBuff_ = pReadBuff;
    pWriteBuff_ = pWriteBuff;
}

void HttpDeal::initHeaderStruct()
{
    struHttpHead_.enumMethod == NONE;
    struHttpHead_.strPath = "";
    struHttpHead_.strVersion = "1.1";
    struHttpHead_.isKeepAlive = false;
    struHttpHead_.iHeaderLen = 0;
    struHttpHead_.iContentLen = 0;
    struHttpHead_.mapHeader.clear();
}

int HttpDeal::httpPraseAuthorization(std::string &strAuth, HTTP_DIGEST_AUTH_T &struDigest)
{
    int iRet = 0;
    do
    {

        findStringKey(strAuth, "username=\"", "\"", struDigest.strUser);
        findStringKey(strAuth, "realm=\"", "\"", struDigest.strRealm);
        findStringKey(strAuth, "nonce=\"", "\"", struDigest.strNonce);
        findStringKey(strAuth, "uri=\"", "\"", struDigest.strUri);
        findStringKey(strAuth, "cnonce=\"", "\"", struDigest.strCnonce);
        findStringKey(strAuth, "response=\"", "\"", struDigest.strResponse);
    } while (0);
}
