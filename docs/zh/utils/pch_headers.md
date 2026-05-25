# PCH Headers (预编译头文件)

PCH Headers (Precompiled Headers) 是一个预编译头文件，用于加速编译过程，包含常用的 C++ 标准库和第三方库头文件。

## 功能特点

- 包含常用的 C++ 标准库头文件
- 包含 Boost 库的常用组件
- 包含 Eigen 数学库
- 包含 Base 模块的基础组件
- 提高编译速度

## 包含的头文件

### 标准库组件
- `<vector>` - 动态数组容器
- `<list>` - 双向链表容器
- `<unordered_map>` - 哈希映射容器
- `<map>` - 有序映射容器
- `<set>` - 有序集合容器
- `<unordered_set>` - 哈希集合容器
- `<any>` - 任意类型存储
- `<variant>` - 类型安全的联合体
- `<string_view>` - 字符串视图
- `<string>` - 字符串类
- `<algorithm>` - 算法库
- `<regex>` - 正则表达式库

### Boost 库组件
- `<boost/any.hpp>` - Boost 任意类型存储
- `<boost/variant.hpp>` - Boost 变体类型
- `<boost/variant2.hpp>` - Boost 变体类型 (第二版)
- `<boost/date_time.hpp>` - Boost 日期时间库

### 数学和基础库
- `<Eigen/Core>` - Eigen 线性代数库核心
- `"base/concepts.hpp"` - 基础概念定义
- `"base/error.hpp"` - 错误处理定义
- `"base/template_helper.hpp"` - 模板辅助功能
- `"base/encoding_convert.hpp"` - 编码转换功能

## 使用方法

该文件通常作为预编译头文件使用，在项目中自动包含，无需手动引入。

## 注意事项

- 预编译头文件只应包含稳定不变的头文件
- 避免在预编译头文件中包含频繁更改的头文件
- 预编译头文件有助于加快编译速度，但会增加内存使用