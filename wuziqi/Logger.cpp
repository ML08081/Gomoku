#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>

Logger::Logger() : current_level_(LOG_INFO) {}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_level_ = level;
}

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_file_.is_open()) {
        log_file_.close();
    }
    log_file_.open(filename, std::ios::app);
}

void Logger::debug(const std::string& message) {
    log(LOG_DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LOG_INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LOG_WARN, message);
}

void Logger::error(const std::string& message) {
    log(LOG_ERROR, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < current_level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string log_str = "[" + getTimestamp() + "] [" + getLevelName(level) + "] " + message + "\n";
    
    std::cout << log_str;
    
    if (log_file_.is_open()) {
        log_file_ << log_str;
        log_file_.flush();
    }
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Logger::getLevelName(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "调试";
        case LOG_INFO:  return "信息";
        case LOG_WARN:  return "警告";
        case LOG_ERROR: return "错误";
        default:        return "未知";
    }
}