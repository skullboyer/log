/**
 * @file log.c
 * @author skull (skull.gu@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-01-27
 *
 * @copyright Copyright (c) 2024 skull
 *
 */

#include <time.h>
#include <sys/time.h>
#include "adapter.h"

#undef TAG
#define TAG    "LOG"
#define CFG_LOG_BACKEND_TERMINAL    0
#define CFG_LOG_BACKEND_FILE        1
#define CFG_LOG_BACKEND_FLASH       0
#define CFG_LOG_COMPRESS            0
#define CFG_THROTTLING_MODE         THROTTLING_MODE_COUNT
#define THROTTLING_MODE_COUNT    1  // To limit viewership of log output
#define THROTTLING_MODE_TIME     2  // To limit viewership of log time interval

#define LOG_THROTTLING_RECORDER_SIZE    (10)  // 大小取决于每秒输出不同位置的日志输出次数

uint32_t BKDRHash(char *str)
{
    uint32_t seed = 131;  // 31 131 1313 13131 131313 etc..
    uint32_t hash = 0;

    CHECK(str != NULL, 0);
    while (*str) {
        hash = hash * seed + (*str++);
    }

    return (hash & 0x7FFFFFFF);
}

/**
 * @brief  log timestamp
 * @param  today_ms  读取毫秒级的当前时间
 * @return  日志时间
 */
char *get_current_time(uint32_t *today_ms)
{
    static char currentTime[20] = {0};
    // Used to obtain the complete timestamp including the date
    static bool time_all = true;

    static uint8_t minute_history = 0;
    // 更新分钟级时间比对历史
    if (strlen(currentTime) > 10) {
        minute_history = (currentTime[9] - '0') * 10 + (currentTime[10] - '0');
    }

    struct timeval now;
    gettimeofday(&now, NULL);
    struct tm* date = localtime((time_t *)&now);
    memset(currentTime, 0, sizeof(currentTime));
    strftime(currentTime, sizeof(currentTime), "%M", date);
    // 时间分钟级
    uint8_t minute = (currentTime[0] - '0') * 10 + (currentTime[1] - '0');

    // 分钟变化时，显示完整时间
    if (minute != minute_history) {
        time_all = true;
    }

    // 入参空，且在分钟级时间发生变化时返回完整时间，否则返回s.ms
    // 入参非空返回完整时间，且入参拿走毫秒级时间
    if (time_all || today_ms != NULL) {
        // 保证首条日志带完整时间
        if (today_ms == NULL) {
            time_all = false;
        }
        strftime(currentTime, sizeof(currentTime), "%m/%d %H:%M:%S", date);
        snprintf(currentTime + 14, 5, ".%03ld", now.tv_usec / 1000);
    } else {
        strftime(currentTime, sizeof(currentTime), "%S", date);
        snprintf(currentTime + 2, 5, ".%03ld", now.tv_usec / 1000);
    }

    if (today_ms != NULL) {
        // Get the number of milliseconds for the day
        *today_ms = now.tv_sec%86400*1000 + now.tv_usec/1000;
    }

    return currentTime;
}

/**
 * @brief log throttling
 */
bool log_throttling(char *file, uint16_t line, uint8_t log_hz)
{
    typedef struct {
        uint32_t hash;
        uint32_t today_ms;
        char file[48];
        uint16_t line;
        uint16_t count;
    } log_throttling_info;

    static struct {
        uint8_t write;
        uint8_t read;  // reserved
        bool empty;
    } log_throttling_handle = {0, 0, true};

    static log_throttling_info log_data[LOG_THROTTLING_RECORDER_SIZE] = {0};

    char buf[64] = {0};
    CHECK(file != NULL, "log throttling arg is null", false);
    snprintf(buf, sizeof(buf) - 1, "%s:%u", file, line);
    uint32_t hash = BKDRHash(buf);

    uint32_t today_ms;
    char time_stamp[20];
    memcpy(time_stamp, get_current_time(&today_ms), sizeof(time_stamp));

    // 首次进入
    if (log_throttling_handle.empty) {
        log_data[0].hash = hash;
        log_data[0].today_ms = today_ms;
        log_data[0].line = line;
        memcpy(log_data[0].file, file, sizeof(log_data[0].file));
        log_throttling_handle.empty = false;
        log_throttling_handle.write = 1;
        return false;
    }

    // Clearing old data in logger with limited viewership of logs
    for (uint8_t i = 0; i < LOG_THROTTLING_RECORDER_SIZE; i++) {
        // 因为要处理的就是高频日志，所以从最近的数据开始遍历命中率高，可提升效率
        uint8_t index = i + log_throttling_handle.write;
        if (index >= LOG_THROTTLING_RECORDER_SIZE) {
            index -= LOG_THROTTLING_RECORDER_SIZE;
        }

        // 跳过空数据，且仅处理同一位置的日志
        // 仅处理 hash 相同的日志可以节省时间，但副作用是部分受控日志不会打印出，会被覆盖
        // 缓解此副作用：1、放开hash限制；2、增大缓存log_data大小
        if (log_data[index].today_ms == 0 || log_data[i].hash != hash) {
            continue;
        }

        uint32_t time_range = today_ms - log_data[index].today_ms;

#if CFG_THROTTLING_MODE == THROTTLING_MODE_TIME
        // 同一位置日志输出自由后，打印受控的数量信息，借助 time_range 与本条日志时间做差可算出受控时间
        if (time_range >= 1000/log_hz) {
            if (log_data[index].count > 0) {
                OUTPUT("W>%s " "{%.8s} " "<%s: %u> (-%ums) ""discard times: %u\r\n", time_stamp, TAG,
                    log_data[index].file, log_data[index].line, time_range, log_data[index].count);
            }
            memset(&log_data[index], 0, sizeof(log_throttling_info));
        }

#elif CFG_THROTTLING_MODE == THROTTLING_MODE_COUNT
        if (time_range > 1000) {
            int32_t count = log_data[index].count - log_hz;
            if (count > 0) {
                OUTPUT("W>%s " "{%.8s} " "<%s: %u> (-%ums) ""discard times: %u\r\n", time_stamp, TAG,
                    log_data[index].file, log_data[index].line, time_range, count);
            }
            memset(&log_data[index], 0, sizeof(log_throttling_info));
        }
#endif
    }

    // Limited viewership of high frequency logs
    for (uint8_t i = 0; i < LOG_THROTTLING_RECORDER_SIZE; i++) {
        if (hash == log_data[i].hash) {
#if CFG_THROTTLING_MODE == THROTTLING_MODE_TIME
            // 同一位置日志输出间隔小于设定值，则丢弃
            if (today_ms - log_data[i].today_ms < 1000/log_hz) {
                log_data[i].count += 1;
                log_throttling_handle.read = i;
                return true;
            }
#elif CFG_THROTTLING_MODE == THROTTLING_MODE_COUNT
            // 同一位置日志1秒内输出数量大于设定值，则丢弃
            if (today_ms - log_data[i].today_ms < 1000) {
                log_data[i].count += 1;
                if (log_data[i].count > log_hz) {
                    return true;
                }
            }
#endif
            return false;
        }
    }

    uint8_t index = log_throttling_handle.write;
    if (index >= LOG_THROTTLING_RECORDER_SIZE) {
        index = 0;
    }
    log_data[index].hash = hash;
    log_data[index].today_ms = today_ms;
    log_data[index].line = line;
    memcpy(log_data[index].file, file, sizeof(log_data[index].file));
    log_throttling_handle.write = index + 1;

    return false;
}

/**
 * @brief  control log output with tags
 *
 * @note
 *  log模块 加个头标识进行输出控制
 */
bool log_control(char *tag)
{
    static bool ret = false;
    CHECK(tag != NULL, "log control arg is null", false);

    switch (tag[0]) {
        // 排除此标签
        case '!':
            return true;
        // 仅允许此标签
        case '#':
            ret = true;  // 此后其他标签均受控，除特权标签'*'
            strcpy(tag, tag+1);
            return false;
        // 此标签开始不再受控，解除标签'#'控制
        case '~':
            ret = false;
            strcpy(tag, tag+1);
            break;
        // 此标签不受控，包括等级
        case '*':
            strcpy(tag, tag+1);
            return false;
        default: break;
    }

    return ret;
}

/**
 * @brief  privilege log output with tags
 *
 * @note
 *  '*'tag不受等级控制：
 *    方便调试 比如要查看指定Tag的所有日志 尤其是debug级别
 *    一般在开发、调试时加较多日志，稳定后就会去除，直接删除则不方便在出问题后继续分析，所以一般控制log输出级别为info，
 *    调试日志级别为debug，就是为了随时激活
 */
bool log_privilege(char *tag)
{
    static bool ret = false;
    CHECK(tag != NULL, "log privilege arg is null", false);

    switch (tag[0]) {
        // 此标签不受控，包括等级
        case '*':
            return true;
        default: break;
    }

    return ret;
}

#if CFG_LOG_BACKEND_FILE
FILE *logFile = NULL;

static int init_log_file(void)
{
    logFile = fopen("Log.log", "w+");
    if (logFile == NULL) {
        printf("Failed to open log file.\n");
        return -1;
    }
    return 0;
}

static int output_file(FILE *file, char *buf, uint16_t len)
{
    if (file != NULL) {
        fwrite(buf, len, 1, file);
        fflush(file);
    }

    return 0;
}
#endif

#if CFG_LOG_BACKEND_TERMINAL
static int output_terminal(char *buf)
{
    printf("%s", buf);
    return 0;
}
#endif

#if CFG_LOG_BACKEND_FLASH
struct {
    uint32_t address;
    uint32_t length;
    uint32_t write;
    uint32_t read;
    bool ferrule;
} log_handle;

static int init_log_flash(void)
{
    log_handle.address = FLASH_ADDRESS;
    log_handle.length = FLASH_RANGE;
    log_handle.write = FLASH_ADDRESS;
    log_handle.read = FLASH_ADDRESS;
    log_handle.ferrule = false;
    return 0;
}

static int output_flash(char *buf, uint16_t len)
{
    if (log_handle.write + len > FLASH_ADDRESS + FLASH_RANGE) {
        flash_write(log_handle.write, buf, FLASH_ADDRESS + FLASH_RANGE - log_handle.write);
        uint16_t remain = log_handle.write + len - FLASH_ADDRESS - FLASH_RANGE;
        log_handle.write = FLASH_ADDRESS;
        flash_write(log_handle.write, buf, remain);
        log_handle.write += remain;
        log_handle.ferrule = true;
    } else {
        flash_write(log_handle.write, buf, len);
        log_handle.write += len;
    }

    return 0;
}

int sync_flash(char *buf)
{
    uint16_t len = 0;

    // 发生套圈
    if (log_handle.ferrule) {
        // 数据存在覆盖，以当前写地址开始回环读完
        if (log_handle.write > log_handle.read) {
            log_handle.read = log_handle.write;
            len = FLASH_RANGE;
        } else {
            // 未覆盖，以当前读地址开始回环读到写地址结束
            len = FLASH_ADDRESS + FLASH_RANGE - log_handle.read;
            len += log_handle.write - FLASH_ADDRESS;
        }
    } else {
        // 没有套圈，以当前读地址开始读到写地址结束
        if (log_handle.write > log_handle.read) {
            len = log_handle.write - log_handle.read;
        }
        // 读地址等于写地址，说明没有新数据
    }

    if (log_handle.read + len > FLASH_ADDRESS + FLASH_RANGE) {
        flash_read(log_handle.read, buf, FLASH_ADDRESS + FLASH_RANGE - log_handle.read);
        uint16_t remain = log_handle.read + len - FLASH_ADDRESS - FLASH_RANGE;
        log_handle.read = FLASH_ADDRESS;
        flash_read(log_handle.read, buf, remain);
        log_handle.read += remain;
        log_handle.ferrule = false;
    } else {
        flash_read(log_handle.read, buf, len);
        log_handle.read += len;
    }
}
#endif

int log_init(void)
{
    int ret = 0;
#if CFG_LOG_BACKEND_FILE
    ret =  init_log_file();
#endif
    return ret;
}

int log_deinit(void)
{
    int ret = 0;
#if CFG_LOG_BACKEND_FILE
    ret = fclose(logFile);
#endif
    return ret;
}

int log_out(const char *format, ...)
{
    int ret = 0;
    va_list args;
    va_start(args, format);
    char log_buffer[LOG_BUFFER_SIZE];
    vsnprintf(log_buffer, sizeof(log_buffer), format, args);
    uint16_t len = strlen(log_buffer);

#if CFG_LOG_BACKEND_TERMINAL
    ret = output_terminal(log_buffer);
#endif
#if CFG_LOG_COMPRESS

#endif
#if CFG_LOG_BACKEND_FILE
    ret = output_file(logFile, log_buffer, len);
#endif
#if CFG_LOG_BACKEND_FLASH
    ret = output_flash(log_buffer, len);
#endif

    va_end(args);
    return ret;
}
