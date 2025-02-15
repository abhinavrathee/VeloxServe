#include "middleware/logger.h"

#include <iostream>
#include <iomanip>
#include <ctime>
#include <chrono>

Logger::Logger(const std::string& access_path, const std::string& error_path) {
    access_log_.open(access_path, std::ios::app);
    if (!access_log_.is_open()) {
        std::cerr << "[Logger] Warning: Could not open access log: " << access_path << std::endl;
    }

    error_log_.open(error_path, std::ios::app);
    if (!error_log_.is_open()) {
        std::cerr << "[Logger] Warning: Could not open error log: " << error_path << std::endl;
    }
}

Logger::~Logger() {
    if (access_log_.is_open()) access_log_.close();
    if (error_log_.is_open()) error_log_.close();
}

std::string Logger::get_current_time() const {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    // Apache format: [10/Oct/2000:13:55:36 -0700]
    ss << std::put_time(std::localtime(&now_c), "%d/%b/%Y:%H:%M:%S %z");
    return ss.str();
}

std::string Logger::level_to_string(Level level) const {
    switch (level) {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO:  return "INFO";
        case Level::WARN:  return "WARN";
        case Level::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

void Logger::log_access(const HttpRequest& req, const HttpResponse& resp,
                        const std::string& client_ip, int duration_ms) {
    // Format: %h %l %u %t "%r" %>s %b "%{Referer}i" "%{User-Agent}i"
    std::string time_str = get_current_time();
    std::string method = req.method.empty() ? "-" : req.method;
    std::string path = req.path.empty() ? "-" : req.path;
    std::string version = req.version.empty() ? "-" : req.version;
    std::string user_agent = req.get_header("user-agent", "-");
    std::string referer = req.get_header("referer", "-");
    
    std::stringstream log_line;
    log_line << client_ip << " - - [" << time_str << "] "
             << "\"" << method << " " << path << " " << version << "\" "
             << resp.status_code() << " " << resp.body().size() << " "
             << "\"" << referer << "\" \"" << user_agent << "\" "
             << duration_ms << "ms\n";

    std::string log_str = log_line.str();

    {
        std::lock_guard<std::mutex> lock(access_mutex_);
        if (access_log_.is_open()) {
            access_log_ << log_str;
            access_log_.flush();
        }
    }

    if (console_out_) {
        std::cout << "\033[90m" << log_str << "\033[0m"; // Grey out access logs on console
    }
}

void Logger::log_error(Level level, const std::string& message) {
    // Format: [Wed Oct 11 14:32:52 2000] [error] [client 127.0.0.1] message
    std::string time_str = get_current_time();
    std::string lvl_str = level_to_string(level);
    
    std::stringstream log_line;
    log_line << "[" << time_str << "] [" << lvl_str << "] " << message << "\n";
    std::string log_str = log_line.str();

    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        if (error_log_.is_open()) {
            error_log_ << log_str;
            error_log_.flush();
        }
    }

    if (console_out_) {
        if (level == Level::ERROR) std::cerr << "\033[31m" << log_str << "\033[0m"; // Red
        else if (level == Level::WARN) std::cerr << "\033[33m" << log_str << "\033[0m"; // Yellow
        else std::cout << log_str;
    }
}
