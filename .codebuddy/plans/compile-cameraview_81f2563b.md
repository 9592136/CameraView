---
name: compile-cameraview
overview: 获取 Windows SDK 头文件，使用已有的便携工具链（CMake + Ninja + clang-cl + .lib）编译 CameraView 项目并运行。
todos:
  - id: fetch-headers
    content: 从 SourceForge 下载 MinGW-w64 便携版或替代源，提取 Windows SDK 头文件到 D:\CameraView\tools\mingw-headers\
    status: completed
  - id: scan-includes
    content: 使用 [subagent:code-explorer] 扫描源码确认完整 Win32 头文件依赖和 MSVC 特定语法
    status: completed
  - id: create-toolchain
    content: 创建 D:\CameraView\tools\toolchain.cmake，配置 clang-cl + lld-link + 头文件/库路径
    status: completed
    dependencies:
      - fetch-headers
      - scan-includes
  - id: cmake-configure
    content: 运行 CMake 配置（Ninja 生成器 + 工具链文件，关闭 OpenCV）
    status: completed
    dependencies:
      - create-toolchain
  - id: compile
    content: 编译 CameraView.exe 和 CameraViewDomainTests.exe
    status: completed
    dependencies:
      - cmake-configure
  - id: run-tests
    content: 运行 ctest 验证单元测试通过，确认编译正确性
    status: completed
    dependencies:
      - compile
---

## 用户需求

使用已下载的便携工具链（CMake + Ninja + LLVM/clang-cl + WinSDK .lib）编译 CameraView 项目及其单元测试，最终运行生成的 CameraView 可执行文件和 CameraViewDomainTests 测试。

## 核心目标

1. 获取缺失的 Windows SDK 头文件（windows.h 等），从不需管理员权限的来源
2. 配置 CMake 工具链文件，将 clang-cl、lld-link 和头文件/库路径统一管理
3. 成功编译 CameraView.exe 和 CameraViewDomainTests.exe
4. 运行单元测试验证编译结果

## 技术栈

- **编译器**: clang-cl 19.1.7 (MSVC ABI 兼容模式)
- **链接器**: lld-link 19.1.7
- **构建系统**: CMake 4.2.1 + Ninja 1.12.1
- **标准**: C++17
- **目标架构**: x64（现有 WinSDK .lib 为 x64，CMake 会自动检测 `CMAKE_SIZEOF_VOID_P EQUAL 8`）
- **系统库**: user32, gdi32, msimg32, comdlg32, ole32, windowscodecs, shell32, comctl32

## 实现方案

### 总体策略

分三步走：头文件获取 → 工具链配置 → 编译运行。每一步独立可验证，失败时有明确降级路径。

### 步骤一：获取 Windows SDK 头文件

由于现有 NuGet `Microsoft.Windows.SDK.CPP.x64` 包仅含 `.lib` 无头文件，GitHub 不可达，首选从 **SourceForge** 下载 MinGW-w64 便携版（SourceForge 在国内通常比 GitHub 更可达）。下载后仅提取 `include/` 目录中的 Windows 头文件，不依赖 MinGW 编译器本身。

**降级路径**：如果 SourceForge 也不可达，尝试从 NuGet 搜索 `Microsoft.Windows.SDK.Headers` 或 `cppwinrt` 包，看是否包含头文件；或尝试 LLVM 官方提供的 Windows SDK 头文件包（`llvm-mingw` 发行版在 SourceForge 也托管）。

### 步骤二：创建 CMake 工具链文件

在 `D:\CameraView\tools\toolchain.cmake` 中配置：

- `CMAKE_C_COMPILER` / `CMAKE_CXX_COMPILER`: 指向 clang-cl.exe
- `CMAKE_LINKER`: 指向 lld-link.exe
- `CMAKE_MAKE_PROGRAM`: 指向 ninja.exe
- 头文件搜索路径（指向提取的 Windows 头文件目录 + LLVM 自带头文件）
- 库搜索路径（指向 `D:\CameraView\tools\winsdk\c\um\x64` 和 `D:\CameraView\tools\winsdk\c\ucrt\x64`）
- 强制 `CMAKE_SYSTEM_NAME=Windows`

**关键配置**：由于 clang-cl 需要模拟 MSVC 环境，必须在编译标志中显式添加 `/imsvc` 前缀指向外部头文件目录，并在链接标志中添加 `/LIBPATH:` 指向库目录。

### 步骤三：配置与编译

```
cmake -S D:\CameraView -B D:\CameraView\build `
  -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=D:\CameraView\tools\toolchain.cmake `
  -DCAMERAVIEW_WITH_OPENCV=OFF `
  -DCAMERAVIEW_BUILD_TESTS=ON

cmake --build D:\CameraView\build --config Release
ctest --test-dir D:\CameraView\build --output-on-failure
```

OpenCV 关闭避免额外的 find_package 失败；测试目标开启用于验证编译正确性。

## 实施注意事项

- **架构选择**: 现有 .lib 为 x64，自然选择 64 位编译。CMakeLists.txt 中 `CMAKE_SIZEOF_VOID_P EQUAL 8` 自动匹配 MUCamSDK x64 DLL 目录。
- **头文件兼容性**: MinGW-w64 的 Windows 头文件 API 定义与 MSVC SDK 头文件高度一致，clang-cl 可以正确解析。但部分 SAL 注解宏可能有差异，若编译报错可添加空宏定义适配。
- **编译器仅头文件依赖**: 本次编译不依赖 MinGW 的 `gcc` 或 `g++`，仅使用其头文件配合 clang-cl 的 MSVC ABI 编译模式。
- **日志**: 使用 CMake 默认输出；编译错误信息直接查看控制台输出，无需额外日志框架。
- **备份兼容**: 不修改 CMakeLists.txt、源代码或任何项目文件，所有配置通过 toolchain.cmake 和命令行参数完成，随时可恢复 MSVC 编译方式。

## 使用的 Agent Extensions

### SubAgent

- **code-explorer**
- 用途：在配置工具链前，扫描源码中所有 `#include <windows.h>` 和 Win32 宏使用，确认所需的头文件完整清单；同时检查是否有隐式的 MSVC 特定扩展（如 `#pragma comment(lib)` 或 `__declspec`）需要适配。
- 预期结果：输出完整的 Windows API 头文件依赖清单和潜在兼容性问题列表，确保工具链配置准确无遗漏。