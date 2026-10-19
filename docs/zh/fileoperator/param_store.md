# ParamStore（工艺参数存储）

`ParamStore`（`fileoperator/param_store.hpp/.cpp`）把经反射展开的工艺参数配置结构体持久化到
关系型数据库并读回。它是 `LibHsBaSlicer/ParamStore/param_store_ops.hpp`（C++/C-ABI 层）以及
`HsBaParamStore*` Lua 接口所封装的存储引擎。

## 宽表存储模型

每种流水线类型对应一张表。配置经反射（`param_reflect`）展开为「每字段一列」，并按 `param_key`
执行 upsert。固定的元数据列有：

| 列名 | 含义 |
|------|------|
| `param_key` | 行主键 |
| `schema_version` | 保存时记录的字段布局版本 |
| `created_at` / `updated_at` | Unix 秒时间戳 |
| `param_id` | 仅 SQLite 有效的 `INTEGER PRIMARY KEY AUTOINCREMENT`；其他后端为 NULL |
| `param_blob` | 可选的加密二进制载荷（见下） |

`EnsureSchema` 逐个遍历流水线 tag 并调用 `EnsureTable`；后者做一次存在性检查，仅当表不存在时才
`CREATE TABLE`。**不会**向已存在的表补加列。

## 加密（`param_blob`）

设置加密口令后，整份配置会被额外序列化为自描述的二进制块，并用 **AES-256-CBC**（密钥/IV 由
`HsBaCipher` 模块从口令派生）加密，写入 `param_blob` 列；同时清空各字段的明文列，避免敏感值残留。

- `ParamStore::SetCryptoKey(std::string_view key)` 启用加密；空口令则禁用并保持原有明文行为
  （完全向后兼容）。
- `ParamStore::CryptoEnabled()` 报告当前状态。
- `Save` 写入加密块；`Load` 读取并解密它，对未加密保存的行回退到明文列。
- `Update` 对加密块执行「读取-解密-修改-再加密」的回写流程。
- `List` 不读取 `param_blob`，因此无需口令即可列出。

无正确口令读取加密行会抛出 `SQL::SQLAdapterQueryError`（在 Lib/C 层收敛为 `success=false`）。

### 二进制格式（`param_codec.hpp/.cpp`）

小端、自描述：

```
[4]  魔数 "HSB1"
[4]  字段数量（uint32）
重复 count 次：
    [4]  字段名长度（uint32）
    [N]  字段名（字节）
    [1]  值类型标签：0=null、1=int64、2=double、3=string
    [..] 载荷（int64/double 占 8 字节；string = uint32 长度 + 字节）
```

仅支持 SQL 白名单类型（`nullptr_t`、`int64_t`、`double`、`std::string`）——与 `SQLAdapter`
可绑定的类型集合一致；其他类型抛出 `SQL::SQLAdapterInvalidArgumentError`。

## 各层口令透传

| 层级 | 字段 / 接口 | 说明 |
|------|-------------|------|
| Lib `ParamStoreConn` | `std::string cryptoKey` | 空 = 明文 |
| C `HsBaParamStoreConn` | `const char* crypto_password` | NULL/空 = 明文；调用时拷贝 |
| Lua | `store:SetCryptoKey(password)` | 实例方法；空/nil 禁用 |
| C++20 模块 `ParamStoreConnection` | `std::string cryptoKey` | 映射进 Lib conn |

## 行为说明

- `param_blob` 列包含在建表 `CREATE` 语句里，因此本特性之后新建的表都会带有它。在本加密特性
  之前就已存在的表不会被自动迁移——需先删除重建（或手动添加 `param_blob` BLOB 列），
  才能向其中保存加密行。
- MySQL 用 `LONGBLOB`、PostgreSQL 用 `BYTEA`、SQLite 用 `BLOB`。
- 所有加解密失败都会转换为 `SQL::SQLAdapterError`，从而收敛进 Lib/C-ABI 的返回结果，
  不会以 C++ 异常形式越过 C 边界。
