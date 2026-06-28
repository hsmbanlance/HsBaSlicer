#pragma once
#ifndef HSBA_SQL_ADAPTER_HPP

#define HSBA_SQL_ADAPTER_HPP

#include <any>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/delegate.hpp"
#include "base/error.hpp"

namespace HsBa::Slicer::SQL
{
/** @brief Default port for MySQL database connections. */
constexpr unsigned int MYSQL_DEFAULT_PORT = 3306;

/** @brief Default port for PostgreSQL database connections. */
constexpr unsigned int POSTGRESQL_DEFAULT_PORT = 5432;

/**
 * @brief Interface for SQL database adapters.
 *
 * This abstract class defines the common interface for different database backends
 * (SQLite, MySQL, PostgreSQL) with a unified API for CRUD operations.
 */
class ISQLAdapter
{
public:
    /** @brief Type alias for query result rows (vector of key-value maps). */
    using Rows = std::vector<std::unordered_map<std::string, std::any>>;

    virtual ~ISQLAdapter() = default;

    /**
     * @brief Connect to a database server.
     * @param host Database server host address.
     * @param user Username for authentication.
     * @param password Password for authentication.
     * @param database Database name to connect to.
     * @param port Port number (default: MYSQL_DEFAULT_PORT).
     */
    virtual void Connect(std::string_view host, std::string_view user, std::string_view password,
                         std::string_view database, unsigned int port = MYSQL_DEFAULT_PORT) = 0;

    /**
     * @brief Execute a SQL statement without returning results.
     * @param query SQL statement to execute (INSERT, UPDATE, DELETE, etc.).
     */
    virtual void Execute(const std::string& query) = 0;

    /**
     * @brief Execute a SQL query and return results.
     * @param query SQL SELECT statement.
     * @return Vector of result rows.
     */
    virtual Rows Query(const std::string& query) = 0;

    /**
     * @brief Check if the adapter is currently connected.
     * @return true if connected, false otherwise.
     */
    virtual bool IsConnected() const noexcept = 0;

    /**
     * @brief Insert a row into a table.
     * @param table Table name.
     * @param data Map of column names to values.
     */
    virtual void Insert(const std::string& table, const std::unordered_map<std::string, std::any>& data) = 0;

    /**
     * @brief Delete rows from a table.
     * @param table Table name.
     * @param data Map of column conditions for WHERE clause.
     */
    virtual void Delete(const std::string& table, const std::unordered_map<std::string, std::any>& data) = 0;

    /**
     * @brief Update rows in a table.
     * @param table Table name.
     * @param set Map of columns to new values (SET clause).
     * @param where Map of column conditions (WHERE clause).
     */
    virtual void Update(const std::string& table, const std::unordered_map<std::string, std::any>& set,
                        const std::unordered_map<std::string, std::any>& where) = 0;

    /**
     * @brief Select rows from a table with filtering and pagination.
     * @param table Table name.
     * @param columns List of column names to select.
     * @param where Map of column conditions (WHERE clause).
     * @param orderBy Optional ORDER BY clause.
     * @param limit Maximum number of rows to return (-1 for no limit).
     * @param offset Number of rows to skip.
     * @return Vector of result rows.
     */
    virtual Rows Select(const std::string& table, const std::vector<std::string>& columns,
                        const std::unordered_map<std::string, std::any>& where,
                        const std::optional<std::string>& orderBy, int64_t limit, int64_t offset) = 0;

    /**
     * @brief Create a new table.
     * @param table Table name.
     * @param columns Map of column names to their SQL type definitions.
     */
    virtual void CreateTable(const std::string& table, const std::unordered_map<std::string, std::string>& columns) = 0;

    /**
     * @brief Remove (drop) a table.
     * @param table Table name to drop.
     */
    virtual void RemoveTable(const std::string& table) = 0;

    ISQLAdapter() = default;
    ISQLAdapter(const ISQLAdapter&) = delete;
    ISQLAdapter& operator=(const ISQLAdapter&) = delete;
    ISQLAdapter(ISQLAdapter&&) = default;
    ISQLAdapter& operator=(ISQLAdapter&&) = default;
};

/**
 * @brief Base exception class for SQL adapter errors.
 *
 * Inherits from IOError to provide consistent error handling across the codebase.
 */
class SQLAdapterError : public HsBa::Slicer::IOError
{
public:
    explicit SQLAdapterError(const std::string& message) : IOError(message) {}
    explicit SQLAdapterError(std::string&& message) : IOError(std::move(message)) {}
    ~SQLAdapterError() override = default;
};

/**
 * @brief Exception thrown when attempting operations on an unconnected adapter.
 */
class SQLAdapterNotConnectedError : public SQLAdapterError
{
public:
    explicit SQLAdapterNotConnectedError(const std::string& message) : SQLAdapterError(message) {}
    explicit SQLAdapterNotConnectedError(std::string&& message) : SQLAdapterError(std::move(message)) {}
    ~SQLAdapterNotConnectedError() override = default;
};

/**
 * @brief Exception thrown when a SQL query fails.
 */
class SQLAdapterQueryError : public SQLAdapterError
{
public:
    explicit SQLAdapterQueryError(const std::string& message) : SQLAdapterError(message) {}
    explicit SQLAdapterQueryError(std::string&& message) : SQLAdapterError(std::move(message)) {}
    ~SQLAdapterQueryError() override = default;
};

/**
 * @brief Exception thrown when database connection fails.
 */
class SQLAdapterConnectionError : public SQLAdapterError
{
public:
    explicit SQLAdapterConnectionError(const std::string& message) : SQLAdapterError(message) {}
    explicit SQLAdapterConnectionError(std::string&& message) : SQLAdapterError(std::move(message)) {}
    ~SQLAdapterConnectionError() override = default;
};

/**
 * @brief Exception thrown when a database operation times out.
 */
class SQLAdapterTimeoutError : public SQLAdapterError
{
public:
    explicit SQLAdapterTimeoutError(const std::string& message) : SQLAdapterError(message) {}
    explicit SQLAdapterTimeoutError(std::string&& message) : SQLAdapterError(std::move(message)) {}
    ~SQLAdapterTimeoutError() override = default;
};

/**
 * @brief Exception thrown when access is denied due to permissions.
 */
class SQLAdapterPermissionDeniedError : public SQLAdapterError
{
public:
    explicit SQLAdapterPermissionDeniedError(const std::string& message) : SQLAdapterError(message) {}
    explicit SQLAdapterPermissionDeniedError(std::string&& message) : SQLAdapterError(std::move(message)) {}
    ~SQLAdapterPermissionDeniedError() override = default;
};

/**
 * @brief Exception thrown when invalid arguments are provided.
 */
class SQLAdapterInvalidArgumentError : public SQLAdapterError
{
public:
    explicit SQLAdapterInvalidArgumentError(const std::string& message) : SQLAdapterError(message) {}
    explicit SQLAdapterInvalidArgumentError(std::string&& message) : SQLAdapterError(std::move(message)) {}
    ~SQLAdapterInvalidArgumentError() override = default;
};

/**
 * @brief SQLite database adapter implementation.
 *
 * This class provides a thread-safe interface to SQLite databases with support
 * for all standard SQL operations and event notifications.
 */
class SQLiteAdapter : public ISQLAdapter,
                      public Utils::EventSource<SQLiteAdapter, void, std::string_view, std::string_view>
{
public:
    SQLiteAdapter();

    /**
     * @brief Connect to a SQLite database file.
     * @param path Path to the SQLite database file.
     */
    void Connect(std::string_view path);

    /**
     * @brief Connect to a database server (not used for SQLite, provided for interface compatibility).
     * @param host Ignored for SQLite.
     * @param user Ignored for SQLite.
     * @param password Ignored for SQLite.
     * @param database Ignored for SQLite.
     * @param port Ignored for SQLite.
     */
    void Connect(std::string_view host, std::string_view user, std::string_view password, std::string_view database,
                 unsigned int port = MYSQL_DEFAULT_PORT) override;

    void Execute(const std::string& query) override;
    Rows Query(const std::string& query) override;
    bool IsConnected() const noexcept override;
    void Insert(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
    void Delete(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
    void Update(const std::string& table, const std::unordered_map<std::string, std::any>& set,
                const std::unordered_map<std::string, std::any>& where) override;
    Rows Select(const std::string& table, const std::vector<std::string>& columns,
                const std::unordered_map<std::string, std::any>& where, const std::optional<std::string>& orderBy,
                int64_t limit, int64_t offset) override;
    void CreateTable(const std::string& table, const std::unordered_map<std::string, std::string>& columns) override;
    void RemoveTable(const std::string& table) override;
    ~SQLiteAdapter() override;

private:
    std::shared_mutex mutex_;  ///< Mutex for thread-safe operations
    class Impl;                ///< Implementation details (PIMPL pattern)
    std::unique_ptr<Impl> impl_;
};

#ifdef HSBA_USE_MYSQL
/**
 * @brief MySQL database adapter implementation.
 *
 * This class provides a thread-safe interface to MySQL databases with support
 * for all standard SQL operations and event notifications.
 */
class MySQLAdapter : public ISQLAdapter,
                     public Utils::EventSource<SQLiteAdapter, void, std::string_view, std::string_view>
{
public:
    MySQLAdapter();
    void Connect(std::string_view host, std::string_view user, std::string_view password, std::string_view database,
                 unsigned int port = MYSQL_DEFAULT_PORT) override;
    void Execute(const std::string& query) override;
    Rows Query(const std::string& query) override;
    bool IsConnected() const noexcept override;
    void Insert(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
    void Delete(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
    void Update(const std::string& table, const std::unordered_map<std::string, std::any>& set,
                const std::unordered_map<std::string, std::any>& where) override;
    Rows Select(const std::string& table, const std::vector<std::string>& columns,
                const std::unordered_map<std::string, std::any>& where, const std::optional<std::string>& orderBy,
                int64_t limit, int64_t offset) override;
    void CreateTable(const std::string& table, const std::unordered_map<std::string, std::string>& columns) override;
    void RemoveTable(const std::string& table) override;
    ~MySQLAdapter() override;

private:
    std::shared_mutex mutex_;  ///< Mutex for thread-safe operations
    class Impl;                ///< Implementation details (PIMPL pattern)
    std::unique_ptr<Impl> impl_;
};
#endif  // HSBA_USE_MYSQL

#ifdef HSBA_USE_PGSQL
/**
 * @brief PostgreSQL database adapter implementation.
 *
 * This class provides a thread-safe interface to PostgreSQL databases with support
 * for all standard SQL operations and event notifications.
 */
class PostgreSQLAdapter
    : public ISQLAdapter,
      public HsBa::Slicer::Utils::EventSource<SQLiteAdapter, void, std::string_view, std::string_view>
{
public:
    PostgreSQLAdapter();
    void Connect(std::string_view host, std::string_view user, std::string_view password, std::string_view database,
                 unsigned int port = POSTGRESQL_DEFAULT_PORT) override;
    void Execute(const std::string& query) override;
    Rows Query(const std::string& query) override;
    bool IsConnected() const noexcept override;
    void Insert(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
    void Delete(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
    void Update(const std::string& table, const std::unordered_map<std::string, std::any>& set,
                const std::unordered_map<std::string, std::any>& where) override;
    Rows Select(const std::string& table, const std::vector<std::string>& columns,
                const std::unordered_map<std::string, std::any>& where, const std::optional<std::string>& orderBy,
                int64_t limit, int64_t offset) override;
    void CreateTable(const std::string& table, const std::unordered_map<std::string, std::string>& columns) override;
    void RemoveTable(const std::string& table) override;
    ~PostgreSQLAdapter() override;

private:
    std::shared_mutex mutex_;  ///< Mutex for thread-safe operations
    class Impl;                ///< Implementation details (PIMPL pattern)
    std::unique_ptr<Impl> impl_;
};
#endif  // HSBA_USE_PGSQL

/**
 * @brief Pipe operator for executing SQL queries (fluent interface).
 * @param db SQL adapter reference.
 * @param sql SQL query string.
 * @return Query result rows.
 */
inline ISQLAdapter::Rows operator|(ISQLAdapter& db, const std::string& sql)
{
    return db.Query(sql);
}

/**
 * @brief Deleted pipe operator for rvalue references (prevents misuse).
 */
inline void operator|(ISQLAdapter&& db, const std::string&) = delete;

/**
 * @brief Pipe operator for applying a consumer function to the database.
 * @tparam F Callable type that accepts ISQLAdapter&.
 * @param db SQL adapter reference.
 * @param consumer Consumer function to apply.
 * @return Result of the consumer function.
 */
template <std::invocable<ISQLAdapter&> F>
inline auto operator|(ISQLAdapter& db, F&& consumer)
{
    return std::forward<F>(consumer)(db);
}

/**
 * @brief Deleted pipe operator for rvalue references with callable (prevents misuse).
 */
template <class F>
inline auto operator|(ISQLAdapter&& db, F&&) = delete;

/**
 * @brief Structure representing a SQL SELECT operation.
 *
 * This structure encapsulates all parameters needed for a SELECT query,
 * enabling fluent interface usage with the pipe operator.
 */
struct SQLSelect
{
    std::string table;                                        ///< Table name to query
    std::vector<std::string> columns;                         ///< Columns to select
    std::unordered_map<std::string, std::any> where;          ///< WHERE clause conditions
    std::optional<std::string> orderBy;                       ///< ORDER BY clause
    int64_t limit = -1;                                       ///< Maximum rows (-1 for no limit)
    int64_t offset = 0;                                       ///< Rows to skip

    /**
     * @brief Construct SQLSelect with move semantics.
     */
    SQLSelect(std::string&& t, std::vector<std::string>&& c, std::unordered_map<std::string, std::any>&& w = {},
              std::optional<std::string>&& o = std::nullopt, int64_t l = -1, int64_t off = 0)
        : table(std::move(t)), columns(std::move(c)), where(std::move(w)), orderBy(std::move(o)), limit(l), offset(off)
    {
    }

    /**
     * @brief Construct SQLSelect with copy semantics.
     */
    SQLSelect(const std::string& t, const std::vector<std::string>& c,
              const std::unordered_map<std::string, std::any>& w = {},
              const std::optional<std::string>& o = std::nullopt, int64_t l = -1, int64_t off = 0)
        : table(t), columns(c), where(w), orderBy(o), limit(l), offset(off)
    {
    }
};

/**
 * @brief Pipe operator for executing SELECT operations.
 * @param db SQL adapter reference.
 * @param select SQLSelect structure with query parameters.
 * @return Query result rows.
 */
inline ISQLAdapter::Rows operator|(ISQLAdapter& db, const SQLSelect& select)
{
    return db.Select(select.table, select.columns, select.where, select.orderBy, select.limit, select.offset);
}

/**
 * @brief Deleted pipe operator for rvalue references (prevents misuse).
 */
inline void operator|(ISQLAdapter&& db, const SQLSelect&) = delete;

/**
 * @brief Structure representing a SQL INSERT operation.
 */
struct SQLInsert
{
    std::string table;                                        ///< Table name
    std::unordered_map<std::string, std::any> data;           ///< Column-value pairs to insert

    /**
     * @brief Construct SQLInsert with move semantics.
     */
    SQLInsert(std::string&& t, std::unordered_map<std::string, std::any>&& d) : table(std::move(t)), data(std::move(d))
    {
    }

    /**
     * @brief Construct SQLInsert with copy semantics.
     */
    SQLInsert(const std::string& t, const std::unordered_map<std::string, std::any>& d) : table(t), data(d) {}
};

/**
 * @brief Pipe operator for executing INSERT operations.
 * @param db SQL adapter reference.
 * @param insert SQLInsert structure with insert parameters.
 * @return Reference to the database adapter (for chaining).
 */
inline auto& operator|(ISQLAdapter& db, const SQLInsert& insert)
{
    db.Insert(insert.table, insert.data);
    return db;
}

/**
 * @brief Deleted pipe operator for rvalue references (prevents misuse).
 */
inline void operator|(ISQLAdapter&& db, const SQLInsert& insert) = delete;

/**
 * @brief Structure representing a SQL DELETE operation.
 */
struct SQLDelete
{
    std::string table;                                        ///< Table name
    std::unordered_map<std::string, std::any> data;           ///< WHERE clause conditions

    /**
     * @brief Construct SQLDelete with move semantics.
     */
    SQLDelete(std::string&& t, std::unordered_map<std::string, std::any>&& d) : table(std::move(t)), data(std::move(d))
    {
    }

    /**
     * @brief Construct SQLDelete with copy semantics.
     */
    SQLDelete(const std::string& t, const std::unordered_map<std::string, std::any>& d) : table(t), data(d) {}
};

/**
 * @brief Pipe operator for executing DELETE operations.
 * @param db SQL adapter reference.
 * @param del SQLDelete structure with delete parameters.
 * @return Reference to the database adapter (for chaining).
 */
inline auto& operator|(ISQLAdapter& db, const SQLDelete& del)
{
    db.Delete(del.table, del.data);
    return db;
}

/**
 * @brief Deleted pipe operator for rvalue references (prevents misuse).
 */
inline void operator|(ISQLAdapter&& db, const SQLDelete& del) = delete;

/**
 * @brief Structure representing a SQL UPDATE operation.
 */
struct SQLUpdate
{
    std::string table;                                        ///< Table name
    std::unordered_map<std::string, std::any> set;            ///< SET clause (column-value pairs)
    std::unordered_map<std::string, std::any> where;          ///< WHERE clause conditions

    /**
     * @brief Construct SQLUpdate with move semantics.
     */
    SQLUpdate(std::string&& t, std::unordered_map<std::string, std::any>&& s,
              std::unordered_map<std::string, std::any> w)
        : table(std::move(t)), set(std::move(s)), where(std::move(w))
    {
    }

    /**
     * @brief Construct SQLUpdate with copy semantics.
     */
    SQLUpdate(const std::string& t, const std::unordered_map<std::string, std::any>& s,
              const std::unordered_map<std::string, std::any>& w)
        : table(t), set(s), where(w)
    {
    }
};

/**
 * @brief Pipe operator for executing UPDATE operations.
 * @param db SQL adapter reference.
 * @param update SQLUpdate structure with update parameters.
 * @return Reference to the database adapter (for chaining).
 */
inline auto& operator|(ISQLAdapter& db, const SQLUpdate& update)
{
    db.Update(update.table, update.set, update.where);
    return db;
}

/**
 * @brief Deleted pipe operator for rvalue references (prevents misuse).
 */
inline void operator|(ISQLAdapter&& db, const SQLUpdate& update) = delete;

/**
 * @brief Structure representing a SQL CREATE TABLE operation.
 */
struct SQLCreateTable
{
    std::string table;                                        ///< Table name
    std::unordered_map<std::string, std::string> columns;     ///< Column definitions (name -> type)

    /**
     * @brief Construct SQLCreateTable with move semantics.
     */
    SQLCreateTable(std::string&& t, std::unordered_map<std::string, std::string>&& c)
        : table(std::move(t)), columns(std::move(c))
    {
    }

    /**
     * @brief Construct SQLCreateTable with copy semantics.
     */
    SQLCreateTable(const std::string& t, const std::unordered_map<std::string, std::string>& c) : table(t), columns(c)
    {
    }
};

/**
 * @brief Pipe operator for executing CREATE TABLE operations.
 * @param db SQL adapter reference.
 * @param create SQLCreateTable structure with table definition.
 */
inline void operator|(ISQLAdapter& db, const SQLCreateTable& create)
{
    db.CreateTable(create.table, create.columns);
}

/**
 * @brief Deleted pipe operator for rvalue references (prevents misuse).
 */
inline void operator|(ISQLAdapter&& db, const SQLCreateTable& create) = delete;

/**
 * @brief Structure representing a SQL DROP TABLE operation.
 */
struct SQLRemoveTable
{
    std::string table;                                        ///< Table name to drop

    /**
     * @brief Construct SQLRemoveTable with move semantics.
     */
    explicit SQLRemoveTable(std::string&& t) : table(std::move(t)) {}

    /**
     * @brief Construct SQLRemoveTable with copy semantics.
     */
    explicit SQLRemoveTable(const std::string& t) : table(t) {}
};

/**
 * @brief Pipe operator for executing DROP TABLE operations.
 * @param db SQL adapter reference.
 * @param remove SQLRemoveTable structure with table name.
 */
inline void operator|(ISQLAdapter& db, const SQLRemoveTable& remove)
{
    db.RemoveTable(remove.table);
}

/**
 * @brief Deleted pipe operator for rvalue references (prevents misuse).
 */
inline void operator|(ISQLAdapter&& db, const SQLRemoveTable& remove) = delete;
}  // namespace HsBa::Slicer::SQL

#endif  // !HSBA_SQL_ADAPTER_HPP