# pragma once
# ifndef HSBA_SQL_ADAPTER_HPP

# define HSBA_SQL_ADAPTER_HPP

#include <utility>
#include <any>
#include <unordered_map>
#include <shared_mutex>
#include <string_view>
#include <optional>
#include <vector>
#include <memory>

#include "base/delegate.hpp"
#include "base/error.hpp"

namespace HsBa::Slicer::SQL
{
	class ISQLAdapter
	{
	public:
		using Rows = std::vector<std::unordered_map<std::string, std::any>>;
		virtual ~ISQLAdapter() = default;
		virtual void Connect(std::string_view host, std::string_view user, std::string_view password,
			std::string_view database, unsigned int port = 3306) = 0;
		virtual void Execute(const std::string& query) = 0;
		virtual Rows Query(const std::string& query) = 0;
		virtual bool IsConnected() const noexcept = 0;
		virtual void Insert(const std::string& table, const std::unordered_map<std::string, std::any>& data) = 0;
		virtual void Delete(const std::string& table, const std::unordered_map<std::string, std::any>& data) = 0;
		virtual void Update(const std::string& table, const std::unordered_map<std::string, std::any>& set,
			const std::unordered_map<std::string, std::any>& where) = 0;
		virtual Rows Select(const std::string& table,
			const std::vector<std::string>& columns,
			const std::unordered_map<std::string, std::any>& where,
			const std::optional<std::string>& orderBy,
			int64_t limit,
			int64_t offset) = 0;
		virtual void CreateTable(
			const std::string& table,
			const std::unordered_map<std::string, std::string>& columns) = 0;
		virtual void RemoveTable(const std::string& table) = 0;
		ISQLAdapter() = default;
		ISQLAdapter(const ISQLAdapter&) = delete;
		ISQLAdapter& operator=(const ISQLAdapter&) = delete;
		ISQLAdapter(ISQLAdapter&&) = default;
		ISQLAdapter& operator=(ISQLAdapter&&) = default;
	};

	class SQLAdapterError : public HsBa::Slicer::IOError
	{
	public:
		explicit SQLAdapterError(const std::string& message) : IOError(message) {}
		explicit SQLAdapterError(std::string&& message) : IOError(std::move(message)) {}
		~SQLAdapterError() override = default;
	};

	class SQLAdapterNotConnectedError : public SQLAdapterError
	{
	public:
		explicit SQLAdapterNotConnectedError(const std::string& message) : SQLAdapterError(message) {}
		explicit SQLAdapterNotConnectedError(std::string&& message) : SQLAdapterError(std::move(message)) {}
		~SQLAdapterNotConnectedError() override = default;
	};
	class SQLAdapterQueryError : public SQLAdapterError
	{
	public:
		explicit SQLAdapterQueryError(const std::string& message) : SQLAdapterError(message) {}
		explicit SQLAdapterQueryError(std::string&& message) : SQLAdapterError(std::move(message)) {}
		~SQLAdapterQueryError() override = default;
	};

	class SQLAdapterConnectionError : public SQLAdapterError
	{
	public:
		explicit SQLAdapterConnectionError(const std::string& message) : SQLAdapterError(message) {}
		explicit SQLAdapterConnectionError(std::string&& message) : SQLAdapterError(std::move(message)) {}
		~SQLAdapterConnectionError() override = default;
	};

	class SQLAdapterTimeoutError : public SQLAdapterError
	{
	public:
		explicit SQLAdapterTimeoutError(const std::string& message) : SQLAdapterError(message) {}
		explicit SQLAdapterTimeoutError(std::string&& message) : SQLAdapterError(std::move(message)) {}
		~SQLAdapterTimeoutError() override = default;
	};

	class SQLAdapterPermissionDeniedError : public SQLAdapterError
	{
	public:
		explicit SQLAdapterPermissionDeniedError(const std::string& message) : SQLAdapterError(message) {}
		explicit SQLAdapterPermissionDeniedError(std::string&& message) : SQLAdapterError(std::move(message)) {}
		~SQLAdapterPermissionDeniedError() override = default;
	};

	class SQLAdapterInvalidArgumentError : public SQLAdapterError
	{
	public:
		explicit SQLAdapterInvalidArgumentError(const std::string& message) : SQLAdapterError(message) {}
		explicit SQLAdapterInvalidArgumentError(std::string&& message) : SQLAdapterError(std::move(message)) {}
		~SQLAdapterInvalidArgumentError() override = default;
	};

	class SQLiteAdapter : public ISQLAdapter, public Utils::EventSource<SQLiteAdapter, void, std::string_view, std::string_view>
	{
	public:
		SQLiteAdapter();
		void Connect(std::string_view path);
		void Connect(std::string_view host, std::string_view user, std::string_view password,
			std::string_view database, unsigned int port = 3306) override;
		void Execute(const std::string& query) override;
		Rows Query(const std::string& query) override;
		bool IsConnected() const noexcept override;
		void Insert(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
		void Delete(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
		void Update(const std::string& table, const std::unordered_map<std::string, std::any>& set,
			const std::unordered_map<std::string, std::any>& where) override;
		Rows Select(const std::string& table,
			const std::vector<std::string>& columns,
			const std::unordered_map<std::string, std::any>& where,
			const std::optional<std::string>& orderBy,
			int64_t limit,
			int64_t offset) override;
		void CreateTable(
			const std::string& table,
			const std::unordered_map<std::string, std::string>& columns) override;
		void RemoveTable(const std::string& table) override;
		~SQLiteAdapter() override;
	private:
		std::shared_mutex mutex_;
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

#ifdef USE_MYSQL
	class MySQLAdapter : public ISQLAdapter, public Utils::EventSource<SQLiteAdapter, void, std::string_view, std::string_view>
	{
	public:
		MySQLAdapter();
		void Connect(std::string_view host, std::string_view user, std::string_view password,
			std::string_view database, unsigned int port = 3306) override;
		void Execute(const std::string& query) override;
		Rows Query(const std::string& query) override;
		bool IsConnected() const noexcept override;
		void Insert(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
		void Delete(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
		void Update(const std::string& table, const std::unordered_map<std::string, std::any>& set,
			const std::unordered_map<std::string, std::any>& where) override;
		Rows Select(const std::string& table,
			const std::vector<std::string>& columns,
			const std::unordered_map<std::string, std::any>& where,
			const std::optional<std::string>& orderBy,
			int64_t limit,
			int64_t offset) override;
		void CreateTable(
			const std::string& table,
			const std::unordered_map<std::string, std::string>& columns) override;
		void RemoveTable(const std::string& table) override;
		~MySQLAdapter() override;
	private:
		std::shared_mutex mutex_;
		class Impl;
		std::unique_ptr<Impl> impl_;
	};
#endif // USE_MYSQL

#ifdef USE_PGSQL
	class PostgreSQLAdapter : public ISQLAdapter, public HsBa::Slicer::Utils::EventSource<SQLiteAdapter, void, std::string_view, std::string_view>
	{
	public:
		PostgreSQLAdapter();
		void Connect(std::string_view host, std::string_view user, std::string_view password,
			std::string_view database, unsigned int port = 5432) override;
		void Execute(const std::string& query) override;
		Rows Query(const std::string& query) override;
		bool IsConnected() const noexcept override;
		void Insert(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
		void Delete(const std::string& table, const std::unordered_map<std::string, std::any>& data) override;
		void Update(const std::string& table, const std::unordered_map<std::string, std::any>& set,
			const std::unordered_map<std::string, std::any>& where) override;
		Rows Select(const std::string& table,
			const std::vector<std::string>& columns,
			const std::unordered_map<std::string, std::any>& where,
			const std::optional<std::string>& orderBy,
			int64_t limit,
			int64_t offset) override;
		void CreateTable(
			const std::string& table,
			const std::unordered_map<std::string, std::string>& columns) override;
		void RemoveTable(const std::string& table) override;
		~PostgreSQLAdapter() override;
	private:
		std::shared_mutex mutex_;
		class Impl;
		std::unique_ptr<Impl> impl_;
	};
#endif // USE_PGSQL

	inline ISQLAdapter::Rows operator|(ISQLAdapter& db, const std::string& sql)
	{
		return db.Query(sql);
	}
	inline void operator|(ISQLAdapter&& db, const std::string&) = delete;

	template <std::invocable<ISQLAdapter&> F>
	inline auto operator|(ISQLAdapter& db, F&& consumer)
	{
		return std::forward<F>(consumer)(db);
	}

	template <class F>
	inline auto operator|(ISQLAdapter&& db, F&&) = delete;

	struct SQLSelect
	{
		std::string table;
		std::vector<std::string> columns;
		std::unordered_map<std::string, std::any> where;
		std::optional<std::string> orderBy;
		int64_t limit = -1;
		int64_t offset = 0;
		SQLSelect(std::string&& t, std::vector<std::string>&& c,
			std::unordered_map<std::string, std::any>&& w = {},
			std::optional<std::string>&& o = std::nullopt,
			int64_t l = -1, int64_t off = 0)
			: table(std::move(t)), columns(std::move(c)), where(std::move(w)),
			  orderBy(std::move(o)), limit(l), offset(off) {
		}
		SQLSelect(const std::string& t, const std::vector<std::string>& c,
			const std::unordered_map<std::string, std::any>& w = {},
			const std::optional<std::string>& o = std::nullopt,
			int64_t l = -1, int64_t off = 0)
			: table(t), columns(c), where(w), orderBy(o), limit(l), offset(off) {
		}
	};

	inline ISQLAdapter::Rows operator|(ISQLAdapter& db, const SQLSelect& select)
	{
		return db.Select(select.table, select.columns, select.where, select.orderBy, select.limit, select.offset);
	}

	inline void operator|(ISQLAdapter&& db, const SQLSelect&) = delete;

	struct SQLInsert
	{
		std::string table;
		std::unordered_map<std::string, std::any> data;
		SQLInsert(std::string&& t, std::unordered_map<std::string, std::any>&& d)
			: table(std::move(t)), data(std::move(d)) {
		}
		SQLInsert(const std::string& t,const std::unordered_map<std::string, std::any>& d)
			: table(t), data(d) {
		}
	};
	inline auto& operator|(ISQLAdapter& db, const SQLInsert& insert)
	{
		db.Insert(insert.table, insert.data);
		return db;
	}
	inline void operator|(ISQLAdapter&& db, const SQLInsert& insert) = delete;
	struct SQLDelete
	{
		std::string table;
		std::unordered_map<std::string, std::any> data;
		SQLDelete(std::string&& t, std::unordered_map<std::string, std::any>&& d)
			: table(std::move(t)), data(std::move(d)) {
		}
		SQLDelete(const std::string& t, const std::unordered_map<std::string, std::any>& d)
			: table(t), data(d) {
		}
	};

	inline auto& operator|(ISQLAdapter& db, const SQLDelete& del)
	{
		db.Delete(del.table, del.data);
		return db;
	}

	inline void operator|(ISQLAdapter&& db, const SQLDelete& del) = delete;

	struct SQLUpdate
	{
		std::string table;
		std::unordered_map<std::string, std::any> set;
		std::unordered_map<std::string, std::any> where;
		SQLUpdate(std::string&& t, std::unordered_map<std::string, std::any>&& s,
			std::unordered_map<std::string, std::any> w)
			: table(std::move(t)), set(std::move(s)), where(std::move(w)) {
		}
		SQLUpdate(const std::string& t, const std::unordered_map<std::string, std::any>& s,
			const std::unordered_map<std::string, std::any>& w)
			: table(t), set(s), where(w) {
		}
	};

	inline auto& operator|(ISQLAdapter& db, const SQLUpdate& update)
	{
		db.Update(update.table, update.set, update.where);
		return db;
	}

	inline void operator|(ISQLAdapter&& db, const SQLUpdate& update) = delete;

	struct SQLCreateTable
	{
		std::string table;
		std::unordered_map<std::string, std::string> columns;
		SQLCreateTable(std::string&& t, std::unordered_map<std::string, std::string>&& c)
			: table(std::move(t)), columns(std::move(c)) {
		}
		SQLCreateTable(const std::string& t, const std::unordered_map<std::string, std::string>& c)
			: table(t), columns(c) {
		}
	};
	inline void operator|(ISQLAdapter& db, const SQLCreateTable& create)
	{
		db.CreateTable(create.table, create.columns);
	}
	inline void operator|(ISQLAdapter&& db, const SQLCreateTable& create) = delete;

	struct SQLRemoveTable
	{
		std::string table;
		explicit SQLRemoveTable(std::string&& t) : table(std::move(t)) {}
		explicit SQLRemoveTable(const std::string& t) : table(t) {}
	};
	inline void operator|(ISQLAdapter& db, const SQLRemoveTable& remove)
	{
		db.RemoveTable(remove.table);
	}
	inline void operator|(ISQLAdapter&& db, const SQLRemoveTable& remove) = delete;
}

#endif // !HSBA_SQL_ADAPTER_HPP