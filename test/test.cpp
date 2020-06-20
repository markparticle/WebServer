/*
 * @Author       : mark
 * @Date         : 2020-06-20
 * @copyleft GPL 2.0
 */ 
#include "log/log.h"

int main() {
    Log::GetInstance()->init("./log", ".log", 200, 200, 1);
    int cnt = 0;
    for(int j = 0; j < 1000; j++ ){
        for(int i = 0; i < 4; i++) {
            LOG(i,"%s============%d=============", "Test", cnt++);
        }
    }
}