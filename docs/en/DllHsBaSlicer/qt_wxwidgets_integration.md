# Qt / wxWidgets Integration Guide

This guide shows how to call the DllHsBaSlicer pipelines from **Qt** and **wxWidgets** desktop applications, covering project setup, background execution, progress-bar updates and result handling.

> Prerequisite: [DllHsBaSlicer Module Overview](./README.md)

## General Rules

1. **Call `initialize()` first**: once after process startup, before any pipeline call;
2. **Never run sync APIs on the UI thread**: `HsBaRunFdmPipeline` and friends block until slicing finishes — run them on a worker thread;
3. **Callbacks are not on the UI thread**: progress/result callbacks fire on library threads; marshal back to the UI thread before touching widgets;
4. **String lifetimes**: `const char*` fields in config structs must stay valid while the pipeline runs; dynamic strings inside results are released by `HsBaFree*PipelineResult()`.

---

## 1. Qt Integration

### 1.1 CMake Project Setup

Option A: HsBaSlicer installed (`cmake --install`), use `find_package`:

```cmake
find_package(HsBaSlicer CONFIG REQUIRED)

add_executable(SlicerApp mainwindow.cpp slicer_worker.cpp)
target_link_libraries(SlicerApp PRIVATE
    Qt6::Widgets
    HsBaSlicer::DllHsBaSlicer   # brings in the include/HsBaSlicer header path
)
```

Option B: point directly at build artifacts (quick validation):

```cmake
target_include_directories(SlicerApp PRIVATE
    ${HSBA_SOURCE_DIR}/DllHsBaSlicer
    ${HSBA_SOURCE_DIR}                 # pipelinetypes/pipeline_types.h
)
target_link_libraries(SlicerApp PRIVATE
    ${HSBA_BIN_DIR}/DllHsBaSlicer.lib  # Windows import lib; link .so/.dylib on Linux/macOS
)
add_custom_command(TARGET SlicerApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${HSBA_BIN_DIR}/DllHsBaSlicer.dll $<TARGET_FILE_DIR:SlicerApp>)
```

### 1.2 Worker Thread Wrapper (QThread + Signals/Slots)

Recommended pattern: **async API + signal forwarding** — the progress callback only emits signals, and slots run on the UI thread automatically:

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
        m_path = path.toStdString();   // keep UTF-8 bytes alive during the callback
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
        cfg.output_path = nullptr;     // persist gcode_content yourself

        // Sync API blocks in this slot's thread (the worker thread)
        HsBaFdmPipelineResult_t r = HsBaRunFdmPipeline(&cfg, &SlicerWorker::onProgress, this);

        const bool ok = r.success != 0;
        const QString gcode = QString::fromUtf8(r.gcode_content ? r.gcode_content : "");
        const QString err   = QString::fromUtf8(r.error_message ? r.error_message : "");
        HsBaFreePipelineResult(&r);    // free only after copying

        emit finished(ok, gcode, err); // queued across threads -> UI-thread slot
    }

private:
    static void onProgress(int percent, const char* stage, void* user_data)
    {
        // NOTE: runs on the library's internal thread
        auto* self = static_cast<SlicerWorker*>(user_data);
        emit self->progressChanged(percent, QString::fromUtf8(stage ? stage : ""));
    }

    std::string m_name;
    std::string m_path;
};
```

### 1.3 Main Window Usage

```cpp
// mainwindow.cpp (excerpt)
#include <QThread>
#include <QProgressBar>
#include "slicer_worker.h"

void MainWindow::startSlice()
{
    static bool inited = [] { initialize(); return true; }();
    Q_UNUSED(inited);

    auto* thread = new QThread(this);
    auto* worker = new SlicerWorker;             // no parent; deleted manually below
    worker->setModel("stanford_bunny", "models/stanford_bunny.stl");
    worker->moveToThread(thread);

    connect(thread, &QThread::started,  worker, &SlicerWorker::run);
    connect(worker, &SlicerWorker::progressChanged,
            m_progressBar, &QProgressBar::setValue);          // runs on UI thread
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
        statusBar()->showMessage(tr("Slicing finished"));
    }
    else
    {
        statusBar()->showMessage(tr("Slicing failed: %1").arg(error));
    }
}
```

### 1.4 QtConcurrent Shortcut (Optional)

If you don't need fine-grained progress, wrap the sync API in `QtConcurrent::run`:

```cpp
#include <QtConcurrent>

auto future = QtConcurrent::run([path = modelPath.toStdString()] {
    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
    cfg.model_name = "model";
    cfg.model_path = path.c_str();
    HsBaFdmPipelineResult_t r = HsBaRunFdmPipeline(&cfg, nullptr, nullptr);
    return r;   // NOTE: caller must HsBaFreePipelineResult
});
```

---

## 2. wxWidgets Integration

### 2.1 Project Setup

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

### 2.2 CallAfter Marshalling

In wxWidgets, use `CallAfter` to marshal worker-thread data back to the UI thread:

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
    std::string  m_modelPath;   // UTF-8; must outlive the whole slicing run
};
```

```cpp
// slice_panel.cpp (excerpt)
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

        // Copy results into wxString, then marshal to the UI thread
        const wxString gcode = wxString::FromUTF8(r.gcode_content ? r.gcode_content : "");
        const wxString err   = wxString::FromUTF8(r.error_message ? r.error_message : "");
        const bool ok = r.success != 0;
        HsBaFreePipelineResult(&r);

        CallAfter([this, ok, gcode, err] {          // runs on UI thread
            m_gauge->SetValue(100);
            m_status->SetLabel(ok ? wxString("Slicing finished")
                                  : wxString("Slicing failed: ") + err);
            if (ok)
            {
                wxFileOutputStream f("output/result.gcode");
                if (f.IsOk()) f.Write(gcode.ToUTF8(), gcode.Length());
            }
        });
    }).detach();
}

// Library-thread callback -> CallAfter forwards to UI thread
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

> In production, prefer `wxThread` or a thread pool over a bare `std::thread::detach`, and wait for pending work in the panel destructor to avoid a dangling `this`.

---

## 3. SLA / SLS Pipelines

SLA / SLS follow exactly the same pattern as FDM — only the functions and config types change:

```cpp
// SLA: outputs a layer-image zip
HsBaSlaPipelineConfig_t sla = HsBaCreateDefaultSlaConfig();
sla.model_name  = "resin_part";
sla.model_path  = "models/resin_part.stl";
sla.output_path = "output/resin_part.zip";
sla.image_type  = HSBA_SLA_IMAGE_PNG;
HsBaSlaPipelineResult_t slaR = HsBaRunSlaPipeline(&sla, nullptr, nullptr);
HsBaFreeSlaPipelineResult(&slaR);

// SLS: a Lua export script is mandatory
HsBaSlsPipelineConfig_t sls = HsBaCreateDefaultSlsConfig();
sls.model_name        = "nylon_part";
sls.model_path        = "models/nylon_part.stl";
sls.export_lua_script = "scripts/my_sls_export.lua";   // must not be NULL
HsBaSlsPipelineResult_t slsR = HsBaRunSlsPipeline(&sls, nullptr, nullptr);
HsBaFreeSlsPipelineResult(&slsR);
```

---

## 4. Lua Extension Function Registration

Before running pipelines, register custom Lua functions via `lua_register.h`; they are automatically injected when each stage creates its Lua environment:

```cpp
#include "initialize.h"
#include "lua_register.h"
#include <lua.hpp>

// Custom Lua C function
static int l_my_database_query(lua_State* L)
{
    const char* sql = luaL_checkstring(L, 1);
    // ... execute query, push results onto Lua stack
    return 1;  // number of return values
}

// Registration function (called when the corresponding stage initializes Lua)
static void registerMyFunctions(lua_State* L)
{
    lua_register(L, "db_query", l_my_database_query);
}

void setupLuaExtensions()
{
    // Register by stage type (after initialize(), before pipeline runs)
    HsBaAdd2DFunction(registerMyFunctions);    // Available in Support/Fill/SLA Output
    HsBaAdd3DFunction(registerMyFunctions);    // Available in Slice/Support
    HsBaAddFileFunction(registerMyFunctions);  // Available in SLS/SLA Output

    // Event callbacks (e.g. Zipper compression events)
    HsBaAddEventCallback("zipper.on_add", registerMyFunctions);
}
```

### Function Types Available Per Stage

| Pipeline Stage | Registration Function | Description |
| --- | --- | --- |
| Slice | `HsBaAdd3DFunction` | 3D model operations |
| Support | `HsBaAdd2DFunction` + `HsBaAdd3DFunction` | 2D contours + 3D model |
| Fill | `HsBaAdd2DFunction` | 2D polygon operations |
| SLS Output | `HsBaAddFileFunction` | File output operations |
| SLA Output | `HsBaAdd2DFunction` + `HsBaAddFileFunction` | 2D + file output |

> **Note**: Registration function pointers must remain valid for the entire process lifetime (use static or global functions).

## FAQ

| Symptom | Cause & Fix |
| --- | --- |
| "DllHsBaSlicer.dll not found" at startup (Windows) | DLL not next to the executable; add a post-build copy (see CMake above) |
| Progress bar frozen / UI hangs | Sync API running on the UI thread; move it to a worker thread |
| Crash when updating widgets inside a callback | Callbacks run on library threads; marshal via signals/slots or `CallAfter` |
| Non-ASCII paths fail | Pass UTF-8 encoded paths (`QString::toStdString` / `wxString::ToStdString(wxConvUTF8)`) |
| Garbled result strings or crash | `HsBaFree*PipelineResult` called too early; copy first, then free |
