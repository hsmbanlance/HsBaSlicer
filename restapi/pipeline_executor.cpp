/** @file pipeline_executor.cpp
 * @brief Implements the kind-dispatched C-ABI wrapper; see pipeline_executor.hpp.
 * @author HsBa
 */
#include "pipeline_executor.hpp"

#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include "base/error.hpp"
#include "fileoperator/param_reflect.hpp"

#include "config_bridge.hpp"

#include "DllHsBaSlicer/custom_pipeline.h"
#include "DllHsBaSlicer/fdm_pipeline.h"
#include "DllHsBaSlicer/file_transfer_pipeline.h"
#include "DllHsBaSlicer/lom_pipeline.h"
#include "DllHsBaSlicer/pipeline_convert.h"
#include "DllHsBaSlicer/sla_pipeline.h"
#include "DllHsBaSlicer/slm_pipeline.h"
#include "DllHsBaSlicer/sls_pipeline.h"
#include "DllHsBaSlicer/tdp_pipeline.h"
#include "DllHsBaSlicer/waam_pipeline.h"


namespace HsBa::Slicer::RestApi
{
namespace
{
std::string SafeStr(const char* s)
{
    return s ? std::string(s) : std::string();
}

// Copy the file-transfer specific counters; a no-op for every other result type
// (detected via requires-expression, valid only because this is a template).
template <typename R>
void CaptureExtra(SliceOutcome& o, const R& r)
{
    if constexpr (requires { r.files_transferred; })
    {
        o.files_transferred = r.files_transferred;
        o.total_files = r.total_files;
    }
}

// ---- per-kind operations ---------------------------------------------------
#define HSBA_DEFINE_KIND(ENUM_NAME, WIRE, CFG, RES, CREATE, RUN, FREE, PATH_EXPR, INLINE_EXPR, LAYERS_EXPR, FT_EXPR,   \
                         FROM_PROTO, TO_PROTO, FREE_STRINGS)                                                           \
    namespace k_##ENUM_NAME                                                                                            \
    {                                                                                                                  \
        Utils::TypeInfo* typeinfo()                                                                                    \
        {                                                                                                              \
            return Utils::GetTypeInfo<CFG>();                                                                          \
        }                                                                                                              \
        void* create()                                                                                                 \
        {                                                                                                              \
            return new CFG(CREATE());                                                                                  \
        }                                                                                                              \
        void destroy(void* p)                                                                                          \
        {                                                                                                              \
            delete static_cast<CFG*>(p);                                                                               \
        }                                                                                                              \
        SliceOutcome run(const void* cfg, HsBaProgressCallback cb, void* ud)                                           \
        {                                                                                                              \
            RES r = RUN(static_cast<const CFG*>(cfg), cb, ud);                                                         \
            SliceOutcome o;                                                                                            \
            o.success = r.success;                                                                                     \
            o.elapsed_seconds = r.elapsed_seconds;                                                                     \
            o.error_message = SafeStr(r.error_message);                                                                \
            o.output_path = PATH_EXPR;                                                                                 \
            o.inline_content = INLINE_EXPR;                                                                            \
            o.total_layers = LAYERS_EXPR;                                                                              \
            CaptureExtra(o, r);                                                                                        \
            FREE(&r);                                                                                                  \
            return o;                                                                                                  \
        }                                                                                                              \
        int from_proto(const void* data, int size, void* cfg)                                                          \
        {                                                                                                              \
            return FROM_PROTO(data, size, static_cast<CFG*>(cfg));                                                     \
        }                                                                                                              \
        int to_proto(const void* cfg, void** out, int* size)                                                           \
        {                                                                                                              \
            return TO_PROTO(static_cast<const CFG*>(cfg), out, size);                                                  \
        }                                                                                                              \
        void free_strings(void* cfg)                                                                                   \
        {                                                                                                              \
            FREE_STRINGS(static_cast<CFG*>(cfg));                                                                      \
        }                                                                                                              \
    }

HSBA_DEFINE_KIND(FDM, "fdm", HsBaFdmPipelineConfig_t, HsBaFdmPipelineResult_t, HsBaCreateDefaultConfig,
                 HsBaRunFdmPipeline, HsBaFreePipelineResult, (std::string()), SafeStr(r.gcode_content), r.total_layers,
                 0, HsBaFdmConfigFromProtoBytes, HsBaFdmConfigToProtoBytes, HsBaFreeFdmConfigStrings)
HSBA_DEFINE_KIND(SLA, "sla", HsBaSlaPipelineConfig_t, HsBaSlaPipelineResult_t, HsBaCreateDefaultSlaConfig,
                 HsBaRunSlaPipeline, HsBaFreeSlaPipelineResult, SafeStr(r.export_path), (std::string()), r.total_layers,
                 0, HsBaSlaConfigFromProtoBytes, HsBaSlaConfigToProtoBytes, HsBaFreeSlaConfigStrings)
HSBA_DEFINE_KIND(SLS, "sls", HsBaSlsPipelineConfig_t, HsBaSlsPipelineResult_t, HsBaCreateDefaultSlsConfig,
                 HsBaRunSlsPipeline, HsBaFreeSlsPipelineResult, SafeStr(r.export_path), (std::string()), r.total_layers,
                 0, HsBaSlsConfigFromProtoBytes, HsBaSlsConfigToProtoBytes, HsBaFreeSlsConfigStrings)
HSBA_DEFINE_KIND(SLM, "slm", HsBaSlmPipelineConfig_t, HsBaSlmPipelineResult_t, HsBaCreateDefaultSlmConfig,
                 HsBaRunSlmPipeline, HsBaFreeSlmPipelineResult, SafeStr(r.export_path), (std::string()), r.total_layers,
                 0, HsBaSlmConfigFromProtoBytes, HsBaSlmConfigToProtoBytes, HsBaFreeSlmConfigStrings)
HSBA_DEFINE_KIND(LOM, "lom", HsBaLomPipelineConfig_t, HsBaLomPipelineResult_t, HsBaCreateDefaultLomConfig,
                 HsBaRunLomPipeline, HsBaFreeLomPipelineResult, SafeStr(r.export_path), (std::string()), r.total_layers,
                 0, HsBaLomConfigFromProtoBytes, HsBaLomConfigToProtoBytes, HsBaFreeLomConfigStrings)
HSBA_DEFINE_KIND(TDP, "tdp", HsBaTdpPipelineConfig_t, HsBaTdpPipelineResult_t, HsBaCreateDefaultTdpConfig,
                 HsBaRunTdpPipeline, HsBaFreeTdpPipelineResult, SafeStr(r.export_path), (std::string()), r.total_layers,
                 0, HsBaTdpConfigFromProtoBytes, HsBaTdpConfigToProtoBytes, HsBaFreeTdpConfigStrings)
HSBA_DEFINE_KIND(WAAM, "waam", HsBaWaamPipelineConfig_t, HsBaWaamPipelineResult_t, HsBaCreateDefaultWaamConfig,
                 HsBaRunWaamPipeline, HsBaFreeWaamPipelineResult, SafeStr(r.output_path), (std::string()),
                 r.total_layers, 0, HsBaWaamConfigFromProtoBytes, HsBaWaamConfigToProtoBytes, HsBaFreeWaamConfigStrings)
HSBA_DEFINE_KIND(CUSTOM, "custom", HsBaCustomPipelineConfig_t, HsBaCustomPipelineResult_t,
                 HsBaCreateDefaultCustomConfig, HsBaRunCustomPipeline, HsBaFreeCustomPipelineResult,
                 SafeStr(r.output_path), SafeStr(r.result_string), r.total_layers, 0, HsBaCustomConfigFromProtoBytes,
                 HsBaCustomConfigToProtoBytes, HsBaFreeCustomConfigStrings)
HSBA_DEFINE_KIND(FILETRANSFER, "filetransfer", HsBaFileTransferPipelineConfig_t, HsBaFileTransferPipelineResult_t,
                 HsBaCreateDefaultFileTransferConfig, HsBaRunFileTransferPipeline, HsBaFreeFileTransferPipelineResult,
                 (std::string()), (std::string()), 0, 1, HsBaFileTransferConfigFromProtoBytes,
                 HsBaFileTransferConfigToProtoBytes, HsBaFreeFileTransferConfigStrings)

using TypeinfoFn = Utils::TypeInfo* (*)();
using CreateFn = void* (*)();
using DestroyFn = void (*)(void*);
using RunFn = SliceOutcome (*)(const void*, HsBaProgressCallback, void*);
using FromProtoFn = int (*)(const void*, int, void*);
using ToProtoFn = int (*)(const void*, void**, int*);
using FreeStringsFn = void (*)(void*);

struct KindOps
{
    const char* name = "";
    TypeinfoFn typeinfo = nullptr;
    CreateFn create = nullptr;
    DestroyFn destroy = nullptr;
    RunFn run = nullptr;
    FromProtoFn from_proto = nullptr;
    ToProtoFn to_proto = nullptr;
    FreeStringsFn free_strings = nullptr;
};

#define HSBA_KIND_ENTRY(ENUM_NAME, WIRE)                                                                               \
    KindOps                                                                                                            \
    {                                                                                                                  \
        WIRE, &k_##ENUM_NAME::typeinfo, &k_##ENUM_NAME::create, &k_##ENUM_NAME::destroy, &k_##ENUM_NAME::run,          \
            &k_##ENUM_NAME::from_proto, &k_##ENUM_NAME::to_proto, &k_##ENUM_NAME::free_strings,                        \
    }

// The table below is indexed positionally, so the enum order must match exactly.
static_assert(HSBA_PIPELINE_FDM == 0, "kind order drift");
static_assert(HSBA_PIPELINE_SLA == 1, "kind order drift");
static_assert(HSBA_PIPELINE_SLS == 2, "kind order drift");
static_assert(HSBA_PIPELINE_SLM == 3, "kind order drift");
static_assert(HSBA_PIPELINE_LOM == 4, "kind order drift");
static_assert(HSBA_PIPELINE_TDP == 5, "kind order drift");
static_assert(HSBA_PIPELINE_WAAM == 6, "kind order drift");
static_assert(HSBA_PIPELINE_CUSTOM == 7, "kind order drift");
static_assert(HSBA_PIPELINE_FILETRANSFER == 8, "kind order drift");

const std::array<KindOps, HSBA_PIPELINE_KIND_COUNT>& KindTable()
{
    static const std::array<KindOps, HSBA_PIPELINE_KIND_COUNT> table = {
        HSBA_KIND_ENTRY(FDM, "fdm"),   HSBA_KIND_ENTRY(SLA, "sla"),       HSBA_KIND_ENTRY(SLS, "sls"),
        HSBA_KIND_ENTRY(SLM, "slm"),   HSBA_KIND_ENTRY(LOM, "lom"),       HSBA_KIND_ENTRY(TDP, "tdp"),
        HSBA_KIND_ENTRY(WAAM, "waam"), HSBA_KIND_ENTRY(CUSTOM, "custom"), HSBA_KIND_ENTRY(FILETRANSFER, "filetransfer"),
    };
    return table;
}
}  // namespace

bool IsValidKind(HsBaPipelineKind kind) noexcept
{
    return kind >= HSBA_PIPELINE_FDM && kind < HSBA_PIPELINE_KIND_COUNT;
}

std::string_view KindName(HsBaPipelineKind kind)
{
    return IsValidKind(kind) ? std::string_view{KindTable()[kind].name} : std::string_view{};
}

HsBaPipelineKind ParseKind(const std::string& name)
{
    for (int i = 0; i < HSBA_PIPELINE_KIND_COUNT; ++i)
    {
        if (name == KindTable()[i].name)
        {
            return static_cast<HsBaPipelineKind>(i);
        }
    }
    return HSBA_PIPELINE_KIND_COUNT;
}

Utils::TypeInfo* ConfigTypeInfo(HsBaPipelineKind kind)
{
    if (!IsValidKind(kind))
    {
        throw InvalidArgumentError(std::string("pipeline_executor: unsupported kind ") +
                                   std::to_string(static_cast<int>(kind)));
    }
    return KindTable()[kind].typeinfo();
}

void* CreateDefaultConfig(HsBaPipelineKind kind)
{
    if (!IsValidKind(kind))
    {
        throw InvalidArgumentError(std::string("pipeline_executor: unsupported kind ") +
                                   std::to_string(static_cast<int>(kind)));
    }
    return KindTable()[kind].create();
}

void DestroyConfig(HsBaPipelineKind kind, void* config)
{
    if (IsValidKind(kind) && config)
    {
        KindTable()[kind].destroy(config);
    }
}

SliceOutcome RunPipeline(HsBaPipelineKind kind, const void* config, HsBaProgressCallback callback, void* user_data)
{
    if (!IsValidKind(kind))
    {
        throw InvalidArgumentError(std::string("pipeline_executor: unsupported kind ") +
                                   std::to_string(static_cast<int>(kind)));
    }
    if (!config)
    {
        throw InvalidArgumentError("pipeline_executor: null config");
    }
    return KindTable()[kind].run(config, callback, user_data);
}

std::string ConfigFromProto(HsBaPipelineKind kind, const void* data, std::size_t size, DataFormat fmt)
{
    if (!IsValidKind(kind))
    {
        throw InvalidArgumentError(std::string("pipeline_executor: unsupported kind ") +
                                   std::to_string(static_cast<int>(kind)));
    }
    const KindOps& ops = KindTable()[kind];
    void* cfg = ops.create();
    // Proto decoding malloc's string fields; they must be released with the
    // matching HsBaFree*ConfigStrings, never the arena.
    if (ops.from_proto(data, static_cast<int>(size), cfg) == 0)
    {
        ops.destroy(cfg);
        throw RuntimeError("pipeline_executor: proto payload failed to parse");
    }
    std::string text = SerializeConfig(ops.typeinfo(), cfg, fmt);
    ops.free_strings(cfg);
    ops.destroy(cfg);
    return text;
}

std::vector<unsigned char> ConfigToProto(HsBaPipelineKind kind, const std::string& text, DataFormat inFmt)
{
    if (!IsValidKind(kind))
    {
        throw InvalidArgumentError(std::string("pipeline_executor: unsupported kind ") +
                                   std::to_string(static_cast<int>(kind)));
    }
    const KindOps& ops = KindTable()[kind];
    void* cfg = ops.create();
    // Deserialization fills const char* fields from the arena, which must
    // outlive the to_proto call; do NOT run free_strings (would free arena new[]
    // buffers with the wrong deallocator).
    StringArena arena;
    DeserializeConfig(ops.typeinfo(), text, inFmt, cfg, arena);
    void* out = nullptr;
    int out_size = 0;
    if (ops.to_proto(cfg, &out, &out_size) == 0 || out == nullptr)
    {
        ops.destroy(cfg);
        throw RuntimeError("pipeline_executor: failed to encode config to proto");
    }
    // The proto buffer is malloc-allocated per pipeline_convert.h contract.
    const auto* first = static_cast<const unsigned char*>(out);
    std::vector<unsigned char> bytes(first, first + out_size);
    std::free(out);
    ops.destroy(cfg);
    return bytes;
}

}  // namespace HsBa::Slicer::RestApi
