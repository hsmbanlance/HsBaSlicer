# SQL Adapter (SQL Adapter)

The SQL Adapter provides SQLite database operation functionality, including database connection pool and SQL execution functions.

## Features

- Supports SQLite database operations
- Provides database connection pool management
- Supports SQL query and execution
- Provides transaction processing functionality
- Supports parameterized queries to prevent SQL injection

## Main Functions

- Database connection management
- SQL statement execution
- Query result processing
- Transaction control
- Connection pool management

## Usage Examples

### 1. SQLite Basic Operations

```cpp
#include "fileoperator/sql_adapter.hpp"
#include <iostream>

int main() {
    // Create SQLite adapter instance
    HsBa::Slicer::SQL::SQLiteAdapter db;
    
    // Connect to SQLite database (file will be created if not exists)
    db.Connect("example.db");
    
    // Create table
    db.CreateTable("users", {
        {"id", "INTEGER PRIMARY KEY AUTOINCREMENT"},
        {"name", "TEXT NOT NULL"},
        {"age", "INTEGER"},
        {"email", "TEXT"}
    });
    
    // Insert data
    db.Insert("users", {
        {"name", std::string("Zhang San")},
        {"age", int64_t(25)},
        {"email", std::string("zhangsan@example.com")}
    });
    
    db.Insert("users", {
        {"name", std::string("Li Si")},
        {"age", int64_t(30)},
        {"email", std::string("lisi@example.com")}
    });
    
    // Query data
    auto results = db.Select("users", 
                            {"id", "name", "age", "email"},
                            {},  // empty where condition
                            std::nullopt,  // no ordering
                            -1,  // limit: -1 means no limit
                            0);   // offset
    
    // Iterate through query results
    for (const auto& row : results) {
        std::cout << "ID: " << std::any_cast<int64_t>(row.at("id")) << ", ";
        std::cout << "Name: " << std::any_cast<std::string>(row.at("name")) << ", ";
        std::cout << "Age: " << std::any_cast<int64_t>(row.at("age")) << std::endl;
    }
    
    // Query with conditions
    auto filtered = db.Select("users",
                             {"name", "email"},
                             {{"age", int64_t(30)}},  // WHERE age = 30
                             "name ASC",  // ORDER BY name
                             10,  // LIMIT 10
                             0);  // OFFSET 0
    
    // Update data
    db.Update("users",
              {{"email", std::string("zhangsan_new@example.com")}},  // SET email = ...
              {{"name", std::string("Zhang San")}});  // WHERE name = 'Zhang San'
    
    // Delete data
    db.Delete("users", {{"name", std::string("Li Si")}});  // WHERE name = 'Li Si'
    
    // Execute SQL query directly
    auto customResults = db.Query("SELECT * FROM users WHERE age > ?", 20);
    
    // Remove table
    db.RemoveTable("users");
    
    return 0;
}
```

### 2. Simplify Code with Pipe Operator

```cpp
#include "fileoperator/sql_adapter.hpp"

int main() {
    HsBa::Slicer::SQL::SQLiteAdapter db;
    db.Connect("example.db");
    
    // Create table
    db | HsBa::Slicer::SQL::SQLCreateTable{
        "products",
        {{"id", "INTEGER PRIMARY KEY"}, {"name", "TEXT"}, {"price", "REAL"}}
    };
    
    // Insert data
    db | HsBa::Slicer::SQL::SQLInsert{
        "products",
        {{"id", int64_t(1)}, {"name", std::string("Apple")}, {"price", 5.5}}
    };
    
    // Query data
    auto products = db | HsBa::Slicer::SQL::SQLSelect{
        "products",
        {"id", "name", "price"},
        {{"price", 5.5}},  // WHERE price = 5.5
        std::nullopt,
        -1,
        0
    };
    
    // Update data
    db | HsBa::Slicer::SQL::SQLUpdate{
        "products",
        {{"price", 6.0}},  // SET price = 6.0
        {{"id", int64_t(1)}}  // WHERE id = 1
    };
    
    // Delete data
    db | HsBa::Slicer::SQL::SQLDelete{
        "products",
        {{"id", int64_t(1)}}
    };
    
    // Remove table
    db | HsBa::Slicer::SQL::SQLRemoveTable{"products"};
    
    return 0;
}
```

### 3. MySQL Operations (Requires HSBA_USE_MYSQL)

```cpp
#ifdef HSBA_USE_MYSQL
#include "fileoperator/sql_adapter.hpp"

int main() {
    HsBa::Slicer::SQL::MySQLAdapter db;
    
    // Connect to MySQL database
    db.Connect("localhost", "username", "password", "database_name", 3306);
    
    // Create table
    db.CreateTable("orders", {
        {"id", "INT AUTO_INCREMENT PRIMARY KEY"},
        {"customer_name", "VARCHAR(100)"},
        {"amount", "DECIMAL(10,2)"},
        {"order_date", "DATETIME DEFAULT CURRENT_TIMESTAMP"}
    });
    
    // Insert data
    db.Insert("orders", {
        {"customer_name", std::string("Wang Wu")},
        {"amount", 199.99}
    });
    
    // Query data
    auto orders = db.Select("orders",
                           {"id", "customer_name", "amount"},
                           {},  // no condition
                           "order_date DESC",
                           100,
                           0);
    
    // Process query results...
    
    return 0;
}
#endif
```

### 4. PostgreSQL Operations (Requires HSBA_USE_PGSQL)

```cpp
#ifdef HSBA_USE_PGSQL
#include "fileoperator/sql_adapter.hpp"

int main() {
    HsBa::Slicer::SQL::PostgreSQLAdapter db;
    
    // Connect to PostgreSQL database
    db.Connect("localhost", "username", "password", "database_name", 5432);
    
    // Create table
    db.CreateTable("inventory", {
        {"id", "SERIAL PRIMARY KEY"},
        {"item_name", "VARCHAR(100)"},
        {"quantity", "INTEGER"},
        {"last_updated", "TIMESTAMP DEFAULT NOW()"}
    });
    
    // Insert data
    db.Insert("inventory", {
        {"item_name", std::string("Office Supplies")},
        {"quantity", int64_t(50)}
    });
    
    // Query data
    auto inventory = db.Select("inventory",
                              {"id", "item_name", "quantity"},
                              {{"quantity", int64_t(50)}},
                              "item_name ASC",
                              -1,
                              0);
    
    // Process query results...
    
    return 0;
}
#endif
```

### 5. Advanced Usage: Transaction and Error Handling

```cpp
#include "fileoperator/sql_adapter.hpp"
#include <iostream>

int main() {
    HsBa::Slicer::SQL::SQLiteAdapter db;
    
    try {
        // Connect to database
        db.Connect("transaction_example.db");
        
        // Create table
        db.CreateTable("accounts", {
            {"id", "INTEGER PRIMARY KEY"},
            {"name", "TEXT"},
            {"balance", "REAL"}
        });
        
        // Initialize accounts
        db.Insert("accounts", {{"id", int64_t(1)}, {"name", std::string("Account A")}, {"balance", 1000.0}});
        db.Insert("accounts", {{"id", int64_t(2)}, {"name", std::string("Account B")}, {"balance", 500.0}});
        
        // Transaction: Transfer operation
        // Note: In the current implementation, multiple operations are automatically executed in the same transaction
        // For explicit transaction control, you can use BEGIN/COMMIT/ROLLBACK
        
        try {
            // Start transaction
            db.Execute("BEGIN TRANSACTION");
            
            // Deduct from Account A
            db.Update("accounts",
                     {{"balance", 800.0}},  // 1000 - 200
                     {{"id", int64_t(1)}});
            
            // Add to Account B
            db.Update("accounts",
                     {{"balance", 700.0}},  // 500 + 200
                     {{"id", int64_t(2)}});
            
            // Commit transaction
            db.Execute("COMMIT");
            std::cout << "Transfer successful!" << std::endl;
            
        } catch (const HsBa::Slicer::SQL::SQLAdapterError& e) {
            // Error occurred, rollback transaction
            db.Execute("ROLLBACK");
            std::cerr << "Transfer failed: " << e.what() << std::endl;
        }
        
        // Query final balances
        auto accounts = db.Select("accounts", {"id", "name", "balance"}, {}, std::nullopt, -1, 0);
        for (const auto& acc : accounts) {
            std::cout << "Account: " << std::any_cast<std::string>(acc.at("name")) 
                      << ", Balance: " << std::any_cast<double>(acc.at("balance")) << std::endl;
        }
        
    } catch (const HsBa::Slicer::SQL::SQLAdapterConnectionError& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        return 1;
    } catch (const HsBa::Slicer::SQL::SQLAdapterQueryError& e) {
        std::cerr << "Query error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unknown error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### 6. Event Listening

```cpp
#include "fileoperator/sql_adapter.hpp"
#include <iostream>

int main() {
    HsBa::Slicer::SQL::SQLiteAdapter db;
    
    // Register event listener
    db += ([](std::string_view eventType, std::string_view details) {
        std::cout << "[SQL Event] " << eventType << ": " << details << std::endl;
    });
    
    db.Connect("event_example.db");
    db.CreateTable("logs", {{"id", "INTEGER PRIMARY KEY"}, {"message", "TEXT"}});
    db.Insert("logs", {{"message", std::string("First log entry")}});
    
    // All operations will trigger event notifications
    
    return 0;
}
```

## Notes

- Use parameterized queries to prevent SQL injection attacks
- Close database connections in time to release resources
- Pay attention to exception handling in transaction processing
- Database file paths should ensure appropriate read/write permissions