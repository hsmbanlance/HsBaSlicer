# Logger Module Documentation

The Logger module provides flexible logging functionality with support for different log levels, file output, and source code location tracking. The module is implemented based on Boost.Log and uses singleton pattern to ensure a globally unique logger instance.

## Component List

- [LoggerSingletone (Singleton Logger Class)](./logger_singletone.md) - Thread-safe singleton logger
- [LogState (Logging State Class)](./log_state.md) - Log state class providing custom literal operators