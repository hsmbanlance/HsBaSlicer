# Logger 模块文档

Logger 模块提供了灵活的日志记录功能，支持不同日志级别、文件输出和源代码位置追踪。该模块基于Boost.Log实现，并采用了单例模式以确保全局唯一的日志记录器实例。

## 组件列表

- [LoggerSingletone (单例日志类)](./logger_singletone.md) - 线程安全的单例日志记录器
- [LogState (日志状态类)](./log_state.md) - 提供自定义字面量操作符的日志状态类