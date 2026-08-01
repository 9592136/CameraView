---
name: split-camerapreviewapp-impl
overview: 将 CameraPreviewApp.h 中约 7600+ 行的内联方法实现移动到 CameraPreviewApp.cpp 中，头文件仅保留类声明、成员变量和必要的内联/模板函数。
todos:
  - id: explore-method-boundaries
    content: 使用 [subagent:code-explorer] 精确扫描 CameraPreviewApp.h 中所有方法的签名和起止行号
    status: completed
  - id: generate-cpp
    content: "根据扫描结果，将全部方法实现写入 CameraPreviewApp.cpp，每个方法加上 CameraPreviewApp:: 前缀"
    status: completed
    dependencies:
      - explore-method-boundaries
  - id: strip-header
    content: 精简 CameraPreviewApp.h，将所有内联方法体替换为纯声明（仅保留签名+分号）
    status: completed
    dependencies:
      - generate-cpp
  - id: verify-build
    content: 构建项目验证编译通过，确认链接成功无符号缺失
    status: completed
    dependencies:
      - strip-header
---

## 用户需求

将 `CameraPreviewApp.h`（7969行）中的全部方法内联实现移动到 `CameraPreviewApp.cpp`，完成此前代码拆分计划的最后一步。

## 产品概述

`CameraPreviewApp` 是 CameraView（MUCam 显微镜相机预览软件）的核心 UI 类，负责相机控制、图像预览、测量工具、拼接处理、EDF 处理、报告模板等全部功能逻辑。当前 ~7800 行方法实现全部内联在头文件中，需要提取到独立的 .cpp 源文件。

## 核心目标

- **头文件精简**：`CameraPreviewApp.h` 仅保留类声明、嵌套类型 `ReportTemplateDesignerState`、成员变量声明、`GetApp()` 内联辅助函数，以及必要的 #include 指令
- **源文件补全**：`CameraPreviewApp.cpp` 填入全部方法实现（约 7800 行），每个方法添加 `CameraPreviewApp::` 前缀
- **编译通过**：确保项目在 MinGW/Clang 工具链下成功编译链接
- **逻辑不变**：保持所有方法签名、调用关系、业务逻辑完全不变

## 技术方案

### 技术栈

- 语言：C++17
- 编译器：MinGW-w64（GCC）或 LLVM/Clang
- 构建系统：CMake 3.20+
- UI 框架：Win32 API（CreateWindowEx、GDI/GDI+）
- 项目架构模式：Coordinator + Actions 分层架构

### 实现策略

**核心方法**：将 CameraPreviewApp.h 中所有内联方法体替换为纯函数声明（仅保留签名 + 分号），同时在 CameraPreviewApp.cpp 中补入所有带 `CameraPreviewApp::` 前缀的实现。

**技术决策**：

1. **所有方法均可移出**：CameraPreviewApp 不是模板类，所有方法均非模板方法，可安全从 .h 移入 .cpp
2. **头文件保留完整 #include**：其他翻译单元（main.cpp、WindowProc.cpp、LayoutControls.cpp）需要完整类定义访问成员变量和调用方法，头文件必须保留全部 include
3. **.cpp 需额外 include**：方法实现依赖 coordinators（模板头文件）、HistogramCalculator、HistogramRenderer、ImageAdjuster、Localization 等，这些头文件仅在 .cpp 中需要（声明阶段不依赖）
4. **inline GetApp() 保留在头文件**：该函数是多翻译单元共享的内联辅助函数，必须在头文件中

**性能分析**：

- 编译时间：单个 .cpp 文件约 7800 行，编译耗时与拆分前相当（原本全在 main.cpp 中）
- 链接时间：从单个大 TU 变为一个 .cpp 提供所有符号，链接开销相近
- 运行时性能：无影响（纯代码重组，无逻辑变更）
- 增量编译：修改方法实现时仅重编译 .cpp（不再影响 main.cpp 等 3 个翻译单元）

**重构模式**：

```
// 重构前（头文件内联）：
void Start() {
    Stop();
    ClearLatestFrame();
    ...
}

// 重构后（头文件仅声明）：
void Start();

// 重构后（源文件实现）：
void CameraPreviewApp::Start() {
    Stop();
    ClearLatestFrame();
    ...
}
```

### 架构设计

```
┌──────────────────────────────────────────────────────────────┐
│  main.cpp (wWinMain 入口)                                    │
│  ├── Include: CameraPreviewApp.h, MainMenu.h, WindowProperties.h │
└────────────────────┬─────────────────────────────────────────┘
                     │
     ┌───────────────┼───────────────┬──────────────────────┐
     │               │               │                      │
     ▼               ▼               ▼                      ▼
┌──────────┐  ┌────────────┐  ┌───────────────┐  ┌──────────────────┐
│WindowProc│  │LayoutControls│  │MainMenu.cpp   │  │CameraPreviewApp  │
│.cpp      │  │.cpp          │  │               │  │.cpp (~7800行)   │
│(WM 分发) │  │(布局计算)    │  │(菜单创建)     │  │(全部方法实现)   │
└────┬─────┘  └──────┬──────┘  └───────┬───────┘  └────────┬─────────┘
     │               │               │                      │
     └───────────────┴───────────────┴──────────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────┐
                    │ CameraPreviewApp.h       │
                    │  • 类声明 (方法签名)      │
                    │  • ReportTemplateDesignerState │
                    │  • 成员变量 (~120行)      │
                    │  • GetApp() inline        │
                    │  • 所有 #include          │
                    └──────────────────────────┘
```

### 目录结构

```
src/ui/
├── CameraPreviewApp.h    # [MODIFY] 仅保留类声明+成员变量+GetApp()
├── CameraPreviewApp.cpp  # [MODIFY] 填入全部 ~7800 行方法实现
├── LayoutControls.cpp    # [不变] 依赖 CameraPreviewApp.h
├── WindowProc.cpp        # [不变] 依赖 CameraPreviewApp.h
└── ...                   # [不变] 其他已拆分文件

src/
└── main.cpp              # [不变] 已完成精简（59行）

CMakeLists.txt            # [不变] CameraPreviewApp.cpp 已在源文件列表中
```

### 关键实现细节

**CameraPreviewApp.cpp 需新增的额外 include**（方法实现依赖但头文件声明不依赖）：

```cpp
#include "ui/CameraViewCoordinator.h"
#include "ui/FluorescenceViewCoordinator.h"
#include "ui/MeasurementViewCoordinator.h"
#include "ui/ProcessingViewCoordinator.h"
#include "ui/ViewportViewCoordinator.h"
#include "imaging/HistogramCalculator.h"
#include "ui/HistogramRenderer.h"
#include "imaging/ImageAdjuster.h"
#include "i18n/Localization.h"
```

**转换规则**：

- 普通方法：`ReturnType Name(args) { body }` → 头文件 `ReturnType Name(args);`，源文件 `ReturnType CameraPreviewApp::Name(args) { body }`
- static 方法：`static ReturnType Name(args) { body }` → 头文件 `static ReturnType Name(args);`，源文件 `ReturnType CameraPreviewApp::Name(args) { body }`
- 构造函数/析构函数：`CameraPreviewApp(HWND hwnd) : hwnd_(hwnd) { body }` → 头文件 `explicit CameraPreviewApp(HWND hwnd);`，源文件 `CameraPreviewApp::CameraPreviewApp(HWND hwnd) : hwnd_(hwnd) { body }`
- 头文件保留不动的：`ReportTemplateDesignerState` 嵌套结构体、所有成员变量（7841-7961行）、`GetApp()` 内联函数（7964-7967行）

**错误处理与回归防控**：

- 保持所有现有方法签名不变
- 移动过程中不修改任何业务逻辑
- 编译时由编译器验证所有声明与实现匹配
- 方法间调用关系不变（均通过 `this->` 或隐式访问成员变量）

## Agent Extensions

### SubAgent

- **code-explorer**
- 用途：精确识别 CameraPreviewApp.h 中每个方法的起止行号，以及方法间的访问权限标签（public:/private:）位置，确保转换时不会遗漏或截断任何方法
- 预期结果：生成完整的方法列表，包含每个方法的签名和行范围（{开始行, 结束行}），用于指导后续的代码转换