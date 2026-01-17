#include "sql_adapter.hpp"

#include <sstream>
#include <format>

#include <boost/container/vector.hpp>

#ifdef HSBA_USE_MYSQL
#include <mysql/mysql.h>
#endif // HSBA_USE_MYSQL

#include <sqlite3.h>

#ifdef HSBA_USE_PGSQL
#include <libpq-fe.h>
#endif // HSBA_USE_PGSQL

#include "base/any_visit.hpp"
#include <filesystem>

namespace HsBa::Slicer::SQL
{
#ifdef HSBA_USE_PGSQL
	namespace
	{
		// PostgreSQL field type constants
		constexpr Oid PG_TYPE_INT8 = 20;      // BIGINT
		constexpr Oid PG_TYPE_INT2 = 21;      // SMALLINT
		constexpr Oid PG_TYPE_INT4 = 23;      // INTEGER
		constexpr Oid PG_TYPE_FLOAT4 = 700;   // REAL
		constexpr Oid PG_TYPE_FLOAT8 = 701;   // DOUBLE PRECISION
		constexpr Oid PG_TYPE_TEXT = 25;      // TEXT
		constexpr Oid PG_TYPE_VARCHAR = 1043; // VARCHAR
		constexpr Oid PG_TYPE_BYTEA = 17;     // BYTEA
	}
#endif // HSBA_USE_PGSQL

	class SQLiteAdapter::Impl
	{
	public:
		sqlite3* db = nullptr;
		bool connected = false;
		std::string lastError;
		~Impl()
		{
			if (db)
			{
				sqlite3_close(db);
				connected = false;
				db = nullptr;
			}
		}

		void Connect(std::string_view database)
		{
			db = nullptr;
			if (sqlite3_open(database.data(), &db) != SQLITE_OK)
			{
				lastError = sqlite3_errmsg(db);
				sqlite3_close(db);
				throw SQLAdapterConnectionError(lastError);
			}
			connected = true;
		}
		void Execute(const std::string& query)
		{
			if (!connected)
			{
				throw SQLAdapterNotConnectedError("Not connected to the database.");
			}
			char* errMsg = nullptr;
			if (sqlite3_exec(db, query.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK)
			{
				lastError = errMsg ? errMsg : "Unknown error";
				sqlite3_free(errMsg);
				throw SQLAdapterQueryError(lastError);
			}
		}
		Rows Query(const std::string& query)
		{
			if (!connected)
			{
				throw SQLAdapterNotConnectedError("Not connected to the database.");
			}
			Rows rows;
			char* errMsg = nullptr;
			sqlite3_stmt* stmt;
			if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
			{
				lastError = sqlite3_errmsg(db);
				throw SQLAdapterQueryError(lastError);
			}
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				std::unordered_map<std::string, std::any> row;
				int columnCount = sqlite3_column_count(stmt);
				for (int i = 0; i < columnCount; ++i)
				{
					const char* columnName = sqlite3_column_name(stmt, i);
					switch (sqlite3_column_type(stmt, i))
					{
					case SQLITE_INTEGER:
						row[columnName] = sqlite3_column_int64(stmt, i);
						break;
					case SQLITE_FLOAT:
						row[columnName] = sqlite3_column_double(stmt, i);
						break;
					case SQLITE_TEXT:
						row[columnName] = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)));
						break;
					case SQLITE_NULL:
						row[columnName] = std::any();
						break;
					case SQLITE_BLOB:
						row[columnName] = std::vector<unsigned char>(
							reinterpret_cast<const unsigned char*>(sqlite3_column_text(stmt, i)),
							reinterpret_cast<const unsigned char*>(sqlite3_column_text(stmt, i)) + sqlite3_column_bytes(stmt, i));
						break;
					default:
						lastError = "Unsupported column type";
						sqlite3_finalize(stmt);
						throw SQLAdapterQueryError(lastError);
					}
				}
				rows.push_back(std::move(row));
			}
			if (sqlite3_finalize(stmt) != SQLITE_OK)
			{
				lastError = sqlite3_errmsg(db);
				throw SQLAdapterQueryError(lastError);
			}
			return rows;
		}
	};
	SQLiteAdapter::SQLiteAdapter() : impl_(std::make_unique<Impl>()) {}

	SQLiteAdapter::~SQLiteAdapter() = default;

	void SQLiteAdapter::Connect(std::string_view path)
	{
		std::lock_guard lock(mutex_);
		if (impl_->connected)
		{
			throw SQLAdapterConnectionError("Already connected to the database.");
		}
		if (path.empty())
		{
			throw SQLAdapterInvalidArgumentError("Database path cannot be empty.");
		}
		impl_->Connect(path);
		RaiseEvent("Connected to SQLite database", std::string(path));
	}

	void SQLiteAdapter::Connect(std::string_view host, std::string_view user, std::string_view password,
		std::string_view database, unsigned int port)
	{
		std::lock_guard lock(mutex_);
		if (impl_->connected)
		{
			throw SQLAdapterConnectionError("Already connected to the database.");
		}
		impl_->Connect(database);
		RaiseEvent("Connected to SQLite database", std::string(database));
	}

	void SQLiteAdapter::Execute(const std::string& query)
	{
		std::lock_guard lock(mutex_);
		impl_->Execute(query);
		RaiseEvent("Execute query", query);
	}

	SQLiteAdapter::Rows SQLiteAdapter::Query(const std::string& query)
	{
		std::lock_guard lock(mutex_);
		auto rows = impl_->Query(query);
		RaiseEvent("Query executed", query);
		return rows;
	}

	void SQLiteAdapter::Insert(const std::string& table,
		const std::unordered_map<std::string, std::any>& data)
	{
		if (!impl_->db) throw SQLAdapterNotConnectedError("SQLite db is null");
		if (data.empty()) return;

		std::ostringstream cols, placeholders;
		std::vector<std::string> keys;
		keys.reserve(data.size());
		for (const auto& [k, _] : data) keys.push_back(k);

		for (size_t i = 0; i < keys.size(); ++i) {
			if (i) { cols << ','; placeholders << ','; }
			cols << keys[i];
			placeholders << '?';
		}

		const std::string sql = "INSERT INTO " + table + " (" + cols.str() +
			") VALUES (" + placeholders.str() + ")";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			throw SQLAdapterQueryError("prepare failed: " +
				std::string(sqlite3_errmsg(impl_->db)));
		}

		for (size_t i = 0; i < keys.size(); ++i) {
			const std::any& val = data.at(keys[i]);
			int idx = static_cast<int>(i + 1);

			if (!val.has_value()) {
				sqlite3_bind_null(stmt, idx);
				continue;
			}

			Utils::Visit<std::nullptr_t, int64_t, double, std::string, std::vector<unsigned char>>(
				[&](const auto& v) {
					using T = std::decay_t<decltype(v)>;
					if constexpr (std::is_same_v<T, int64_t>)
					{
						sqlite3_bind_int64(stmt, idx, v);
					}
					else if constexpr (std::is_same_v<T, double>)
					{
						sqlite3_bind_double(stmt, idx, v);
					}
					else if constexpr (std::is_same_v<T, std::string>)
					{
						sqlite3_bind_text(stmt, idx, v.c_str(), v.size(), SQLITE_TRANSIENT);
					}
					else if constexpr (std::is_same_v<T, std::vector<unsigned char>>)
					{
						sqlite3_bind_blob(stmt, idx, v.data(), v.size(), SQLITE_TRANSIENT);
					}
					else if constexpr (std::is_same_v<T, std::nullptr_t>)
					{
						sqlite3_bind_null(stmt, idx);
					}
				}, val
			);
		}

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			impl_->lastError = sqlite3_errmsg(impl_->db);
			sqlite3_finalize(stmt);
			throw SQLAdapterQueryError("execute failed: " + impl_->lastError);
		}

		sqlite3_finalize(stmt);
		RaiseEvent("Insert executed", sql);
	}

	bool SQLiteAdapter::IsConnected() const noexcept
	{
		return impl_->connected;
	}

	void SQLiteAdapter::Delete(const std::string& table,
		const std::unordered_map<std::string, std::any>& data)
	{
		if (!impl_->db) throw SQLAdapterNotConnectedError("SQLite db is null");
		if (data.empty()) return;

		std::ostringstream where;
		std::vector<std::string> keys;
		keys.reserve(data.size());
		for (const auto& [k, _] : data) keys.push_back(k);

		for (size_t i = 0; i < keys.size(); ++i) {
			if (i) where << " AND ";
			where << keys[i] << "=?";
		}

		const std::string sql = "DELETE FROM " + table + " WHERE " + where.str();

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			throw SQLAdapterQueryError("prepare failed: " +
				std::string(sqlite3_errmsg(impl_->db)));
		}

		for (size_t i = 0; i < keys.size(); ++i) {
			const std::any& val = data.at(keys[i]);
			int idx = static_cast<int>(i + 1);

			if (!val.has_value()) {
				sqlite3_bind_null(stmt, idx);
				continue;
			}

			Utils::Visit<std::nullptr_t, int64_t, double, std::string, std::vector<unsigned char>>(
				[&](const auto& v) {
					using T = std::decay_t<decltype(v)>;
					if constexpr (std::is_same_v<T, int64_t>)
						sqlite3_bind_int64(stmt, idx, v);
					else if constexpr (std::is_same_v<T, double>)
						sqlite3_bind_double(stmt, idx, v);
					else if constexpr (std::is_same_v<T, std::string>)
						sqlite3_bind_text(stmt, idx, v.c_str(), v.size(), SQLITE_TRANSIENT);
					else if constexpr (std::is_same_v<T, std::vector<unsigned char>>)
						sqlite3_bind_blob(stmt, idx, v.data(), v.size(), SQLITE_TRANSIENT);
					else if constexpr (std::is_same_v<T, std::nullptr_t>)
						sqlite3_bind_null(stmt, idx);
				}, val
			);
		}

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			impl_->lastError = sqlite3_errmsg(impl_->db);
			sqlite3_finalize(stmt);
			throw SQLAdapterQueryError("execute failed: " + impl_->lastError);
		}

		sqlite3_finalize(stmt);
		RaiseEvent("Delete executed", sql);
	}

	void SQLiteAdapter::Update(const std::string& table,
		const std::unordered_map<std::string, std::any>& set,
		const std::unordered_map<std::string, std::any>& where)
	{
		if (!impl_->db) throw SQLAdapterNotConnectedError("SQLite db is null");
		if (set.empty()) return;

		std::ostringstream setClause;
		std::vector<std::string> setKeys;
		setKeys.reserve(set.size());
		for (const auto& [k, _] : set) setKeys.push_back(k);

		for (size_t i = 0; i < setKeys.size(); ++i) {
			if (i) setClause << ',';
			setClause << setKeys[i] << "=?";
		}

		std::ostringstream whereClause;
		std::vector<std::string> whereKeys;
		whereKeys.reserve(where.size());
		for (const auto& [k, _] : where) whereKeys.push_back(k);

		for (size_t i = 0; i < whereKeys.size(); ++i) {
			if (i) whereClause << " AND ";
			whereClause << whereKeys[i] << "=?";
		}

		std::string sql = "UPDATE " + table + " SET " + setClause.str();
		if (!whereKeys.empty()) sql += " WHERE " + whereClause.str();

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			throw SQLAdapterQueryError("prepare failed: " +
				std::string(sqlite3_errmsg(impl_->db)));
		}

		size_t bindIndex = 0;

		auto bindValue = [&](const std::any& val) {
			++bindIndex;
			int idx = static_cast<int>(bindIndex);

			if (!val.has_value()) {
				sqlite3_bind_null(stmt, idx);
				return;
			}

			Utils::Visit<std::nullptr_t, int64_t, double, std::string, std::vector<unsigned char>>(
				[&](const auto& v) {
					using T = std::decay_t<decltype(v)>;
					if constexpr (std::is_same_v<T, int64_t>)
						sqlite3_bind_int64(stmt, idx, v);
					else if constexpr (std::is_same_v<T, double>)
						sqlite3_bind_double(stmt, idx, v);
					else if constexpr (std::is_same_v<T, std::string>)
						sqlite3_bind_text(stmt, idx, v.c_str(), v.size(), SQLITE_TRANSIENT);
					else if constexpr (std::is_same_v<T, std::vector<unsigned char>>)
						sqlite3_bind_blob(stmt, idx, v.data(), v.size(), SQLITE_TRANSIENT);
					else if constexpr (std::is_same_v<T, std::nullptr_t>)
						sqlite3_bind_null(stmt, idx);
				}, val
			);
			};

		for (const auto& key : setKeys) bindValue(set.at(key));

		for (const auto& key : whereKeys) bindValue(where.at(key));

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			impl_->lastError = sqlite3_errmsg(impl_->db);
			sqlite3_finalize(stmt);
			throw SQLAdapterQueryError("execute failed: " + impl_->lastError);
		}

		sqlite3_finalize(stmt);
		RaiseEvent("Update executed", sql);
	}

	SQLiteAdapter::Rows
		SQLiteAdapter::Select(const std::string& table,
			const std::vector<std::string>& columns,
			const std::unordered_map<std::string, std::any>& where,
			const std::optional<std::string>& orderBy,
			int64_t limit,
			int64_t offset)
	{
		if (!impl_->db) throw SQLAdapterNotConnectedError("SQLite db is null");

		std::string cols;
		if (columns.empty()) cols = "*";
		else {
			std::ostringstream oss;
			for (size_t i = 0; i < columns.size(); ++i) {
				if (i) oss << ',';
				oss << columns[i];
			}
			cols = oss.str();
		}

		std::ostringstream whereClause;
		std::vector<std::string> whereKeys;
		whereKeys.reserve(where.size());
		for (const auto& [k, _] : where) whereKeys.push_back(k);

		for (size_t i = 0; i < whereKeys.size(); ++i) {
			if (i) whereClause << " AND ";
			whereClause << whereKeys[i] << "=?";
		}

		std::ostringstream sql;
		sql << "SELECT " << cols << " FROM " << table;
		if (!whereClause.str().empty()) sql << " WHERE " << whereClause.str();
		if (orderBy) sql << " ORDER BY " << *orderBy;
		if (limit >= 0) {
			sql << " LIMIT ?";
			if (offset >= 0) sql << " OFFSET ?";
		}

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(impl_->db, sql.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			throw SQLAdapterQueryError("prepare failed: " +
				std::string(sqlite3_errmsg(impl_->db)));
		}

		size_t bindIndex = 0;
		auto bindOne = [&](const std::any& val) {
			++bindIndex;
			int idx = static_cast<int>(bindIndex);

			if (!val.has_value()) {
				sqlite3_bind_null(stmt, idx);
				return;
			}

			Utils::Visit<std::nullptr_t, int64_t, double, std::string, std::vector<unsigned char>>(
				[&](const auto& v) {
					using T = std::decay_t<decltype(v)>;
					if constexpr (std::is_same_v<T, int64_t>)
						sqlite3_bind_int64(stmt, idx, v);
					else if constexpr (std::is_same_v<T, double>)
						sqlite3_bind_double(stmt, idx, v);
					else if constexpr (std::is_same_v<T, std::string>)
						sqlite3_bind_text(stmt, idx, v.c_str(), v.size(), SQLITE_TRANSIENT);
					else if constexpr (std::is_same_v<T, std::vector<unsigned char>>)
						sqlite3_bind_blob(stmt, idx, v.data(), v.size(), SQLITE_TRANSIENT);
					else if constexpr (std::is_same_v<T, std::nullptr_t>)
						sqlite3_bind_null(stmt, idx);
				}, val
			);
			};

		for (const auto& key : whereKeys) bindOne(where.at(key));

		if (limit >= 0) {
			++bindIndex;
			sqlite3_bind_int64(stmt, static_cast<int>(bindIndex), limit);
			if (offset >= 0) {
				++bindIndex;
				sqlite3_bind_int64(stmt, static_cast<int>(bindIndex), offset);
			}
		}

		std::vector<std::unordered_map<std::string, std::any>> rows;
		const int colCount = sqlite3_column_count(stmt);

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			std::unordered_map<std::string, std::any> row;
			for (int i = 0; i < colCount; ++i) {
				const char* colName = sqlite3_column_name(stmt, i);
				switch (sqlite3_column_type(stmt, i)) {
				case SQLITE_NULL:
					row.emplace(colName, std::any(nullptr));
					break;
				case SQLITE_INTEGER:
					row.emplace(colName, std::any(sqlite3_column_int64(stmt, i)));
					break;
				case SQLITE_FLOAT:
					row.emplace(colName, std::any(sqlite3_column_double(stmt, i)));
					break;
				case SQLITE_TEXT: {
					const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
					int bytes = sqlite3_column_bytes(stmt, i);
					row.emplace(colName, std::any(std::string(txt, txt + bytes)));
					break;
				}
				case SQLITE_BLOB: {
					const unsigned char* blob = static_cast<const unsigned char*>(sqlite3_column_blob(stmt, i));
					int bytes = sqlite3_column_bytes(stmt, i);
					row.emplace(colName, std::any(std::vector<unsigned char>(blob, blob + bytes)));
					break;
				}
				}
			}
			rows.emplace_back(std::move(row));
		}

		sqlite3_finalize(stmt);
		RaiseEvent("Select executed", sql.str());
		return rows;
	}

	void SQLiteAdapter::CreateTable(const std::string& table,
		const std::unordered_map<std::string, std::string>& columns)
	{
		if (!impl_->db) throw SQLAdapterNotConnectedError("SQLite db is null");
		if (columns.empty()) throw SQLAdapterInvalidArgumentError("No columns provided for table creation");

		std::ostringstream cols;
		for (const auto& [colName, colType] : columns) {
			if (!cols.str().empty()) cols << ", ";
			cols << colName << " " << colType;
		}

		const std::string sql = "CREATE TABLE " + table + " (" + cols.str() + ")";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			throw SQLAdapterQueryError("prepare failed: " + std::string(sqlite3_errmsg(impl_->db)));
		}

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			impl_->lastError = sqlite3_errmsg(impl_->db);
			sqlite3_finalize(stmt);
			throw SQLAdapterQueryError("execute failed: " + impl_->lastError);
		}

		sqlite3_finalize(stmt);
		RaiseEvent("Table created", sql);
	}

	void SQLiteAdapter::RemoveTable(const std::string& table)
	{
		if (!impl_->db) throw SQLAdapterNotConnectedError("SQLite db is null");

		const std::string sql = "DROP TABLE IF EXISTS " + table;

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			throw SQLAdapterQueryError("prepare failed: " + std::string(sqlite3_errmsg(impl_->db)));
		}

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			impl_->lastError = sqlite3_errmsg(impl_->db);
			sqlite3_finalize(stmt);
			throw SQLAdapterQueryError("execute failed: " + impl_->lastError);
		}

		sqlite3_finalize(stmt);
		RaiseEvent("Table removed", sql);
	}

#ifdef HSBA_USE_MYSQL
	class MySQLAdapter::Impl
	{
	public:
		MYSQL* conn = nullptr;
		bool connected = false;
		std::string lastError;
		~Impl()
		{
			if (conn)
			{
				mysql_close(conn);
				connected = false;
				conn = nullptr;
			}
		}
		void Connect(std::string_view host, std::string_view user, std::string_view password,
			std::string_view database, unsigned int port)
		{
			conn = mysql_init(nullptr);
			if (!conn)
			{
				lastError = "mysql_init() failed";
				throw SQLAdapterConnectionError(lastError);
			}
			if (!mysql_real_connect(conn, host.data(), user.data(), password.data(), database.data(), port, nullptr, 0))
			{
				lastError = mysql_error(conn);
				mysql_close(conn);
				throw SQLAdapterConnectionError(lastError);
			}
			connected = true;
		}
		void Execute(const std::string& query)
		{
			if (!connected)
			{
				throw SQLAdapterNotConnectedError("Not connected to the database.");
			}
			if (mysql_query(conn, query.c_str()))
			{
				lastError = mysql_error(conn);
				throw SQLAdapterQueryError(lastError);
			}
		}
		Rows Query(const std::string& query)
		{
			if (!connected)
			{
				throw SQLAdapterNotConnectedError("Not connected to the database.");
			}
			if (mysql_query(conn, query.c_str()))
			{
				lastError = mysql_error(conn);
				throw SQLAdapterQueryError(lastError);
			}
			MYSQL_RES* result = mysql_store_result(conn);
			if (!result)
			{
				lastError = mysql_error(conn);
				throw SQLAdapterQueryError(lastError);
			}
			Rows rows;
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(result)))
			{
				unsigned int numFields = mysql_num_fields(result);
				std::unordered_map<std::string, std::any> rowData;
				for (unsigned int i = 0; i < numFields; ++i)
				{
					const char* fieldName = mysql_fetch_field_direct(result, i)->name;
					if (row[i] == nullptr)
					{
						rowData[fieldName] = std::any();
					}
					else
					{
						switch (mysql_fetch_field_direct(result, i)->type)
						{
						case MYSQL_TYPE_LONG:
							rowData[fieldName] = std::stoll(row[i]);
							break;
						case MYSQL_TYPE_FLOAT:
						case MYSQL_TYPE_DOUBLE:
							rowData[fieldName] = std::stod(row[i]);
							break;
						case MYSQL_TYPE_STRING:
						case MYSQL_TYPE_VAR_STRING:
							rowData[fieldName] = std::string(row[i]);
							break;
						case MYSQL_TYPE_BLOB:
							rowData[fieldName] = std::vector<unsigned char>(row[i], row[i] + mysql_fetch_field_direct(result, i)->length);
							break;
						default:
							lastError = "Unsupported column type";
							mysql_free_result(result);
							throw SQLAdapterQueryError(lastError);
						}
					}
				}
				rows.push_back(std::move(rowData));
			}
			mysql_free_result(result);
			return rows;
		}
	};
	MySQLAdapter::MySQLAdapter() : impl_(std::make_unique<Impl>()) {}
	MySQLAdapter::~MySQLAdapter() = default;
	void MySQLAdapter::Connect(std::string_view host, std::string_view user, std::string_view password,
		std::string_view database, unsigned int port)
	{
		std::lock_guard lock(mutex_);
		if (impl_->connected)
		{
			throw SQLAdapterConnectionError("Already connected to the database.");
		}
		impl_->Connect(host, user, password, database, port);
		RaiseEvent("Connected to MySQL database", std::format("Host: {}, User: {}, Database: {}, Port: {}", host, user, database, port));
	}
	void MySQLAdapter::Execute(const std::string& query)
	{
		std::lock_guard lock(mutex_);
		impl_->Execute(query);
		RaiseEvent("Execute query", query);
	}
	MySQLAdapter::Rows MySQLAdapter::Query(const std::string& query)
	{
		std::lock_guard lock(mutex_);
		auto rows = impl_->Query(query);
		RaiseEvent("Query executed", query);
		return rows;
	}

	bool MySQLAdapter::IsConnected() const noexcept
	{
		return impl_->connected;
	}

	void MySQLAdapter::Insert(const std::string& table, const std::unordered_map<std::string, std::any>& data)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->connected)
			throw SQLAdapterNotConnectedError("Not connected");

		if (data.empty()) return;

		std::ostringstream cols, ph;
		std::vector<std::string> keys;
		keys.reserve(data.size());
		for (const auto& [k, _] : data) keys.push_back(k);

		for (size_t i = 0; i < keys.size(); ++i) {
			if (i) { cols << ','; ph << ','; }
			cols << keys[i];
			ph << '?';
		}
		const std::string sql =
			"INSERT INTO " + table + " (" + cols.str() + ") VALUES (" + ph.str() + ")";

		struct StmtDeleter { void operator()(MYSQL_STMT* s) const { if (s) mysql_stmt_close(s); } };

		std::unique_ptr<MYSQL_STMT, StmtDeleter> stmt(mysql_stmt_init(impl_->conn));
		if (!stmt)
			throw SQLAdapterQueryError("mysql_stmt_init failed");
		if (!stmt)
			throw SQLAdapterQueryError("mysql_stmt_init failed");

		if (mysql_stmt_prepare(stmt.get(), sql.c_str(), sql.size())) {
			throw SQLAdapterQueryError("prepare failed: " +
				std::string(mysql_error(impl_->conn)));
		}

		std::vector<MYSQL_BIND> bind(keys.size());
		boost::container::vector<bool> is_null(keys.size());  // Use boost::container for better performance, std::vector<bool> is not suitable for this case
		std::vector<int64_t>    int_storage;
		std::vector<double>     double_storage;
		std::vector<std::string> str_storage;
		std::vector<std::vector<unsigned char>> blob_storage;
		std::vector<unsigned long> blob_lengths;

		std::memset(bind.data(), 0, sizeof(MYSQL_BIND) * bind.size());

		for (size_t i = 0; i < keys.size(); ++i) {
			const auto& value = data.at(keys[i]);
			MYSQL_BIND& b = bind[i];

			if (!value.has_value())
			{
				is_null[i] = 1;
				b.buffer_type = MYSQL_TYPE_NULL;
			}

			Utils::Visit<std::nullptr_t, std::string, int64_t, double, std::vector<unsigned char>>([&](const auto& v) {
				using T = std::decay_t<decltype(v)>;
				if constexpr (std::is_same_v<T, std::string>) {
					str_storage.emplace_back(v);
					const std::string& s = str_storage.back();
					b.buffer_type = MYSQL_TYPE_STRING;
					b.buffer = (char*)s.c_str();
					b.buffer_length = s.size();
				}
				else if constexpr (std::is_same_v<T, int64_t>) {
					int_storage.push_back(v);
					b.buffer_type = MYSQL_TYPE_LONGLONG;
					b.buffer = &int_storage.back();
				}
				else if constexpr (std::is_same_v<T, double>) {
					double_storage.push_back(v);
					b.buffer_type = MYSQL_TYPE_DOUBLE;
					b.buffer = &double_storage.back();
				}
				else if constexpr (std::is_same_v<T, std::vector<unsigned char>>) {
					blob_storage.emplace_back(v);
					const auto& blob = blob_storage.back();
					b.buffer_type = MYSQL_TYPE_BLOB;
					b.buffer = (void*)blob.data();
					b.buffer_length = blob.size();
					blob_lengths.push_back(blob.size());
					b.length = &blob_lengths.back();
				}
				else if (std::is_same_v<T, std::nullptr_t>)
				{
					is_null[i] = 1;
					b.buffer_type = MYSQL_TYPE_NULL;
				}
				}, value);
			b.is_null = &is_null[i];
		}

		if (mysql_stmt_bind_param(stmt.get(), bind.data())) {
			throw SQLAdapterQueryError("mysql_stmt_bind_param failed: " + std::string(mysql_stmt_error(stmt.get())));
		}

		if (mysql_stmt_execute(stmt.get())) {
			throw SQLAdapterQueryError("mysql_stmt_execute failed: " + std::string(mysql_stmt_error(stmt.get())));
		}
		RaiseEvent("Insert executed", sql);
	}

	void MySQLAdapter::Delete(const std::string& table,
		const std::unordered_map<std::string, std::any>& key)
	{
		if (key.empty()) return;

		std::lock_guard lock(mutex_);
		if (!impl_->connected)
			throw SQLAdapterNotConnectedError("Not connected");

		std::ostringstream where;
		std::vector<std::string> colNames;
		colNames.reserve(key.size());
		for (const auto& [k, _] : key) colNames.push_back(k);

		for (size_t i = 0; i < colNames.size(); ++i) {
			if (i) where << " AND ";
			where << colNames[i] << " = ?";
		}
		const std::string sql =
			"DELETE FROM " + table + " WHERE " + where.str();

		struct StmtDeleter { void operator()(MYSQL_STMT* s) const { if (s) mysql_stmt_close(s); } };
		std::unique_ptr<MYSQL_STMT, StmtDeleter> stmt(mysql_stmt_init(impl_->conn));
		if (!stmt)
			throw SQLAdapterQueryError("mysql_stmt_init failed");

		if (mysql_stmt_prepare(stmt.get(), sql.c_str(), sql.size()))
			throw SQLAdapterQueryError("prepare failed: " +
				std::string(mysql_error(impl_->conn)));

		std::vector<MYSQL_BIND> bind(key.size());
		boost::container::vector<bool> isNull(key.size(), false);
		std::vector<int64_t>    intBuf;
		std::vector<double>     dblBuf;
		std::vector<std::string> strBuf;
		std::vector<std::vector<unsigned char>> blobBuf;
		std::vector<unsigned long> blobLen;

		std::memset(bind.data(), 0, sizeof(MYSQL_BIND) * bind.size());

		for (size_t i = 0; i < colNames.size(); ++i) {
			const auto& v = key.at(colNames[i]);
			MYSQL_BIND& b = bind[i];

			Utils::Visit<std::nullptr_t, int64_t, double, std::string, std::vector<unsigned char>>(
				[&](const auto& val) {
					using T = std::decay_t<decltype(val)>;
					if constexpr (std::is_same_v<T, int64_t>) {
						intBuf.push_back(val);
						b.buffer_type = MYSQL_TYPE_LONGLONG;
						b.buffer = &intBuf.back();
					}
					else if constexpr (std::is_same_v<T, double>) {
						dblBuf.push_back(val);
						b.buffer_type = MYSQL_TYPE_DOUBLE;
						b.buffer = &dblBuf.back();
					}
					else if constexpr (std::is_same_v<T, std::string>) {
						strBuf.emplace_back(val);
						const std::string& s = strBuf.back();
						b.buffer_type = MYSQL_TYPE_STRING;
						b.buffer = (char*)s.c_str();
						b.buffer_length = s.size();
					}
					else if constexpr (std::is_same_v<T, std::vector<unsigned char>>) {
						blobBuf.emplace_back(val);
						const auto& blob = blobBuf.back();
						b.buffer_type = MYSQL_TYPE_BLOB;
						b.buffer = (void*)blob.data();
						b.buffer_length = blob.size();
						blobLen.push_back(blob.size());
						b.length = &blobLen.back();
					}
					else if constexpr (std::is_same_v<T, std::nullptr_t>) {
						isNull[i] = true;
						b.buffer_type = MYSQL_TYPE_NULL;
					}
				}, v);
			b.is_null = &isNull[i];
		}

		if (mysql_stmt_bind_param(stmt.get(), bind.data()))
			throw SQLAdapterQueryError("mysql_stmt_bind_param failed: " +
				std::string(mysql_stmt_error(stmt.get())));

		if (mysql_stmt_execute(stmt.get()))
			throw SQLAdapterQueryError("mysql_stmt_execute failed: " +
				std::string(mysql_stmt_error(stmt.get())));
	}

	void MySQLAdapter::Update(const std::string& table,
		const std::unordered_map<std::string, std::any>& set,
		const std::unordered_map<std::string, std::any>& where)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->connected)
			throw SQLAdapterNotConnectedError("Not connected");

		if (set.empty())
			return;

		std::vector<std::string> setKeys, whereKeys;
		setKeys.reserve(set.size());
		whereKeys.reserve(where.size());
		for (const auto& [k, _] : set)   setKeys.emplace_back(k);
		for (const auto& [k, _] : where) whereKeys.emplace_back(k);

		/* 2. 构造 SQL 语句 */
		std::ostringstream sql;
		sql << "UPDATE " << table << " SET ";
		for (size_t i = 0; i < setKeys.size(); ++i) {
			if (i) sql << ',';
			sql << setKeys[i] << "=?";
		}

		sql << " WHERE ";
		for (size_t i = 0; i < whereKeys.size(); ++i) {
			if (i) sql << " AND ";
			sql << whereKeys[i] << "=?";
		}

		struct StmtDeleter { void operator()(MYSQL_STMT* s) const { if (s) mysql_stmt_close(s); } };
		std::unique_ptr<MYSQL_STMT, StmtDeleter> stmt(mysql_stmt_init(impl_->conn));
		if (!stmt)
			throw SQLAdapterQueryError("mysql_stmt_init failed");

		if (mysql_stmt_prepare(stmt.get(), sql.str().c_str(), sql.str().size()))
			throw SQLAdapterQueryError("prepare failed: " + std::string(mysql_error(impl_->conn)));

		const size_t total = setKeys.size() + whereKeys.size();
		std::vector<MYSQL_BIND> bind(total);
		boost::container::vector<bool> is_null(total);
		std::vector<int64_t>    int_storage;
		std::vector<double>     double_storage;
		std::vector<std::string> str_storage;
		std::vector<std::vector<unsigned char>> blob_storage;
		std::vector<unsigned long> blob_lengths;

		std::memset(bind.data(), 0, sizeof(MYSQL_BIND) * total);

		auto bindOne = [&](size_t idx, const std::any& value) -> void {
			MYSQL_BIND& b = bind[idx];
			if (!value.has_value()) {
				is_null[idx] = 1;
				b.buffer_type = MYSQL_TYPE_NULL;
				b.is_null = &is_null[idx];
				return;
			}

			Utils::Visit<std::nullptr_t, std::string, int64_t, double, std::vector<unsigned char>>(
				[&](const auto& v) {
					using T = std::decay_t<decltype(v)>;
					if constexpr (std::is_same_v<T, std::string>) {
						str_storage.emplace_back(v);
						const std::string& s = str_storage.back();
						b.buffer_type = MYSQL_TYPE_STRING;
						b.buffer = (char*)s.c_str();
						b.buffer_length = static_cast<unsigned long>(s.size());
					}
					else if constexpr (std::is_same_v<T, int64_t>) {
						int_storage.push_back(v);
						b.buffer_type = MYSQL_TYPE_LONGLONG;
						b.buffer = &int_storage.back();
					}
					else if constexpr (std::is_same_v<T, double>) {
						double_storage.push_back(v);
						b.buffer_type = MYSQL_TYPE_DOUBLE;
						b.buffer = &double_storage.back();
					}
					else if constexpr (std::is_same_v<T, std::vector<unsigned char>>) {
						blob_storage.emplace_back(v);
						const auto& blob = blob_storage.back();
						b.buffer_type = MYSQL_TYPE_BLOB;
						b.buffer = (void*)blob.data();
						b.buffer_length = static_cast<unsigned long>(blob.size());
						blob_lengths.push_back(b.buffer_length);
						b.length = &blob_lengths.back();
					}
					else if constexpr (std::is_same_v<T, std::nullptr_t>) {
						is_null[idx] = 1;
						b.buffer_type = MYSQL_TYPE_NULL;
					}
				},
				value);
			b.is_null = &is_null[idx];
			};

		size_t idx = 0;
		for (const auto& k : setKeys)   bindOne(idx++, set.at(k));
		for (const auto& k : whereKeys) bindOne(idx++, where.at(k));

		if (mysql_stmt_bind_param(stmt.get(), bind.data()))
			throw SQLAdapterQueryError("mysql_stmt_bind_param failed: " +
				std::string(mysql_stmt_error(stmt.get())));

		if (mysql_stmt_execute(stmt.get()))
			throw SQLAdapterQueryError("mysql_stmt_execute failed: " +
				std::string(mysql_stmt_error(stmt.get())));

		RaiseEvent("Update executed", sql.str());
	}

	MySQLAdapter::Rows
		MySQLAdapter::Select(const std::string& table,
			const std::vector<std::string>& columns,
			const std::unordered_map<std::string, std::any>& where,
			const std::optional<std::string>& orderBy,
			int64_t limit,
			int64_t offset)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->connected)
			throw SQLAdapterNotConnectedError("Not connected");

		std::ostringstream sql;
		sql << "SELECT ";
		if (columns.empty())
			sql << '*';
		else {
			for (size_t i = 0; i < columns.size(); ++i) {
				if (i) sql << ',';
				sql << columns[i];
			}
		}
		sql << " FROM " << table;

		std::vector<std::string> whereKeys;
		whereKeys.reserve(where.size());
		for (const auto& [k, _] : where) whereKeys.emplace_back(k);

		if (!whereKeys.empty()) {
			sql << " WHERE ";
			for (size_t i = 0; i < whereKeys.size(); ++i) {
				if (i) sql << " AND ";
				sql << whereKeys[i] << "=?";
			}
		}

		if (orderBy) sql << " ORDER BY " << *orderBy;
		if (limit > 0) sql << " LIMIT " << limit;
		if (offset > 0) sql << " OFFSET " << offset;

		struct StmtDeleter { void operator()(MYSQL_STMT* s) const { if (s) mysql_stmt_close(s); } };
		std::unique_ptr<MYSQL_STMT, StmtDeleter> stmt(mysql_stmt_init(impl_->conn));
		if (!stmt)
			throw SQLAdapterQueryError("mysql_stmt_init failed");

		if (mysql_stmt_prepare(stmt.get(), sql.str().c_str(), sql.str().size()))
			throw SQLAdapterQueryError("prepare failed: " + std::string(mysql_error(impl_->conn)));

		if (!whereKeys.empty()) {
			std::vector<MYSQL_BIND> bind(whereKeys.size());
			boost::container::vector<bool>        is_null(whereKeys.size(), false);
			std::vector<int64_t>     int_storage;
			std::vector<double>      double_storage;
			std::vector<std::string> str_storage;
			std::vector<std::vector<unsigned char>> blob_storage;
			std::vector<unsigned long> blob_lengths;

			std::memset(bind.data(), 0, sizeof(MYSQL_BIND) * bind.size());

			for (size_t i = 0; i < whereKeys.size(); ++i) {
				const auto& value = where.at(whereKeys[i]);
				MYSQL_BIND& b = bind[i];

				if (!value.has_value()) {
					is_null[i] = 1;
					b.buffer_type = MYSQL_TYPE_NULL;
					b.is_null = &is_null[i];
					continue;
				}

				Utils::Visit<std::nullptr_t, std::string, int64_t, double, std::vector<unsigned char>>(
					[&](const auto& v) {
						using T = std::decay_t<decltype(v)>;
						if constexpr (std::is_same_v<T, std::string>) {
							str_storage.emplace_back(v);
							const std::string& s = str_storage.back();
							b.buffer_type = MYSQL_TYPE_STRING;
							b.buffer = (char*)s.c_str();
							b.buffer_length = static_cast<unsigned long>(s.size());
						}
						else if constexpr (std::is_same_v<T, int64_t>) {
							int_storage.push_back(v);
							b.buffer_type = MYSQL_TYPE_LONGLONG;
							b.buffer = &int_storage.back();
						}
						else if constexpr (std::is_same_v<T, double>) {
							double_storage.push_back(v);
							b.buffer_type = MYSQL_TYPE_DOUBLE;
							b.buffer = &double_storage.back();
						}
						else if constexpr (std::is_same_v<T, std::vector<unsigned char>>) {
							blob_storage.emplace_back(v);
							const auto& blob = blob_storage.back();
							b.buffer_type = MYSQL_TYPE_BLOB;
							b.buffer = (void*)blob.data();
							b.buffer_length = static_cast<unsigned long>(blob.size());
							blob_lengths.push_back(b.buffer_length);
							b.length = &blob_lengths.back();
						}
						else if constexpr (std::is_same_v<T, std::nullptr_t>) {
							is_null[i] = 1;
							b.buffer_type = MYSQL_TYPE_NULL;
						}
					},
					value);
				b.is_null = &is_null[i];
			}

			if (mysql_stmt_bind_param(stmt.get(), bind.data()))
				throw SQLAdapterQueryError("mysql_stmt_bind_param failed: " +
					std::string(mysql_stmt_error(stmt.get())));
		}

		if (mysql_stmt_execute(stmt.get()))
			throw SQLAdapterQueryError("mysql_stmt_execute failed: " +
				std::string(mysql_stmt_error(stmt.get())));

		if (mysql_stmt_store_result(stmt.get()))
			throw SQLAdapterQueryError("mysql_stmt_store_result failed: " +
				std::string(mysql_stmt_error(stmt.get())));

		MYSQL_RES* meta = mysql_stmt_result_metadata(stmt.get());
		if (!meta)
			throw SQLAdapterQueryError("mysql_stmt_result_metadata failed");

		struct MetaDeleter { void operator()(MYSQL_RES* r) const { if (r) mysql_free_result(r); } };
		std::unique_ptr<MYSQL_RES, MetaDeleter> metaGuard(meta);

		const unsigned int numFields = mysql_num_fields(meta);
		if (numFields == 0)
			return {};

		MYSQL_FIELD* fields = mysql_fetch_fields(meta);

		std::vector<MYSQL_BIND> bindOut(numFields);
		boost::container::vector<bool>       isNull(numFields);
		std::vector<unsigned long> lengths(numFields);

		std::vector<int64_t>          intBuf(numFields);
		std::vector<double>           doubleBuf(numFields);
		std::vector<std::string>      strBuf(numFields);   // 预设空串，后续 resize
		std::vector<std::vector<unsigned char>> blobBuf(numFields);

		std::memset(bindOut.data(), 0, sizeof(MYSQL_BIND) * numFields);

		for (unsigned int i = 0; i < numFields; ++i) {
			MYSQL_BIND& b = bindOut[i];
			b.length = &lengths[i];
			b.is_null = &isNull[i];
			b.error = nullptr;

			switch (fields[i].type) {
			case MYSQL_TYPE_TINY:
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_LONG:
			case MYSQL_TYPE_LONGLONG:
				b.buffer_type = MYSQL_TYPE_LONGLONG;
				b.buffer = &intBuf[i];
				break;

			case MYSQL_TYPE_FLOAT:
			case MYSQL_TYPE_DOUBLE:
				b.buffer_type = MYSQL_TYPE_DOUBLE;
				b.buffer = &doubleBuf[i];
				break;

			default:
				strBuf[i].resize(fields[i].max_length + 1, '\0');
				b.buffer_type = MYSQL_TYPE_STRING;
				b.buffer = &strBuf[i][0];
				b.buffer_length = static_cast<unsigned long>(strBuf[i].size());
			}
		}

		if (mysql_stmt_bind_result(stmt.get(), bindOut.data()))
			throw SQLAdapterQueryError("mysql_stmt_bind_result failed");

		std::vector<std::unordered_map<std::string, std::any>> result;

		while (true) {
			int ret = mysql_stmt_fetch(stmt.get());
			if (ret == MYSQL_NO_DATA) break;
			if (ret == 1)
				throw SQLAdapterQueryError("mysql_stmt_fetch failed");

			std::unordered_map<std::string, std::any> row;
			for (unsigned int i = 0; i < numFields; ++i) {
				const MYSQL_FIELD& f = fields[i];
				if (isNull[i]) {
					row[f.name] = nullptr;
					continue;
				}

				switch (fields[i].type) {
				case MYSQL_TYPE_TINY:
				case MYSQL_TYPE_SHORT:
				case MYSQL_TYPE_LONG:
				case MYSQL_TYPE_LONGLONG:
					row[f.name] = intBuf[i];
					break;

				case MYSQL_TYPE_FLOAT:
				case MYSQL_TYPE_DOUBLE:
					row[f.name] = doubleBuf[i];
					break;

				default:
					row[f.name] = std::string(strBuf[i].data(), lengths[i]);
				}
			}
			result.emplace_back(std::move(row));
		}

		mysql_stmt_free_result(stmt.get());
		RaiseEvent("Select executed", sql.str());
		return result;
	}
	void MySQLAdapter::CreateTable(const std::string& table,
		const std::unordered_map<std::string, std::string>& columns)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->connected)
			throw SQLAdapterNotConnectedError("Not connected");

		if (columns.empty())
			throw SQLAdapterQueryError("No columns provided for table creation");

		std::ostringstream sql;
		sql << "CREATE TABLE IF NOT EXISTS " << table << " (";

		for (size_t i = 0; i < columns.size(); ++i) {
			if (i > 0) sql << ", ";
			for (const auto& [colName, colType] : columns) {
				sql << colName << " " << colType;
			}
		}
		sql << ")";

		if (mysql_real_query(impl_->conn, sql.str().c_str(), sql.str().size()))
			throw SQLAdapterQueryError("CREATE TABLE failed: " +
				std::string(mysql_error(impl_->conn)));

		RaiseEvent("Table created", sql.str());
	}

	void MySQLAdapter::RemoveTable(const std::string& table)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->connected)
			throw SQLAdapterNotConnectedError("Not connected");

		std::ostringstream sql;
		sql << "DROP TABLE IF EXISTS " << table;

		if (mysql_real_query(impl_->conn, sql.str().c_str(), sql.str().size()))
			throw SQLAdapterQueryError("DROP TABLE failed: " +
				std::string(mysql_error(impl_->conn)));

		RaiseEvent("Table removed", sql.str());
	}
#endif // HSBA_USE_MYSQL

#ifdef HSBA_USE_PGSQL
	class PostgreSQLAdapter::Impl
	{
	public:
		PGconn* conn = nullptr;
		bool connected = false;
		std::string lastError;
		~Impl()
		{
			if (conn)
			{
				PQfinish(conn);
				connected = false;
				conn = nullptr;
			}
		}
		void Connect(std::string_view host, std::string_view user, std::string_view password,
			std::string_view database, unsigned int port)
		{
			conn = PQsetdbLogin(host.data(), std::to_string(port).c_str(), nullptr, nullptr, database.data(), user.data(), password.data());
			if (PQstatus(conn) != CONNECTION_OK)
			{
				lastError = PQerrorMessage(conn);
				PQfinish(conn);
				throw SQLAdapterConnectionError(lastError);
			}
			connected = true;
		}
		void Execute(const std::string& query)
		{
			if (!connected)
			{
				throw SQLAdapterNotConnectedError("Not connected to the database.");
			}
			PGresult* result = PQexec(conn, query.c_str());
			if (PQresultStatus(result) != PGRES_COMMAND_OK)
			{
				lastError = PQerrorMessage(conn);
				PQclear(result);
				throw SQLAdapterQueryError(lastError);
			}
			PQclear(result);
		}
		Rows Query(const std::string& query)
		{
			if (!connected)
			{
				throw SQLAdapterNotConnectedError("Not connected to the database.");
			}
			PGresult* result = PQexec(conn, query.c_str());
			if (PQresultStatus(result) != PGRES_TUPLES_OK)
			{
				lastError = PQerrorMessage(conn);
				PQclear(result);
				throw SQLAdapterQueryError(lastError);
			}
			Rows rows;
			int numRows = PQntuples(result);
			int numFields = PQnfields(result);
			for (int i = 0; i < numRows; ++i)
			{
				std::unordered_map<std::string, std::any> rowData;
				for (int j = 0; j < numFields; ++j)
				{
					std::string fieldName = PQfname(result, j);
					if (PQgetisnull(result, i, j))
					{
						rowData[fieldName] = std::any();
					}
					else
					{
						Oid fieldType = PQftype(result, j);
						switch (fieldType)
						{
						case PG_TYPE_INT8: // INT8
						case PG_TYPE_INT2: // INT2
						case PG_TYPE_INT4: // INT4
							rowData[fieldName] = std::stoll(PQgetvalue(result, i, j));
							break;
						case PG_TYPE_FLOAT4: // FLOAT4
						case PG_TYPE_FLOAT8: // FLOAT8
							rowData[fieldName] = std::stod(PQgetvalue(result, i, j));
							break;
						case PG_TYPE_TEXT: // TEXT
						case PG_TYPE_VARCHAR: // VARCHAR
							rowData[fieldName] = std::string(PQgetvalue(result, i, j));
							break;
						case PG_TYPE_BYTEA: // BYTEA
						{
							size_t length;
							unsigned char* value = PQunescapeBytea(
								reinterpret_cast<const unsigned char*>(PQgetvalue(result, i, j)),
								&length);
							rowData[fieldName] = std::vector<unsigned char>(value, value + length);
							PQfreemem(value);
							break;
						}
						default:
							lastError = "Unsupported column type";
							PQclear(result);
							throw SQLAdapterQueryError(lastError);
						}
					}
				}
				rows.push_back(std::move(rowData));
			}
			PQclear(result);
			return rows;
		}
	};
	PostgreSQLAdapter::PostgreSQLAdapter() : impl_(std::make_unique<Impl>()) {}
	PostgreSQLAdapter::~PostgreSQLAdapter() = default;
	void PostgreSQLAdapter::Connect(std::string_view host, std::string_view user, std::string_view password,
		std::string_view database, unsigned int port)
	{
		std::lock_guard lock(mutex_);
		if (impl_->connected)
		{
			throw SQLAdapterConnectionError("Already connected to the database.");
		}
		impl_->Connect(host, user, password, database, port);
		RaiseEvent("Connected to PostgreSQL database", std::format("Host: {}, User: {}, Database: {}, Port: {}", host, user, database, port));
	}
	void PostgreSQLAdapter::Execute(const std::string& query)
	{
		std::lock_guard lock(mutex_);
		impl_->Execute(query);
		RaiseEvent("Execute query", query);
	}
	PostgreSQLAdapter::Rows PostgreSQLAdapter::Query(const std::string& query)
	{
		std::lock_guard lock(mutex_);
		auto rows = impl_->Query(query);
		RaiseEvent("Query executed", query);
		return rows;
	}
	bool PostgreSQLAdapter::IsConnected() const noexcept
	{
		return impl_->connected;
	}
	namespace
	{
		std::string to_pg_literal(const std::any& value)
		{
			if (!value.has_value())
			{
				return "NULL";
			}
			return Utils::Visit<std::nullopt_t, int64_t, double, std::string, std::vector<unsigned char>>([](const auto& v) -> std::string {
				using T = std::decay_t<decltype(v)>;

				if constexpr (std::is_same_v<T, std::nullptr_t>) {
					return "NULL";
				}
				else if constexpr (std::is_same_v<T, int64_t>) {
					return std::to_string(v);
				}
				else if constexpr (std::is_same_v<T, double>) {
					return std::to_string(v);
				}
				else if constexpr (std::is_same_v<T, std::string>) {
					std::string s = v;
					std::string out;
					out.reserve(s.size() + 2);
					out.push_back('\'');
					for (char c : s) {
						if (c == '\'') out.push_back('\'');
						out.push_back(c);
					}
					out.push_back('\'');
					return out;
				}
				else if constexpr (std::is_same_v<T, std::vector<unsigned char>>)
				{
					const auto& bytes = v;
					std::ostringstream os;
					os << "E'\\\\x" << std::hex << std::setfill('0');
					for (unsigned char b : bytes) os << std::setw(2) << +b;
					os << "'";
					return os.str();
				}
				else
				{
					throw SQLAdapterQueryError("Unsupported type in to_pg_literal");
				}
				}, value);
		}
	}
	void PostgreSQLAdapter::Insert(const std::string& table, const std::unordered_map<std::string, std::any>& data)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->conn || PQstatus(impl_->conn) != CONNECTION_OK)
			throw SQLAdapterNotConnectedError("PostgreSQL connection not ready");

		if (data.empty())
			throw SQLAdapterQueryError("Insert data is empty");

		std::ostringstream cols, vals;
		bool first = true;
		for (const auto& [k, v] : data) {
			if (!first) { cols << ','; vals << ','; }
			first = false;

			cols << k;
			vals << to_pg_literal(v);
		}

		std::string sql =
			"INSERT INTO " + table + " (" + cols.str() + ") VALUES (" + vals.str() + ");";

		PGresult* res = PQexec(impl_->conn, sql.c_str());
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			impl_->lastError = PQerrorMessage(impl_->conn);
			PQclear(res);
			throw SQLAdapterQueryError(std::string("Insert failed: ") + impl_->lastError);
		}
		PQclear(res);
		RaiseEvent("Insert executed", sql);
	}
	void PostgreSQLAdapter::Delete(const std::string& table, const std::unordered_map<std::string, std::any>& data)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->conn || PQstatus(impl_->conn) != CONNECTION_OK)
			throw SQLAdapterNotConnectedError("PostgreSQL connection not ready");

		std::ostringstream sql;
		sql << "DELETE FROM " << table;

		if (!data.empty()) {
			sql << " WHERE ";
			bool first = true;
			for (const auto& [k, v] : data) {
				if (!first) sql << " AND ";
				first = false;

				sql << k << " = " << to_pg_literal(v);
			}
		}
		sql << ';';

		PGresult* res = PQexec(impl_->conn, sql.str().c_str());
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			impl_->lastError = PQerrorMessage(impl_->conn);
			PQclear(res);
			throw SQLAdapterQueryError(std::string("Delete failed: ") + impl_->lastError);
		}
		PQclear(res);
		RaiseEvent("Delete executed", sql.str());
	}
	void PostgreSQLAdapter::Update(
		const std::string& table,
		const std::unordered_map<std::string, std::any>& set,
		const std::unordered_map<std::string, std::any>& where)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->conn || PQstatus(impl_->conn) != CONNECTION_OK)
			throw SQLAdapterNotConnectedError("PostgreSQL connection not ready");

		if (set.empty())
			throw SQLAdapterQueryError("Update SET clause is empty");

		std::ostringstream sql;
		sql << "UPDATE " << table << " SET ";

		/* ---- build SET clause ---- */
		bool first = true;
		for (const auto& [col, val] : set) {
			if (!first) sql << ',';
			first = false;
			sql << col << " = " << to_pg_literal(val);
		}

		/* ---- build WHERE clause ---- */
		if (!where.empty()) {
			sql << " WHERE ";
			first = true;
			for (const auto& [col, val] : where) {
				if (!first) sql << " AND ";
				first = false;
				sql << col << " = " << to_pg_literal(val);
			}
		}

		sql << ';';

		PGresult* res = PQexec(impl_->conn, sql.str().c_str());
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			impl_->lastError = PQerrorMessage(impl_->conn);
			PQclear(res);
			throw SQLAdapterQueryError(std::string("Update failed: ") + impl_->lastError);
		}
		PQclear(res);
		RaiseEvent("Update executed", sql.str());
	}
	PostgreSQLAdapter::Rows
		PostgreSQLAdapter::Select(const std::string& table,
			const std::vector<std::string>& columns,
			const std::unordered_map<std::string, std::any>& where,
			const std::optional<std::string>& orderBy,
			int64_t limit,
			int64_t offset)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->conn || PQstatus(impl_->conn) != CONNECTION_OK)
			throw SQLAdapterNotConnectedError("PostgreSQL connection not ready");

		std::ostringstream sql;
		sql << "SELECT ";

		/* ---- build column list ---- */
		if (columns.empty()) {
			sql << '*';
		}
		else {
			bool first = true;
			for (const auto& col : columns) {
				if (!first) sql << ',';
				first = false;
				sql << col;
			}
		}

		sql << " FROM " << table;

		/* ---- build WHERE clause ---- */
		if (!where.empty()) {
			sql << " WHERE ";
			bool first = true;
			for (const auto& [col, val] : where) {
				if (!first) sql << " AND ";
				first = false;
				sql << col << " = " << to_pg_literal(val);
			}
		}

		/* ---- ORDER BY ---- */
		if (orderBy.has_value() && !orderBy->empty()) {
			sql << " ORDER BY " << *orderBy;
		}

		/* ---- LIMIT / OFFSET ---- */
		if (limit >= 0) {
			sql << " LIMIT " << limit;
		}
		if (offset > 0) {
			sql << " OFFSET " << offset;
		}

		sql << ';';

		PGresult* res = PQexec(impl_->conn, sql.str().c_str());
		if (PQresultStatus(res) != PGRES_TUPLES_OK) {
			impl_->lastError = PQerrorMessage(impl_->conn);
			PQclear(res);
			throw SQLAdapterQueryError(std::string("Select failed: ") + impl_->lastError);
		}

		const int nCols = PQnfields(res);
		const int nRows = PQntuples(res);

		std::vector<std::unordered_map<std::string, std::any>> result;
		result.reserve(nRows);

		for (int r = 0; r < nRows; ++r) {
			std::unordered_map<std::string, std::any> row;
			for (int c = 0; c < nCols; ++c) {
				const char* colName = PQfname(res, c);
				if (PQgetisnull(res, r, c)) {
					row.emplace(colName, nullptr);
					continue;
				}
				const char* val = PQgetvalue(res, r, c);
				const Oid oid = PQftype(res, c);

				switch (oid) {
				case PG_TYPE_INT8:   row.emplace(colName, std::stoll(val)); break;          // int8 / bigint
				case PG_TYPE_INT2:   row.emplace(colName, static_cast<int16_t>(std::stoi(val))); break; // int2
				case PG_TYPE_INT4:   row.emplace(colName, std::stoi(val)); break;           // int4
				case PG_TYPE_FLOAT4:  row.emplace(colName, std::stof(val)); break;           // float4
				case PG_TYPE_FLOAT8:  row.emplace(colName, std::stod(val)); break;           // float8
				case PG_TYPE_BYTEA: {                                                    // bytea
					size_t len = 0;
					unsigned char* data = PQunescapeBytea(reinterpret_cast<const unsigned char*>(val), &len);
					std::vector<unsigned char> bytes(data, data + len);
					PQfreemem(data);
					row.emplace(colName, bytes);
					break;
				}
				default:   row.emplace(colName, std::string(val)); break;        // text, varchar, etc.
				case PG_TYPE_TEXT: [[fallthrough]];         // text
				case PG_TYPE_VARCHAR:   row.emplace(colName, std::string(val)); break;        // varchar
				}
			}
			result.emplace_back(std::move(row));
		}

		PQclear(res);
		RaiseEvent("Select executed", sql.str());
		return result;
	}
	void PostgreSQLAdapter::CreateTable(
		const std::string& table,
		const std::unordered_map<std::string, std::string>& columns)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->conn || PQstatus(impl_->conn) != CONNECTION_OK)
			throw SQLAdapterNotConnectedError("PostgreSQL connection not ready");

		if (columns.empty())
			throw SQLAdapterQueryError("CreateTable: No columns specified");

		std::ostringstream sql;
		sql << "CREATE TABLE " << table << " (";

		bool first = true;
		for (const auto& [colName, colType] : columns) {
			if (!first) sql << ", ";
			first = false;
			sql << colName << " " << colType;
		}
		sql << ");";

		PGresult* res = PQexec(impl_->conn, sql.str().c_str());
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			std::string err = PQerrorMessage(impl_->conn);
			PQclear(res);
			throw SQLAdapterQueryError(std::string("CreateTable failed: ") + impl_->lastError);
		}
		PQclear(res);
		RaiseEvent("CreateTable executed", sql.str());
	}
	void PostgreSQLAdapter::RemoveTable(const std::string& table)
	{
		std::lock_guard lock(mutex_);
		if (!impl_->conn || PQstatus(impl_->conn) != CONNECTION_OK)
			throw SQLAdapterNotConnectedError("PostgreSQL connection not ready");

		std::ostringstream sql;
		sql << "DROP TABLE IF EXISTS " << table << ";";

		PGresult* res = PQexec(impl_->conn, sql.str().c_str());
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			impl_->lastError = PQerrorMessage(impl_->conn);
			PQclear(res);
			throw SQLAdapterQueryError(std::string("RemoveTable failed: ") + impl_->lastError);
		}
		PQclear(res);
		RaiseEvent("RemoveTable executed", sql.str());
	}
#endif // HSBA_USE_PGSQL
}