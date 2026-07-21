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

## 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| Windows 启动即报"找不到 DllHsBaSlicer.dll" | DLL 未与可执行文件同目录；用 post-build 复制（见上文 CMake） |
| 进度条不动 / 界面卡死 | 同步接口跑在了 UI 线程；移到工作线程 |
| 回调中更新控件崩溃 | 回调在库线程；必须经信号槽 / `CallAfter` 调度回 UI 线程 |
| 中文路径切片失败 | 确保传入 UTF-8 编码路径（`QString::toStdString` / `wxString::ToStdString(wxConvUTF8)`） |
| 结果字符串乱码或崩溃 | 提前调用了 `HsBaFree*PipelineResult`；先拷贝再释放 |
