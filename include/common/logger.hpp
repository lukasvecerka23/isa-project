#include <iostream>

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(const std::string& message) {
        std::cout << message + "\n";
    }

    void error(const std::string& message) {
        std::cerr << message + "\n";
    }

private:
    // Private constructor to prevent instantiation
    Logger() {}
};