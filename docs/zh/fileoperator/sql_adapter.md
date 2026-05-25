# SQL Adapter (SQL适配器)

SQL Adapter 提供了SQLite数据库操作功能，包括数据库连接池和SQL执行等功能。

## 功能特点

- 支持SQLite数据库操作
- 提供数据库连接池管理
- 支持SQL查询和执行
- 提供事务处理功能
- 支持参数化查询防止SQL注入

## 主要功能

- 数据库连接管理
- SQL语句执行
- 查询结果处理
- 事务控制
- 连接池管理

## 使用示例

### 1. SQLite 基本操作

```cpp
#include "fileoperator/sql_adapter.hpp"
#include <iostream>

int main() {
    // 创建 SQLite 适配器实例
    HsBa::Slicer::SQL::SQLiteAdapter db;
    
    // 连接到 SQLite 数据库（文件不存在会自动创建）
    db.Connect("example.db");
    
    // 创建表
    db.CreateTable("users", {
        {"id", "INTEGER PRIMARY KEY AUTOINCREMENT"},
        {"name", "TEXT NOT NULL"},
        {"age", "INTEGER"},
        {"email", "TEXT"}
    });
    
    // 插入数据
    db.Insert("users", {
        {"name", std::string("张三")},
        {"age", int64_t(25)},
        {"email", std::string("zhangsan@example.com")}
    });
    
    db.Insert("users", {
        {"name", std::string("李四")},
        {"age", int64_t(30)},
        {"email", std::string("lisi@example.com")}
    });
    
    // 查询数据
    auto results = db.Select("users", 
                            {"id", "name", "age", "email"},
                            {},  // where 条件为空
                            std::nullopt,  // 不排序
                            -1,  // limit: -1 表示不限制
                            0);   // offset
    
    // 遍历查询结果
    for (const auto& row : results) {
        std::cout << "ID: " << std::any_cast<int64_t>(row.at("id")) << ", ";
        std::cout << "Name: " << std::any_cast<std::string>(row.at("name")) << ", ";
        std::cout << "Age: " << std::any_cast<int64_t>(row.at("age")) << std::endl;
    }
    
    // 带条件的查询
    auto filtered = db.Select("users",
                             {"name", "email"},
                             {{"age", int64_t(30)}},  // WHERE age = 30
                             "name ASC",  // ORDER BY name
                             10,  // LIMIT 10
                             0);  // OFFSET 0
    
    // 更新数据
    db.Update("users",
              {{"email", std::string("zhangsan_new@example.com")}},  // SET email = ...
              {{"name", std::string("张三")}});  // WHERE name = '张三'
    
    // 删除数据
    db.Delete("users", {{"name", std::string("李四")}});  // WHERE name = '李四'
    
    // 直接执行 SQL 查询
    auto customResults = db.Query("SELECT * FROM users WHERE age > ?", 20);
    
    // 删除表
    db.RemoveTable("users");
    
    return 0;
}
```

### 2. 使用管道操作符简化代码

```cpp
#include "fileoperator/sql_adapter.hpp"

int main() {
    HsBa::Slicer::SQL::SQLiteAdapter db;
    db.Connect("example.db");
    
    // 创建表
    db | HsBa::Slicer::SQL::SQLCreateTable{
        "products",
        {{"id", "INTEGER PRIMARY KEY"}, {"name", "TEXT"}, {"price", "REAL"}}
    };
    
    // 插入数据
    db | HsBa::Slicer::SQL::SQLInsert{
        "products",
        {{"id", int64_t(1)}, {"name", std::string("苹果")}, {"price", 5.5}}
    };
    
    // 查询数据
    auto products = db | HsBa::Slicer::SQL::SQLSelect{
        "products",
        {"id", "name", "price"},
        {{"price", 5.5}},  // WHERE price = 5.5
        std::nullopt,
        -1,
        0
    };
    
    // 更新数据
    db | HsBa::Slicer::SQL::SQLUpdate{
        "products",
        {{"price", 6.0}},  // SET price = 6.0
        {{"id", int64_t(1)}}  // WHERE id = 1
    };
    
    // 删除数据
    db | HsBa::Slicer::SQL::SQLDelete{
        "products",
        {{"id", int64_t(1)}}
    };
    
    // 删除表
    db | HsBa::Slicer::SQL::SQLRemoveTable{"products"};
    
    return 0;
}
```

### 3. MySQL 操作（需要启用 HSBA_USE_MYSQL）

```cpp
#ifdef HSBA_USE_MYSQL
#include "fileoperator/sql_adapter.hpp"

int main() {
    HsBa::Slicer::SQL::MySQLAdapter db;
    
    // 连接到 MySQL 数据库
    db.Connect("localhost", "username", "password", "database_name", 3306);
    
    // 创建表
    db.CreateTable("orders", {
        {"id", "INT AUTO_INCREMENT PRIMARY KEY"},
        {"customer_name", "VARCHAR(100)"},
        {"amount", "DECIMAL(10,2)"},
        {"order_date", "DATETIME DEFAULT CURRENT_TIMESTAMP"}
    });
    
    // 插入数据
    db.Insert("orders", {
        {"customer_name", std::string("王五")},
        {"amount", 199.99}
    });
    
    // 查询数据
    auto orders = db.Select("orders",
                           {"id", "customer_name", "amount"},
                           {},  // 无条件
                           "order_date DESC",
                           100,
                           0);
    
    // 处理查询结果...
    
    return 0;
}
#endif
```

### 4. PostgreSQL 操作（需要启用 HSBA_USE_PGSQL）

```cpp
#ifdef HSBA_USE_PGSQL
#include "fileoperator/sql_adapter.hpp"

int main() {
    HsBa::Slicer::SQL::PostgreSQLAdapter db;
    
    // 连接到 PostgreSQL 数据库
    db.Connect("localhost", "username", "password", "database_name", 5432);
    
    // 创建表
    db.CreateTable("inventory", {
        {"id", "SERIAL PRIMARY KEY"},
        {"item_name", "VARCHAR(100)"},
        {"quantity", "INTEGER"},
        {"last_updated", "TIMESTAMP DEFAULT NOW()"}
    });
    
    // 插入数据
    db.Insert("inventory", {
        {"item_name", std::string("办公用品")},
        {"quantity", int64_t(50)}
    });
    
    // 查询数据
    auto inventory = db.Select("inventory",
                              {"id", "item_name", "quantity"},
                              {{"quantity", int64_t(50)}},
                              "item_name ASC",
                              -1,
                              0);
    
    // 处理查询结果...
    
    return 0;
}
#endif
```

### 5. 高级用法：事务处理和错误处理

```cpp
#include "fileoperator/sql_adapter.hpp"
#include <iostream>

int main() {
    HsBa::Slicer::SQL::SQLiteAdapter db;
    
    try {
        // 连接数据库
        db.Connect("transaction_example.db");
        
        // 创建表
        db.CreateTable("accounts", {
            {"id", "INTEGER PRIMARY KEY"},
            {"name", "TEXT"},
            {"balance", "REAL"}
        });
        
        // 初始化账户
        db.Insert("accounts", {{"id", int64_t(1)}, {"name", std::string("账户 A")}, {"balance", 1000.0}});
        db.Insert("accounts", {{"id", int64_t(2)}, {"name", std::string("账户 B")}, {"balance", 500.0}});
        
        // 事务处理：转账操作
        // 注意：当前实现中，多个操作会自动在同一个事务中执行
        // 如需显式事务控制，可以使用 BEGIN/COMMIT/ROLLBACK
        
        try {
            // 开始事务
            db.Execute("BEGIN TRANSACTION");
            
            // 从账户 A 扣款
            db.Update("accounts",
                     {{"balance", 800.0}},  // 1000 - 200
                     {{"id", int64_t(1)}});
            
            // 向账户 B 加款
            db.Update("accounts",
                     {{"balance", 700.0}},  // 500 + 200
                     {{"id", int64_t(2)}});
            
            // 提交事务
            db.Execute("COMMIT");
            std::cout << "转账成功！" << std::endl;
            
        } catch (const HsBa::Slicer::SQL::SQLAdapterError& e) {
            // 发生错误，回滚事务
            db.Execute("ROLLBACK");
            std::cerr << "转账失败：" << e.what() << std::endl;
        }
        
        // 查询最终余额
        auto accounts = db.Select("accounts", {"id", "name", "balance"}, {}, std::nullopt, -1, 0);
        for (const auto& acc : accounts) {
            std::cout << "账户：" << std::any_cast<std::string>(acc.at("name")) 
                      << ", 余额：" << std::any_cast<double>(acc.at("balance")) << std::endl;
        }
        
    } catch (const HsBa::Slicer::SQL::SQLAdapterConnectionError& e) {
        std::cerr << "连接错误：" << e.what() << std::endl;
        return 1;
    } catch (const HsBa::Slicer::SQL::SQLAdapterQueryError& e) {
        std::cerr << "查询错误：" << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "未知错误：" << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### 6. 事件监听

```cpp
#include "fileoperator/sql_adapter.hpp"
#include <iostream>

int main() {
    HsBa::Slicer::SQL::SQLiteAdapter db;
    
    // 注册事件监听器
    db+=([](std::string_view eventType, std::string_view details) {
        std::cout << "[SQL Event] " << eventType << ": " << details << std::endl;
    });
    
    db.Connect("event_example.db");
    db.CreateTable("logs", {{"id", "INTEGER PRIMARY KEY"}, {"message", "TEXT"}});
    db.Insert("logs", {{"message", std::string("第一条日志")}});
    
    // 所有操作都会触发事件通知
    
    return 0;
}
```

## 注意事项

- 使用参数化查询来防止SQL注入攻击
- 及时关闭数据库连接以释放资源
- 在事务处理中要注意异常处理
- 数据库文件路径应确保有适当的读写权限