//
// Created by James Chen on 5/27/16.
//

#include "HelloWorld.h"

#include <android/log.h>
#include <functional>

#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, "hi",__VA_ARGS__)

extern int sles_main();

void HelloWorld::hello() {
    LOGD("HelloWorld in cpp");
    int a = 0;
    auto helloLambda = [=] () {
        LOGD("I'm in lambda ...");
    };

    helloLambda();
}

extern "C" void hellocpp() {
    HelloWorld a;
    a.hello();
}