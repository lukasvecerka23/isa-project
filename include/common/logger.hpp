#include <iostream>
#include <mutex>

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << message << std::endl;
    }

    void error(const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cerr << message << std::endl;
    }

private:
    std::mutex log_mutex;

    // Private constructor to prevent instantiation
    Logger() {}
};