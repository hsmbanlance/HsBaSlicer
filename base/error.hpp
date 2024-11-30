#pragma once
#ifndef HSBA_SLICER_ERROR_HPP
#define HSBA_SLICER_ERROR_HPP

#include <stdexcept>

namespace HsBa::Slicer 
{ 
    /// <summary>
    /// Runtime Error
    /// </summary>
    class RuntimeError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
        RuntimeError(const std::string& msg) : std::runtime_error(msg) {}
        RuntimeError(const char* msg) : std::runtime_error(msg) {}
        using std::runtime_error::what;
    };
    /// <summary>
    /// Out Of Range Error
    /// </summary>
    class OutOfRangeError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Invalid Argument Error
    /// </summary>
    class InvalidArgumentError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// IO Error
    /// </summary>
    class IOError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Not Implemented Error
    /// </summary>
    class NotImplementedError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Null Value Error
    /// </summary>
    class NullValueError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Not Supported Error
    /// </summary>
    class NotSupportedError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Not Found Error
    /// </summary>
    class NotFoundError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Already Exists Error
    /// </summary>
    class AlreadyExistsError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Permission Denied Error
    /// </summary>
    class PermissionDeniedError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Timeout Error
    /// </summary>
    class TimeoutError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Interrupted Error
    /// </summary>
    class InterruptedError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Cancelled Error
    /// </summary>
    class CancelledError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
    /// <summary>
    /// Out Of Memory Error
    /// </summary>
    class OutOfMemoryError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
        using RuntimeError::what;
    };
}// namespace HsBa::Slicer 

#endif // HSBA_SLICER_ERROR_HPP