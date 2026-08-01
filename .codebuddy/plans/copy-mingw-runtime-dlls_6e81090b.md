---
name: copy-mingw-runtime-dlls
overview: 在 CMakeLists.txt 中添加 POST_BUILD 步骤，自动将 mingw64 运行时 DLL（libstdc++-6.dll、libgcc_s_seh-1.dll、libwinpthread-1.dll）复制到编译输出目录。
todos:
  - id: copy-mingw-dlls
    content: 在 CMakeLists.txt 中为 CameraView 和 CameraViewDomainTests 添加 POST_BUILD 命令，将 libstdc++-6.dll、libgcc_s_seh-1.dll、libwinpthread-1.dll 复制到编译输出目录
    status: completed
---

## 用户需求

将项目依赖的 MinGW-w64 运行时库 DLL 自动复制到编译输出目录，使 CameraView.exe 和 CameraViewDomainTests.exe 可以直接从 build/ 目录运行，无需手动配置 PATH 环境变量。

## 核心功能

- 在 CMakeLists.txt 中为 CameraView 目标添加 POST_BUILD 命令，编译后将 libstdc++-6.dll、libgcc_s_seh-1.dll、libwinpthread-1.dll 从 tools/mingw64/bin/ 复制到 build/ 目录
- 为 CameraViewDomainTests 目标同样添加 POST_BUILD 命令，将相同的运行时 DLL 复制到 build/ 目录

## 技术方案

### 实现方法

在 CMakeLists.txt 中复用现有的 POST_BUILD 模式（MUCam DLL 复制已使用此模式），为 mingw64 运行时 DLL 添加 `add_custom_command(TARGET ... POST_BUILD)` 指令。

### 关键决策

- 使用 `copy_if_different` 而非 `copy`，避免不必要的时间戳更新，符合现有 MUCam DLL 复制的一致做法
- 仅复制实际链接的 3 个运行时 DLL（libstdc++-6.dll、libgcc_s_seh-1.dll、libwinpthread-1.dll），不复制不需要的 libgomp、libatomic 等
- CameraView 和 CameraViewDomainTests 输出到同一目录（build/），只需在一个目标上复制即可；但为保证两个目标独立可运行，两个目标各添加一次（`copy_if_different` 不会重复复制）

### 修改文件

- **CMakeLists.txt**：在 MUCam DLL 复制代码块后、Tests 部分前，添加 mingw64 运行时 DLL 的 POST_BUILD 复制逻辑