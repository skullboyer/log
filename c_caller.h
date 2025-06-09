/**
 * @file c_caller.h
 * @author skull (skull.gu@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-28
 *
 * @copyright Copyright (c) 2025 skull
 *
 */
#pragma once
#include "adapter.h"

#define CALLER_PRINT

#if defined(__GNUC__) || defined(__clang__)
    #define CALLER_INFO __func__, __FILENAME__, __LINE__
#elif defined(_MSC_VER)
    #define CALLER_INFO __FUNCTION__, __FILENAME__, __LINE__
#endif


/*
  ({ ... }) 语法简介
    GCC 扩展 ({ ... }) 允许你在宏中执行一段语句块，并返回块中最后一条语句的结果。它类似于在函数中使用大括号 {} 来包裹多条语句，
    但区别在于它允许返回一个表达式的值。

    这个语法的常见用途是在宏中进行多条语句的执行，并且允许宏有返回值。这使得宏不仅能够执行副作用（例如打印日志），还能够返回一个值，
    避免了宏只能执行单一表达式的局限。
*/

#ifdef CALLER_PRINT

#define LOG_CALLER(func) \
    LOGD("[CCALL] %s <= %s (%s:%d)", func, CALLER_INFO)

#define CALL_FUNC_ARG(func, ...) \
    ({ \
        LOG_CALLER(#func); \
        func(__VA_ARGS__); \
    })

#define CALL_FUNC(func_exec) \
    ({ \
        LOG_CALLER(#func_exec); \
        func_exec; \
    })

#else

#define LOG_CALLER(func)

#define CALL_FUNC_ARG(func, ...) \
    func(__VA_ARGS__)

#define CALL_FUNC(func_exec) \
    func_exec

#endif
