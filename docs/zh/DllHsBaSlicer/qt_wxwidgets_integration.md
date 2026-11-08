# Qt / wxWidgets 集成指南

本文演示如何在 **Qt** 与 **wxWidgets** 桌面应用中调用 DllHsBaSlicer 流水线，覆盖工程配置、后台线程执行、进度条更新与结果处理。

> 前置阅读：[DllHsBaSlicer 模块总览](./README.md)

## 通用要点

1. **先调用 `initialize()`**：进程启动后、任何流水线调用之前执行一次；
2. **不要在 UI 线程跑同步接口**：`HsBaRunFdmPipeline` 等同步函数会阻塞直到切片完成，必须放入工作线程；
3. **回调不在 UI 线程**：进度/结果回调触发于库内部线程，更新控件前必须调度回 UI 线程；
4. **字符串生命周期**：配置结构体中的 `const char*` 字段在流水线运行期间必须保持有效；结果中的动态字符串用 `HsBaFree*PipelineResult()` 统一释放。

---

## 一、Qt 集成

### 1. CMake 工程配置

方式 A：已安装 HsBaSlicer（`cmake --install`），用 `find_package`：

```cmake
find_package(HsBaSlicer CONFIG REQUIRED)

add_executable(SlicerApp mainwindow.cpp slicer_worker.cpp)
target_link_libraries(SlicerApp PRIVATE
    Qt6::Widgets
    HsBaSlicer::DllHsBaSlicer   # 自动带入 include/HsBaSlicer 头文件路径
)
```

方式 B：直接指向构建产物（快速验证）：

```cmake
target_include_directories(SlicerApp PRIVATE
    ${HSBA_SOURCE_DIR}/DllHsBaSlicer
    ${HSBA_SOURCE_DIR}                 # pipelinetypes/pipeline_types.h
)
target_link_libraries(SlicerApp PRIVATE
    ${HSBA_BIN_DIR}/DllHsBaSlicer.lib  # Windows 导入库；Linux/macOS 直接链 .so/.dylib
)
add_custom_command(TARGET SlicerApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${HSBA_BIN_DIR}/DllHsBaSlicer.dll $<TARGET_FILE_DIR:SlicerApp>)
```

### 2. 工作线程封装（QThread + 信号槽）

推荐用**异步接口 + 信号转发**，进度回调中只发信号，槽函数自动在 UI 线程执行：

```cpp
// slicer_worker.h
#pragma once
#include <QObject>
#include <QString>
#include <string>

#include "fdm_pipeline.h"

class SlicerWorker : public QObject
{
    Q_OBJECT
public:
    explicit SlicerWorker(QObject* parent = nullptr) : QObject(parent) {}

    void setModel(const QString& name, const QString& path)
    {
        m_name = name.toStdString();
        m_path = path.toStdString();   // 保存 UTF-8 字节，避免回调期间悬空
    }

signals:
    void progressChanged(int percent, QString stage);
    void finished(bool success, QString gcode, QString error);

public slots:
    void run()
    {
        HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
        cfg.model_name  = m_name.c_str();
        cfg.model_path  = m_path.c_str();
        cfg.output_path = nullptr;     // 由结果 gcode_content 自行落盘

        // 同步接口在本 slot 所属线程（工作线程）中阻塞执行
        HsBaFdmPipelineResult_t r = HsBaRunFdmPipeline(&cfg, &SlicerWorker::onProgress, this);

        const bool ok = r.success != 0;
        const QString gcode = QString::fromUtf8(r.gcode_content ? r.gcode_content : "");
        const QString err   = QString::fromUtf8(r.error_message ? r.error_message : "");
        HsBaFreePipelineResult(&r);    // 必须在拷贝完成后释放

        emit finished(ok, gcode, err); // 跨线程信号 → UI 线程槽
    }

private:
    static void onProgress(int percent, const char* stage, void* user_data)
    {
        // 注意：此函数运行在库内部线程
        auto* self = static_cast<SlicerWorker*>(user_data);
        emit self->progressChanged(percent, QString::fromUtf8(stage ? stage : ""));
    }

    std::string m_name;
    std::string m_path;
};
```

### 3. 主窗口调用

```cpp
// mainwindow.cpp（节选）
#include <QThread>
#include <QProgressBar>
#include "slicer_worker.h"

void MainWindow::startSlice()
{
    static bool inited = [] { initialize(); return true; }();
    Q_UNUSED(inited);

    auto* thread = new QThread(this);
    auto* worker = new SlicerWorker;             // 无需 parent，稍后手动删除
    worker->setModel("stanford_bunny", "models/stanford_bunny.stl");
    worker->moveToThread(thread);

    connect(thread, &QThread::started,  worker, &SlicerWorker::run);
    connect(worker, &SlicerWorker::progressChanged,
            m_progressBar, &QProgressBar::setValue);          // UI 线程执行
    connect(worker, &SlicerWorker::finished, this, &MainWindow::onSliceDone);
    connect(worker, &SlicerWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void MainWindow::onSliceDone(bool success, const QString& gcode, const QString& error)
{
    if (success)
    {
        QFile f("output/result.gcode");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            f.write(gcode.toUtf8());
        statusBar()->showMessage(tr("切片完成"));
    }
    else
    {
        statusBar()->showMessage(tr("切片失败：%1").arg(error));
    }
}
```

### 4. QtConcurrent 简化写法（可选）

若不需要细粒度进度，可直接用 `QtConcurrent::run` 包裹同步接口：

```cpp
#include <QtConcurrent>

auto future = QtConcurrent::run([path = modelPath.toStdString()] {
    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
    cfg.model_name = "model";
    cfg.model_path = path.c_str();
    HsBaFdmPipelineResult_t r = HsBaRunFdmPipeline(&cfg, nullptr, nullptr);
    return r;   // 注意：调用方需负责 HsBaFreePipelineResult
});
```

---

## 二、wxWidgets 集成

### 1. 工程配置

```cmake
find_package(wxWidgets COMPONENTS core base REQUIRED)
include(${wxWidgets_USE_FILE})

add_executable(SlicerApp WIN32 main.cpp slice_panel.cpp)
target_include_directories(SlicerApp PRIVATE
    ${HSBA_SOURCE_DIR}/DllHsBaSlicer ${HSBA_SOURCE_DIR})
target_link_libraries(SlicerApp PRIVATE
    ${wxWidgets_LIBRARIES}
    ${HSBA_BIN_DIR}/DllHsBaSlicer.lib)
```

### 2. 自定义事件 + CallAfter 调度

wxWidgets 中用 `CallAfter` 把工作线程数据安全调度回 UI 线程：

```cpp
// slice_panel.h
#pragma once
#include <wx/wx.h>
#include <wx/thread.h>
#include <string>

#include "fdm_pipeline.h"

class SlicePanel : public wxPanel
{
public:
    SlicePanel(wxWindow* parent);
    void StartSlice(const wxString& modelPath);

private:
    static void OnProgress(int percent, const char* stage, void* userData);

    void OnSliceButton(wxCommandEvent&);
    wxGauge*     m_gauge;
    wxStaticText* m_status;
    std::string  m_modelPath;   // UTF-8，生命周期覆盖整个切片过程
};
```

```cpp
// slice_panel.cpp（节选）
void SlicePanel::StartSlice(const wxString& modelPath)
{
    static bool inited = [] { initialize(); return true; }();
    (void)inited;

    m_modelPath = modelPath.ToStdString(wxConvUTF8);

    std::thread([this] {
        HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
        cfg.model_name = "model";
        cfg.model_path = m_modelPath.c_str();

        HsBaFdmPipelineResult_t r =
            HsBaRunFdmPipeline(&cfg, &SlicePanel::OnProgress, this);

        // 结果拷贝为 wxString 后调度回 UI 线程
        const wxString gcode = wxString::FromUTF8(r.gcode_content ? r.gcode_content : "");
        const wxString err   = wxString::FromUTF8(r.error_message ? r.error_message : "");
        const bool ok = r.success != 0;
        HsBaFreePipelineResult(&r);

        CallAfter([this, ok, gcode, err] {          // UI 线程执行
            m_gauge->SetValue(100);
            m_status->SetLabel(ok ? wxString("切片完成")
                                  : wxString("切片失败：") + err);
            if (ok)
            {
                wxFileOutputStream f("output/result.gcode");
                if (f.IsOk()) f.Write(gcode.ToUTF8(), gcode.Length());
            }
        });
    }).detach();
}

// 库内部线程回调 → CallAfter 转发到 UI 线程
void SlicePanel::OnProgress(int percent, const char* stage, void* userData)
{
    auto* self = static_cast<SlicePanel*>(userData);
    const wxString stageText = wxString::FromUTF8(stage ? stage : "");
    self->CallAfter([self, percent, stageText] {
        self->m_gauge->SetValue(percent);
        self->m_status->SetLabel(stageText);
    });
}
```

> 生产代码建议用 `wxThread` 或线程池替代裸 `std::thread::detach`，并在窗口析构时等待任务结束，避免悬空 `this`。

---

## 三、SLA / SLS 流水线

SLA / SLS 的调用方式与 FDM 完全一致，仅替换函数与配置类型：

```cpp
// SLA：输出层图 zip
HsBaSlaPipelineConfig_t sla = HsBaCreateDefaultSlaConfig();
sla.model_name  = "resin_part";
sla.model_path  = "models/resin_part.stl";
sla.output_path = "output/resin_part.zip";
sla.image_type  = HSBA_SLA_IMAGE_PNG;
HsBaSlaPipelineResult_t slaR = HsBaRunSlaPipeline(&sla, nullptr, nullptr);
HsBaFreeSlaPipelineResult(&slaR);

// SLS：必须提供 Lua 导出脚本
HsBaSlsPipelineConfig_t sls = HsBaCreateDefaultSlsConfig();
sls.model_name        = "nylon_part";
sls.model_path        = "models/nylon_part.stl";
sls.export_lua_script = "scripts/my_sls_export.lua";   // 不可为 NULL
HsBaSlsPipelineResult_t slsR = HsBaRunSlsPipeline(&sls, nullptr, nullptr);
HsBaFreeSlsPipelineResult(&slsR);
```

---

## 四、Lua 扩展函数注册

在流水线运行前，可通过 `lua_register.h` 注册自定义 Lua 函数，各阶段创建 Lua 环境时自动注入：

```cpp
#include "initialize.h"
#include "lua_register.h"
#include <lua.hpp>

// 自定义 Lua C 函数
static int l_my_database_query(lua_State* L)
{
    const char* sql = luaL_checkstring(L, 1);
    // ... 执行查询，将结果压入 Lua 栈
    return 1;  // 返回值个数
}

// 注册函数（此函数将在对应阶段初始化 Lua 时被调用）
static void registerMyFunctions(lua_State* L)
{
    lua_register(L, "db_query", l_my_database_query);
}

void setupLuaExtensions()
{
    // 按阶段类型注册（在 initialize() 之后、流水线运行之前）
    HsBaAdd2DFunction(registerMyFunctions);    // 支撑/填充/SLA输出阶段可用
    HsBaAdd3DFunction(registerMyFunctions);    // 切片/支撑阶段可用
    HsBaAddFileFunction(registerMyFunctions);  // SLS/SLA输出阶段可用

    // 事件回调（如 Zipper 压缩事件）
    HsBaAddEventCallback("zipper.on_add", registerMyFunctions);
}
```

### 各阶段可用函数类型

| 流水线阶段 | 注册函数 | 说明 |
| --- | --- | --- |
| Slice（切片） | `HsBaAdd3DFunction` | 3D 模型操作函数 |
| Support（支撑） | `HsBaAdd2DFunction` + `HsBaAdd3DFunction` | 2D 轮廓 + 3D 模型 |
| Fill（填充） | `HsBaAdd2DFunction` | 2D 多边形操作 |
| SLS Output | `HsBaAddFileFunction` | 文件输出操作 |
| SLA Output | `HsBaAdd2DFunction` + `HsBaAddFileFunction` | 2D + 文件输出 |

> **注意**：注册函数指针必须在整个进程生命周期内保持有效（建议用静态函数或全局函数）。

## 五、作业句柄：进度上色 / 取消按钮 / 单层轮廓

有了 `HsBaSubmitJob`（真异步、返回即解放 UI 线程）与 `HsBaStage_t`，桌面端不再需要自己写工作线程：提交可以直接在 UI 线程做，回调仍然在作业线程触发，照旧需要调度回 UI。

### Qt：阶段上色 + 取消按钮

```cpp
// slicer_worker.h —— 不再需要 QThread，句柄存成员变量供按钮使用
class SlicerWorker : public QObject {
    Q_OBJECT
public:
    explicit SlicerWorker(QObject* parent = nullptr) : QObject(parent) {}
    ~SlicerWorker() { if (m_job) HsBaReleaseJob(m_job); }

    void start(const HsBaFdmPipelineConfig_t& cfg) {
        if (m_job) { HsBaReleaseJob(m_job); m_job = nullptr; }
        m_job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &cfg, &SlicerWorker::onProgress, this,
                             &SlicerWorker::onDone, this);
    }
    void cancel() { if (m_job) HsBaCancelJob(m_job); }   // 协作式，UI 线程直接调用

signals:
    void stageChanged(int percent, const QString& stageId, const QString& text);
    void finished();

private:
    static void onProgress(int percent, HsBaStage_t stage, const char* text, void* ud) {
        auto* self = static_cast<SlicerWorker*>(ud);
        // 跨线程信号自动排队；stageId 是稳定标识符，可作 i18n 键与配色键
        emit self->stageChanged(percent, QString::fromUtf8(HsBaStageName(stage)),
                                 QString::fromUtf8(text ? text : ""));
    }
    static void onDone(HsBaJobHandle, void* ud) {
        emit static_cast<SlicerWorker*>(ud)->stageChanged(100, QString(), QString());
        emit static_cast<SlicerWorker*>(ud)->finished();
    }
    HsBaJobHandle m_job = nullptr;
};
```

槽函数里按阶段前缀上色，比匹配易变的英文文本可靠得多：

```cpp
static QColor stageColor(const QString& id) {
    if (id.startsWith("model"))    return Qt::gray;         // 网格加载
    if (id.startsWith("slicing"))  return Qt::darkBlue;     // 切片
    if (id.startsWith("support"))  return Qt::darkYellow;   // 支撑（skipped 也走这里）
    if (id.startsWith("floor"))    return Qt::darkMagenta;
    if (id.startsWith("fill"))     return Qt::darkGreen;    // 填充
    if (id == "path.start" || id == "file.write" || id == "export") return Qt::darkCyan;
    if (id == "cancelled")         return Qt::red;
    if (id == "done")              return Qt::darkGreen;
    return Qt::lightGray;                                   // unknown / 空串
}
// connect(&w, &SlicerWorker::stageChanged, [&](int pct, const QString& id, const QString&) {
//     ui->progress->setValue(pct); ui->progress->setStyleSheet(stageColor(id).name()); });
```

> `on_done` 触发时库已释放自己的引用，但成员 `m_job` 仍是宿主引用，可以在 `finished()` 槽里安全调 `HsBaJobCopyResult` / `HsBaJobGeometryId`；最后一次 `HsBaReleaseJob` 交给析构。

### wxWidgets：CallAfter 搬运阶段与几何

```cpp
void SlicePanel::OnSubmit(wxCommandEvent&)
{
    HsBaFdmPipelineConfig_t cfg = MakeConfig();
    cfg.capture_geometry = 1;                 // 需要逐层预览时才打开
    m_job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &cfg, ProgressTrampoline, this, DoneTrampoline, this);
}

void SlicePanel::OnCancel(wxCommandEvent&) { if (m_job) HsBaCancelJob(m_job); }

void SlicePanel::ProgressTrampoline(int percent, HsBaStage_t stage, const char* text, void* ud) {
    auto* self = static_cast<SlicePanel*>(ud);
    const wxString id = wxString::FromUTF8(HsBaStageName(stage));   // 先拷贝，回调返回后指针失效
    const wxString msg = wxString::FromUTF8(text ? text : "");
    self->CallAfter([self, percent, id, msg] {                    // 调度回 UI 线程
        self->m_gauge->SetValue(percent);
        self->SetLabel(msg.empty() ? id : msg);
    });
}
```

单层轮廓绘制：几何只在作业终态后可读，绘制时把点集拷贝进画布自己的容器，`HsBaFreeLayerGeometry` 调用后原始指针即失效。

```cpp
void SlicePanel::LoadLayer(int layer) {
    const long long gid = HsBaJobGeometryId(m_job);
    if (HsBaGeometryLayerCount(gid) <= 0) return;             // 未采集或已被池回收
    HsBaLayerGeometry_t geom = {0};
    if (!HsBaGetLayerGeometry(gid, layer, &geom)) return;
    const wxRect2DDouble bounds(geom.bounds_min[0], geom.bounds_min[1],
                                geom.bounds_max[0], geom.bounds_max[1]);   // free 前读完数值
    m_polylines.clear();                                      // std::vector<std::vector<wxPoint2DDouble>>
    const HsBaRing_t* slots[3] = { geom.outlines, geom.fills, geom.supports };
    const int counts[3] = { geom.outline_count, geom.fill_count, geom.support_count };
    for (int s = 0; s < 3; ++s)
        for (int r = 0; r < counts[s]; ++r) {
            std::vector<wxPoint2DDouble> pts;
            pts.reserve(slots[s][r].point_count);
            for (int i = 0; i < slots[s][r].point_count; ++i)
                pts.emplace_back(slots[s][r].xy[2 * i], slots[s][r].xy[2 * i + 1]);
            m_polylines.push_back(std::move(pts));
        }
    HsBaFreeLayerGeometry(&geom);     // 整块 free，不可逐个 free 子数组（调用后 geom 被清零）
    m_canvas->Fit(bounds);            // 用该层 bounds_min/max 做自适应缩放
    m_canvas->Refresh();
}
```

## 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| Windows 启动即报"找不到 DllHsBaSlicer.dll" | DLL 未与可执行文件同目录；用 post-build 复制（见上文 CMake） |
| 进度条不动 / 界面卡死 | 同步接口跑在了 UI 线程；移到工作线程 |
| 回调中更新控件崩溃 | 回调在库线程；必须经信号槽 / `CallAfter` 调度回 UI 线程 |
| 中文路径切片失败 | 确保传入 UTF-8 编码路径（`QString::toStdString` / `wxString::ToStdString(wxConvUTF8)`） |
| 结果字符串乱码或崩溃 | 提前调用了 `HsBaFree*PipelineResult`；先拷贝再释放 |
| 点了取消按钮但作业还在跑 | 取消是协作式的，只在阶段边界与逐层循环内生效；正在进行的是网格加载、一次 Lua 回调或 zip 压缩，等它结束 |
| 层预览一片空白 | 提交时 `capture_geometry` 为 0；或作业尚未终态（采集在流水线末尾）；或快照被池淘汰（见 `HSBA_GEOMETRY_POOL_MAX`） |
