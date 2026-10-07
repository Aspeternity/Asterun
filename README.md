<p align="center">
  <img src="docs/assets/brand/asterun-hero.webp" width="100%" alt="Asterun — Launch Faster, Go Further">
</p>

<h1 align="center">Asterun</h1>

<p align="center">
  <strong>快速、智能、键盘优先的 Windows 启动器。</strong><br>
  <sub>Launch Faster, Go Further</sub>
</p>

<p align="center">
  <a href="README.md"><strong>简体中文</strong></a> ·
  <a href="README.en.md">English</a>
</p>

<p align="center">
  <a href="https://github.com/Aspeternity/Asterun/releases/latest"><img alt="最新版本" src="https://img.shields.io/github/v/release/Aspeternity/Asterun?display_name=tag&sort=semver"></a>
  <a href="https://github.com/Aspeternity/Asterun/actions/workflows/build.yml"><img alt="构建状态" src="https://github.com/Aspeternity/Asterun/actions/workflows/build.yml/badge.svg?branch=main"></a>
  <a href="LICENSE"><img alt="许可证" src="https://img.shields.io/github/license/Aspeternity/Asterun"></a>
  <img alt="平台" src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?logo=windows11&logoColor=white">
  <img alt="C++23" src="https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white">
</p>

<p align="center">
  <a href="#界面预览">界面预览</a> ·
  <a href="#下载">下载</a> ·
  <a href="#核心特性">核心特性</a> ·
  <a href="#快速开始">快速开始</a> ·
  <a href="docs/USER_GUIDE.md"><strong>使用教程</strong></a> ·
  <a href="#everything-文件搜索">Everything 搜索</a> ·
  <a href="#从源码构建">源码构建</a> ·
  <a href="CHANGELOG.md">更新日志</a>
</p>

---

## 项目简介

Asterun 是一款面向 Windows 10/11 的原生、轻量、键盘优先启动器，目标是在尽可能低的打扰下，让“搜索 → 定位 → 启动”足够快地融入日常工作流。

它以 C++23 + Win32 构建，将程序发现、拼音与中英文混合搜索、使用习惯排序、智能数字键启动、自定义快捷项、可选 Everything 文件/文件夹搜索、原生更新与卸载统一在一个紧凑的启动体验中。

## v1.0.4 — 开发中

v1.0.4 当前正在开发，首批优化聚焦启动器输入体验与快捷词可见性；正式发布前仍以 v1.0.3 为稳定推荐版本。

## v1.0.3 — 当前稳定版

Asterun 1.0.3 是当前稳定版本，也是目前推荐普通用户使用的版本。完整版本历史统一保存在 [CHANGELOG.md](CHANGELOG.md)，主页只保留下载、功能与使用信息。

## 界面预览

<p align="center">
  <img src="docs/assets/readme/modern-compact.webp" alt="Asterun Modern Compact 启动器界面" width="920">
</p>

<p align="center"><sub>Modern Compact — 现代紧凑启动器界面</sub></p>

<table>
  <tr>
    <td align="center" width="50%">
      <img src="docs/assets/readme/classic.webp" alt="Asterun Classic 启动器界面" width="100%">
    </td>
    <td align="center" width="50%">
      <img src="docs/assets/readme/settings-search-sources.webp" alt="Asterun 搜索来源设置界面" width="100%">
    </td>
  </tr>
  <tr>
    <td align="center"><sub>Classic — 受经典 ALTRun 启发的紧凑界面</sub></td>
    <td align="center"><sub>搜索来源 — Windows Providers 与托管 Everything</sub></td>
  </tr>
</table>

> 预览图基于 Windows 实机界面截图进行轻量整理，用于 GitHub 项目展示。

## 下载

| 架构 | 稳定版 |
| --- | --- |
| Windows x64 | [下载 Asterun-x64.zip](https://github.com/Aspeternity/Asterun/releases/latest/download/Asterun-x64.zip) |
| Windows ARM64 | [下载 Asterun-ARM64.zip](https://github.com/Aspeternity/Asterun/releases/latest/download/Asterun-ARM64.zip) |

- [最新稳定版 Release](https://github.com/Aspeternity/Asterun/releases/latest)
- [SHA-256 校验值](https://github.com/Aspeternity/Asterun/releases/latest/download/SHA256SUMS.txt)
- 支持系统：**Windows 10 / Windows 11**
- 发布形式：**便携 ZIP**，无需安装器

> 请先把 ZIP 解压到正常可写目录，再运行程序；不要直接在压缩包内部启动。

## 核心特性

| | |
| --- | --- |
| **原生轻量** | C++23 + Win32，目标是快速启动和尽可能小的便携体积。 |
| **两套启动器界面** | Classic（ALTRun-inspired）+ Modern Compact 现代紧凑风格。 |
| **键盘优先** | 默认全局唤醒快捷键为 <kbd>Alt</kbd> + <kbd>Space</kbd>。 |
| **智能搜索** | 支持模糊匹配、英文首字母、中文全拼/拼音首字母以及中英文混合搜索。 |
| **智能排序** | 根据使用频率和最近使用情况，让常用结果更靠前。 |
| **智能数字键启动** | 可选 1–9、0 快速启动，并根据输入意图判断数字是继续输入还是执行结果。 |
| **Windows 程序发现** | 支持开始菜单、UWP/MSIX、App Paths、PATH 等来源。 |
| **Everything 集成** | 可选高速文件/文件夹搜索，支持 Asterun 托管获取，也兼容用户自行维护的 Everything。 |
| **快捷项管理** | 原生快捷项管理器，支持别名、参数、工作目录、动态输入与路径转换。 |
| **便携数据** | 设置、快捷项、使用记录和搜索缓存都保存在本地 `data/` 目录。 |
| **原生更新与卸载** | 自带 `Update.exe` 和 `Uninstall.exe`，提供 x64 / ARM64 版本。 |
| **双语界面** | 支持简体中文和 English。 |

## 快速开始

1. 从 [最新 Release](https://github.com/Aspeternity/Asterun/releases/latest) 下载对应架构的 ZIP。
2. 解压到你希望长期使用的目录。
3. 运行 `Asterun.exe`。
4. 按 <kbd>Alt</kbd> + <kbd>Space</kbd> 唤出启动器。
5. 通过托盘菜单进入 **设置**、**快捷项管理**、**关于** 或退出程序。

Asterun 为便携软件。首次运行后会在程序旁创建运行时数据目录。

## 搜索来源

Asterun 可以把多个 Windows 来源统一到同一个搜索界面中：

- 开始菜单快捷方式
- UWP / MSIX / Microsoft Store 应用
- App Paths
- `PATH` 中的可执行程序
- 用户自定义快捷项
- 可选的 Everything 文件和文件夹结果

可以在 **设置 → 搜索来源** 中分别开启或关闭。

## Everything 文件搜索

Everything 集成是可选功能。

在 **设置 → 搜索来源** 中启用后，Asterun 会提供 **获取并启动 Everything** 流程。托管模式会在使用前校验下载内容，并维护 Asterun 自己的运行时/服务生命周期。

如果你已经自行安装并维护兼容的 Everything，Asterun 的设计原则是不接管外部用户安装。

## 启动器样式

### Classic

Classic 保留经典 ALTRun 的紧凑布局和键盘操作方式，并在下方“致谢”中注明使用的原始授权素材。

### Modern Compact

Modern Compact 使用同一套搜索与排序核心，但提供更适合 Windows 10/11 的现代紧凑界面、DPI 自适应尺寸和更清晰的结果层级。

两套界面共享相同的搜索、排序、快捷项和 Provider 核心。

## 便携数据

运行时数据保存在程序本地：

```text
Asterun/
├─ Asterun.exe
├─ Update.exe
├─ Uninstall.exe
├─ VERSION
├─ README.md
├─ dict/
├─ third_party/
└─ data/                  # 首次运行后创建
   ├─ settings.json
   ├─ commands.json
   ├─ usage.json
   └─ provider-cache.json
```

配置采用独立版本化 Schema、原子写入和备份恢复机制。详细设计见 [Config Core schemas](docs/CONFIG_SCHEMA.md)。

## 发布渠道

| 渠道 | 用途 | 链接 |
| --- | --- | --- |
| **Stable** | 推荐普通用户使用。正式版本号、不可变 Release。 | [最新稳定版](https://github.com/Aspeternity/Asterun/releases/latest) |
| **Development** | 最新成功通过 CI 的 `main` 滚动构建，可能包含尚未完成的修改。 | [dev-latest](https://github.com/Aspeternity/Asterun/releases/tag/dev-latest) |

官方 Release 会提供 x64 / ARM64 便携包、SHA-256 校验文件和更新清单。

## 从源码构建

### 环境要求

- Windows 10 或 Windows 11
- Visual Studio 2022，并安装 **Desktop development with C++**
- CMake **3.24+**
- Git

### x64

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

### ARM64

需要安装 Visual Studio ARM64 工具链：

```powershell
cmake -S . -B build-arm64 -A ARM64
cmake --build build-arm64 --config Release
```

CMake 会获取项目锁定的构建依赖。正式发布包还会通过 CI 的版本一致性、包结构和运行时 Smoke Test 检查。

## 项目文档

| 文档 | 内容 |
| --- | --- |
| [docs/USER_GUIDE.md](docs/USER_GUIDE.md) | 中文完整使用教程：快捷键、智能数字键、Everything、快捷项与便携使用 |
| [CHANGELOG.md](CHANGELOG.md) | 正式更新记录与历史版本变更 |
| [ROADMAP.md](ROADMAP.md) | 已完成里程碑和后续方向 |
| [docs/CONFIG_SCHEMA.md](docs/CONFIG_SCHEMA.md) | 便携配置、Schema 与迁移规则 |
| [docs/UPDATE_SYSTEM.md](docs/UPDATE_SYSTEM.md) | 原生更新系统架构 |
| [docs/EVERYTHING_COMPATIBILITY.md](docs/EVERYTHING_COMPATIBILITY.md) | Everything 集成与兼容策略 |
| [docs/V1.0_RELEASE_VALIDATION.md](docs/V1.0_RELEASE_VALIDATION.md) | 1.0.0 正式版发布验证 |
| [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) | 第三方组件与许可证说明 |

原来首页里大量 Alpha / Beta 开发记录已经统一归档到 [CHANGELOG.md](CHANGELOG.md)，主页只保留用户真正需要的信息。

## 问题反馈

如果遇到可以复现的 Bug 或兼容性问题，请提交 [GitHub Issue](https://github.com/Aspeternity/Asterun/issues)。

建议同时提供：

- Asterun 版本
- Windows 版本和系统架构
- 复现步骤
- 预期行为与实际行为
- 可以说明问题的截图或错误代码

## 致谢

Asterun 是独立开发的 Windows 启动器。Classic 模式的交互与视觉方向受到经典 ALTRun 启发。

Classic 模式包含经授权使用的原版启动器背景、两个角落图形素材以及 `Popup.wav`。**Asterun 的应用图标、托盘图标与品牌视觉均为 Asterun 自有设计，不再使用原版 ALTRun 应用图标。**

Everything 为 voidtools 的第三方产品。其他依赖、授权和归属信息见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

## 许可证

Asterun 源代码采用 [MIT License](LICENSE)。

随项目分发的第三方组件和原始 ALTRun 素材仍分别遵循其对应许可证、声明和授权范围。

---

<p align="center">
  <strong>Asterun</strong><br>
  Launch Faster, Go Further
</p>
