/*
 * @Author       : mark
 * @Date         : 2020-06-20
 * @copyleft GPL 2.0
 */ 
#include "../src/log/log.h"

void TestLog() {
    int cnt = 0, level = 0;
    Log::GetInstance()->init(level, "./test/testlog1", ".log", 5000, 0);
    for(level = 3; level >= 0; level--) {
        Log::GetInstance()->setLevel(level);
        for(int j = 0; j < 1000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_TEST(i,"% s============%d 2222222222 ", "Test", cnt++);
            }
        }
    }
    cnt = 0;
    Log::GetInstance()->init(level, "./test/testlog2", ".log", 5000, 200);
    for(level = 0; level < 4; level++) {
        Log::GetInstance()->setLevel(level);
        for(int j = 0; j < 1000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_TEST(i,"%s 111111111 %d ============= ", "Test", cnt++);
            }
        }
    }

}

int main() {
    TestLog();


}