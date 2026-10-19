---
kind: external_dependency
name: SQLite 嵌入式数据库
slug: sqlite
category: external_dependency
category_hints:
    - vendor_identity
    - client_constraint
scope:
    - '**'
source_files:
    - fileoperator/sql_adapter.cpp
    - fileoperator/LayersPath.cpp
---

### 身份与用途
- 通过 sqlpp11 的 sqlite3 connector 使用（vcpkg 上按平台启用不同 feature：桌面端含 sqlite3/mysql/postgresql，移动端仅 sqlite3）。
- 项目内 `SQLAdapter` 基于 sqlpp11 抽象，当前生产代码实际落库到 SQLite 文件（`LayersPath::Save` 直接调 `sqlite3_open`，ParamStore 宽表模型也存 SQLite）。

### 稳定约束
- 文件名编码：`sqlite3_open` 期望 UTF-8 字符串，不能传 ANSI 路径；项目通过 `path_to_utf8` 在调用点保证这一点。
- 列迁移：`EnsureTable` 仅在表不存在时 `CREATE TABLE`，**不会**给已存在表自动补列；加密特性前已存在的表需删除重建或手动添加 `param_blob` BLOB 列。