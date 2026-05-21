# LogState (Logging State Class)

The LogState class provides auxiliary functionality for custom literal operators, simplifying the logging process.

## Features

- Provides custom literal operators
- Supports concise logging syntax
- Works with LoggerSingletone
- Automatically includes source code location information

## Method Description

### Custom Literals
- `operator""_log_debug` - Debug log literal operator
- `operator""_log_info` - Info log literal operator
- `operator""_log_warning` - Warning log literal operator
- `operator""_log_error` - Error log literal operator

## Usage Examples

```cpp
#include "logger/logger.hpp"

using namespace HsBa::Slicer::Log;

// Using custom literals to record logs (more concise way)
"Application started"_log_info;
"Configuration loaded"_log_debug;
"Invalid parameter detected"_log_warning;
"Failed to connect to database"_log_error;
```

## Notes

- Custom literals provide a more concise way of logging
- Usage automatically includes source code location information
- Still relies on LoggerSingletone for actual logging
- Requires C++11 or higher for custom literal support