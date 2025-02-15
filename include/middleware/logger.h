#pragma once

#include "http/http_parser.h"
#include "http/http_response.h"

#include <string>
#include <fstream>
#include <mutex>

// Apache Combined Log Format structured logger
// Thread-safe async-ready file logging
class Logger {
public:
    enum class Level { DEBUG, INFO, WARN, ERROR };

    // Initialize with paths to log files
    Logger(const std::string& access_path = "logs/access.log",
           const std::string& error_path = "logs/error.log");
    
    ~Logger();

    // No copy
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Log HTTP request/response in Apache Combined Format
    void log_access(const HttpRequest& req, const HttpResponse& resp,
                    const std::string& client_ip, int duration_ms);

    // Log server errors or debug info
    void log_error(Level level, const std::string& message);

    // Log to standard out / standard err in addition to file
    void set_console_output(bool enabled) { console_out_ = enabled; }

private:
    std::string get_current_time() const;
    std::string level_to_string(Level level) const;

    std::ofstream access_log_;
    std::ofstream error_log_;
    
    std::mutex access_mutex_;
    std::mutex error_mutex_;

    bool console_out_ = true;
};
