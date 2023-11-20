/**
 * @file common/logger.hpp
 * @brief Header file for logger singleton class
 * @author Lukas Vecerka (xvecer30)
*/

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>

/**
 * @brief Singleton class for logging
*/
class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    /**
     * @brief Log a message on std::cout
     * @param message The message to log
    */
    void log(const std::string& message) {
        std::cout << message + "\n";
    }

    /**
     * @brief Log an error message on std::cerr
     * @param message The error message to log
    */
    void error(const std::string& message) {
        std::cerr << message + "\n";
    }

private:
    // Private constructor to prevent instantiation
    Logger() {}
};

#endif