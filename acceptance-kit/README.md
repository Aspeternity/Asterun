# Asterun 批次 A 验收套件

本套件只用于验收，不属于 Asterun 产品，也不进入仓库或安装包。它用来验证 PR #64（批次 A：静态运行库与 DLL 加载加固）中 CI 无法验证的部分。

## 被测版本

| 项 | 值 |
|---|---|
| 被测提交 | `d3259c8728356c6ea16e3efe8ae1b861229e5ab9`（PR #64 最终 head） |
| 基线 | main `eefa61a7131e0d770d0e5455c97ba5466dfff450`；对照包用正式版 v1.0.5 |
| CI 运行 | https://github.com/Aspeternity/Asterun/actions/runs/38147765931 |
| 被测包 | 上述运行页面底部 Artifacts 中的 `Asterun-x64`（artifact id 11705615069，GitHub 记录的 digest 为 `sha256:bc777d084cf6d8089ed3936eb970922503167879f810d96c4269617cd96da109`）。下载得到的 zip 里面是 `Asterun-x64.zip`。ARM64 包为 `Asterun-ARM64`（id 11705685461） |
| 对照包 | https://github.com/Aspeternity/Asterun/releases/download/v1.0.5/Asterun-x64.zip ，sha256 `2bf93316a54d84fa7721016d3b0bf48b6a6ddec92f58acde9c94d7401321bd23` |
| 探针 DLL | `probe-dll/out/x64/asterun_dll_probe.dll`，sha256 `e01b82a8da56ea4721ccaeb84d3bc64a4ee98cd18d2c3cba129c09e63cf74e82`（ARM64 版 `a78ba7252e3783fad6c79a072e398925d51495ddb3f54ffcbd025c91b4c07699` 见 probe-dll/SHA256SUMS.txt） |

本会话无法下载 CI artifact（存放 artifact 的主机被网络策略拒绝），所以被测包的 SHA-256 由 `Prepare-Packages.ps1` 在你的环境里计算并记录。

## 分工：哪些已由 CI 完成，哪些需要你操作

| 内容 | 由谁完成 | 状态 |
|---|---|---|
| x64 与 ARM64 全部目标编译（静态运行库，无 LNK2038） | CI | 已通过 |
| 自动化测试：Linux 33、Windows x64 两套各 54（含新增 dll_search_hardening_tests）、ARM64 54 只编译 | CI | 已通过；ARM64 未运行 |
| 3 个 EXE × 2 种架构（x64、ARM64）打包后的导入白名单、无 CRT DLL、DependentLoadFlags=0x800 | CI（verify_pe_contract.py） | 已通过 |
| 打包后的 x64 启动冒烟（原有步骤，runner 上装有运行库） | CI | 已通过 |
| 未装 VC++ 运行库的系统上能否启动 | 你（本套件第 2 阶段） | 待执行 |
| 静态导入与运行时加载是否仍从应用目录或临时目录查找 DLL | 你（第 3–4 阶段，Process Monitor） | 待执行 |
| 主程序、卸载、Everything 服务、SendTo、打包应用、对话框的兼容性 | 你（第 2、4 阶段） | 待执行 |
| 新 Update.exe 执行一次完整更新 | 无法在本批次完成 | 见“已知限制” |
| ARM64 实际运行 | 需要 ARM64 Windows 设备 | 没有设备则记为“未运行” |

## 环境：不需要动你的日常电脑

不要在日常电脑上卸载运行库。用下面任一种一次性环境：

1. **Windows Sandbox（推荐）**：Windows 10/11 专业版、企业版或教育版。在“启用或关闭 Windows 功能”中勾选“Windows 沙盒”，重启。关闭沙盒后其中的一切都会被丢弃，只有映射目录保留。
2. **虚拟机**：Hyper-V、VMware 或 VirtualBox，安装微软提供的 Windows 11 企业版评估版镜像，不安装任何其他软件。

Windows 10 与 Windows 11 各做一次。如果只有一种系统，就如实记录只覆盖了哪一种。

是否“干净”不靠假设：第 1 阶段的 `Check-Environment.ps1` 会检查 System32 中的运行库文件和注册表，并输出 `CLEAN` 或 `NOT clean`。结果是 NOT clean 的环境不能用于“无运行库启动”这一项。

## 套件内容

| 文件 | 用途 |
|---|---|
| `CHECKLIST.md` | 逐步操作、预期结果和结果记录表 |
| `PROCMON.md` | Process Monitor 的过滤条件、采集、导出与汇总 |
| `asterun-acceptance.wsb` | Windows Sandbox 配置，把宿主机的 `C:\AsterunAcceptance` 映射为沙盒内的 `C:\Acceptance` |
| `scripts/Check-Environment.ps1` | 记录系统版本，判断是否装有 VC++ 运行库 |
| `scripts/Prepare-Packages.ps1` | 计算两个包与三个 EXE 的 SHA-256，并解压 |
| `scripts/Summarize-Procmon.ps1` | 从 Process Monitor 导出的 CSV 中列出应用目录和临时目录下的 DLL 访问 |
| `probe-dll/` | 无害探针 DLL 的源码、构建脚本、预构建的 x64/ARM64 二进制和自检脚本 |

探针 DLL 的预构建文件由 `build-llvm.sh` 生成；源码注释中的 MSVC 构建命令没有在本会话中执行过。所有脚本都已在 PowerShell 7.4（Linux）下做过语法解析检查，清单中以 Windows 自带的 PowerShell 5.1 运行，脚本只使用 5.1 可用的语法；`Summarize-Procmon.ps1` 还用样例 CSV 做过功能测试。探针 DLL 由 LLVM 交叉编译，已确认只导入 KERNEL32、不导出任何符号。但这几个文件还没有在 Windows 上实际运行过：第 1 阶段的探针自检就是用来确认这一点的。

## 已知限制

- **完整更新流程**：更新执行的是已安装旧版里的 Update.exe，并且客户端只接受版本号更高的更新。要验证新 Update.exe 做一次完整更新，需要两个都含批次 A、且版本号更高的构建，这需要修改 VERSION 并发布，不在本次授权内。本套件只验证新 Update.exe 的启动、DLL 查找和参数错误时的退出。
- **系统 DLL 自身的依赖**：DEPENDENTLOADFLAG 是否也作用于系统 DLL 自身的依赖链，只作记录，不作断言。
- **主程序正常模式**：按设计不调用 SetDefaultDllDirectories，因为它会在进程内加载第三方 Shell 扩展。它在运行中按名称查找 DLL 的行为保持原样，只作记录。
