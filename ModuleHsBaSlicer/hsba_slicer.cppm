/// @file hsba_slicer.cppm
/// @brief C++20 module interface for HsBaSlicer (single-file: declarations + definitions).
///
/// Provides a class-based C++ API wrapping LibHsBaSlicer's free functions.
/// Consumers use `import hsba.slicer;` to access the full slicing pipeline.
///
/// NOTE: This is a single interface unit (no separate implementation unit)
/// to avoid MSVC C2572 errors caused by implicit-import std:: redefinition.

module;

// ---- Global module fragment (not exported) ----
#ifdef _WIN32
#include <windows.h>
#endif

// Standard library
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// Third-party: Clipper2
#include <clipper2/clipper.h>
#include <clipper2/clipper.offset.h>

// Third-party: Eigen
#include <Eigen/Core>
#include <Eigen/Geometry>

// Third-party: Lua
#include <lua.hpp>

// Project headers (types + declarations)
#include "pipelinetypes/pipeline_types.h"
#include "2D/IntPolygon.hpp"
#include "2D/FloatPolygons.hpp"
#include "2D/PolygonFill.hpp"
#include "base/IModel.hpp"
#include "meshmodel/FullTopoModel.hpp"
#include "paths/IPath.hpp"
#include "paths/pointspath.hpp"
#include "paths/gcodepath.hpp"
#include "support/SupportConfig.hpp"
#include "LibHsBaSlicer/export.h"
#include "LibHsBaSlicer/version_info.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Path/sls_export.hpp"
#include "LibHsBaSlicer/Path/waam_export.hpp"
#include "LibHsBaSlicer/Floor/sla_floor.hpp"
#include "LibHsBaSlicer/Transfer/file_transfer.hpp"
#include "LibHsBaSlicer/ParamStore/param_store_ops.hpp"
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"
#include "LibHsBaSlicer/Extends/EventSourceFunction.hpp"
#include "LibHsBaSlicer/Extends/lua_pipeline.hpp"
#include "LibHsBaSlicer/Path/spiral_path.hpp"
#include "LibHsBaSlicer/ParamStore/config_json_ops.hpp"

// Pipeline orchestration kernel (shared with the C ABI Dll layer)
#include "LibHsBaSlicer/Pipeline/kernel_types.hpp"
#include "LibHsBaSlicer/Pipeline/fdm_pipeline.hpp"
#include "LibHsBaSlicer/Pipeline/sla_pipeline.hpp"
#include "LibHsBaSlicer/Pipeline/sls_pipeline.hpp"
#include "LibHsBaSlicer/Pipeline/slm_pipeline.hpp"
#include "LibHsBaSlicer/Pipeline/lom_pipeline.hpp"
#include "LibHsBaSlicer/Pipeline/tdp_pipeline.hpp"
#include "LibHsBaSlicer/Pipeline/waam_pipeline.hpp"
#include "LibHsBaSlicer/Pipeline/custom_pipeline.hpp"

// ---- Module interface ----
export module hsba.slicer;

export namespace HsBa::Slicer
{

// ===========================================================================
// Exception
// ===========================================================================

/// @brief Base exception for all slicer errors.
class SlicerError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// ===========================================================================
// Type aliases (re-exported for consumer convenience)
// ===========================================================================
// These aliases already exist in namespace HsBa::Slicer through the GMF
// headers (2D/IntPolygon.hpp, 2D/FloatPolygons.hpp,
// LibHsBaSlicer/Extends/LuaAddFunction.hpp). GMF entities belong to the global
// module and cannot be exported; re-declaring the same qualified name in the
// module purview is a conflicting declaration, which GCC (>= 15) and Clang
// reject. MSVC still tolerates it, so the re-export is kept only there
// (clang-cl defines _MSC_VER too, but follows Clang's stricter checking).
// On Clang/GCC consumers must #include those headers to name the types.
#if defined(_MSC_VER) && !defined(__clang__)
using Point2    = Clipper2Lib::Point64;
using Polygon   = Clipper2Lib::Path64;
using Polygons  = Clipper2Lib::Paths64;
using Point2D   = Clipper2Lib::PointD;
using PolygonD  = Clipper2Lib::PathD;
using PolygonsD = Clipper2Lib::PathsD;
#endif

// Re-export pipeline_types.h config/result types
using ::HsBaFillMode_t;
using ::HsBaSupportPattern_t;
using ::HsBaGCodeFirmware_t;
using ::HsBaSlaSupportPattern_t;
using ::HsBaSlaImageType_t;
using ::HsBaFdmPipelineConfig_t;
using ::HsBaFdmPipelineResult_t;
using ::HsBaSlaPipelineConfig_t;
using ::HsBaSlaPipelineResult_t;
using ::HsBaSlsPipelineConfig_t;
using ::HsBaSlsPipelineResult_t;
using ::HsBaFileTransferPipelineConfig_t;
using ::HsBaFileTransferPipelineResult_t;
using ::HsBaCustomPipelineConfig_t;
using ::HsBaCustomPipelineResult_t;
using ::HsBaSlmPipelineConfig_t;
using ::HsBaSlmPipelineResult_t;
using ::HsBaLomPipelineConfig_t;
using ::HsBaLomPipelineResult_t;
using ::HsBaTdpPipelineConfig_t;
using ::HsBaTdpPipelineResult_t;
using ::HsBaWaamPipelineConfig_t;
using ::HsBaWaamPipelineResult_t;

// Re-export support config types into HsBa::Slicer namespace
using Support::SupportConfig;
using Support::FdmSupportConfig;
using Support::SlaSupportConfig;

// Re-export Lua registration function type (MSVC only, see alias note above;
// elsewhere it comes from LibHsBaSlicer/Extends/LuaAddFunction.hpp)
#if defined(_MSC_VER) && !defined(__clang__)
using LuaRegFunc = std::function<void(lua_State*)>;
#endif

// Re-export event callback function type
using ZipperEventCallbackFunc = std::function<void(double, std::string_view)>;
using DBEventCallbackFunc = std::function<void(std::string_view, std::string_view)>;

/// @brief Create default FDM pipeline config.
HsBaFdmPipelineConfig_t defaultFdmConfig();
/// @brief Create default SLA pipeline config.
HsBaSlaPipelineConfig_t defaultSlaConfig();
/// @brief Create default SLS pipeline config.
HsBaSlsPipelineConfig_t defaultSlsConfig();
/// @brief Create default file transfer pipeline config.
HsBaFileTransferPipelineConfig_t defaultFileTransferConfig();
/// @brief Create default custom Lua pipeline config.
HsBaCustomPipelineConfig_t defaultCustomConfig();
/// @brief Create default SLM pipeline config.
HsBaSlmPipelineConfig_t defaultSlmConfig();
/// @brief Create default LOM pipeline config.
HsBaLomPipelineConfig_t defaultLomConfig();
/// @brief Create default 3DP pipeline config.
HsBaTdpPipelineConfig_t defaultTdpConfig();
/// @brief Create default WAAM pipeline config.
HsBaWaamPipelineConfig_t defaultWaamConfig();

// ===========================================================================
// Shared async / geometry types (③ cooperative job control, ④ geometry export)
// ===========================================================================

/// @brief Progress report callback: percent 0-100 and human readable stage text.
using ProgressFunc = std::function<void(int, std::string_view)>;

/// @brief Terminal state of a pipeline run (mirrors the kernel's Output flags).
enum class RunStatus
{
    Ok,         ///< Completed successfully
    Cancelled,  ///< Stopped by a cooperative cancel request
    Failed,     ///< Pipeline reported a failure
};

/// @brief Layer geometry a run captured (④). Native counterpart of the C ABI
///        geometry snapshot: valid only when the run produced geometry.
struct Geometry
{
    bool valid = false;
    std::vector<PolygonsD> outlines;   ///< Part contours per layer
    std::vector<PolygonsD> fills;      ///< FDM infill / SLA floor rings per layer
    std::vector<PolygonsD> supports;   ///< Support cross sections per layer
    std::vector<double> zs;            ///< Layer platform heights (mm)
    std::vector<SpiralPoint> path;     ///< Continuous (spiral) path; empty when not produced
};

/// @brief Worker result carrying the mapped output plus terminal status.
template <class ResultT>
struct JobOutcome
{
    RunStatus status = RunStatus::Ok;
    ResultT value{};
    std::string error;
};

enum class JobState
{
    Pending,
    Running,
    Succeeded,
    Failed,
    Cancelled,
};

/// @brief Native asynchronous pipeline job with cooperative cancellation (③).
///
/// A Job owns a worker thread that drives the shared kernel; @ref cancel sets the
/// flag the kernel polls at stage boundaries and inside per-layer loops, so a run
/// stops mid-way rather than only before it starts. Move-only: exactly one owner
/// joins the thread (in @ref get or the destructor).
template <class ResultT>
class Job
{
public:
    Job() = default;
    Job(Job&&) = default;
    Job& operator=(Job&&) = default;
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;

    ~Job()
    {
        if (impl_)
            impl_->join();
    }

    /// @brief Launch a job. @p fn runs on the worker thread: it receives the
    ///        cooperative cancel flag and a progress emitter, and returns the
    ///        mapped outcome. @p progress forwards kernel progress to the caller.
    template <class Fn>
    static Job Start(Fn fn, ProgressFunc progress = {})
    {
        Job job;
        auto impl = std::make_shared<Impl>();
        impl->state.store(JobState::Running);
        impl->worker = std::thread([impl, fn = std::move(fn), progress = std::move(progress)]() {
            ProgressFunc emit = [impl, &progress](int percent, std::string_view stage) {
                impl->progress_percent.store(percent);
                if (progress)
                    progress(percent, stage);
            };
            JobOutcome<ResultT> outcome = fn(impl->cancel, emit);
            std::lock_guard<std::mutex> lock(impl->mtx);
            impl->error = std::move(outcome.error);
            if (outcome.status == RunStatus::Ok)
            {
                impl->result.emplace(std::move(outcome.value));
                impl->state.store(JobState::Succeeded);
            }
            else if (outcome.status == RunStatus::Cancelled)
            {
                impl->state.store(JobState::Cancelled);
            }
            else
            {
                impl->state.store(JobState::Failed);
            }
            impl->cv.notify_all();
        });
        job.impl_ = std::move(impl);
        return job;
    }

    /// @brief Current job state.
    JobState state() const
    {
        return impl_ ? impl_->state.load() : JobState::Pending;
    }

    /// @brief Whether the job reached a terminal state.
    bool done() const
    {
        const JobState s = state();
        return s == JobState::Succeeded || s == JobState::Failed || s == JobState::Cancelled;
    }

    /// @brief Last progress percent reported by the kernel (0-100).
    int progress() const
    {
        return impl_ ? impl_->progress_percent.load() : 0;
    }

    /// @brief Request cooperative cancellation; the kernel stops at the next checkpoint.
    void cancel()
    {
        if (impl_)
            impl_->cancel.store(true);
    }

    /// @brief Block until the job finishes.
    void wait() const
    {
        if (!impl_)
            return;
        std::unique_lock<std::mutex> lock(impl_->mtx);
        impl_->cv.wait(lock, [this] { return done(); });
    }

    /// @brief Block up to @p ms for completion; returns true when done.
    bool wait(std::chrono::milliseconds ms) const
    {
        if (!impl_)
            return true;
        std::unique_lock<std::mutex> lock(impl_->mtx);
        return impl_->cv.wait_for(lock, ms, [this] { return done(); });
    }

    /// @brief Join the worker and return the result.
    /// @throws SlicerError when the job was cancelled or failed.
    ResultT get()
    {
        if (!impl_)
            throw SlicerError("Job not started");
        impl_->join();
        switch (impl_->state.load())
        {
        case JobState::Cancelled:
            throw SlicerError(impl_->error.empty() ? "Job cancelled" : impl_->error);
        case JobState::Failed:
            throw SlicerError(impl_->error.empty() ? "Job failed" : impl_->error);
        case JobState::Succeeded:
            if (impl_->result)
                return std::move(*impl_->result);
            [[fallthrough]];
        default:
            throw SlicerError("Job produced no result");
        }
    }

private:
    struct Impl
    {
        std::thread worker;
        mutable std::mutex mtx;
        std::condition_variable cv;
        std::atomic<JobState> state{JobState::Pending};
        std::atomic<bool> cancel{false};
        std::atomic<int> progress_percent{0};
        std::optional<ResultT> result;
        std::string error;
        bool joined = false;

        void join()
        {
            if (worker.joinable())
                worker.join();
            joined = true;
        }
    };

    std::shared_ptr<Impl> impl_;
};

// ===========================================================================
// Model (RAII wrapper)
// ===========================================================================

/// @brief RAII model handle. Loads on construction, removes on destruction.
class Model
{
public:
    /// @brief Load a model into the pool.
    /// @throws SlicerError on failure.
    Model(std::string name, const std::filesystem::path& file);
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    inline Model(Model&& other) noexcept;
    inline Model& operator=(Model&& other) noexcept;

    /// @brief Get model bounding box and volume.
    inline ModelInfo info() const;

    /// @brief Apply translation.
    inline void translate(const Eigen::Vector3f& t);
    /// @brief Apply rotation.
    inline void rotate(const Eigen::Quaternionf& r);
    /// @brief Apply uniform scale.
    inline void scale(float s);
    /// @brief Apply non-uniform scale.
    inline void scale(const Eigen::Vector3f& s);

    /// @brief Slice at given height (integer coordinates).
    inline Polygons slice(float height) const;
    /// @brief Slice at given height (double coordinates).
    inline PolygonsD sliceD(float height) const;

    /// @brief Access underlying IModel.
    inline const IModel& raw() const;
    /// @brief Model name.
    inline const std::string& name() const;

    // ---- Pool queries (②, mirror the C ABI model-management surface) ----

    /// @brief Whether a model with this name exists (local pool or published).
    static bool exists(std::string_view name);
    /// @brief Number of models in the caller's pool.
    static std::size_t count();
    /// @brief Names of all models in the pool.
    static std::vector<std::string> names();
    /// @brief Remove models no longer referenced externally; returns count removed.
    static std::size_t cleanup();
    /// @brief Adopt an already-loaded model by name (does not load a file).
    /// @throws SlicerError when no model resolves under @p name.
    static Model fromExisting(std::string name);

    /// @brief Expose this pooled model to by-name lookups from other threads.
    bool publish() const;
    /// @brief Withdraw the process-wide reference created by publish().
    bool unpublish() const;

    /// @brief Export the triangle mesh (vertices N x 3 mm, faces M x 3 zero-based).
    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> triangles() const;

private:
    struct AdoptTag {};
    Model(AdoptTag, std::string name, std::shared_ptr<IModel> ptr);

    std::string name_;
    std::shared_ptr<IModel> ptr_;
};

// ---- Advanced model operations (②, kernel-gated like model_preprocess.hpp) ----

/// @brief Whether this build can evaluate text CSG scripts.
inline bool hasTextCsgSupport();

#ifdef USE_CGAL
/// @brief Boolean union; the result is stored under @p resultName and adopted.
inline Model booleanUnion(const std::string& left, const std::string& right, const std::string& resultName);
/// @brief Boolean intersection.
inline Model booleanIntersection(const std::string& left, const std::string& right, const std::string& resultName);
/// @brief Boolean difference (left - right).
inline Model booleanDifference(const std::string& left, const std::string& right, const std::string& resultName);
/// @brief Boolean XOR.
inline Model booleanXor(const std::string& left, const std::string& right, const std::string& resultName);
/// @brief Thick-solid (inward shell) of a B-Rep model stored under @p resultName.
inline Model thickSolidModel(const std::string& source, const std::string& resultName, float thickness);
#endif  // USE_CGAL

#ifdef USE_OCCT
/// @brief Build a B-Rep model from an OCCT DRAW (text CSG) script.
inline Model buildCsgModel(const std::string& name, std::string_view script, std::string_view shapeName = {});
#endif  // USE_OCCT

// ===========================================================================
// FDM Pipeline
// ===========================================================================

/// @brief FDM pipeline result (C++ style).
struct FdmResult
{
    std::string gcode;
    int total_layers = 0;
    Geometry geometry;   ///< Captured layer geometry (④); valid only when produced
};

/// @brief FDM full pipeline: Slice -> Support -> Fill -> Path.
class FdmPipeline
{
public:
    explicit FdmPipeline(HsBaFdmPipelineConfig_t cfg = defaultFdmConfig());

    /// @brief Run full pipeline on a model via the shared kernel.
    /// @throws SlicerError on failure.
    FdmResult run(const Model& model, ProgressFunc progress = {}) const;

    /// @brief Launch the pipeline asynchronously with cooperative cancellation (③).
    Job<FdmResult> runAsync(const Model& model, ProgressFunc progress = {}) const;

    /// @brief Continuous spiralized outer-wall path for this model (④).
    std::vector<SpiralPoint> spiralPath(const Model& model) const;

    /// @brief Slice all layers.
    inline std::vector<Polygons> sliceAll(const Model& model) const;
    /// @brief Generate supports for all layers.
    inline std::vector<PolygonsD> generateSupports(const std::vector<PolygonsD>& layers) const;
    /// @brief Fill a single layer contour.
    inline Polygons fill(const Polygons& contour) const;
    /// @brief Generate G-code path from layer data.
    inline std::unique_ptr<GCodePath> generatePath(const std::vector<LayerPathData>& data) const;

private:
    HsBaFdmPipelineConfig_t cfg_;
};

// ===========================================================================
// SLA Pipeline
// ===========================================================================

/// @brief SLA pipeline result (C++ style).
struct SlaResult
{
    bool saved        = false;
    int total_layers  = 0;
    std::string export_path;   ///< Produced package path
    Geometry geometry;         ///< Captured layer geometry (④)
};

/// @brief SLA full pipeline: Slice -> Support -> Floor -> Render -> Package.
class SlaPipeline
{
public:
    explicit SlaPipeline(HsBaSlaPipelineConfig_t cfg = defaultSlaConfig());

    /// @brief Run full SLA pipeline via the shared kernel.
    /// @throws SlicerError on failure.
    /// NOTE: intentionally not inline - the body reaches the module-internal
    /// kernel-bridge helpers, so an inlined copy in a consumer TU would not link.
    SlaResult run(const Model& model, const std::filesystem::path& output_zip,
                  ProgressFunc progress = {}) const;

    /// @brief Launch the pipeline asynchronously with cooperative cancellation (③).
    Job<SlaResult> runAsync(const Model& model, const std::filesystem::path& output_zip,
                            ProgressFunc progress = {}) const;

    /// @brief Generate floor/raft from bottom layer.
    inline Polygons generateFloor(const Polygons& bottom_layer) const;
    /// @brief Render polygons to image file.
    inline  bool renderLayer(const PolygonsD& polys, int width, int height,
                     const std::string& out_path) const;
    /// @brief Save SLA package to zip.
    inline bool savePackage(const SlaPackage& pkg, const std::string& output_zip) const;

private:
    HsBaSlaPipelineConfig_t cfg_;
};

// ===========================================================================
// SLS Pipeline
// ===========================================================================

/// @brief SLS pipeline result (C++ style).
struct SlsResult
{
    bool saved        = false;
    int total_layers  = 0;
    std::string export_path;
    Geometry geometry;
};

/// @brief SLS pipeline (Lua-driven export).
class SlsPipeline
{
public:
    explicit SlsPipeline(HsBaSlsPipelineConfig_t cfg = defaultSlsConfig());

    /// @brief Run SLS export via the shared kernel.
    /// @throws SlicerError on failure.
    SlsResult run(const Model& model, ProgressFunc progress = {}) const;

    /// @brief Launch the pipeline asynchronously with cooperative cancellation (③).
    Job<SlsResult> runAsync(const Model& model, ProgressFunc progress = {}) const;

private:
    HsBaSlsPipelineConfig_t cfg_;
};

// ===========================================================================
// SLM Pipeline (metal powder-bed, Lua-driven export like SLS)
// ===========================================================================

/// @brief SLM pipeline result (C++ style).
struct SlmResult
{
    bool saved        = false;
    int total_layers  = 0;
    std::string export_path;
    Geometry geometry;
};

/// @brief SLM pipeline (Lua-driven export).
class SlmPipeline
{
public:
    explicit SlmPipeline(HsBaSlmPipelineConfig_t cfg = defaultSlmConfig());

    /// @brief Run SLM export via the shared kernel.
    /// @throws SlicerError on failure.
    SlmResult run(const Model& model, ProgressFunc progress = {}) const;

    /// @brief Launch the pipeline asynchronously with cooperative cancellation (③).
    Job<SlmResult> runAsync(const Model& model, ProgressFunc progress = {}) const;

private:
    HsBaSlmPipelineConfig_t cfg_;
};

// ===========================================================================
// LOM Pipeline (laminated sheets, Lua-driven export like SLS)
// ===========================================================================

/// @brief LOM pipeline result (C++ style).
struct LomResult
{
    bool saved        = false;
    int total_layers  = 0;
    std::string export_path;
    Geometry geometry;
};

/// @brief LOM pipeline (Lua-driven export).
class LomPipeline
{
public:
    explicit LomPipeline(HsBaLomPipelineConfig_t cfg = defaultLomConfig());

    /// @brief Run LOM export via the shared kernel.
    /// @throws SlicerError on failure.
    LomResult run(const Model& model, ProgressFunc progress = {}) const;

    /// @brief Launch the pipeline asynchronously with cooperative cancellation (③).
    Job<LomResult> runAsync(const Model& model, ProgressFunc progress = {}) const;

private:
    HsBaLomPipelineConfig_t cfg_;
};

// ===========================================================================
// 3DP Pipeline (binder jetting, Lua-driven export like SLS)
// ===========================================================================

/// @brief 3DP pipeline result (C++ style).
struct TdpResult
{
    bool saved        = false;
    int total_layers  = 0;
    std::string export_path;
    Geometry geometry;
};

/// @brief 3DP pipeline (Lua-driven export).
class TdpPipeline
{
public:
    explicit TdpPipeline(HsBaTdpPipelineConfig_t cfg = defaultTdpConfig());

    /// @brief Run 3DP export via the shared kernel.
    /// @throws SlicerError on failure.
    TdpResult run(const Model& model, ProgressFunc progress = {}) const;

    /// @brief Launch the pipeline asynchronously with cooperative cancellation (③).
    Job<TdpResult> runAsync(const Model& model, ProgressFunc progress = {}) const;

    /// @brief Continuous spiralized outer-wall path for this model (④).
    std::vector<SpiralPoint> spiralPath(const Model& model) const;

private:
    HsBaTdpPipelineConfig_t cfg_;
};

// ===========================================================================
// WAAM Pipeline (wire arc deposition, robot-program export)
// ===========================================================================

/// @brief WAAM pipeline result (C++ style).
struct WaamResult
{
    bool saved        = false;
    int total_layers  = 0;
    std::string output_path;
    Geometry geometry;
};

/// @brief WAAM pipeline (robot path export).
class WaamPipeline
{
public:
    explicit WaamPipeline(HsBaWaamPipelineConfig_t cfg = defaultWaamConfig());

    /// @brief Run WAAM robot-program export via the shared kernel.
    /// @throws SlicerError on failure.
    WaamResult run(const Model& model, ProgressFunc progress = {}) const;

    /// @brief Launch the pipeline asynchronously with cooperative cancellation (③).
    Job<WaamResult> runAsync(const Model& model, ProgressFunc progress = {}) const;

    /// @brief Continuous spiralized outer-wall path for this model (④).
    std::vector<SpiralPoint> spiralPath(const Model& model) const;

private:
    HsBaWaamPipelineConfig_t cfg_;
};

// ===========================================================================
// File Transfer Pipeline
// ===========================================================================

/// @brief File transfer result (C++ style).
struct FileTransferOutcome
{
    bool success = false;
    int files_transferred = 0;
    int total_files = 0;
};

/// @brief File transfer pipeline: Validate -> Connect -> Transfer.
class FileTransferPipeline
{
public:
    explicit FileTransferPipeline(HsBaFileTransferPipelineConfig_t cfg = defaultFileTransferConfig());

    /// @brief Run file transfer to remote executor.
    /// @throws SlicerError on failure.
    inline FileTransferOutcome run() const;

    /// @brief Run file transfer with progress reporting.
    /// @throws SlicerError on failure.
    inline FileTransferOutcome run(FileTransferProgressFunc progress) const;

private:
    HsBaFileTransferPipelineConfig_t cfg_;
};

// ===========================================================================
// Param Store Pipeline (persist / reuse process parameters)
// ===========================================================================

/// @brief Pipeline config kind for the parameter store (order aligned with
///        HsBaPipelineKind / PipelineConfigTag).
enum class ParamStoreKind
{
    Fdm,
    Sla,
    Sls,
    Slm,
    Lom,
    Tdp,
    Waam,
    Custom,
    FileTransfer,
};

/// @brief Storage backend; mobile builds only support Sqlite.
enum class ParamStoreBackend
{
    Sqlite,
    MySql,
    PostgreSql,
};

/// @brief Connection parameters. Sqlite uses sqlitePath; MySql/PostgreSql use
///        host/port/user/password/database.
struct ParamStoreConnection
{
    ParamStoreBackend backend = ParamStoreBackend::Sqlite;
    std::string sqlitePath;
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    unsigned port = 0;  ///< 0 means use the adapter's default port
    std::string cryptoKey;  ///< Optional at-rest encryption password; empty means plaintext storage
};

/// @brief Process-parameter persistence pipeline: write (Save) and read (Load) a
///        PipelineConfig struct via the Lib-layer ParamStore.
class ParamStorePipeline
{
public:
    explicit ParamStorePipeline(ParamStoreConnection conn);

    /// @brief Upsert a config struct under @p key; returns the stored row id.
    ///        @p cfg must point to the C config struct matching @p kind.
    /// @throws SlicerError on failure.
    /// NOTE: out-of-line definitions (not inline) so the bodies can reach the
    /// module-internal ToLibKind/ToLibConn helpers from a single linked copy.
    long long save(ParamStoreKind kind, const void* cfg, const std::string& key,
                   const std::string& table = {}) const;

    /// @brief Load config under @p key into @p outCfg (a C config struct matching @p kind).
    ///        The const char* fields of outCfg are heap-allocated by the library and must
    ///        be released with freeLoaded().
    /// @throws SlicerError on failure or when the key is not found.
    long long load(ParamStoreKind kind, const std::string& key, void* outCfg,
                   const std::string& table = {}) const;

    /// @brief Release the heap strings produced by load() for this config struct.
    void freeLoaded(ParamStoreKind kind, void* cfg) const;

private:
    ParamStoreConnection conn_;
};

// ===========================================================================
// Config JSON codec (①, thin forwarding over the Lib reflection-driven codec)
// ===========================================================================

/// @brief Pretty-printed JSON of a kind's factory-default config.
std::string defaultConfigJson(ParamStoreKind kind);
/// @brief Serialize @p cfg (a C config struct matching @p kind) to pretty JSON.
std::string configToJson(ParamStoreKind kind, const void* cfg);
/// @brief Land @p json into @p cfg (a default-initialized struct matching @p kind).
///        On success the non-null const char* fields are malloc-owned; release them
///        with ParamStorePipeline::freeLoaded() before discarding the struct.
/// NOTE: these are out-of-line (not inline) so their bodies can reach the
/// module-internal ToLibKind helper from a single linked copy.
bool configFromJson(ParamStoreKind kind, std::string_view json, void* cfg);

// ===========================================================================
// Custom Lua Pipeline (fully Lua-driven workflow)
// ===========================================================================

/// @brief Custom Lua pipeline result (C++ style).
struct CustomLuaResult
{
    bool success = false;
    int total_layers = 0;
    std::string output_path;   ///< Output path reported by the Lua script
    std::string result_string; ///< String returned by the Lua entry function
};

/// @brief Fully Lua-driven pipeline: the Lua entry function orchestrates all
///        stages through the global `HsBa` table (loadModel/slice/fill/
///        fdmSupport/toGcode/saveSlsPackage/...). The stage order is defined
///        entirely by the script.
class CustomLuaPipeline
{
public:
    explicit CustomLuaPipeline(HsBaCustomPipelineConfig_t cfg = defaultCustomConfig());

    /// @brief Set the progress callback driven by `HsBa.progress()` in Lua.
    void setProgressFunc(std::function<void(int, std::string_view)> func);

    /// @brief Run the custom pipeline via the shared kernel.
    /// @throws SlicerError on failure.
    CustomLuaResult run() const;

    /// @brief Launch the custom pipeline asynchronously with cooperative cancellation (③).
    /// @throws SlicerError when no script/source is configured.
    Job<CustomLuaResult> runAsync();

private:
    HsBaCustomPipelineConfig_t cfg_;
    std::function<void(int, std::string_view)> progress_;
};

// ===========================================================================
// Lua custom functions
// ===========================================================================

/// @brief Custom fill via Lua script file.
inline Polygons luaCustomFill(const Polygons& poly, const std::string& script_path,
                       const std::string& func_name = "generate_fill",
                       double line_thickness = 0.5);

/// @brief Custom floor via Lua script file.
inline Polygons luaCustomFloor(const Polygons& bottom_layer, const std::string& script_path,
                        const std::string& func_name, const SlaFloorConfig& config);

/// @brief Custom support via Lua script.
inline std::vector<PolygonsD> luaCustomSupport(const std::vector<PolygonsD>& layers,
                                         const SupportConfig& config,
                                         std::string_view script,
                                         std::string_view func_name = "generate_support");

// ===========================================================================
// External Lua function registration
// ===========================================================================

/// @brief Register an external 2D Lua function (available in Support, Fill, SLA Output stages).
inline void add2DFunction(LuaRegFunc func);

/// @brief Register an external 3D Lua function (available in Slice, Support stages).
inline void add3DFunction(LuaRegFunc func);

/// @brief Register an external File Lua function (available in SLS Output, SLA Output stages).
inline void addFileFunction(LuaRegFunc func);

/// @brief Register an event callback by name (e.g. "zipper.on_add").
inline void addEventCallback(const std::string& event_name, LuaRegFunc func);

/// @brief Register a C++ event callback for zipper events.
inline void addZipperEventCallback(ZipperEventCallbackFunc func);

/// @brief Register a C++ event callback for database events.
inline void addDBEventCallback(DBEventCallbackFunc func);

// ===========================================================================
// Version
// ===========================================================================

/// @brief Get version info as JSON.
inline std::string versionJson();
/// @brief Get version info as XML.
inline std::string versionXml();

// ===========================================================================
// Utilities
// ===========================================================================

/// @brief Convert integer polygons to double precision.
inline PolygonsD toDouble(const Polygons& polys);
/// @brief Convert double polygons to integer precision.
inline Polygons toInt(const PolygonsD& polys);

}  // namespace HsBa::Slicer

// ===========================================================================
// ======================== IMPLEMENTATION ===================================
// ===========================================================================

namespace HsBa::Slicer
{

// ===========================================================================
// Default config factories (defined here so consumers don't need GMF statics).
// They also serve as the classes' default arguments: the C ABI helpers
// HsBa*ConfigDefault() are `static inline` (internal linkage), and a module
// interface shall not expose a TU-local entity ([module.interface]), which
// GCC 15+ diagnoses.
// ===========================================================================

HsBaFdmPipelineConfig_t defaultFdmConfig() { return HsBaFdmConfigDefault(); }
HsBaSlaPipelineConfig_t defaultSlaConfig() { return HsBaSlaConfigDefault(); }
HsBaSlsPipelineConfig_t defaultSlsConfig() { return HsBaSlsConfigDefault(); }
HsBaFileTransferPipelineConfig_t defaultFileTransferConfig() { return HsBaFileTransferConfigDefault(); }
HsBaCustomPipelineConfig_t defaultCustomConfig() { return HsBaCustomConfigDefault(); }
HsBaSlmPipelineConfig_t defaultSlmConfig() { return HsBaSlmConfigDefault(); }
HsBaLomPipelineConfig_t defaultLomConfig() { return HsBaLomConfigDefault(); }
HsBaTdpPipelineConfig_t defaultTdpConfig() { return HsBaTdpConfigDefault(); }
HsBaWaamPipelineConfig_t defaultWaamConfig() { return HsBaWaamConfigDefault(); }

// ===========================================================================
// Kernel bridge helpers (shared by every pipeline run()/runAsync()).
// ===========================================================================

namespace
{

/// @brief Convert a kernel geometry capture into the module's native Geometry (④).
Geometry ToGeometry(const Kernel::GeometryCapture& cap)
{
    Geometry g;
    g.valid = cap.valid;
    g.outlines = cap.outlines;
    g.fills = cap.fills;
    g.supports = cap.supports;
    g.zs = cap.zs;
    g.path.reserve(cap.path_xyz.size() / 3);
    for (std::size_t i = 0; i + 2 < cap.path_xyz.size(); i += 3)
        g.path.push_back(SpiralPoint{cap.path_xyz[i], cap.path_xyz[i + 1], cap.path_xyz[i + 2]});
    return g;
}

/// @brief Build kernel hooks from a module progress callback and cancel flag.
Kernel::Hooks MakeHooks(const ProgressFunc& progress, const std::atomic<bool>* cancel)
{
    Kernel::Hooks hooks;
    hooks.progress = progress;
    hooks.cancel_flag = cancel;
    return hooks;
}

/// @brief Fold an Output (success/cancelled/error) into a job outcome (③).
template <class ResultT>
JobOutcome<ResultT> ToOutcome(const bool success, const bool cancelled, const std::string& error,
                              ResultT value)
{
    JobOutcome<ResultT> oc;
    oc.value = std::move(value);
    if (cancelled)
    {
        oc.status = RunStatus::Cancelled;
        oc.error = error.empty() ? "Cancelled" : error;
    }
    else if (!success)
    {
        oc.status = RunStatus::Failed;
        oc.error = error.empty() ? "Pipeline failed" : error;
    }
    return oc;
}

/// @brief Slice a model into per-layer double contours and spiralize the outer wall (④).
std::vector<SpiralPoint> SpiralFromModel(const Model& model, float layer_height, float first_layer_height)
{
    const ModelInfo mi     = model.info();
    const float height     = mi.bbox_max.z() - mi.bbox_min.z();
    const int layers       = static_cast<int>(height / layer_height) + 1;
    std::vector<PolygonsD> sections;
    std::vector<double> zs;
    sections.reserve(static_cast<std::size_t>(layers));
    zs.reserve(static_cast<std::size_t>(layers));
    for (int i = 0; i < layers; ++i)
    {
        const float z = (i == 0) ? first_layer_height : first_layer_height + i * layer_height;
        sections.push_back(UnIntegerization(Slice(model.raw(), z)));
        zs.push_back(z);
    }
    return SpiralizeOuterWall(sections, zs);
}

}  // namespace

// ===========================================================================
// Model
// ===========================================================================

Model::Model(std::string name, const std::filesystem::path& file)
    : name_(std::move(name))
{
    ptr_ = LoadModel(name_, file.string());
    if (!ptr_)
        throw SlicerError("Failed to load model: " + name_);
}

Model::~Model()
{
    if (!name_.empty())
        RemoveModel(name_);
}

Model::Model(Model&& other) noexcept
    : name_(std::move(other.name_)), ptr_(std::move(other.ptr_))
{
    other.name_.clear();
}

Model& Model::operator=(Model&& other) noexcept
{
    if (this != &other)
    {
        if (!name_.empty())
            RemoveModel(name_);
        name_ = std::move(other.name_);
        ptr_  = std::move(other.ptr_);
        other.name_.clear();
    }
    return *this;
}

ModelInfo Model::info() const { return GetModelInfo(name_); }

void Model::translate(const Eigen::Vector3f& t) { TranslateModel(name_, t); }
void Model::rotate(const Eigen::Quaternionf& r) { RotateModel(name_, r); }
void Model::scale(float s) { ScaleModel(name_, s); }
void Model::scale(const Eigen::Vector3f& s) { ScaleModel(name_, s); }

Polygons Model::slice(float height) const { return Slice(*ptr_, height); }

PolygonsD Model::sliceD(float height) const
{
    return UnIntegerization(Slice(*ptr_, height));
}

const IModel& Model::raw() const { return *ptr_; }
const std::string& Model::name() const { return name_; }

// ---- Advanced model operations (②) ----

Model::Model(AdoptTag, std::string name, std::shared_ptr<IModel> ptr)
    : name_(std::move(name)), ptr_(std::move(ptr))
{
}

bool Model::exists(std::string_view name) { return ContainsModel(std::string(name)); }
std::size_t Model::count() { return ModelCount(); }
std::vector<std::string> Model::names() { return GetModelNames(); }
std::size_t Model::cleanup() { return CleanupModels(); }

Model Model::fromExisting(std::string name)
{
    auto ptr = GetModel(name);
    if (!ptr)
        throw SlicerError("No model resolves under name: " + name);
    return Model(AdoptTag{}, std::move(name), std::move(ptr));
}

bool Model::publish() const { return PublishModel(name_); }
bool Model::unpublish() const { return UnpublishModel(name_); }

std::pair<Eigen::MatrixXf, Eigen::MatrixXi> Model::triangles() const
{
    return GetModelTriangles(name_);
}

bool hasTextCsgSupport() { return HasTextCsgSupport(); }

#ifdef USE_CGAL
Model booleanUnion(const std::string& left, const std::string& right, const std::string& resultName)
{
    BooleanUnion(left, right, resultName);
    return Model::fromExisting(resultName);
}
Model booleanIntersection(const std::string& left, const std::string& right, const std::string& resultName)
{
    BooleanIntersection(left, right, resultName);
    return Model::fromExisting(resultName);
}
Model booleanDifference(const std::string& left, const std::string& right, const std::string& resultName)
{
    BooleanDifference(left, right, resultName);
    return Model::fromExisting(resultName);
}
Model booleanXor(const std::string& left, const std::string& right, const std::string& resultName)
{
    BooleanXor(left, right, resultName);
    return Model::fromExisting(resultName);
}
Model thickSolidModel(const std::string& source, const std::string& resultName, float thickness)
{
    ThickSolidModel(source, resultName, thickness);
    return Model::fromExisting(resultName);
}
#endif  // USE_CGAL

#ifdef USE_OCCT
Model buildCsgModel(const std::string& name, std::string_view script, std::string_view shapeName)
{
    BuildCsgModel(name, script, shapeName);
    return Model::fromExisting(name);
}
#endif  // USE_OCCT

// ===========================================================================
// FdmPipeline
// ===========================================================================

FdmPipeline::FdmPipeline(HsBaFdmPipelineConfig_t cfg) : cfg_(cfg) {}

std::vector<Polygons> FdmPipeline::sliceAll(const Model& model) const
{
    const auto mi     = model.info();
    const float height = mi.bbox_max.z() - mi.bbox_min.z();
    const int layers   = static_cast<int>(height / cfg_.layer_height) + 1;

    std::vector<Polygons> result;
    result.reserve(layers);
    for (int i = 0; i < layers; ++i)
    {
        float z = (i == 0) ? cfg_.first_layer_height
                           : cfg_.first_layer_height + i * cfg_.layer_height;
        result.push_back(Slice(model.raw(), z));
    }
    return result;
}

std::vector<PolygonsD> FdmPipeline::generateSupports(const std::vector<PolygonsD>& layers) const
{
    if (!cfg_.enable_support)
        return {};

    Support::FdmSupportConfig sc;
    sc.overhang_angle_threshold = cfg_.overhang_angle;
    sc.layer_height             = cfg_.layer_height;
    sc.support_gap              = cfg_.support_gap;
    sc.support_diameter         = cfg_.support_diameter;
    sc.support_density          = cfg_.support_density;
    sc.support_pattern          = static_cast<int>(cfg_.support_pattern);
    sc.interface_layers         = cfg_.interface_layers;
    sc.interface_density        = cfg_.interface_density;

    if (cfg_.support_lua_script)
    {
        return GenerateAllLuaSupport(layers, sc, cfg_.support_lua_script,
                                     cfg_.support_lua_func ? cfg_.support_lua_func : "generate_support");
    }
    return GenerateAllFdmSupport(layers, sc);
}

Polygons FdmPipeline::fill(const Polygons& contour) const
{
    static constexpr FillMode kFillModes[] = { FillMode::Line, FillMode::SimpleZigzag, FillMode::Zigzag };
    FillMode mode = kFillModes[static_cast<int>(cfg_.fill_mode)];

    if (cfg_.infill_lua_script)
    {
        return LuaCustomFillByFile(contour, cfg_.infill_lua_script,
                                   cfg_.infill_lua_func ? cfg_.infill_lua_func : "generate_fill",
                                   cfg_.fill_spacing);
    }
    return FillWithBorder(contour, cfg_.fill_spacing, cfg_.wall_count, mode, cfg_.fill_angle);
}

std::unique_ptr<GCodePath> FdmPipeline::generatePath(const std::vector<LayerPathData>& data) const
{
    FdmPathConfig pc;
    pc.layer_height         = cfg_.layer_height;
    pc.line_width           = cfg_.line_width;
    pc.print_speed          = cfg_.print_speed;
    pc.travel_speed         = cfg_.travel_speed;
    pc.extrusion_multiplier = cfg_.extrusion_multiplier;
    pc.units                = GCodeUnits::mm;

    GCodePrinterConfig printer_cfg;
    printer_cfg.nozzle_diameter       = cfg_.nozzle_diameter;
    printer_cfg.filament_diameter     = cfg_.filament_diameter;
    printer_cfg.nozzle_temp           = cfg_.nozzle_temp;
    printer_cfg.bed_temp              = cfg_.bed_temp;
    printer_cfg.retract_length        = cfg_.retract_length;
    printer_cfg.retract_speed         = cfg_.retract_speed;
    printer_cfg.print_speed           = cfg_.print_speed;
    printer_cfg.travel_speed          = cfg_.travel_speed;
    printer_cfg.first_layer_speed     = cfg_.first_layer_speed;
    printer_cfg.layer_height          = cfg_.layer_height;
    printer_cfg.line_width            = cfg_.line_width;
    printer_cfg.extrusion_multiplier  = cfg_.extrusion_multiplier;

    return GenerateGCodePathV2(data, pc, printer_cfg);
}

FdmResult FdmPipeline::run(const Model& model, ProgressFunc progress) const
{
    HsBaFdmPipelineConfig_t cfg = cfg_;
    cfg.model_name = model.name().c_str();
    cfg.capture_geometry = 1;
    cfg.cancel_flag = nullptr;

    const Kernel::FdmOutput out = Kernel::RunFdm(cfg, MakeHooks(progress, nullptr));
    if (out.cancelled)
        throw SlicerError(out.error.empty() ? "Cancelled" : out.error);
    if (!out.success)
        throw SlicerError(out.error);

    FdmResult result;
    result.total_layers = out.total_layers;
    result.gcode = out.gcode;
    result.geometry = ToGeometry(out.geometry);
    return result;
}

Job<FdmResult> FdmPipeline::runAsync(const Model& model, ProgressFunc progress) const
{
    HsBaFdmPipelineConfig_t cfg = cfg_;
    std::string name = model.name();
    if (!PublishModel(name))
        throw SlicerError("Model not available for async run: " + name);

    return Job<FdmResult>::Start(
        [cfg, name](const std::atomic<bool>& cancel, const ProgressFunc& emit) {
            HsBaFdmPipelineConfig_t c = cfg;
            c.model_name = name.c_str();
            c.capture_geometry = 1;
            c.cancel_flag = &cancel;
            const Kernel::FdmOutput out = Kernel::RunFdm(c, MakeHooks(emit, &cancel));

            FdmResult value;
            value.total_layers = out.total_layers;
            value.gcode = out.gcode;
            value.geometry = ToGeometry(out.geometry);
            UnpublishModel(name);
            return ToOutcome(out.success, out.cancelled, out.error, std::move(value));
        },
        std::move(progress));
}

std::vector<SpiralPoint> FdmPipeline::spiralPath(const Model& model) const
{
    return SpiralFromModel(model, cfg_.layer_height, cfg_.first_layer_height);
}

// ===========================================================================
// SlaPipeline
// ===========================================================================

SlaPipeline::SlaPipeline(HsBaSlaPipelineConfig_t cfg) : cfg_(cfg) {}

Polygons SlaPipeline::generateFloor(const Polygons& bottom_layer) const
{
    SlaFloorConfig fc;
    fc.raft_offset     = cfg_.floor_raft_offset;
    fc.border_width    = cfg_.floor_border_width;
    fc.fill_spacing    = cfg_.floor_fill_spacing;
    fc.fill_angle_deg  = cfg_.floor_fill_angle;
    fc.border_count    = cfg_.floor_border_count;
    fc.use_convex_hull = (cfg_.floor_use_convex_hull != 0);

    if (cfg_.floor_lua_script)
    {
        return LuaCustomFloorByFile(bottom_layer, cfg_.floor_lua_script,
                                    cfg_.floor_lua_func ? cfg_.floor_lua_func : "generate_floor", fc);
    }
    return GenerateFloorRaft(bottom_layer, fc);
}

bool SlaPipeline::renderLayer(const PolygonsD& polys, int width, int height,
                              const std::string& out_path) const
{
    return RenderPolygonsToImage(polys, width, height, out_path);
}

bool SlaPipeline::savePackage(const SlaPackage& pkg, const std::string& output_zip) const
{
    if (cfg_.export_lua_script)
    {
        return SaveSlaPackageLua(pkg, output_zip, cfg_.export_lua_script,
                                 cfg_.export_lua_func ? cfg_.export_lua_func : "export_sla");
    }
    return SaveSlaPackage(pkg, output_zip);
}

SlaResult SlaPipeline::run(const Model& model, const std::filesystem::path& output_zip,
                           ProgressFunc progress) const
{
    HsBaSlaPipelineConfig_t cfg = cfg_;
    const std::string out = output_zip.string();
    cfg.model_name = model.name().c_str();
    cfg.output_path = out.c_str();
    cfg.capture_geometry = 1;
    cfg.cancel_flag = nullptr;

    const Kernel::SlaOutput o = Kernel::RunSla(cfg, MakeHooks(progress, nullptr));
    if (o.cancelled)
        throw SlicerError(o.error.empty() ? "Cancelled" : o.error);
    if (!o.success)
        throw SlicerError(o.error);

    SlaResult result;
    result.saved = true;
    result.total_layers = o.total_layers;
    result.export_path = o.export_path;
    result.geometry = ToGeometry(o.geometry);
    return result;
}

Job<SlaResult> SlaPipeline::runAsync(const Model& model, const std::filesystem::path& output_zip,
                                     ProgressFunc progress) const
{
    HsBaSlaPipelineConfig_t cfg = cfg_;
    std::string name = model.name();
    std::string out = output_zip.string();
    if (!PublishModel(name))
        throw SlicerError("Model not available for async run: " + name);

    return Job<SlaResult>::Start(
        [cfg, name, out](const std::atomic<bool>& cancel, const ProgressFunc& emit) {
            HsBaSlaPipelineConfig_t c = cfg;
            c.model_name = name.c_str();
            c.output_path = out.c_str();
            c.capture_geometry = 1;
            c.cancel_flag = &cancel;
            const Kernel::SlaOutput o = Kernel::RunSla(c, MakeHooks(emit, &cancel));

            SlaResult value;
            value.saved = o.success;
            value.total_layers = o.total_layers;
            value.export_path = o.export_path;
            value.geometry = ToGeometry(o.geometry);
            UnpublishModel(name);
            return ToOutcome(o.success, o.cancelled, o.error, std::move(value));
        },
        std::move(progress));
}

// ===========================================================================
// SlsPipeline
// ===========================================================================

SlsPipeline::SlsPipeline(HsBaSlsPipelineConfig_t cfg) : cfg_(cfg) {}

SlsResult SlsPipeline::run(const Model& model, ProgressFunc progress) const
{
    HsBaSlsPipelineConfig_t cfg = cfg_;
    cfg.model_name = model.name().c_str();
    cfg.capture_geometry = 1;
    cfg.cancel_flag = nullptr;

    const Kernel::SlsOutput o = Kernel::RunSls(cfg, MakeHooks(progress, nullptr));
    if (o.cancelled)
        throw SlicerError(o.error.empty() ? "Cancelled" : o.error);
    if (!o.success)
        throw SlicerError(o.error);

    SlsResult result;
    result.saved = true;
    result.total_layers = o.total_layers;
    result.export_path = o.export_path;
    result.geometry = ToGeometry(o.geometry);
    return result;
}

Job<SlsResult> SlsPipeline::runAsync(const Model& model, ProgressFunc progress) const
{
    HsBaSlsPipelineConfig_t cfg = cfg_;
    std::string name = model.name();
    if (!PublishModel(name))
        throw SlicerError("Model not available for async run: " + name);

    return Job<SlsResult>::Start(
        [cfg, name](const std::atomic<bool>& cancel, const ProgressFunc& emit) {
            HsBaSlsPipelineConfig_t c = cfg;
            c.model_name = name.c_str();
            c.capture_geometry = 1;
            c.cancel_flag = &cancel;
            const Kernel::SlsOutput o = Kernel::RunSls(c, MakeHooks(emit, &cancel));
            SlsResult value;
            value.saved = o.success;
            value.total_layers = o.total_layers;
            value.export_path = o.export_path;
            value.geometry = ToGeometry(o.geometry);
            UnpublishModel(name);
            return ToOutcome(o.success, o.cancelled, o.error, std::move(value));
        },
        std::move(progress));
}

// ===========================================================================
// SlmPipeline
// ===========================================================================

SlmPipeline::SlmPipeline(HsBaSlmPipelineConfig_t cfg) : cfg_(cfg) {}

SlmResult SlmPipeline::run(const Model& model, ProgressFunc progress) const
{
    HsBaSlmPipelineConfig_t cfg = cfg_;
    cfg.model_name = model.name().c_str();
    cfg.capture_geometry = 1;
    cfg.cancel_flag = nullptr;

    const Kernel::SlmOutput o = Kernel::RunSlm(cfg, MakeHooks(progress, nullptr));
    if (o.cancelled)
        throw SlicerError(o.error.empty() ? "Cancelled" : o.error);
    if (!o.success)
        throw SlicerError(o.error);

    SlmResult result;
    result.saved = true;
    result.total_layers = o.total_layers;
    result.export_path = o.export_path;
    result.geometry = ToGeometry(o.geometry);
    return result;
}

Job<SlmResult> SlmPipeline::runAsync(const Model& model, ProgressFunc progress) const
{
    HsBaSlmPipelineConfig_t cfg = cfg_;
    std::string name = model.name();
    if (!PublishModel(name))
        throw SlicerError("Model not available for async run: " + name);

    return Job<SlmResult>::Start(
        [cfg, name](const std::atomic<bool>& cancel, const ProgressFunc& emit) {
            HsBaSlmPipelineConfig_t c = cfg;
            c.model_name = name.c_str();
            c.capture_geometry = 1;
            c.cancel_flag = &cancel;
            const Kernel::SlmOutput o = Kernel::RunSlm(c, MakeHooks(emit, &cancel));
            SlmResult value;
            value.saved = o.success;
            value.total_layers = o.total_layers;
            value.export_path = o.export_path;
            value.geometry = ToGeometry(o.geometry);
            UnpublishModel(name);
            return ToOutcome(o.success, o.cancelled, o.error, std::move(value));
        },
        std::move(progress));
}

// ===========================================================================
// LomPipeline
// ===========================================================================

LomPipeline::LomPipeline(HsBaLomPipelineConfig_t cfg) : cfg_(cfg) {}

LomResult LomPipeline::run(const Model& model, ProgressFunc progress) const
{
    HsBaLomPipelineConfig_t cfg = cfg_;
    cfg.model_name = model.name().c_str();
    cfg.capture_geometry = 1;
    cfg.cancel_flag = nullptr;

    const Kernel::LomOutput o = Kernel::RunLom(cfg, MakeHooks(progress, nullptr));
    if (o.cancelled)
        throw SlicerError(o.error.empty() ? "Cancelled" : o.error);
    if (!o.success)
        throw SlicerError(o.error);

    LomResult result;
    result.saved = true;
    result.total_layers = o.total_layers;
    result.export_path = o.export_path;
    result.geometry = ToGeometry(o.geometry);
    return result;
}

Job<LomResult> LomPipeline::runAsync(const Model& model, ProgressFunc progress) const
{
    HsBaLomPipelineConfig_t cfg = cfg_;
    std::string name = model.name();
    if (!PublishModel(name))
        throw SlicerError("Model not available for async run: " + name);

    return Job<LomResult>::Start(
        [cfg, name](const std::atomic<bool>& cancel, const ProgressFunc& emit) {
            HsBaLomPipelineConfig_t c = cfg;
            c.model_name = name.c_str();
            c.capture_geometry = 1;
            c.cancel_flag = &cancel;
            const Kernel::LomOutput o = Kernel::RunLom(c, MakeHooks(emit, &cancel));
            LomResult value;
            value.saved = o.success;
            value.total_layers = o.total_layers;
            value.export_path = o.export_path;
            value.geometry = ToGeometry(o.geometry);
            UnpublishModel(name);
            return ToOutcome(o.success, o.cancelled, o.error, std::move(value));
        },
        std::move(progress));
}

// ===========================================================================
// TdpPipeline
// ===========================================================================

TdpPipeline::TdpPipeline(HsBaTdpPipelineConfig_t cfg) : cfg_(cfg) {}

TdpResult TdpPipeline::run(const Model& model, ProgressFunc progress) const
{
    HsBaTdpPipelineConfig_t cfg = cfg_;
    cfg.model_name = model.name().c_str();
    cfg.capture_geometry = 1;
    cfg.cancel_flag = nullptr;

    const Kernel::TdpOutput o = Kernel::RunTdp(cfg, MakeHooks(progress, nullptr));
    if (o.cancelled)
        throw SlicerError(o.error.empty() ? "Cancelled" : o.error);
    if (!o.success)
        throw SlicerError(o.error);

    TdpResult result;
    result.saved = true;
    result.total_layers = o.total_layers;
    result.export_path = o.export_path;
    result.geometry = ToGeometry(o.geometry);
    return result;
}

Job<TdpResult> TdpPipeline::runAsync(const Model& model, ProgressFunc progress) const
{
    HsBaTdpPipelineConfig_t cfg = cfg_;
    std::string name = model.name();
    if (!PublishModel(name))
        throw SlicerError("Model not available for async run: " + name);

    return Job<TdpResult>::Start(
        [cfg, name](const std::atomic<bool>& cancel, const ProgressFunc& emit) {
            HsBaTdpPipelineConfig_t c = cfg;
            c.model_name = name.c_str();
            c.capture_geometry = 1;
            c.cancel_flag = &cancel;
            const Kernel::TdpOutput o = Kernel::RunTdp(c, MakeHooks(emit, &cancel));
            TdpResult value;
            value.saved = o.success;
            value.total_layers = o.total_layers;
            value.export_path = o.export_path;
            value.geometry = ToGeometry(o.geometry);
            UnpublishModel(name);
            return ToOutcome(o.success, o.cancelled, o.error, std::move(value));
        },
        std::move(progress));
}

std::vector<SpiralPoint> TdpPipeline::spiralPath(const Model& model) const
{
    return SpiralFromModel(model, cfg_.layer_height, cfg_.first_layer_height);
}

// ===========================================================================
// WaamPipeline
// ===========================================================================

WaamPipeline::WaamPipeline(HsBaWaamPipelineConfig_t cfg) : cfg_(cfg) {}

WaamResult WaamPipeline::run(const Model& model, ProgressFunc progress) const
{
    HsBaWaamPipelineConfig_t cfg = cfg_;
    cfg.model_name = model.name().c_str();
    cfg.capture_geometry = 1;
    cfg.cancel_flag = nullptr;

    const Kernel::WaamOutput o = Kernel::RunWaam(cfg, MakeHooks(progress, nullptr));
    if (o.cancelled)
        throw SlicerError(o.error.empty() ? "Cancelled" : o.error);
    if (!o.success)
        throw SlicerError(o.error);

    WaamResult result;
    result.saved = true;
    result.total_layers = o.total_layers;
    result.output_path = o.output_path;
    result.geometry = ToGeometry(o.geometry);
    return result;
}

Job<WaamResult> WaamPipeline::runAsync(const Model& model, ProgressFunc progress) const
{
    HsBaWaamPipelineConfig_t cfg = cfg_;
    std::string name = model.name();
    if (!PublishModel(name))
        throw SlicerError("Model not available for async run: " + name);

    return Job<WaamResult>::Start(
        [cfg, name](const std::atomic<bool>& cancel, const ProgressFunc& emit) {
            HsBaWaamPipelineConfig_t c = cfg;
            c.model_name = name.c_str();
            c.capture_geometry = 1;
            c.cancel_flag = &cancel;
            const Kernel::WaamOutput o = Kernel::RunWaam(c, MakeHooks(emit, &cancel));
            WaamResult value;
            value.saved = o.success;
            value.total_layers = o.total_layers;
            value.output_path = o.output_path;
            value.geometry = ToGeometry(o.geometry);
            UnpublishModel(name);
            return ToOutcome(o.success, o.cancelled, o.error, std::move(value));
        },
        std::move(progress));
}

std::vector<SpiralPoint> WaamPipeline::spiralPath(const Model& model) const
{
    return SpiralFromModel(model, cfg_.layer_height, cfg_.first_layer_height);
}

// ===========================================================================
// FileTransferPipeline
// ===========================================================================

FileTransferPipeline::FileTransferPipeline(HsBaFileTransferPipelineConfig_t cfg) : cfg_(cfg) {}

FileTransferOutcome FileTransferPipeline::run() const
{
    return run(nullptr);
}

FileTransferOutcome FileTransferPipeline::run(FileTransferProgressFunc progress) const
{
    FileTransferConfig lib_cfg;
    lib_cfg.host = cfg_.host ? cfg_.host : "";
    lib_cfg.port = cfg_.port ? cfg_.port : "";
    lib_cfg.pool_size = cfg_.pool_size > 0 ? static_cast<std::size_t>(cfg_.pool_size) : 4;

    if (cfg_.file_paths && cfg_.file_count > 0)
    {
        lib_cfg.files.reserve(static_cast<std::size_t>(cfg_.file_count));
        for (int i = 0; i < cfg_.file_count; ++i)
        {
            if (cfg_.file_paths[i])
                lib_cfg.files.emplace_back(cfg_.file_paths[i]);
        }
    }

    FileTransferResult result = TransferFiles(lib_cfg, progress);
    if (!result.success)
        throw SlicerError(result.error_message);

    FileTransferOutcome outcome;
    outcome.success = result.success;
    outcome.files_transferred = result.files_transferred;
    outcome.total_files = result.total_files;
    return outcome;
}

// ===========================================================================
// ParamStorePipeline
// ===========================================================================

namespace
{

ParamPipelineKind ToLibKind(ParamStoreKind k)
{
    switch (k)
    {
        case ParamStoreKind::Sla:
            return ParamPipelineKind::Sla;
        case ParamStoreKind::Sls:
            return ParamPipelineKind::Sls;
        case ParamStoreKind::Slm:
            return ParamPipelineKind::Slm;
        case ParamStoreKind::Lom:
            return ParamPipelineKind::Lom;
        case ParamStoreKind::Tdp:
            return ParamPipelineKind::Tdp;
        case ParamStoreKind::Waam:
            return ParamPipelineKind::Waam;
        case ParamStoreKind::Custom:
            return ParamPipelineKind::Custom;
        case ParamStoreKind::FileTransfer:
            return ParamPipelineKind::FileTransfer;
        case ParamStoreKind::Fdm:
        default:
            return ParamPipelineKind::Fdm;
    }
}

ParamBackend ToLibBackend(ParamStoreBackend b)
{
    switch (b)
    {
        case ParamStoreBackend::MySql:
            return ParamBackend::MySql;
        case ParamStoreBackend::PostgreSql:
            return ParamBackend::PostgreSql;
        case ParamStoreBackend::Sqlite:
        default:
            return ParamBackend::Sqlite;
    }
}

ParamStoreConn ToLibConn(const ParamStoreConnection& c)
{
    ParamStoreConn o;
    o.backend = ToLibBackend(c.backend);
    o.sqlitePath = c.sqlitePath;
    o.host = c.host;
    o.user = c.user;
    o.password = c.password;
    o.database = c.database;
    o.cryptoKey = c.cryptoKey;
    o.port = c.port;
    return o;
}

}  // namespace

ParamStorePipeline::ParamStorePipeline(ParamStoreConnection conn) : conn_(std::move(conn)) {}

long long ParamStorePipeline::save(ParamStoreKind kind, const void* cfg, const std::string& key,
                                   const std::string& table) const
{
    auto outcome = SavePipelineParams(ToLibConn(conn_), ToLibKind(kind), table, key, cfg);
    if (!outcome.success)
        throw SlicerError(outcome.error);
    return outcome.paramId;
}

long long ParamStorePipeline::load(ParamStoreKind kind, const std::string& key, void* outCfg,
                                   const std::string& table) const
{
    auto outcome = LoadPipelineParams(ToLibConn(conn_), ToLibKind(kind), table, key, outCfg);
    if (!outcome.success)
        throw SlicerError(outcome.error);
    return outcome.paramId;
}

void ParamStorePipeline::freeLoaded(ParamStoreKind kind, void* cfg) const
{
    FreeLoadedConfigStrings(ToLibKind(kind), cfg);
}

// ===========================================================================
// Config JSON codec (①)
// ===========================================================================

std::string defaultConfigJson(ParamStoreKind kind) { return DefaultConfigJson(ToLibKind(kind)); }

std::string configToJson(ParamStoreKind kind, const void* cfg) { return ConfigToJson(ToLibKind(kind), cfg); }

bool configFromJson(ParamStoreKind kind, std::string_view json, void* cfg)
{
    return ConfigFromJson(ToLibKind(kind), json, cfg);
}

// ===========================================================================
// CustomLuaPipeline
// ===========================================================================

CustomLuaPipeline::CustomLuaPipeline(HsBaCustomPipelineConfig_t cfg) : cfg_(cfg) {}

void CustomLuaPipeline::setProgressFunc(std::function<void(int, std::string_view)> func)
{
    progress_ = std::move(func);
}

CustomLuaResult CustomLuaPipeline::run() const
{
    if (!cfg_.pipeline_lua_script && !cfg_.pipeline_lua_source)
        throw SlicerError("Custom Lua pipeline requires pipeline_lua_script or pipeline_lua_source");

    const Kernel::CustomOutput out = Kernel::RunCustom(cfg_, MakeHooks(progress_, nullptr));
    if (out.cancelled)
        throw SlicerError(out.error.empty() ? "Cancelled" : out.error);
    if (!out.success)
        throw SlicerError(out.error.empty() ? "Custom Lua pipeline failed" : out.error);

    CustomLuaResult result;
    result.success = out.success;
    result.total_layers = out.total_layers;
    result.output_path = out.output_path;
    result.result_string = out.result_string;
    return result;
}

Job<CustomLuaResult> CustomLuaPipeline::runAsync()
{
    if (!cfg_.pipeline_lua_script && !cfg_.pipeline_lua_source)
        throw SlicerError("Custom Lua pipeline requires pipeline_lua_script or pipeline_lua_source");

    HsBaCustomPipelineConfig_t cfg = cfg_;
    ProgressFunc progress = progress_;
    return Job<CustomLuaResult>::Start(
        [cfg](const std::atomic<bool>& cancel, const ProgressFunc& emit) {
            HsBaCustomPipelineConfig_t c = cfg;
            c.cancel_flag = &cancel;
            const Kernel::CustomOutput out = Kernel::RunCustom(c, MakeHooks(emit, &cancel));
            CustomLuaResult value;
            value.success = out.success;
            value.total_layers = out.total_layers;
            value.output_path = out.output_path;
            value.result_string = out.result_string;
            return ToOutcome(out.success, out.cancelled, out.error, std::move(value));
        },
        std::move(progress));
}

// ===========================================================================
// Lua custom functions
// ===========================================================================

Polygons luaCustomFill(const Polygons& poly, const std::string& script_path,
                       const std::string& func_name, double line_thickness)
{
    return LuaCustomFillByFile(poly, script_path, func_name, line_thickness);
}

Polygons luaCustomFloor(const Polygons& bottom_layer, const std::string& script_path,
                        const std::string& func_name, const SlaFloorConfig& config)
{
    return LuaCustomFloorByFile(bottom_layer, script_path, func_name, config);
}

std::vector<PolygonsD> luaCustomSupport(const std::vector<PolygonsD>& layers,
                                         const SupportConfig& config,
                                         std::string_view script,
                                         std::string_view func_name)
{
    return GenerateAllLuaSupport(layers, config, script, func_name);
}

// ===========================================================================
// External Lua function registration
// ===========================================================================

void add2DFunction(LuaRegFunc func) { Add2DFunctions(std::move(func)); }
void add3DFunction(LuaRegFunc func) { Add3DFunctions(std::move(func)); }
void addFileFunction(LuaRegFunc func) { AddFileFunctions(std::move(func)); }
void addEventCallback(const std::string& event_name, LuaRegFunc func)
{
    AddEventCallback(event_name, std::move(func));
}

void addZipperEventCallback(ZipperEventCallbackFunc func)
{
    AddZipperEventCallback(std::move(func));
}

void addDBEventCallback(DBEventCallbackFunc func)
{
    AddDBEventCallback(std::move(func));
}

// ===========================================================================
// Version
// ===========================================================================

std::string versionJson() { return GetVersionJson(); }
std::string versionXml() { return GetVersionXml(); }

// ===========================================================================
// Utilities
// ===========================================================================

PolygonsD toDouble(const Polygons& polys) { return UnIntegerization(polys); }
Polygons toInt(const PolygonsD& polys) { return Integerization(polys); }

}  // namespace HsBa::Slicer
