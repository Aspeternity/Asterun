# 批次 A 验收清单

Windows 10 与 Windows 11 各执行一遍。把下面的结果表复制两份分别填写，并把 `C:\AsterunAcceptance\logs` 整个文件夹交回。截图统一放在 logs 中，按编号命名，例如 `P2-1-baseline.png`。

约定：`C:\Acceptance` 是沙盒或虚拟机内的工作目录；如使用虚拟机，就把同样的结构复制到虚拟机的 `C:\Acceptance`。

关于 UAC：Windows Sandbox 中的账户通常已经以管理员身份运行，提权步骤可能不会弹出 UAC，普通模式的 Asterun 也会以提权身份运行。P1-1 会记录当前完整性级别。涉及提权的步骤（P2-9 至 P2-13、P3-4 至 P3-6）请另在虚拟机中，用默认开启 UAC 的普通管理员账户再做一遍，并在结果表中注明是哪种环境。

探针 DLL 未签名，杀毒软件可能隔离它。只在一次性的沙盒或虚拟机中使用，不要放入 System32，也不要在日常电脑上使用。

## 第 0 阶段：在宿主机上准备（只下载文件，不改动系统）

1. 新建 `C:\AsterunAcceptance\packages`、`tools`、`kit`、`logs` 四个文件夹。
2. 下载对照包 v1.0.5 x64：https://github.com/Aspeternity/Asterun/releases/download/v1.0.5/Asterun-x64.zip ，另存为 `packages\Asterun-x64-v1.0.5.zip`。
3. 下载被测包：打开 https://github.com/Aspeternity/Asterun/actions/runs/38147765931 ，在页面底部 Artifacts 中下载 `Asterun-x64`。解压得到 `Asterun-x64.zip`，放到 `packages\Asterun-x64.zip`。
4. 下载 Process Monitor，把 `Procmon64.exe` 放到 `tools\`（见 PROCMON.md）。
5. 把整个验收套件解压到 `kit\`，使 `kit\scripts`、`kit\probe-dll` 等直接位于 `kit` 下。
6. 使用 Windows Sandbox 时，双击 `kit\asterun-acceptance.wsb` 启动沙盒；使用虚拟机时，把 `C:\AsterunAcceptance` 复制为虚拟机内的 `C:\Acceptance`。

## 第 1 阶段：环境检查（在沙盒或虚拟机内，打开 PowerShell）

| 编号 | 操作 | 预期 |
|---|---|---|
| P1-1 | `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Check-Environment.ps1 -Log C:\Acceptance\logs\P1-1-environment.txt` | 输出 `vc-runtime=ABSENT ... CLEAN`，并记录完整性级别和 %TEMP% 的长路径。如果是 NOT clean，停止第 2 阶段，换一个环境 |
| P1-2 | `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Prepare-Packages.ps1 -Baseline C:\Acceptance\packages\Asterun-x64-v1.0.5.zip -Candidate C:\Acceptance\packages\Asterun-x64.zip -Root C:\Acceptance\work -Log C:\Acceptance\logs\P1-2-packages.txt` | 对照包的 sha256 等于 `2bf93316a54d84fa7721016d3b0bf48b6a6ddec92f58acde9c94d7401321bd23`；两个包都解压成功并列出 3 个 EXE |
| P1-3 | `Copy-Item C:\Acceptance\kit\probe-dll\out\x64\asterun_dll_probe.dll C:\Acceptance\work\probe-selftest.dll`，然后 `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\probe-dll\Test-Probe.ps1 -Dll C:\Acceptance\work\probe-selftest.dll` | 输出 `probe OK: pid=... process=...powershell.exe dll=...probe-selftest.dll`。探针本身可用，后续“探针没有日志”才有意义 |

## 第 2 阶段：无运行库环境下的启动与功能（必须在 P1-1 为 CLEAN 的环境中）

| 编号 | 操作 | 预期 |
|---|---|---|
| P2-1 | 双击 `C:\Acceptance\work\baseline-v1.0.5\Asterun.exe` | 系统报错，提示找不到 VCRUNTIME140.dll 或 MSVCP140.dll，程序不启动。截图。这证明环境确实没有运行库 |
| P2-2 | 在 PowerShell 运行 `(Start-Process C:\Acceptance\work\baseline-v1.0.5\Update.exe -Wait -PassThru).ExitCode` | 同样报找不到运行库，退出码为 -1073741515（0xC0000135） |
| P2-3 | 双击 `C:\Acceptance\work\candidate-batch-a\Asterun.exe` | 正常启动，出现托盘图标或主窗口，没有缺少 DLL 的提示。截图 |
| P2-4 | `(Start-Process C:\Acceptance\work\candidate-batch-a\Update.exe -Wait -PassThru).ExitCode` | 不弹窗，退出码为 2（参数无效，未做任何修改） |
| P2-5 | 在被测 Asterun 中搜索一个开始菜单程序（例如“记事本”或 notepad）并回车启动 | 程序正常启动（ShellExecute 路径） |
| P2-6 | 搜索“计算器”或 Calculator 并启动 | 打包应用正常激活 |
| P2-7 | 打开设置窗口，切换几个开关，触发一个确认对话框（例如在快捷项管理中删除一项后取消） | 窗口和对话框正常显示，按钮和控件为 Windows 主题样式，不是老式灰色样式（comctl32 v6） |
| P2-8 | 设置中勾选“添加到‘发送到’菜单”；在资源管理器中右键 `C:\Windows\System32\notepad.exe`，选择“发送到 > Asterun” | 快捷项被添加，可以搜索并启动（SendTo 与链接解析） |
| P2-9 | 设置中启用“Everything 文件与文件夹”，按提示获取并启动 Everything；出现 UAC 时同意 | 下载、服务安装成功，状态显示“Everything 正在运行”。这一步以提权方式运行 Asterun.exe 的服务子命令（A3 覆盖的路径）。首次启用时服务尚不存在，实际运行的通常是 `--repair-managed-everything-service`，也可能是 `--set-managed-everything-service enabled`，两者都算覆盖。需要联网 |
| P2-10 | 搜索一个文件名，确认能返回 Everything 结果；再关闭“Everything 文件与文件夹”，出现 UAC 时同意 | 结果正常；关闭成功，没有错误提示（`--set-managed-everything-service disabled`） |
| P2-11 | （可选，覆盖修复路径）把 `candidate-batch-a` 复制为 `candidate-repair`，启动其中的 Asterun 并启用 Everything；然后先在托盘中退出 Everything，再退出 Asterun，把 `candidate-repair` 改名为 `candidate-repair-moved`，再启动其中的 Asterun.exe | 出现 UAC（沙盒中可能不出现），之后 Everything 恢复正常（`--repair-managed-everything-service`）。之后的步骤继续使用 `candidate-batch-a`，不要用改名后的目录 |
| P2-12 | 退出 Asterun，复制一份 `candidate-batch-a` 为 `candidate-uninstall-keep`，运行其中的 Uninstall.exe，选择保留数据 | 卸载完成，没有错误。卸载的执行进程总是以提权方式从 `%TEMP%\Asterun-Elevated.<随机>` 运行，所以会出现 UAC（沙盒中可能不出现）。注意：卸载会移除所有副本共用的 Everything 服务，此后其他目录中的 Everything 会停止工作，P3-4 前需要重新启用 |
| P2-13 | 再复制一份为 `candidate-uninstall-delete`，启动 Asterun 一次后退出，运行 Uninstall.exe，选择删除数据 | 卸载完成，目录被清除，没有错误 |

## 第 3 阶段：DLL 查找取证（Process Monitor，见 PROCMON.md）

此阶段可以接着在同一个干净环境中做。每一项都先 Clear 并开始采集，操作结束后停止采集、导出 CSV，再运行汇总脚本。被测项的 `-Watch` 为 `"C:\Acceptance\work\candidate-batch-a;$env:TEMP"`，对照项为 `"C:\Acceptance\work\baseline-v1.0.5;$env:TEMP"`；如果复制了目录，就用实际目录。

| 编号 | 场景 | 预期（CHECKED 部分） |
|---|---|---|
| P3-1 | 对照：运行 baseline-v1.0.5\Update.exe（无参数） | 出现应用目录下 VCRUNTIME140.dll 等运行库的查找记录。这证明取证方法能看到应用目录查找 |
| P3-2 | 被测：candidate-batch-a\Update.exe（无参数） | 0 条 |
| P3-3 | 被测：主程序正常启动，完成一次搜索后退出 | 不得出现 vcruntime、msvcp 或 api-ms-win-crt 名称；其他名称只作记录 |
| P3-4 | 被测：在 `candidate-batch-a` 中重新启用 Everything（P2-12 已移除共用服务，所以会再次走服务安装） | 提权的 Asterun.exe 进程 0 条。用汇总中的 cmd 列区分：提权进程的命令行含 `--repair-managed-everything-service` 或 `--set-managed-everything-service` |
| P3-5 | 被测：P2-10 关闭 Everything | 提权进程 0 条 |
| P3-6 | 被测：P2-12 卸载（保留数据） | Uninstall.exe 的所有进程（含临时副本）0 条 |

## 第 4 阶段：探针 DLL 植入（证明“即使目录里放了同名 DLL 也不会被加载”）

探针 DLL 被加载时，会在 `%PUBLIC%\AsterunDllProbe\probe-log.txt` 中写一行。每一项开始前先记录该文件的行数。

| 编号 | 操作 | 预期 |
|---|---|---|
| P4-0 | 运行 `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs" > C:\Acceptance\logs\P4-0-knowndlls.txt` | 记录本系统的 KnownDLLs。KnownDLLs 中的名称总是从系统副本加载，不能用来做植入测试 |
| P4-1 | 对照组（证明方法有效）：复制 `baseline-v1.0.5` 为 `p4-baseline`，把探针复制为 `p4-baseline\VCRUNTIME140.dll`，在 Process Monitor 采集下运行 `p4-baseline\Update.exe` | 进程启动失败（找不到入口点等提示属正常）；Process Monitor 中有对 `p4-baseline\VCRUNTIME140.dll` 的 Load Image 记录。这说明 v1.0.5 会从应用目录加载静态导入 |
| P4-2 | 被测组静态导入（预期见本行末列）：复制 `candidate-batch-a` 为 `p4-candidate`。从 Update.exe 的静态导入（bcrypt.dll）和 Uninstall.exe 的静态导入（WINMM.dll、RstrtMgr.dll）中，选出不在 P4-0 KnownDLLs 列表里的名称，把探针复制为这些名称放进 `p4-candidate`。在采集下运行 `p4-candidate\Update.exe`（无参数）和 `p4-candidate\Uninstall.exe`（打开后取消） | 两者正常运行（这本身就是证据：如果加载了探针，探针没有这些 DLL 的导出函数，进程会在启动时失败），Process Monitor 显示这些 DLL 都从 `C:\Windows\System32` 加载。对静态导入来说 probe-log.txt 不作判断依据：即使探针被加载，绑定导入失败也发生在 DllMain 之前，不会写日志 |
| P4-3 | 被测组运行时加载：如果 P3 的对照或主程序 CHECKED 中出现过被测进程在应用目录查找的 DLL 名称，就把探针复制为这个名称放进 `p4-candidate`，重复对应场景（例如卸载）。对照：同样的名称放进 `p4-baseline`（需在装有运行库的环境中，例如在沙盒里安装 VC++ 运行库之后）重复一次 | 被测组 probe-log.txt 没有新增行；对照组若有新增行，说明 v1.0.5 会在运行时从应用目录加载该名称。这一项中探针日志是判断依据。如果 P3 没有出现任何此类名称，记录“未观察到运行时按名称查找”，本项不适用 |
| P4-4 | 结束后删除 `p4-baseline`、`p4-candidate` 和 `%PUBLIC%\AsterunDllProbe` | 无残留。沙盒关闭后也会自动丢弃 |

## 第 5 阶段：体积（自动记录）

P1-2 的日志已包含两个版本 3 个 EXE 的大小。静态运行库会让每个 EXE 增大几百 KB，这一项只作记录，由你判断是否可以接受。

## ARM64（可选）

如果有 ARM64 Windows 设备，使用 Artifacts 中的 `Asterun-ARM64` 和 `probe-dll\out\arm64\asterun_dll_probe.dll`，重复第 1–4 阶段。Process Monitor 用 `Procmon64a.exe`。没有设备时，结果记为“ARM64：已编译并通过 PE 检查，未运行”。

## 结果记录表

| 编号 | 系统（Win10 / Win11，版本号） | 结果（通过 / 不通过 / 不适用） | 证据文件 | 备注 |
|---|---|---|---|---|
| P1-1 | | | P1-1-environment.txt | |
| P1-2 | | | P1-2-packages.txt | |
| P1-3 | | | 截图或输出 | |
| P2-1 | | | P2-1-baseline.png | |
| P2-2 | | | 输出 | |
| P2-3 | | | P2-3-candidate.png | |
| P2-4 | | | 输出 | |
| P2-5 | | | | |
| P2-6 | | | | |
| P2-7 | | | 截图 | |
| P2-8 | | | | |
| P2-9 | | | 截图 | |
| P2-10 | | | | |
| P2-11 | | | | 可选 |
| P2-12 | | | | |
| P2-13 | | | | |
| P3-1 | | | CSV + summary | |
| P3-2 | | | CSV + summary | |
| P3-3 | | | CSV + summary | |
| P3-4 | | | CSV + summary | |
| P3-5 | | | CSV + summary | |
| P3-6 | | | CSV + summary | |
| P4-0 | | | P4-0-knowndlls.txt | |
| P4-1 | | | PML/CSV | |
| P4-2 | | | probe-log + CSV | |
| P4-3 | | | | |
| P4-4 | | | | |
