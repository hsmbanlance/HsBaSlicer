# ParamStore (Process Parameter Store)

`ParamStore` (`fileoperator/param_store.hpp/.cpp`) persists reflected process-parameter
config structs to a relational database and reads them back. It is the storage engine
wrapped by `LibHsBaSlicer/ParamStore/param_store_ops.hpp` (C++/C-ABI layer) and by the
`HsBaParamStore*` Lua API.

## Wide-table storage model

Each pipeline type maps to one table. A config is expanded via reflection
(`param_reflect`) into one column per field and upserted by `param_key`.
The fixed metadata columns are:

| Column | Meaning |
|--------|---------|
| `param_key` | Primary key of the row |
| `schema_version` | Layout version recorded at save time |
| `created_at` / `updated_at` | Unix-second timestamps |
| `param_id` | SQLite-only `INTEGER PRIMARY KEY AUTOINCREMENT`; NULL on other backends |
| `param_blob` | Optional encrypted binary payload (see below) |

`EnsureSchema` iterates every pipeline tag and calls `EnsureTable`, which runs a
`CREATE TABLE IF NOT EXISTS`-style existence check and only creates a table that is
missing. Columns are **not** added to an already-existing table.

## Encryption (`param_blob`)

When a crypto password is set, the whole config is additionally serialized into a
self-describing binary blob and encrypted with **AES-256-CBC** (key/IV derived from the
password by the `HsBaCipher` module), then stored in the `param_blob` column. The per-field
plaintext columns are cleared so no sensitive value is left in the clear.

- `ParamStore::SetCryptoKey(std::string_view key)` enables encryption; an empty key
  disables it and keeps the original plaintext behaviour (fully backward compatible).
- `ParamStore::CryptoEnabled()` reports the current state.
- `Save` writes the encrypted blob; `Load` reads and decrypts it, falling back to the
  plaintext columns for rows saved without encryption.
- `Update` performs a read-decrypt-modify-re-encrypt cycle on the blob.
- `List` never touches `param_blob`, so listing works without the password.

Loading an encrypted row without the correct key fails with
`SQL::SQLAdapterQueryError` (surfaced as `success=false` at the Lib/C layers).

### Binary format (`param_codec.hpp/.cpp`)

Little-endian, self-describing:

```
[4]  magic "HSB1"
[4]  field count (uint32)
repeat count times:
    [4]  name length (uint32)
    [N]  field name (bytes)
    [1]  value tag: 0=null, 1=int64, 2=double, 3=string
    [..] payload  (int64/double = 8 bytes; string = uint32 length + bytes)
```

Only the SQL whitelisted types (`nullptr_t`, `int64_t`, `double`, `std::string`) are
encodable — the same set `SQLAdapter` binds; any other type raises
`SQL::SQLAdapterInvalidArgumentError`.

## Key passing through the layers

| Layer | Field / API | Notes |
|-------|-------------|-------|
| Lib `ParamStoreConn` | `std::string cryptoKey` | Empty = plaintext |
| C `HsBaParamStoreConn` | `const char* crypto_password` | NULL/empty = plaintext; copied on call |
| Lua | `store:SetCryptoKey(password)` | Instance method; empty/nil disables |
| C++20 module `ParamStoreConnection` | `std::string cryptoKey` | Mapped into the Lib conn |

## Behaviour notes

- The `param_blob` column is part of the `CREATE` statement, so it is present on any table
  created after this feature. A table created before encryption existed is not migrated
  automatically — drop/recreate it (or add the `param_blob` BLOB column manually) before
  saving encrypted rows into it.
- MySQL uses `LONGBLOB`, PostgreSQL uses `BYTEA`, SQLite uses `BLOB`.
- All encryption/decryption failures are converted to `SQL::SQLAdapterError` so they
  collapse into the Lib/C-ABI outcome and never propagate as C++ exceptions across the
  C boundary.
