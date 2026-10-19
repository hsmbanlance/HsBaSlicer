/** @file param_store.cpp
 * @brief Implements ParamStore: staged Save pipeline, CRUD, batch transactions and the migration hook.
 */
#include "param_store.hpp"

#include <cctype>
#include <ctime>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "cipher/encrypt.hpp"
#include "param_codec.hpp"

namespace HsBa::Slicer
{
namespace
{
constexpr int64_t kSchemaVersion = 1;

// Metadata columns that always stay in plaintext so rows can be looked up, listed and diffed.
bool IsMetaColumn(const std::string& name)
{
    return name == "param_key" || name == "schema_version" || name == "created_at" || name == "updated_at" ||
           name == "param_id" || name == ParamSchema::BlobColumn();
}

// Return the current wall-clock time in whole seconds.
int64_t NowSeconds()
{
    return static_cast<int64_t>(std::time(nullptr));
}

// Reflect a registered Config AnyObject into {column name -> converged std::any}.
std::unordered_map<std::string, std::any> ReflectColumns(Utils::AnyObject cfg)
{
    std::unordered_map<std::string, std::any> out;
    cfg.ForeachField([&out](std::string_view name, const Utils::AnyObject& child)
                     { out[std::string(name)] = FieldToAny(name, child.get_type_info(), child.get_data()); });
    return out;
}

// Build a whitelist std::any from a rapidjson scalar member.
std::any JsonValueToAny(const rapidjson::Value& v)
{
    if (v.IsNull())
    {
        return std::any(nullptr);
    }
    if (v.IsBool())
    {
        return std::any(static_cast<int64_t>(v.GetBool() ? 1 : 0));
    }
    if (v.IsInt64())
    {
        return std::any(v.GetInt64());
    }
    if (v.IsUint64())
    {
        return std::any(static_cast<int64_t>(v.GetUint64()));
    }
    if (v.IsDouble())
    {
        return std::any(v.GetDouble());
    }
    if (v.IsString())
    {
        return std::any(std::string(v.GetString(), v.GetStringLength()));
    }
    return std::any(std::string(v.GetString(), v.GetStringLength()));
}

// Derive a short table suffix from a full type name, e.g. "HsBaFdmPipelineConfig" -> "fdm".
std::string TypeShortName(std::string_view full_name)
{
    if (full_name.starts_with("HsBa"))
    {
        full_name.remove_prefix(4);
    }
    if (full_name.size() >= 13 && full_name.compare(full_name.size() - 13, 13, "PipelineConfig") == 0)
    {
        full_name.remove_suffix(13);
    }
    std::string out;
    out.reserve(full_name.size());
    for (char const c : full_name)
    {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (out == "filetransfer")
    {
        out = "filetransfer";
    }
    return out;
}
}  // namespace

template <typename Fn>
auto ParamStore::Guard(Fn&& fn) -> decltype(fn())
{
    try
    {
        last_error_.clear();
        if constexpr (!std::is_void_v<decltype(fn())>)
        {
            return std::forward<Fn>(fn)();
        }
        else
        {
            std::forward<Fn>(fn)();
        }
    }
    catch (const RuntimeError& e)
    {
        last_error_ = e.what();
        throw;
    }
}

void ParamStore::Raise(int percent, const char* stage)
{
    if (progress_)
    {
        progress_(percent, stage);
    }
}

ParamStore::ParamStore(SQL::ISQLAdapter& db, std::optional<Backend> backend)
    : db_(&db), backend_(backend.value_or(ParamSchema::DetectBackend(db)))
{
}

std::string ParamStore::ResolveTable(std::string_view table, const Utils::AnyObject& cfg)
{
    if (!table.empty())
    {
        return std::string(table);
    }
    std::string_view const derived = DefaultTableName(cfg.get_type_info());
    if (!derived.empty())
    {
        return std::string(derived);
    }
    // Fallback: derive a name from the type name
    return "hsba_param_" + TypeShortName(cfg.get_type_info() ? cfg.get_type_info()->Name : "");
}

void ParamStore::EnsureTable(PipelineConfigTag tag)
{
    Guard(
        [this, tag]
        {
            Utils::TypeInfo* ti = TypeInfoOf(tag);
            if (!ti)
            {
                throw SQL::SQLAdapterInvalidArgumentError("ParamStore::EnsureTable: unknown tag");
            }
            const std::string table(DefaultTableName(tag));
            const ParamSchema& schema = ParamSchema::forConfig(ti);
            auto rows = db_->Query(HsBa::Slicer::ParamSchema::ExistenceCheck(table, backend_));
            if (rows.empty())
            {
                db_->Execute(schema.CreateStatement(table, backend_));
                Raise(100, "schema created");
            }
        });
}

void ParamStore::EnsureSchema()
{
    Guard(
        [this]
        {
            RegisterPipelineConfigTypes();
            for (int tag = static_cast<int>(PipelineConfigTag::Fdm);
                 tag <= static_cast<int>(PipelineConfigTag::FileTransfer); ++tag)
            {
                EnsureTable(static_cast<PipelineConfigTag>(tag));
            }
        });
}

int64_t ParamStore::Save(std::string_view table, std::string_view key, const Utils::AnyObject& cfg)
{
    return Guard(
        [this, table, key, cfg]() mutable -> int64_t
        {
            // Stage 1: Validate
            Raise(10, "validate");
            RegisterPipelineConfigTypes();
            if (!IsPipelineConfigType(cfg.get_type_info()))
            {
                throw SQL::SQLAdapterInvalidArgumentError("ParamStore::Save: config type not registered");
            }
            const std::string tbl = ResolveTable(table, cfg);

            // Stage 2+3: Reflect + Coerce
            Raise(40, "reflect & coerce");
            auto data = ReflectColumns(cfg);

            // FileTransfer's file_paths are stored additionally as a JSON TEXT column.
            if (TagOf(cfg.get_type_info()) == PipelineConfigTag::FileTransfer)
            {
                auto* ft = static_cast<HsBaFileTransferPipelineConfig_t*>(cfg.get_data());
                rapidjson::Document doc;
                doc.SetArray();
                auto& alloc = doc.GetAllocator();
                for (int i = 0; i < ft->file_count && ft->file_paths; ++i)
                {
                    const char* p = ft->file_paths[i];
                    doc.PushBack(rapidjson::Value(p ? p : "", alloc), alloc);
                }
                rapidjson::StringBuffer buf;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
                doc.Accept(writer);
                data["file_paths"] = std::string(buf.GetString(), buf.GetSize());
            }

            data["param_key"] = std::string(key);
            data["schema_version"] = kSchemaVersion;
            const int64_t now = NowSeconds();
            data["updated_at"] = now;

            // Stage 3.5: When a crypto key is set, fold the field columns into an encrypted param_blob.
            if (CryptoEnabled())
            {
                std::unordered_map<std::string, std::any> fields;
                for (const auto& [name, value] : data)
                {
                    if (!IsMetaColumn(name))
                    {
                        fields.emplace(name, value);
                    }
                }
                const std::vector<unsigned char> plain = EncodeColumns(fields);
                std::vector<unsigned char> cipher;
                try
                {
                    cipher = Cipher::Encrypt::aes256_cbc_encrypt(plain, crypto_key_);
                }
                catch (const std::exception& e)
                {
                    throw SQL::SQLAdapterQueryError(std::string("ParamStore::Save: encryption failed: ") + e.what());
                }
                for (const auto& [name, value] : fields)
                {
                    (void)value;
                    data.erase(name);
                }
                data[ParamSchema::BlobColumn()] = std::move(cipher);
            }

            // Stage 4: Upsert
            Raise(70, "upsert");
            auto existing = db_->Select(tbl, {"param_id"}, {{"param_key", std::string(key)}}, std::nullopt, 1, 0);
            if (!existing.empty())
            {
                auto set = data;
                set.erase("param_key");
                int64_t const id = std::any_cast<int64_t>(existing.at(0).at("param_id"));
                db_->Update(tbl, set, {{"param_key", std::string(key)}});
                Raise(100, "updated");
                return id;
            }
            data["created_at"] = now;
            db_->Insert(tbl, data);
            auto probe = db_->Select(tbl, {"param_id"}, {{"param_key", std::string(key)}}, std::nullopt, 1, 0);
            if (probe.empty())
            {
                throw SQL::SQLAdapterQueryError("ParamStore::Save: row vanished after insert");
            }
            Raise(100, "inserted");
            return std::any_cast<int64_t>(probe.at(0).at("param_id"));
        });
}

std::vector<int64_t> ParamStore::SaveBatch(std::string_view table,
                                           const std::vector<std::pair<std::string, Utils::AnyObject>>& items)
{
    return Guard(
        [this, table, &items]() -> std::vector<int64_t>
        {
            const bool use_txn = true;
            const char* begin = backend_ == Backend::PostgreSQL ? "BEGIN" : "BEGIN TRANSACTION";
            const char* commit = backend_ == Backend::PostgreSQL ? "COMMIT" : "COMMIT TRANSACTION";
            const char* rollback = backend_ == Backend::PostgreSQL ? "ROLLBACK" : "ROLLBACK TRANSACTION";
            if (use_txn)
            {
                db_->Execute(begin);
            }
            std::vector<int64_t> ids;
            try
            {
                for (const auto& [key, obj] : items)
                {
                    Utils::AnyObject const cfg = obj;
                    ids.push_back(Save(table, key, cfg));
                }
                if (use_txn)
                {
                    db_->Execute(commit);
                }
            }
            catch (const SQL::SQLAdapterError&)
            {
                if (use_txn)
                {
                    try
                    {
                        db_->Execute(rollback);
                    }
                    catch (const SQL::SQLAdapterError&)  // NOLINT(bugprone-empty-catch) rollback failure intentionally
                                                         // swallowed to preserve the original exception
                    {
                        // A rollback failure must not mask the original exception.
                    }
                }
                throw;
            }
            return ids;
        });
}

bool ParamStore::Load(std::string_view table, std::string_view key, Utils::AnyObject outCfg, StringArena& arena)
{
    return Guard(
        [this, table, key, outCfg, &arena]() mutable -> bool
        {
            if (!IsPipelineConfigType(outCfg.get_type_info()))
            {
                throw SQL::SQLAdapterInvalidArgumentError("ParamStore::Load: config type not registered");
            }
            const std::string tbl = ResolveTable(table, outCfg);
            auto rows = db_->Select(tbl, {}, {{"param_key", std::string(key)}}, std::nullopt, 1, 0);
            if (rows.empty())
            {
                return false;
            }
            const auto& row = rows.at(0);
            Raise(50, "mapping");

            // When the row carries an encrypted param_blob, decode it back into the column map;
            // otherwise fall through to the plaintext wide-table columns.
            auto blobIt = row.find(ParamSchema::BlobColumn());
            bool const hasBlob = blobIt != row.end() && blobIt->second.type() == typeid(std::vector<unsigned char>) &&
                                 !std::any_cast<const std::vector<unsigned char>&>(blobIt->second).empty();
            if (hasBlob)
            {
                if (!CryptoEnabled())
                {
                    throw SQL::SQLAdapterQueryError("ParamStore::Load: row is encrypted but no crypto key is set");
                }
                const auto& cipher = std::any_cast<const std::vector<unsigned char>&>(blobIt->second);
                std::vector<unsigned char> plain;
                try
                {
                    plain = Cipher::Encrypt::aes256_cbc_decrypt(cipher, crypto_key_);
                }
                catch (const std::exception& e)
                {
                    throw SQL::SQLAdapterQueryError(std::string("ParamStore::Load: decryption failed: ") + e.what());
                }
                auto cols = DecodeColumns(plain);
                outCfg.ForeachField(
                    [&cols, &arena](std::string_view name, const Utils::AnyObject& child)
                    {
                        auto it = cols.find(std::string(name));
                        if (it == cols.end())
                        {
                            return;  // Missing column: keep the target struct's original value
                        }
                        AnyToField(it->second, child.get_type_info(), child.get_data(), arena);
                    });
                Raise(100, "loaded");
                return true;
            }

            outCfg.ForeachField(
                [&row, &arena](std::string_view name, const Utils::AnyObject& child)
                {
                    auto it = row.find(std::string(name));
                    if (it == row.end())
                    {
                        return;  // Missing column: keep the target struct's original value
                    }
                    AnyToField(it->second, child.get_type_info(), child.get_data(), arena);
                });
            Raise(100, "loaded");
            return true;
        });
}

std::vector<std::string> ParamStore::List(std::string_view table, const std::string& whereJson)
{
    return Guard(
        [this, table, &whereJson]() -> std::vector<std::string>
        {
            if (table.empty())
            {
                throw SQL::SQLAdapterInvalidArgumentError("ParamStore::List: table name required");
            }
            const std::string tbl(table);
            std::unordered_map<std::string, std::any> where;
            if (!whereJson.empty())
            {
                rapidjson::Document doc;
                doc.Parse(whereJson.c_str());
                if (doc.HasParseError() || !doc.IsObject())
                {
                    throw SQL::SQLAdapterInvalidArgumentError("ParamStore::List: whereJson must be a JSON object");
                }
                for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
                {
                    where[it->name.GetString()] = JsonValueToAny(it->value);
                }
            }
            auto rows = db_->Select(tbl, {"param_key"}, where, std::nullopt, -1, 0);
            std::vector<std::string> keys;
            keys.reserve(rows.size());
            for (const auto& row : rows)
            {
                auto it = row.find("param_key");
                if (it != row.end() && it->second.type() == typeid(std::string))
                {
                    keys.push_back(std::any_cast<std::string>(it->second));
                }
            }
            return keys;
        });
}

bool ParamStore::Update(std::string_view table, std::string_view key, const Utils::AnyObject& partial,
                        const std::vector<std::string>& changedFields)
{
    return Guard(
        [this, table, key, partial, &changedFields]() mutable -> bool
        {
            if (!IsPipelineConfigType(partial.get_type_info()))
            {
                throw SQL::SQLAdapterInvalidArgumentError("ParamStore::Update: config type not registered");
            }
            const std::string tbl = ResolveTable(table, partial);
            auto existing = db_->Select(tbl, {}, {{"param_key", std::string(key)}}, std::nullopt, 1, 0);
            if (existing.empty())
            {
                return false;
            }

            auto reflected = ReflectColumns(partial);

            // Under encryption, changes must fold into the blob via a read-decode-modify-re-encrypt cycle
            // so no plaintext field column is ever written for an encrypted table.
            if (CryptoEnabled())
            {
                const auto& row = existing.at(0);
                auto blobIt = row.find(ParamSchema::BlobColumn());
                bool const hasBlob = blobIt != row.end() &&
                                     blobIt->second.type() == typeid(std::vector<unsigned char>) &&
                                     !std::any_cast<const std::vector<unsigned char>&>(blobIt->second).empty();
                std::unordered_map<std::string, std::any> cols;
                if (hasBlob)
                {
                    const auto& oldCipher = std::any_cast<const std::vector<unsigned char>&>(blobIt->second);
                    std::vector<unsigned char> plain;
                    try
                    {
                        plain = Cipher::Encrypt::aes256_cbc_decrypt(oldCipher, crypto_key_);
                    }
                    catch (const std::exception& e)
                    {
                        throw SQL::SQLAdapterQueryError(std::string("ParamStore::Update: decryption failed: ") +
                                                        e.what());
                    }
                    cols = DecodeColumns(plain);
                }
                for (const auto& field : changedFields)
                {
                    auto it = reflected.find(field);
                    if (it != reflected.end())
                    {
                        cols[it->first] = it->second;
                    }
                }
                const std::vector<unsigned char> plain = EncodeColumns(cols);
                std::vector<unsigned char> cipher;
                try
                {
                    cipher = Cipher::Encrypt::aes256_cbc_encrypt(plain, crypto_key_);
                }
                catch (const std::exception& e)
                {
                    throw SQL::SQLAdapterQueryError(std::string("ParamStore::Update: encryption failed: ") + e.what());
                }
                std::unordered_map<std::string, std::any> set;
                set[ParamSchema::BlobColumn()] = std::move(cipher);
                set["updated_at"] = NowSeconds();
                db_->Update(tbl, set, {{"param_key", std::string(key)}});
                Raise(100, "updated");
                return true;
            }

            std::unordered_map<std::string, std::any> set;
            for (const auto& field : changedFields)
            {
                auto it = reflected.find(field);
                if (it != reflected.end())
                {
                    set[field] = it->second;
                }
            }
            set["updated_at"] = NowSeconds();
            db_->Update(tbl, set, {{"param_key", std::string(key)}});
            Raise(100, "updated");
            return true;
        });
}

bool ParamStore::Delete(std::string_view table, std::string_view key)
{
    return Guard(
        [this, table, key]() -> bool
        {
            if (table.empty())
            {
                throw SQL::SQLAdapterInvalidArgumentError("ParamStore::Delete: table name required");
            }
            const std::string tbl(table);
            auto existing = db_->Select(tbl, {"param_id"}, {{"param_key", std::string(key)}}, std::nullopt, 1, 0);
            if (existing.empty())
            {
                return false;
            }
            db_->Delete(tbl, {{"param_key", std::string(key)}});
            Raise(100, "deleted");
            return true;
        });
}

}  // namespace HsBa::Slicer
