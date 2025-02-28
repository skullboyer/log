/**
 * @file adapter.h
 * @author skull (skull.gu@gmail.com)
 * @brief
 * @version 0.1
 * @date 2022-08-03
 *
 * @copyright Copyright (c) 2023 skull
 *
 */
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

// V：裸数据，不带标签
// V: view, VO: only view, NO: no output
enum {LOG_LEVEL, V, D, I, W, E, NO, VO, DO, IO, WO, EO};

#define FILTER    V  // log filtering level (exclude oneself)
#define LOGV(...)    LOG(V, __VA_ARGS__)
#define LOGD(...)    LOG(D, __VA_ARGS__)
#define LOGI(...)    LOG(I, __VA_ARGS__)
#define LOGW(...)    LOG(W, __VA_ARGS__)
#define LOGE(...)    LOG(E, __VA_ARGS__)

#define OUTPUT    log_out
#define TAG    "NoTag"  // default tag
#define LOG_BUFFER_SIZE    (256)
#define LOG_HZ    (0)  // 0: not care, >1: max number of ouput per second

// Level - Date Time - {Tag} - <func: line> - message
// E>09/16 11:17:33.990 {TEST-sku} <test: 373> This is test.
#define LOG(level, ...) \
    do { \
        if (level + NO == FILTER) { \
        } else if (level < FILTER) { \
            break; \
        } \
        char tag[] = TAG; \
        if (log_control(tag)) break; \
        bool jump = false; \
        level == V ? : LOG_HZ == 0 ? : (jump = log_throttling(__FILENAME__, __LINE__, LOG_HZ)); \
        if (jump) break; \
        char buffer[LOG_BUFFER_SIZE]; \
        sprintf(buffer, __VA_ARGS__); \
        level == V ? : OUTPUT(#level">%s " "{%.8s} " "%u:%s() ", get_current_time(NULL), tag, __LINE__, __func__); \
        OUTPUT(buffer); \
        level == V ? : OUTPUT("\r\n"); \
    } while(0)

#define UNIX_DIRECTORY_SEPARATOR    '/'
#define WINDOWS_DIRECTORY_SEPARATOR    '\\'
#define DIRECTORY_SEPARATOR    UNIX_DIRECTORY_SEPARATOR
#define __FILENAME__    (strrchr(__FILE__, DIRECTORY_SEPARATOR) ? (strrchr(__FILE__, DIRECTORY_SEPARATOR) + 1) : __FILE__)
#define UNUSED(x)    (void)(x)
#define ASSERT(expr)    (void)((!!(expr)) || (OUTPUT("%s assert fail: \"%s\" @ %s, %u\r\n", TAG, #expr, __FILE__, __LINE__), assert_abort()))
#define CHECK_MSG(expr, msg, ...) \
    do { \
        if (!(expr)) { \
            LOGE("check fail: \"%s\". %s @ %s, %u", #expr, msg, __FILE__, __LINE__); \
            return __VA_ARGS__; \
        } \
    } while(0)
#define CHECK_MSG_PRE(expr, ...)    CHECK_MSG(expr, "Error occurred", __VA_ARGS__)
#define VA_NUM_ARGS_IMPL(_1,_2,__N,...)    __N
#define GET_MACRO(...)    VA_NUM_ARGS_IMPL(__VA_ARGS__, CHECK_MSG, CHECK_MSG_PRE)
#define CHECK(expr, ...)    GET_MACRO(__VA_ARGS__)(expr, __VA_ARGS__)

uint32_t BKDRHash(char *str);
int log_init(void);
int log_deinit(void);
int log_out(const char *format, ...);
char *get_current_time(uint32_t *today_ms);
bool log_throttling(char *file, uint16_t line, uint8_t log_hz);
bool log_control(char *tag);
