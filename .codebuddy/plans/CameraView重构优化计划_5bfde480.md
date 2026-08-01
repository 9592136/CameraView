---
name: CameraView重构优化计划
overview: 对 CameraView（C++ Win32 显微观察测量程序，约 1.3MB 源码 / 183 文件）进行持续重构优化，覆盖四大目标：改善代码结构/分层、消除重复与冗余、提升可测试性、性能与资源优化。遵循"小而稳、每步保证 CMake/VS 编译通过且 CameraViewDomainTests 全绿"的原则分批推进；暂不接入实验性的 src/ai/ 模块。
todos:
  - id: batch1-dedup-utils
    content: 新增 GeometryOps/StringOps 工具类，集中坐标换算与枚举字符串重复逻辑
    status: completed
  - id: batch1-tests
    content: 用 [subagent:code-explorer] 确认抽离点后，为纯逻辑补最小断言单测并扩 CameraViewDomainTests
    status: completed
    dependencies:
      - batch1-dedup-utils
  - id: batch2-perf
    content: 优化 FrameBuffer/PreviewFrameComposer 缓冲复用与 HistogramCalculator 单次遍历
    status: completed
  - id: batch3-split-wndproc
    content: 用 [subagent:code-explorer] 定位面板分支，新增各 ui/*ViewCoordinator 并下沉 main.cpp 消息分发
    status: completed
    dependencies:
      - batch1-dedup-utils
  - id: batch4-domain-tests
    content: 为 Measurement/Geometry/ProcessingParameterRules/TextInputParser 补充可执行断言单测
    status: completed
    dependencies:
      - batch3-split-wndproc
  - id: batch5-ai-isolate
    content: 清理 main.cpp 中未编译使用的 src/ai include，不接入构建
    status: completed
    dependencies:
      - batch3-split-wndproc
  - id: batch6-docs
    content: 更新 README 与 docs/implementation_progress.md 记录重构与 UML 一致性
    status: completed
    dependencies:
      - batch1-tests
      - batch2-perf
      - batch3-split-wndproc
      - batch4-domain-tests
      - batch5-ai-isolate
---

## 用户需求

对 CameraView（C++ Win32 显微观察测量程序，约 1.3MB 源码 / 183 文件）进行持续重构优化，覆盖四项既定目标：改善代码结构/分层、消除重复与冗余、提升可测试性、性能与资源优化。

## 产品概述

在不改变相机 SDK 调用语义与用户可见功能的前提下，对现有高度模块化代码做小步、安全、可验证的重构。每批改动后必须保证 CMake/VS Release 编译通过且相机领域单元测试全绿，随时可停。

## 核心特性

- 消除重复：统一各 *Formatter / 枚举与字符串转换中的重复逻辑，合并相似代码路径
- 性能优化：复用 FrameBuffer/PreviewFrameComposer 后台缓冲、减少大图重复拷贝与伪彩重算、合并 HistogramCalculator 冗余计算
- 分层改善：将 main.cpp 巨型 WndProc 中按面板域（相机/荧光/测量/处理/视口）的消息分发下沉到各 ui/*Actions 或新增协调类，降低全局状态耦合
- 可测试性：为 domain/imaging 纯逻辑类（Measurement、几何换算、ProcessingParameterRules、TextInputParser）补充可执行断言的单元测试，扩大 CameraViewDomainTests 实质覆盖
- 实验模块隔离：main.cpp 中未被编译使用的 src/ai 头文件引用予以清理/隔离，不接入构建
- 文档同步：更新 README 与 docs/implementation_progress.md，保持与代码一致（仅文本，不重渲染 UML PNG）

## 技术栈

- 语言/标准：C++17（cxx_std_17），Win32 API，无 OpenCV/Qt 硬依赖（OpenCV 仅作可选拼接后端）
- 构建：CMake 3.20 + Visual Studio 2022 工具链；双目标 CameraView 与 CameraViewDomainTests
- 测试：基于 tests/domain_smoke.cpp 的编译期 + 断言式单元测试，通过 ctest 运行
- 架构模式：沿用现有分层（app / camera / domain / imaging / platform / storage / ui），保持 *Actions/*Formatter/*State 小类职责单一约定

## 实现策略

采用"小步提交、每步编译+测试绿灯"的增量重构（Strangler Fig 式）。优先抽取纯函数与无状态工具类（低风险、易测试），再逐步把 main.cpp 窗口过程的面板消息分发下沉到协调类（中风险），最后补实质性单元测试扩大覆盖。所有改动保持向后兼容，不修改相机 SDK 调用语义。

### 关键技术决策

1. **重复逻辑下沉到工具类**：新建 `src/imaging/GeometryOps.h` / `src/platform/StringOps.h` 等无状态头文件，集中坐标换算、枚举↔字符串映射，避免各 *Formatter 重复实现。复用现有 `MeasurementFormatter`/`Localization.h` 既有约定，不另起炉灶。
2. **缓冲复用沿用 FrameBuffer 模式**：PreviewFrameComposer 已复用后台缓冲，本次仅消除仍存在的整帧深拷贝与伪彩重复计算，不改变对外接口。
3. **WndProc 拆分按面板域下沉**：新增 `src/ui/{Camera,Fluorescence,Measurement,Processing,Viewport}ViewCoordinator` 协调类（或扩展现有 *Actions），把对应 WM_COMMAND/WM_xxx 分支从 main.cpp 迁走；main.cpp 仅保留窗口创建、消息路由与全局装配。优先复用现有 ui/*Actions 已有方法，避免重复逻辑。
4. **ai 模块隔离**：main.cpp 对 `src/ai/*.h` 的 include 实际未被编译使用（ai 不在 CMake 目标内），本轮移除这些无用 include，保持编译稳定；不新增 ai 源码到构建。

### 性能与可靠性

- 帧路径为热点：FrameBuffer 发布/读取与 PreviewFrameComposer 合成需 O(1) 引用或最小拷贝；避免每帧重新分配 std::vector 缓冲（复用容量）。
- HistogramCalculator 在预览刷新高频调用，合并多次遍历为单次 pass。
- 单元测试为纯逻辑、内存小、毫秒级；不引入真实相机依赖。

## 实现注意事项

- 每批改动后必须执行：CMake 配置 + Release 构建 + ctest，确保 CameraViewDomainTests 全绿。
- 复用现有 logger 习惯（状态栏/诊断报告文本），不新增日志框架；不打印大负载。
- 不改动 MUCamApi.cpp/.h 与相机驱动调用语义；保持 binning/触发/Bayer 流程不变。
- 改动 CMakeLists.txt 时主目标与测试目标需同步增删源文件，避免链接缺失。
- 保持 docs/ UML 与代码文本一致即可，不重渲染 PNG（避免引入渲染工具依赖）。

## 架构设计

沿用现有分层，不引入新架构模式。重构前后数据流保持一致：
UI 消息 →（原 main.cpp 巨型 WndProc）→ 各 *Actions/*Formatter → domain/imaging 纯逻辑 → FrameBuffer/PreviewFrameComposer → 绘制。
目标：UI 消息 → 各面板 ViewCoordinator → 复用现有 *Actions → 纯逻辑 → 缓冲/绘制，main.cpp 退化为装配与路由。

## 目录结构

```
CameraView/
├── src/
│   ├── main.cpp                         # [MODIFY] 移除无用 ai include；把面板消息分支下沉到协调类，仅保留窗口创建/路由/装配
│   ├── imaging/
│   │   ├── GeometryOps.h                # [NEW] 无状态几何/坐标换算工具，集中重复的坐标↔屏幕换算
│   │   ├── PreviewFrameComposer.cpp/.h  # [MODIFY] 消除整帧深拷贝与伪彩重复计算，强化缓冲复用
│   │   ├── HistogramCalculator.cpp/.h   # [MODIFY] 合并多次遍历为单次 pass
│   │   └── ProcessingParameterRules.cpp/.h # [MODIFY] 复用既有校验逻辑，供单测覆盖
│   ├── platform/
│   │   └── StringOps.h                  # [NEW] 枚举↔字符串、通用文本格式化辅助，统一各 Formatter 重复
│   ├── camera/
│   │   └── FrameBuffer.cpp/.h           # [MODIFY] 复用缓冲容量，减少发布/读取拷贝
│   ├── ui/
│   │   ├── CameraViewCoordinator.cpp/.h       # [NEW] 相机面板消息协调，复用 CameraPanelActions
│   │   ├── FluorescenceViewCoordinator.cpp/.h  # [NEW] 荧光面板消息协调，复用 Fluorescence*/Dye*
│   │   ├── MeasurementViewCoordinator.cpp/.h   # [NEW] 测量面板消息协调，复用 Measurement*
│   │   ├── ProcessingViewCoordinator.cpp/.h    # [NEW] 处理面板消息协调，复用 Processing*
│   │   └── ViewportViewCoordinator.cpp/.h       # [NEW] 视口交互消息协调，复用 ViewportInteractionActions
│   └── domain/
│       └── Measurement.cpp/.h           # [MODIFY] 确认几何算法可单测，补充纯函数出口
├── tests/
│   └── domain_smoke.cpp                 # [MODIFY] 补充可执行断言的单测（Measurement/GeometryOps/TextInputParser/ProcessingParameterRules）
├── CMakeLists.txt                       # [MODIFY] 同步主/测试目标新增与移除的源文件
├── README.md                            # [MODIFY] 记录本次重构范围与原则
└── docs/
    └── implementation_progress.md       # [MODIFY] 追加重构进度与 UML 一致性说明（仅文本）
```

## Agent Extensions

### Subagent

- **code-explorer**
- 用途：在拆分 main.cpp WndProc 与定位各面板消息分支、确认 *Actions 复用点、统计重复逻辑分布时，跨多文件/多目录搜索调用关系与定义
- 预期结果：精确列出 main.cpp 中可下沉的消息分支及其对应的现有 ui/*Actions 入口，避免遗漏或重复实现