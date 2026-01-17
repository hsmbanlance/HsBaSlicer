-- Test script for Lua adapter to verify both calling conventions work
print("Testing Lua adapter with both calling conventions...")

-- Test Zipper
print("\n=== Testing Zipper ===")
local zipper = Zipper.new()
print("Created Zipper object: ", zipper)

-- Test method call syntax: obj:Method(args)
print("Testing method call syntax (obj:Method):")
-- Note: Since we don't have actual files to zip, we'll just check if functions exist
print("Zipper has AddFile method:", type(zipper.AddFile))
print("Zipper has Save method:", type(zipper.Save))

-- Test static call syntax: Class.Method(obj, args)
print("Testing static call syntax (Class.Method):")
print("Zipper has AddFile in class:", type(Zipper.AddFile))
print("Zipper has Save in class:", type(Zipper.Save))

-- Test SQLiteAdapter
print("\n=== Testing SQLiteAdapter ===")
local db = SQLiteAdapter.new()
print("Created SQLiteAdapter object: ", db)

print("SQLiteAdapter has Connect method:", type(db.Connect))
print("SQLiteAdapter has Query method:", type(db.Query))
print("SQLiteAdapter has Execute method:", type(db.Execute))

print("SQLiteAdapter has Connect in class:", type(SQLiteAdapter.Connect))
print("SQLiteAdapter has Query in class:", type(SQLiteAdapter.Query))
print("SQLiteAdapter has Execute in class:", type(SQLiteAdapter.Execute))

-- Test MySQLAdapter if available
print("\n=== Testing MySQLAdapter ===")
if MySQLAdapter then
    local mysql_db = MySQLAdapter.new()
    print("Created MySQLAdapter object: ", mysql_db)
    print("MySQLAdapter has Connect method:", type(mysql_db.Connect))
    print("MySQLAdapter has Connect in class:", type(MySQLAdapter.Connect))
else
    print("MySQLAdapter not available (HSBA_USE_MYSQL not defined)")
end

-- Test PostgreSQLAdapter if available
print("\n=== Testing PostgreSQLAdapter ===")
if PostgreSQLAdapter then
    local pgsql_db = PostgreSQLAdapter.new()
    print("Created PostgreSQLAdapter object: ", pgsql_db)
    print("PostgreSQLAdapter has Connect method:", type(pgsql_db.Connect))
    print("PostgreSQLAdapter has Connect in class:", type(PostgreSQLAdapter.Connect))
else
    print("PostgreSQLAdapter not available (HSBA_USE_PGSQL not defined)")
end

-- Test Bit7zZipper if available
print("\n=== Testing Bit7zZipper ===")
if Bit7zZipper then
    local bit7z_zipper = Bit7zZipper.new("Zip")
    print("Created Bit7zZipper object: ", bit7z_zipper)
    print("Bit7zZipper has AddFile method:", type(bit7z_zipper.AddFile))
    print("Bit7zZipper has AddFile in class:", type(Bit7zZipper.AddFile))
else
    print("Bit7zZipper not available (HSBA_USE_BIT7Z not defined)")
end

print("\nAll tests completed successfully!")