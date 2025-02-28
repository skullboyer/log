#include <stdio.h>
#include "c_caller.h"

int calculateSum(int a, int b)
{
    LOGD("Call: %s", __func__);
    return a + b;
}

int calculateSub(int a, int b)
{
    LOGD("Call: %s", __func__);
    return a - b;
}

#undef TAG
#define TAG    "*LOG_TIPS"
static void log_tips(uint8_t type)
{
    switch (type) {
        case 0:
            LOGV("normal out\n");
            break;
        case 1:
            LOGV("only out '#Tag'\n");
            break;
        case 2:
            LOGV("should output, but it was limited\n");
            break;
        case 3:
            LOGV("privilege out '*Tag'\n");
            break;
        case 4:
            LOGV("free out '~Tag'\n");
            break;
        default:
            LOGE("unknown type");
            break;
    }
}

void main()
{
    log_init();
#undef TAG
#define TAG    "MAIN-1"
    log_tips(0);
    int result = CALL_FUNC_ARG(calculateSum, 3, 5);
    LOGI("Result: %d", result);
    LOGI("Result: %d", CALL_FUNC_ARG(calculateSub, 3, 5));
#undef TAG
#define TAG    "#MAIN-2"
    log_tips(1);
    LOGI("Result: %d", CALL_FUNC_ARG(calculateSub, 3 - 6, 5));
#undef TAG
#define TAG    "MAIN-3"
    log_tips(2);
    LOGI("Result: %d", CALL_FUNC_ARG(calculateSum, 3, 5));
#undef TAG
#define TAG    "*MAIN-4"
    log_tips(3);
    LOGD("Result: %d", calculateSub(7, 5));
#undef TAG
#define TAG    "~MAIN-5"
    log_tips(4);
    LOGI("Result: %d", CALL_FUNC(calculateSub(7, 5)));
    log_deinit();
}
