/** @file error.hpp
 * @brief A header file containing the definition of custom error classes for the HsBa Slicer project.
 * This file defines a set of custom exception classes that inherit from std::runtime_error. These
 * classes represent various types of errors that can occur within the HsBa Slicer project, such as runtime errors, out
 * of range errors, invalid argument errors, and more. Each class provides constructors for creating instances with
 * error messages and inherits the what() method from std::runtime_error for retrieving the error message.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_ERROR_HPP
#define HSBA_SLICER_ERROR_HPP

#include <stdexcept>

namespace HsBa::Slicer
{
/**
 * @brief Runtime Error
 */
class RuntimeError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
    RuntimeError(const std::string& msg) : std::runtime_error(msg) {}
    RuntimeError(const char* msg) : std::runtime_error(msg) {}
    using std::runtime_error::what;
};
/**
 * @brief Out Of Range Error
 */
class OutOfRangeError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Invalid Argument Error
 */
class InvalidArgumentError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Invalid Argument Error
 */
class IOError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Not Implemented Error
 */
class NotImplementedError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Null Value Error
 */
class NullValueError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Not Supported Error
 */
class NotSupportedError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Not Supported Error
 */
class NotFoundError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Already Exists Error
 */
class AlreadyExistsError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Permission Denied Error
 */
class PermissionDeniedError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Timeout Error
 */
class TimeoutError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Interrupted Error
 */
class InterruptedError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Cancelled Error
 */
class CancelledError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
/**
 * @brief Out Of Memory Error
 */
class OutOfMemoryError : public RuntimeError
{
public:
    using RuntimeError::RuntimeError;
    using RuntimeError::what;
};
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_ERROR_HPP