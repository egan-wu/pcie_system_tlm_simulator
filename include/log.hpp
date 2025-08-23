#pragma once
#include <iostream>
#include <string>
#include <systemc>
#include <cstdarg> // 處理可變參數
#include <cstdio>  // 處理格式化字串

// 定義日誌級別 (LogLevel)
// 數字越低，日誌級別越不緊急。
enum LogLevel {
    VERB    = 0, // 灰色 (最詳細)
    TRACE   = 1, // 白色
    DEBUG   = 2, // 藍色
    INFO    = 3, // 綠色 (預設)
    WARN    = 4, // 黃色
    ERROR   = 5  // 紅色 (最緊急)
};

// ===========================================
//  使用者可以修改這個巨集來控制輸出的日誌級別
// ===========================================
// 任何比此級別更緊急的日誌訊息都會被印出。
// 例如，如果設定為 LOG_INFO，則 INFO、WARN 和 ERROR 訊息都會顯示。
#ifndef LOG_LVL
#define LOG_LVL INFO
#endif

// ANSI 顏色碼
const std::string RESET = "\033[0m";
const std::string GRAY = "\033[90m";
const std::string WHITE = "\033[37m";
const std::string BLUE = "\033[34m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string RED = "\033[31m";

// 獲取日誌級別的字串
inline std::string get_log_level_string(LogLevel level) {
    switch (level) {
        case VERB:    return "verb";
        case TRACE:   return "trace";
        case DEBUG:   return "debug";
        case INFO:    return "info";
        case WARN:    return "warn";
        case ERROR:   return "error";
        default:      return "unknown";
    }
}

inline std::string get_align_string(LogLevel level) {
    switch (level) {
        case VERB:    return " ";
        case TRACE:   return "";
        case DEBUG:   return "";
        case INFO:    return " ";
        case WARN:    return " ";
        case ERROR:   return "";
        default:      return "";
    }
}

// 獲取日誌級別的顏色碼
inline std::string get_color_code(LogLevel level) {
    switch (level) {
        case VERB: return GRAY;
        case TRACE:   return WHITE;
        case DEBUG:   return BLUE;
        case INFO:    return GREEN;
        case WARN:    return YELLOW;
        case ERROR:   return RED;
        default:      return RESET;
    }
}

// ==========================================================
// 新增的輔助函式：用於處理可變參數並格式化字串
// 這個函式會安全地處理記憶體，避免緩衝區溢位。
// ==========================================================
inline std::string format_message(const char* format, ...) {
    // 取得可變參數列表
    va_list args;
    va_start(args, format);

    // 第一次呼叫 vsnprintf，找出需要的緩衝區大小
    int size = vsnprintf(nullptr, 0, format, args);

    // 結束可變參數列表
    va_end(args);

    if (size < 0) {
        return ""; // 格式化錯誤
    }

    // 建立一個 std::string 來作為緩衝區
    std::string result(size + 1, '\0');

    // 再次取得可變參數列表，並將格式化後的字串寫入緩衝區
    va_start(args, format);
    vsnprintf(&result[0], size + 1, format, args);
    va_end(args);

    // 移除結尾的空字元
    result.pop_back();

    return result;
}

inline std::string format_timestamp() {
    sc_core::sc_time current_time = sc_core::sc_time_stamp(); // ps
    long long total_ns = static_cast<long long>((current_time.to_double() / 1000));
    
    long long ns = total_ns % 1000;
    long long us = (total_ns / 1000) % 1000;
    long long ms = (total_ns / 1000000) % 1000;
    long long s = total_ns / 1000000000;

    char buffer[32];
    sprintf(buffer, "%03lld:%03lld:%03lld:%03lld", s, ms, us, ns);
    return std::string(buffer);
}

// ==========================================================
// 修改後的日誌巨集：支援 printf 樣式的格式化
// 使用 __VA_ARGS__ 將所有可變參數傳遞給 format_message 函式
// ==========================================================
#define SC_LOG(level, ...) \
    do { \
        if (level >= LOG_LVL) { \
            std::cout << "[" << format_timestamp() << "]" \
                      << get_color_code(level) \
                      << "[" << get_log_level_string(level) << "] " \
                      << RESET \
                      << get_align_string(level) \
                      << "[" << this->name() << "] " \
                      << format_message(__VA_ARGS__) << std::endl; \
        } \
    } while (0)
