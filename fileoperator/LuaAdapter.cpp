#include "LuaAdapter.hpp"
#include <format>

namespace HsBa::Slicer
{
	void PushAnyToLua(lua_State* L, const std::any& value)
	{
		if (!value.has_value())
		{
			lua_pushnil(L);
			return;
		}
		if (value.type() == typeid(int))
		{
			lua_pushinteger(L, std::any_cast<int>(value));
		}
		else if (value.type() == typeid(int64_t))
		{
			lua_pushinteger(L, static_cast<lua_Integer>(std::any_cast<int64_t>(value)));
		}
		else if (value.type() == typeid(double))
		{
			lua_pushnumber(L, std::any_cast<double>(value));
		}
		else if (value.type() == typeid(std::string))
		{
			lua_pushstring(L, std::any_cast<std::string>(value).c_str());
		}
		else if (value.type() == typeid(std::vector<unsigned char>))
		{
			const auto& vec = std::any_cast<std::vector<unsigned char>>(value);
			lua_pushlstring(L, reinterpret_cast<const char*>(vec.data()), vec.size());
		}
		else
		{
			lua_pushnil(L); // unsupported type
		}
	}
	namespace
	{
		// ============= Zipper Wrapper =============
		int lua_zipper_new(lua_State* L)
		{
			NewLuaObject<Zipper>(L, "Zipper");
			return 1;
		}

		int lua_zipper_add_file(lua_State* L)
		{
			auto* zipper = (Zipper*)lua_topointer(L, 1);
			std::string name = luaL_checkstring(L, 2);
			std::string path = luaL_checkstring(L, 3);
			if (!zipper)
			{
				lua_pushstring(L, "Invalid Zipper object");
				return lua_error(L);
			}
			try
			{
				zipper->AddFile(name, path);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_zipper_add_byte_file(lua_State* L)
		{
			auto* zipper = (Zipper*)lua_topointer(L, 1);
			std::string name = luaL_checkstring(L, 2);
			std::string data = luaL_checkstring(L, 3);
			if (!zipper)
			{
				lua_pushstring(L, "Invalid Zipper object");
				return lua_error(L);
			}
			try
			{
				zipper->AddByteFile(name, data);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_zipper_save(lua_State* L)
		{
			auto* zipper = (Zipper*)lua_topointer(L, 1);
			std::string path = luaL_checkstring(L, 2);
			if (!zipper)
			{
				lua_pushstring(L, "Invalid Zipper object");
				return lua_error(L);
			}
			try
			{
				zipper->Save(path);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_zipper_gc(lua_State* L)
		{
			LuaGC<Zipper>(L);
			return 0;
		}

		static const luaL_Reg zipperLib[] =
		{
			{"new", lua_zipper_new},
			{"AddFile", lua_zipper_add_file},
			{"AddByteFile", lua_zipper_add_byte_file},
			{"Save", lua_zipper_save},
			{"__gc", lua_zipper_gc},
			{NULL, NULL}
		};


#ifdef USE_BIT7Z
		// ============= Bit7zZipper Wrapper =============
		int lua_bit7z_zipper_new(lua_State* L)
		{
			//first arg: format
			luaL_checktype(L, 1, LUA_TSTRING);
			std::string format_str = luaL_checkstring(L, 1);
			ZipperFormat zipper_format = ZipperFormat::Undefine;
			if (format_str == "Zip") zipper_format = ZipperFormat::Zip;
			else if (format_str == "SevenZip") zipper_format = ZipperFormat::SevenZip;
			else if (format_str == "XZ") zipper_format = ZipperFormat::XZ;
			else if (format_str == "BZIP2") zipper_format = ZipperFormat::BZIP2;
			else if (format_str == "GZIP") zipper_format = ZipperFormat::GZIP;
			else if (format_str == "TAR") zipper_format = ZipperFormat::TAR;
			else
			{
				lua_pushstring(L, "Unsupported Bit7z Zipper format");
				return lua_error(L);
			}
			//second arg: dll_path
			std::string dll_path = HSBA_7Z_DLL;
			if (lua_gettop(L) >= 2 && lua_isstring(L, 2))
			{
				dll_path = luaL_checkstring(L, 2);
			}
			//third arg: password (optional)
			std::string password = "";
			if (lua_gettop(L) >= 3 && lua_isstring(L, 3))
			{
				password = luaL_checkstring(L, 3);
			}
			auto* zipper = new Bit7zZipper(dll_path, zipper_format, password);
			lua_pushlightuserdata(L, zipper);
			NewLuaObject<Bit7zZipper>(L, "Bit7zZipper", dll_path, zipper_format, password);
			return 0;
		}

		int lua_bit7z_zipper_add_file(lua_State* L)
		{
			auto* zipper = (Bit7zZipper*)lua_topointer(L, 1);
			std::string name = luaL_checkstring(L, 2);
			std::string path = luaL_checkstring(L, 3);
			if (!zipper)
			{
				lua_pushstring(L, "Invalid Bit7zZipper object");
				return lua_error(L);
			}
			try
			{
				zipper->AddFile(name, path);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_bit7z_zipper_add_byte_file(lua_State* L)
		{
			auto* zipper = (Bit7zZipper*)lua_topointer(L, 1);
			std::string name = luaL_checkstring(L, 2);
			size_t len;
			const char* data = luaL_checklstring(L, 3, &len);
			if (!zipper)
			{
				lua_pushstring(L, "Invalid Bit7zZipper object");
				return lua_error(L);
			}
			try
			{
				zipper->AddByteFile(name, std::vector<unsigned char>(data, data + len));
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_bit7z_zipper_save(lua_State* L)
		{
			auto* zipper = (Bit7zZipper*)lua_topointer(L, 1);
			std::string path = luaL_checkstring(L, 2);
			if (!zipper)
			{
				lua_pushstring(L, "Invalid Bit7zZipper object");
				return lua_error(L);
			}
			try
			{
				zipper->Save(path);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_bit7z_zipper_gc(lua_State* L)
		{
			LuaGC<Bit7zZipper>(L);
			return 0;
		}

		static const luaL_Reg bit7z_zipperLib[] =
		{
			{"new", lua_bit7z_zipper_new},
			{"AddFile", lua_bit7z_zipper_add_file},
			{"AddByteFile", lua_bit7z_zipper_add_byte_file},
			{"Save", lua_bit7z_zipper_save},
			{"__gc", lua_bit7z_zipper_gc},
			{NULL, NULL}
		};

#endif // USE_BIT7Z

		// ============= SQLiteAdapter Wrapper =============
		int lua_sqlite_new(lua_State* L)
		{
			NewLuaObject<SQL::SQLiteAdapter>(L, "SQLiteAdapter");
			return 1;
		}

		int lua_sqlite_connect(lua_State* L)
		{
			auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
			std::string path = luaL_checkstring(L, 2);
			if (!db)
			{
				lua_pushstring(L, "Invalid SQLiteAdapter object");
				return lua_error(L);
			}
			try
			{
				db->Connect(path);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_sqlite_execute(lua_State* L)
		{
			auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
			std::string query = luaL_checkstring(L, 2);
			if (!db)
			{
				lua_pushstring(L, "Invalid SQLiteAdapter object");
				return lua_error(L);
			}
			try
			{
				db->Execute(query);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_sqlite_query(lua_State* L)
		{
			auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
			std::string query = luaL_checkstring(L, 2);
			if (!db)
			{
				lua_pushstring(L, "Invalid SQLiteAdapter object");
				return lua_error(L);
			}
			try {
				auto rows = db->Query(query);
				lua_createtable(L, rows.size(), 0);
				for (size_t i = 0; i < rows.size(); ++i)
				{
					lua_createtable(L, 0, rows[i].size());
					for (const auto& [key, val] : rows[i])
					{
						PushAnyToLua(L, val);
						lua_setfield(L, -2, key.c_str());
					}
					lua_rawseti(L, -2, i + 1);
				}
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_sqlite_insert(lua_State* L)
		{
			auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid SQLiteAdapter object");
				return lua_error(L);
			}
			try
			{
				std::unordered_map<std::string, std::any> data;
				lua_pushnil(L);
				while (lua_next(L, 3))
				{
					std::string key = lua_tostring(L, -2);
					std::any val;
					if (lua_isnumber(L, -1)) val = (int64_t)lua_tointeger(L, -1);
					else if (lua_isstring(L, -1)) val = std::string(lua_tostring(L, -1));
					else if (lua_isboolean(L, -1)) val = (int64_t)(lua_toboolean(L, -1) ? 1 : 0);
					data[key] = val;
					lua_pop(L, 1);
				}
				db->Insert(table, data);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_sqlite_update(lua_State* L)
		{
			auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			luaL_checktype(L, 4, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid SQLiteAdapter object");
				return lua_error(L);
			}
			try
			{
				auto parse_table = [](lua_State* L, int idx)->std::unordered_map<std::string, std::any> {
					std::unordered_map<std::string, std::any> result;
					lua_pushnil(L);
					while (lua_next(L, idx))
					{
						std::string key = lua_tostring(L, -2);
						std::any val;
						if (lua_isnumber(L, -1)) val = (int64_t)lua_tointeger(L, -1);
						else if (lua_isstring(L, -1)) val = std::string(lua_tostring(L, -1));
						result[key] = val;
						lua_pop(L, 1);
					}
					return result;
					};
				auto set_data = parse_table(L, 3);
				auto where_data = parse_table(L, 4);
				db->Update(table, set_data, where_data);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_sqlite_delete(lua_State* L)
		{
			auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid SQLiteAdapter object");
				return lua_error(L);
			}
			try
			{
				std::unordered_map<std::string, std::any> data;
				lua_pushnil(L);
				while (lua_next(L, 3))
				{
					std::string key = lua_tostring(L, -2);
					std::any val;
					if (lua_isnumber(L, -1)) val = (int64_t)lua_tointeger(L, -1);
					else if (lua_isstring(L, -1)) val = std::string(lua_tostring(L, -1));
					data[key] = val;
					lua_pop(L, 1);
				}
				db->Delete(table, data);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_sqlite_create_table(lua_State* L)
		{
			auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid SQLiteAdapter object");
				return lua_error(L);
			}
			try
			{
				std::unordered_map<std::string, std::string> columns;
				lua_pushnil(L);
				while (lua_next(L, 3))
				{
					std::string key = lua_tostring(L, -2);
					std::string col_type = lua_tostring(L, -1);
					columns[key] = col_type;
					lua_pop(L, 1);
				}
				db->CreateTable(table, columns);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}

		int lua_sqlite_close(lua_State* L)
		{
			auto* db = static_cast<SQL::SQLiteAdapter*>(
				luaL_checkudata(L, 1, "SQLiteAdapter"));
			db->~SQLiteAdapter();
			return 0;
		}

		int lua_sqlite_gc(lua_State* L)
		{
			LuaGC<SQL::SQLiteAdapter>(L);
			return 0;
		}

		static const luaL_Reg sqlite_adapter_methods[] =
		{
			{"Connect", lua_sqlite_connect},
			{"Execute", lua_sqlite_execute},
			{"Query", lua_sqlite_query},
			{"Insert", lua_sqlite_insert},
			{"Update", lua_sqlite_update},
			{"Delete", lua_sqlite_delete},
			{"CreateTable", lua_sqlite_create_table},
			{"Close", lua_sqlite_close},
			{"__gc", lua_sqlite_gc},
			{nullptr, nullptr}
		};

#ifdef USE_MYSQL
		// ============= MySQLAdapter Wrapper =============
		int lua_mysql_new(lua_State* L)
		{
			NewLuaObject<SQL::MySQLAdapter>(L, "MySQLAdapter");
			return 1;
		}
		int lua_mysql_connect(lua_State* L)
		{
			auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
			//first host
			std::string host = luaL_checkstring(L, 2);
			//second user
			std::string user = luaL_checkstring(L, 3);
			//third password
			std::string password = luaL_checkstring(L, 4);
			//fourth database
			std::string database = luaL_checkstring(L, 5);
			//fifth port (optional)
			unsigned int port = 3306;
			if (lua_gettop(L) >= 6 && lua_isinteger(L, 6))
			{
				port = static_cast<unsigned int>(lua_tointeger(L, 6));
			}
			if (!db)
			{
				lua_pushstring(L, "Invalid MySQLAdapter object");
				return lua_error(L);
			}
			try
			{
				db->Connect(host, user, password, database, port);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
			return 0;
		}
		int lua_mysql_execute(lua_State* L)
		{
			auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
			std::string query = luaL_checkstring(L, 2);
			if (!db)
			{
				lua_pushstring(L, "Invalid MySQLAdapter object");
				return lua_error(L);
			}
			try
			{
				db->Execute(query);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_mysql_query(lua_State* L)
		{
			auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
			std::string query = luaL_checkstring(L, 2);
			if (!db)
			{
				lua_pushstring(L, "Invalid MySQLAdapter object");
				return lua_error(L);
			}
			try {
				auto rows = db->Query(query);
				lua_createtable(L, rows.size(), 0);
				for (size_t i = 0; i < rows.size(); ++i)
				{
					lua_createtable(L, 0, rows[i].size());
					for (const auto& [key, val] : rows[i])
					{
						PushAnyToLua(L, val);
						lua_setfield(L, -2, key.c_str());
					}
					lua_rawseti(L, -2, i + 1);
				}
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_mysql_insert(lua_State* L)
		{
			auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid MySQLAdapter object");
				return lua_error(L);
			}
			try
			{
				std::unordered_map<std::string, std::any> data;
				lua_pushnil(L);
				while (lua_next(L, 3))
				{
					std::string key = lua_tostring(L, -2);
					std::any val;
					if (lua_isnumber(L, -1)) val = (int64_t)lua_tointeger(L, -1);
					else if (lua_isstring(L, -1)) val = std::string(lua_tostring(L, -1));
					else if (lua_isboolean(L, -1)) val = (int64_t)(lua_toboolean(L, -1) ? 1 : 0);
					data[key] = val;
					lua_pop(L, 1);
				}
				db->Insert(table, data);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_mysql_update(lua_State* L)
		{
			auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			luaL_checktype(L, 4, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid MySQLAdapter object");
				return lua_error(L);
			}
			try
			{
				auto parse_table = [](lua_State* L, int idx)->std::unordered_map<std::string, std::any> {
					std::unordered_map<std::string, std::any> result;
					lua_pushnil(L);
					while (lua_next(L, idx))
					{
						std::string key = lua_tostring(L, -2);
						std::any val;
						if (lua_isnumber(L, -1)) val = (int64_t)lua_tointeger(L, -1);
						else if (lua_isstring(L, -1)) val = std::string(lua_tostring(L, -1));
						result[key] = val;
						lua_pop(L, 1);
					}
					return result;
					};
				auto set_data = parse_table(L, 3);
				auto where_data = parse_table(L, 4);
				db->Update(table, set_data, where_data);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_mysql_delete(lua_State* L)
		{
			auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid MySQLAdapter object");
				return lua_error(L);
			}
			try
			{
				std::unordered_map<std::string, std::any> data;
				lua_pushnil(L);
				while (lua_next(L, 3))
				{
					std::string key = lua_tostring(L, -2);
					std::any val;
					if (lua_isnumber(L, -1)) val = (int64_t)lua_tointeger(L, -1);
					else if (lua_isstring(L, -1)) val = std::string(lua_tostring(L, -1));
					data[key] = val;
					lua_pop(L, 1);
				}
				db->Delete(table, data);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_mysql_create_table(lua_State* L)
		{
			auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid MySQLAdapter object");
				return lua_error(L);
			}
			try
			{
				std::unordered_map<std::string, std::string> columns;
				lua_pushnil(L);
				while (lua_next(L, 3))
				{
					std::string key = lua_tostring(L, -2);
					std::string col_type = lua_tostring(L, -1);
					columns[key] = col_type;
					lua_pop(L, 1);
				}
				db->CreateTable(table, columns);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_mysql_close(lua_State* L)
		{
			auto* db = static_cast<SQL::MySQLAdapter*>(
				luaL_checkudata(L, 1, "MySQLAdapter"));
			db->~MySQLAdapter();
			return 0;
		}
		int lua_mysql_gc(lua_State* L)
		{
			LuaGC<SQL::MySQLAdapter>(L);
			return 0;
		}
		static const luaL_Reg mysql_adapter_methods[] =
		{
			{"Connect", lua_mysql_connect},
			{"Execute", lua_mysql_execute},
			{"Query", lua_mysql_query},
			{"Insert", lua_mysql_insert},
			{"Update", lua_mysql_update},
			{"Delete", lua_mysql_delete},
			{"CreateTable", lua_mysql_create_table},
			{"Close", lua_mysql_close},
			{"__gc", lua_mysql_gc},
			{nullptr, nullptr}
		};
#endif // USE_MYSQL
#ifdef USE_PGSQL
		// ============= PostgreSQLAdapter Wrapper =============
		// Similar implementation as MySQLAdapter can be done here
		int lua_pgsql_new(lua_State* L)
		{
			NewLuaObject<SQL::PostgreSQLAdapter>(L, "PostgreSQLAdapter");
			return 1;
		}
		int lua_pgsql_connect(lua_State* L)
		{
			auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
			//first host
			std::string host = luaL_checkstring(L, 2);
			//second user
			std::string user = luaL_checkstring(L, 3);
			//third password
			std::string password = luaL_checkstring(L, 4);
			//fourth database
			std::string database = luaL_checkstring(L, 5);
			//fifth port (optional)
			unsigned int port = 5432;
			if (lua_gettop(L) >= 6 && lua_isinteger(L, 6))
			{
				port = static_cast<unsigned int>(lua_tointeger(L, 6));
			}
			if (!db)
			{
				lua_pushstring(L, "Invalid PostgreSQLAdapter object");
				return lua_error(L);
			}
			try
			{
				db->Connect(host, user, password, database, port);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
			return 0;
		}
		int lua_pgsql_execute(lua_State* L)
		{
			auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
			std::string query = luaL_checkstring(L, 2);
			if (!db)
			{
				lua_pushstring(L, "Invalid PostgreSQLAdapter object");
				return lua_error(L);
			}
			try
			{
				db->Execute(query);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_pgsql_query(lua_State* L)
		{
			auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
			std::string query = luaL_checkstring(L, 2);
			if (!db)
			{
				lua_pushstring(L, "Invalid PostgreSQLAdapter object");
				return lua_error(L);
			}
			try {
				auto rows = db->Query(query);
				lua_createtable(L, rows.size(), 0);
				for (size_t i = 0; i < rows.size(); ++i)
				{
					lua_createtable(L, 0, rows[i].size());
					for (const auto& [key, val] : rows[i])
					{
						PushAnyToLua(L, val);
						lua_setfield(L, -2, key.c_str());
					}
					lua_rawseti(L, -2, i + 1);
				}
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_pgsql_insert(lua_State* L)
		{
			auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid PostgreSQLAdapter object");
				return lua_error(L);
			}
			try
			{
				std::unordered_map<std::string, std::any> data;
				lua_pushnil(L);
				while (lua_next(L, 3))
				{
					std::string key = lua_tostring(L, -2);
					std::any val;
					if (lua_isnumber(L, -1)) val = (int64_t)lua_tointeger(L, -1);
					else if (lua_isstring(L, -1)) val = std::string(lua_tostring(L, -1));
					else if (lua_isboolean(L, -1)) val = (int64_t)(lua_toboolean(L, -1) ? 1 : 0);
					data[key] = val;
					lua_pop(L, 1);
				}
				db->Insert(table, data);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_pgsql_update(lua_State* L)
		{
			auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			luaL_checktype(L, 4, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid PostgreSQLAdapter object");
				return lua_error(L);
			}
			try
			{
				auto parse_table = [](lua_State* L, int idx)->std::unordered_map<std::string, std::any> {
					std::unordered_map<std::string, std::any> result;
					lua_pushnil(L);
					while (lua_next(L, idx))
					{
						std::string key = lua_tostring(L, -2);
						std::any val;
						if (lua_isnumber(L, -1)) val = (int64_t)lua_tointeger(L, -1);
						else if (lua_isstring(L, -1)) val = std::string(lua_tostring(L, -1));
						result[key] = val;
						lua_pop(L, 1);
					}
					return result;
					};
				auto set_data = parse_table(L, 3);
				auto where_data = parse_table(L, 4);
				db->Update(table, set_data, where_data);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_pgsql_delete(lua_State* L)
		{
			auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid PostgreSQLAdapter object");
				return lua_error(L);
			}
			try
			{
				std::unordered_map<std::string, std::any> data;
				lua_pushnil(L);
				while (lua_next(L, 3))
				{
					std::string key = lua_tostring(L, -2);
					std::any val;
					if (lua_isnumber(L, -1)) val = (int64_t)lua_tointeger(L, -1);
					else if (lua_isstring(L, -1)) val = std::string(lua_tostring(L, -1));
					data[key] = val;
					lua_pop(L, 1);
				}
				db->Delete(table, data);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_pgsql_create_table(lua_State* L)
		{
			auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
			std::string table = luaL_checkstring(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);
			if (!db)
			{
				lua_pushstring(L, "Invalid PostgreSQLAdapter object");
				return lua_error(L);
			}
			try
			{
				std::unordered_map<std::string, std::string> columns;
				lua_pushnil(L);
				while (lua_next(L, 3))
				{
					std::string key = lua_tostring(L, -2);
					std::string col_type = lua_tostring(L, -1);
					columns[key] = col_type;
					lua_pop(L, 1);
				}
				db->CreateTable(table, columns);
				lua_pushboolean(L, 1);
				return 1;
			}
			catch (const std::exception& e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
		}
		int lua_pgsql_close(lua_State* L)
		{
			auto* db = static_cast<SQL::PostgreSQLAdapter*>(
				luaL_checkudata(L, 1, "PostgreSQLAdapter"));
			db->~PostgreSQLAdapter();
			return 0;
		}
		int lua_pgsql_gc(lua_State* L)
		{
			LuaGC<SQL::PostgreSQLAdapter>(L);
			return 0;
		}
		static const luaL_Reg pgsql_adapter_methods[] =
		{
			{"Connect", lua_pgsql_connect},
			{"Execute", lua_pgsql_execute},
			{"Query", lua_pgsql_query},
			{"Insert", lua_pgsql_insert},
			{"Update", lua_pgsql_update},
			{"Delete", lua_pgsql_delete},
			{"CreateTable", lua_pgsql_create_table},
			{"Close", lua_pgsql_close},
			{"__gc", lua_pgsql_gc},
			{nullptr, nullptr}
		};
#endif // USE_PGSQL

	}// namespace

	void RegisterLuaZipper(lua_State* L)
	{
		luaL_newmetatable(L, "Zipper");
		lua_pushcfunction(L, lua_zipper_gc);
		lua_setfield(L, -2, "__gc");
		lua_pop(L, 1);

		lua_getglobal(L, "Zipper");
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			lua_newtable(L);
		}
		lua_pushcfunction(L, lua_zipper_new);
		lua_setfield(L, -2, "new");
		lua_pushcfunction(L, lua_zipper_add_file);
		lua_setfield(L, -2, "AddFile");
		lua_pushcfunction(L, lua_zipper_add_byte_file);
		lua_setfield(L, -2, "AddByteFile");
		lua_pushcfunction(L, lua_zipper_save);
		lua_setfield(L, -2, "Save");
		lua_setglobal(L, "Zipper");
	}

#ifdef USE_BIT7Z
	void RegisterLuaBit7zZipper(lua_State* L)
	{
		luaL_newmetatable(L, "Bit7zZipper");
		lua_pushcfunction(L, lua_bit7z_zipper_gc);
		lua_setfield(L, -2, "__gc");
		lua_pop(L, 1);
		lua_getglobal(L, "Bit7zZipper");
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			lua_newtable(L);
		}
		lua_pushcfunction(L, lua_bit7z_zipper_new);
		lua_setfield(L, -2, "new");
		lua_pushcfunction(L, lua_bit7z_zipper_add_file);
		lua_setfield(L, -2, "AddFile");
		lua_pushcfunction(L, lua_bit7z_zipper_add_byte_file);
		lua_setfield(L, -2, "AddByteFile");
		lua_pushcfunction(L, lua_bit7z_zipper_save);
		lua_setfield(L, -2, "Save");
		lua_setglobal(L, "Bit7zZipper");
	}
#endif // USE_BIT7Z

	void RegisterLuaSQLiteAdapter(lua_State* L)
	{
		luaL_newmetatable(L, "SQLiteAdapter");

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, lua_sqlite_gc);
		lua_setfield(L, -2, "__gc");

		lua_pushcfunction(L, lua_sqlite_connect);
		lua_setfield(L, -2, "Connect");
		lua_pushcfunction(L, lua_sqlite_execute);
		lua_setfield(L, -2, "Execute");
		lua_pushcfunction(L, lua_sqlite_query);
		lua_setfield(L, -2, "Query");
		lua_pushcfunction(L, lua_sqlite_insert);
		lua_setfield(L, -2, "Insert");
		lua_pushcfunction(L, lua_sqlite_update);
		lua_setfield(L, -2, "Update");
		lua_pushcfunction(L, lua_sqlite_delete);
		lua_setfield(L, -2, "Delete");
		lua_pushcfunction(L, lua_sqlite_create_table);
		lua_setfield(L, -2, "CreateTable");
		lua_pushcfunction(L, lua_sqlite_close);
		lua_setfield(L, -2, "Close");

		lua_pop(L, 1);

		lua_newtable(L);
		lua_pushcfunction(L, lua_sqlite_new);
		lua_setfield(L, -2, "new");
		lua_setglobal(L, "SQLiteAdapter");
	}

#ifdef USE_MYSQL
	void RegisterLuaMySQLAdapter(lua_State* L)
	{
		luaL_newmetatable(L, "MySQLAdapter");
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, lua_mysql_gc);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, lua_mysql_connect);
		lua_setfield(L, -2, "Connect");
		lua_pushcfunction(L, lua_mysql_execute);
		lua_setfield(L, -2, "Execute");
		lua_pushcfunction(L, lua_mysql_query);
		lua_setfield(L, -2, "Query");
		lua_pushcfunction(L, lua_mysql_insert);
		lua_setfield(L, -2, "Insert");
		lua_pushcfunction(L, lua_mysql_update);
		lua_setfield(L, -2, "Update");
		lua_pushcfunction(L, lua_mysql_delete);
		lua_setfield(L, -2, "Delete");
		lua_pushcfunction(L, lua_mysql_create_table);
		lua_setfield(L, -2, "CreateTable");
		lua_pushcfunction(L, lua_mysql_close);
		lua_setfield(L, -2, "Close");
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushcfunction(L, lua_mysql_new);
		lua_setfield(L, -2, "new");
		lua_setglobal(L, "MySQLAdapter");
	}
#endif // USE_MYSQL

#ifdef USE_PGSQL
	void RegisterLuaPostgreSQLAdapter(lua_State* L)
	{
		luaL_newmetatable(L, "PostgreSQLAdapter");
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, lua_pgsql_gc);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, lua_pgsql_connect);
		lua_setfield(L, -2, "Connect");
		lua_pushcfunction(L, lua_pgsql_execute);
		lua_setfield(L, -2, "Execute");
		lua_pushcfunction(L, lua_pgsql_query);
		lua_setfield(L, -2, "Query");
		lua_pushcfunction(L, lua_pgsql_insert);
		lua_setfield(L, -2, "Insert");
		lua_pushcfunction(L, lua_pgsql_update);
		lua_setfield(L, -2, "Update");
		lua_pushcfunction(L, lua_pgsql_delete);
		lua_setfield(L, -2, "Delete");
		lua_pushcfunction(L, lua_pgsql_create_table);
		lua_setfield(L, -2, "CreateTable");
		lua_pushcfunction(L, lua_pgsql_close);
		lua_setfield(L, -2, "Close");
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushcfunction(L, lua_pgsql_new);
		lua_setfield(L, -2, "new");
		lua_setglobal(L, "PostgreSQLAdapter");
	}
#endif // USE_PGSQL


} // namespace HsBa::Slicer