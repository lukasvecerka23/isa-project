/**
 * @file common/exceptions.hpp
 * @brief Header file with declaration for exceptions
 * @author Lukas Vecerka (xvecer30)
*/

#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

/**
 * @brief Exception class for errors during parsing of packets
*/
class ParsingError : public std::runtime_error {
public:
    ParsingError(const std::string& message) : std::runtime_error(message) {}
    static const int errorCode = 4;  // Error code for parsing errors
};

/**
 * @brief Exception class for errors during option parsing
*/
class OptionError : public std::runtime_error {
public:
    OptionError(const std::string& message) : std::runtime_error(message) {}
    static const int errorCode = 8;  // Error code for option errors
};

#endif // EXCEPTIONS_HPP