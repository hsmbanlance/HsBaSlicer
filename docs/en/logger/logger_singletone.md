# LoggerSingletone (Singleton Logger Class)

The LoggerSingletone class implements the core logger with singleton pattern, providing thread-safe logging functionality.

## Features

- Uses singleton pattern to ensure globally unique instance
- Provides multi-level logging (debug, info, warning, error)
- Supports source code location tracking
- Thread-safe implementation
- Supports log file output control

## Method Description

### Instance Retrieval
- `GetInstance()` - Gets the logger singleton instance
- `CreateInstance()` - Creates the singleton instance (internal use)

### Logging Functions
- `Log()` - General logging function supporting specified log level
- `LogDebug()` - Records debug level logs
- `LogInfo()` - Records info level logs
- `LogWarning()` - Records warning level logs
- `LogError()` - Records error level logs

### Configuration Properties
- `UseLogFile()` - Queries whether log file output is enabled

## Usage Examples

```cpp
#include "logger/logger.hpp"

using namespace HsBa::Slicer::Log;

// Get logger instance
auto logger = LoggerSingletone::GetInstance();

// Using conventional methods to record logs
LoggerSingletone::LogDebug("This is debug information");
LoggerSingletone::LogInfo("This is general information");
LoggerSingletone::LogWarning("This is a warning");
LoggerSingletone::LogError("This is an error");
```

## Notes

- The logger uses singleton pattern to ensure only one instance globally
- In multithreaded environments, shared mutex is used internally to ensure thread safety
- Source code location tracking requires C++20 support
- Set appropriate log levels to balance debugging information and performance