#ifndef NETWORK_EXCEPTION_H
#define NETWORK_EXCEPTION_H

#include <stdexcept>
#include <string>

class NetworkException : public std::runtime_error {
public:
    explicit NetworkException(const std::string& message) 
        : std::runtime_error(message) {}
    
    NetworkException(const std::string& operation, int error_code)
        : std::runtime_error(operation + ": " + std::to_string(error_code)) {}
};

#endif
