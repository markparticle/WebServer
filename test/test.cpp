/*
 * @Author       : mark
 * @Date         : 2020-06-20
 * @copyleft GPL 2.0
 */ 
#include "../code/log/log.h"

void TestLog() {
    int cnt = 0, level = 0;
    Log::Instance()->init(level, "./log//testlog1", ".log", 0);
    for(level = 3; level >= 0; level--) {
        Log::Instance()->SetLevel(level);
        for(int j = 0; j < 100000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s============%d 2222222222 ", "Test", cnt++);
            }
        }
    }
    cnt = 0;
    Log::Instance()->init(level, "./log/testlog2", ".log", 5000);
    for(level = 0; level < 4; level++) {
        Log::Instance()->SetLevel(level);
        for(int j = 0; j < 100000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s 111111111 %d ============= ", "Test", cnt++);
            }
        }
    }
}

int main() {
    TestLog();
}