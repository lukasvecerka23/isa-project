#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

class ParsingError : public std::runtime_error {
public:
    ParsingError(const std::string& message) : std::runtime_error(message) {}
    static const int errorCode = 4;  // Error code for parsing errors
};

class OptionError : public std::runtime_error {
public:
    OptionError(const std::string& message) : std::runtime_error(message) {}
    static const int errorCode = 8;  // Error code for option errors
};

#endif // EXCEPTIONS_HPP