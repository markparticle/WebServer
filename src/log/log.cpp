/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft GPL 2.0
 */ 
#include "log.h"

using namespace std;

Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writePID_ = nullptr;
    deque_ = nullptr;
}

Log::~Log() {
    delete[] buffer_;
    if(writePID_ != nullptr) {
        delete writePID_;
    }
    if(deque_ != nullptr) {
        delete deque_;
    }
    if(fp_ != nullptr) {
        fclose(fp_);
    }
}

void Log::init(const char* path, const char* suffix,
    int buffSize, int maxLines,
    int maxQueueSize) {
    if(maxQueueSize > 0) {
        isAsync_ = true;
        deque_ = new BlockDeque<string>(maxQueueSize);
        writePID_ = new thread(FlushLogThread);
        writePID_->detach();
    }
    BUFF_SIZE = buffSize;
    buffer_ = new char[BUFF_SIZE];
    memset(buffer_, '\0', BUFF_SIZE);
    MAX_LINE = maxLines;

    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;

    strcpy(path_, path);
    strcpy(suffix_, suffix);

    char fileName[LOG_NAME_LEN] = {0};

    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    toDay_ = t.tm_mday;

    fp_ = fopen(fileName, "a");
    if(fp_ == nullptr) {
        mkdir(path_, 0777);
        fp_ = fopen(fileName, "a");
    } 
    assert(fp_ != nullptr);
}

void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;

    char str[16] = {0};
    switch(level) {
    case 0:
        strcpy(str, "[debug]:");
        break;
    case 1:
        strcpy(str, "[info]:");
        break;
    case 2:
        strcpy(str, "[warn]:");
        break;
    case 3:
        strcpy(str, "[error]:");
        break;
    default:
        strcpy(str, "[info]:");
        break;
    }
    {
        lock_guard<mutex> locker(mtx_);
        lineCount_++;
        //如果不是今天日志或者满了
        if(toDay_ != t.tm_mday || lineCount_ % MAX_LINE == 0) {
            char newFile[LOG_NAME_LEN];
            fflush(fp_);
            fclose(fp_);
            char tail[16] = {0};
            snprintf(tail, 16, "%d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

            if(toDay_ != t.tm_mday) {
                snprintf(newFile, LOG_NAME_LEN - 1, "%s%s%s", path_, tail, suffix_);
                toDay_ = t.tm_mday;
                lineCount_ = 0;
            } else {
                snprintf(newFile, LOG_NAME_LEN - 1, "%s%s-%d%s", 
                    path_, tail, lineCount_ / MAX_LINE, suffix_);
            }
            fp_ = fopen(newFile, "a");
            assert(fp_ != nullptr);
        }
    }
    va_list vaList;
    va_start(vaList, format);

    string context;
    {
        lock_guard<mutex> locker(mtx_);
        int n = snprintf(buffer_, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec, str);
        int m = vsnprintf(buffer_ + n, BUFF_SIZE - 1, format, vaList);
        buffer_[n + m] = '\n';
        buffer_[n + m + 1] = '\0';
        context = buffer_;
    }

    if(isAsync_) {
         deque_->push_back(context);
    } 
    else {
        lock_guard<mutex> locker(mtx_);
        fputs(context.c_str(), fp_);
    }
    va_end(vaList);
}

void Log::flush(void) {
    {
        lock_guard<mutex> locker(mtx_);
        fflush(fp_);
    }
}

void Log::AsyncWrite_() {
    std::string str;
    while(deque_->pop(str)) {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

Log* Log::GetInstance() {
    static Log inst;
    return &inst;
}

void Log::FlushLogThread() {
    Log::GetInstance()->AsyncWrite_();
}