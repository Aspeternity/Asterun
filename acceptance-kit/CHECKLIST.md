# 批次 A 验收清单（实机部分）

本清单只包含 CI 无法替你完成的部分。CI 已在 Windows runner 上实际运行过同一套脚本，结果见 README 的“CI 已完成”一节。

Windows 10 与 Windows 11 各执行一遍；只有一种系统时，在结果表中如实写明。

## 约定

- 宿主机：`C:\AsterunAcceptance\packages`、`kit`、`tools`、`logs` 四个文件夹。
- 沙盒内：`C:\Acceptance\packages`、`C:\Acceptance\kit`、`C:\Acceptance\tools` 只读；`C:\Acceptance\logs` 可写，是唯一写回宿主机的位置。
- 所有运行都在沙盒内部的新工作目录 `C:\AsterunWork\run-<时间>` 中进行。脚本不会删除已有目录：目录已存在时直接停止。关闭沙盒后工作目录随沙盒一起丢弃。
- 使用虚拟机时，把四个文件夹复制到虚拟机内的同名路径（`C:\Acceptance\...`）即可；脚本不会写入 packages、kit、tools。
- 下文的 PowerShell 都指 Windows 自带的 Windows PowerShell 5.1，需以管理员身份运行（U-3 除外）。
- 探针 DLL 未签名，杀毒软件可能隔离它。只在一次性的沙盒或虚拟机中使用。

关于 UAC：Windows Sandbox 的账户通常已经是提权状态，UAC 不会弹出。标注“UAC”的步骤，请在默认开启 UAC 的虚拟机中，用普通的管理员账户（拆分令牌）再做一遍，并在结果表中注明环境。

## 第 0 阶段：宿主机准备（只下载文件，不改动系统）

1. 新建 `C:\AsterunAcceptance\packages`、`kit`、`tools`、`logs`。
2. 对照包：下载 https://github.com/Aspeternity/Asterun/releases/download/v1.0.5/Asterun-x64.zip ，保存为 `packages\Asterun-x64-v1.0.5.zip`。
3. 被测包：打开 README“被测版本”一节给出的 CI 运行页面，在页面底部 Artifacts 中下载 `Asterun-x64`，把下载得到的 zip 原样保存为 `packages\Asterun-x64-candidate.zip`（脚本会自动解开外层）。
4. 下载 https://download.sysinternals.com/files/ProcessMonitor.zip ，原样放到 `tools\`（不用解压）。
5. 把验收套件解压到 `kit\`，使 `kit\scripts`、`kit\probe-dll`、`kit\tests` 直接位于 `kit` 下。
6. 双击 `kit\asterun-acceptance.wsb` 启动沙盒。

## 第 1 阶段：环境与工作区

| 编号 | 操作 | 预期 |
|---|---|---|
| E-1 | `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Check-Environment.ps1 -Log C:\Acceptance\logs\E-1-environment.txt` | 输出 `vc-runtime=ABSENT ... CLEAN`。结果为 NOT clean 时，第 2 阶段不能在此环境中判定 |
| E-2 | `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Initialize-Workspace.ps1 -BaselineZip C:\Acceptance\packages\Asterun-x64-v1.0.5.zip -CandidateZip C:\Acceptance\packages\Asterun-x64-candidate.zip -Tools C:\Acceptance\tools -Kit C:\Acceptance\kit -Logs C:\Acceptance\logs -CandidateSha256 <README 中的被测包 SHA-256>` | 最后两行为 `RESULT: PASS` 和 `WORKSPACE=C:\AsterunWork\run-...`。记下这个路径，下文写作 `$run`（在 PowerShell 中执行 `$run = 'C:\AsterunWork\run-...'`）。对照包哈希与正式发布一致、Procmon 签名为 Valid、两个包各有 3 个 EXE |
| E-3 | `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\tests\Test-Analyzer.ps1 -Out C:\Acceptance\logs\E-3-analyzer-selftest` | `RESULT: all 20 cases as expected` |

## 第 2 阶段：无运行库环境下的启动（需要 E-1 为 CLEAN）

| 编号 | 操作 | 预期 |
|---|---|---|
| R-1 | 双击 `$run\baseline\Asterun.exe` | 系统提示找不到 VCRUNTIME140.dll 或 MSVCP140.dll，程序不启动。截图。这证明环境中确实没有运行库 |
| R-2 | `(Start-Process $run\baseline\Update.exe -Wait -PassThru).ExitCode` | 同样提示缺少运行库，退出码 -1073741515（0xC0000135） |
| R-3 | 双击 `$run\candidate\Asterun.exe` | 正常启动，出现托盘图标或主窗口，没有缺少 DLL 的提示。截图 |
| R-4 | `(Start-Process $run\candidate\Update.exe -Wait -PassThru).ExitCode` | 不弹窗，退出码 2（参数无效，未做任何修改） |
| R-5 | `Start-Process $run\candidate\Uninstall.exe`，看到对话框后点取消 | 正常显示卸载对话框，没有缺少 DLL 的提示 |

完成后退出 R-3 启动的 Asterun。

## 第 3 阶段：自动取证（一条命令）

| 编号 | 操作 | 预期 |
|---|---|---|
| A-1 | `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Run-Acceptance.ps1 -Work $run -Logs C:\Acceptance\logs` | 每一行的预期见下表。当前候选版本在 CI 中有两行为 REVIEW（发现 F-1，见 README），末行因此为 `RESULT: 2 deviation(s)`；请如实记录你环境中的结果 |

| 步骤 | 内容 | 预期 |
|---|---|---|
| probe-self-test | 探针在当前系统上能被加载并写日志 | PASS |
| control-main | v1.0.5 Asterun.exe，目录中植入 version.dll | CONTROL-VALID |
| control-update-noargs | v1.0.5 Update.exe，目录中植入 bcrypt.dll | CONTROL-VALID |
| control-uninstall-launcher | v1.0.5 Uninstall.exe，目录中植入 rstrtmgr.dll | CONTROL-VALID |
| candidate-main | 批次 A Asterun.exe 主程序 | PASS |
| candidate-update-noargs | 批次 A Update.exe 无参数 | PASS |
| candidate-update-chain | 批次 A Update.exe 完整执行链（见第 4 阶段） | PASS |
| candidate-service-command | 批次 A `Asterun.exe --set-managed-everything-service disabled` | PASS |
| candidate-uninstall-launcher | 批次 A Uninstall.exe 启动器 | 验收标准为 PASS；当前候选版本为 REVIEW（加载器在 EXE 目录查找 ncrypt.dll、NTASN1.dll，见 F-1） |
| candidate-uninstall-worker | 批次 A Uninstall.exe `--perform`，从 `%TEMP%\Asterun-Elevated.*` 运行 | 同上 |
| investigate-uninstall-launcher-ncrypt | 调查项（不计入结果）：在 Uninstall.exe 目录另植入 ncrypt.dll、ntasn1.dll | 记录。当前候选版本为 FAIL：植入的 ncrypt.dll 被加载（F-1） |
| investigate-uninstall-worker-ncrypt | 调查项：同上，提权执行进程 | 记录。当前为 FAIL，执行进程因加载探针而异常退出（F-1） |
| investigate-main-ncrypt | 调查项：在 Asterun.exe 目录另植入 ncrypt.dll、ntasn1.dll | 记录。当前为 FAIL：主程序正常模式在运行时通过 crypt32 的延迟导入加载了植入的 ncrypt.dll（主程序按设计不限制运行时搜索，见 F-1 第 4 点） |

判定规则见 PROCMON.md 第 4 节。简要地说：被测项只有在“植入的同名 DLL 被目标进程从 System32 加载（看 Load Image 路径）+ 同名对照为 CONTROL-VALID + 功能检查通过”时才是 PASS；没有有效对照则为 UNVERIFIED，不会因为没有探针日志就判通过。

在没有运行库的环境中，v1.0.5 可能在加载到植入的 DLL 之前就因为缺少 VCRUNTIME140.dll 而终止，这时对照为 CONTROL-INVALID，相应的被测项为 UNVERIFIED。出现这种情况时：在沙盒中安装 VC++ 运行库（https://aka.ms/vs/17/release/vc_redist.x64.exe ，只装在沙盒里，关闭后丢弃），再从 E-2 开始用一个新的工作区重新执行 A-1，两次的结果都保留。

## 第 4 阶段：更新器执行链（不需要发布新版本）

上一轮清单中“验证完整更新必须修改 VERSION 并发布”的说法不准确，撤回。Update.exe 的 `ValidateSource` 只要求暂存目录中的 VERSION 与 `--version` 参数一致，并不要求版本号更高；版本比较发生在客户端的在线更新发现阶段。所以可以直接用本次构建的 Update.exe，在隔离目录里验证执行链，不改产品版本规则，不触碰日常安装。

`tests\UpdaterIntegrationTests.ps1` 做的事情：在 `%TEMP%\asterun-updater-it-<随机>`（带标记文件，只删除自己创建的目录）中准备暂存源、旧安装目录和独立备份目录，用 .NET ZipFile 打包并计算 SHA-256，把 Update.exe 复制到 `Asterun-Elevated.updater-it-*` 目录后以 `--apply` 运行，然后检查：

- 成功用例：退出码 0；安装目录中的文件被替换、新文件出现、Asterun.exe 与包一致；备份目录、暂存源和压缩包被清理；没有残留的 `.asterun-update-work.*`；新的 Asterun.exe 以 `--post-update-health-event` 启动并发出健康信号（Update.exe 等到信号才返回 0）。
- 负面用例：SHA-256 错误、VERSION 不一致时退出码 2，安装目录不变，暂存源和压缩包保留，没有启动 Asterun。
- 受保护目录（Protected 模式）：安装目录建在 `%ProgramFiles%\AsterunUpdaterIT-<随机>`，`--secure-reextract 1`，需要提权，更新器在安装目录内的受保护工作目录中重新解压并删除用户可写的压缩包。

| 编号 | 操作 | 预期 |
|---|---|---|
| U-1 | 已由 A-1 的 candidate-update-chain 完成（Portable 模式，含探针植入与取证） | PASS |
| U-2 | `powershell -ExecutionPolicy Bypass -File $run\tests\UpdaterIntegrationTests.ps1 -AppDir $run\candidate -VersionFile $run\candidate\VERSION -Mode Protected *> C:\Acceptance\logs\U-2-protected.txt` | 末行 `Updater integration test passed (Protected)` |
| U-3（UAC） | 在开启 UAC 的虚拟机中，用**未提权**的 PowerShell 运行同一命令 | 输出 `SKIPPED: the protected-install case needs an elevated test process`，退出码 77。再用提权的 PowerShell 运行一次，预期同 U-2 |

这里验证的是“更新器执行链”：从本地压缩包校验到新程序启动并发出健康信号。**不包括“客户端在线发现更新的完整流程”**（读取更新清单、比较版本、下载资产、提示用户、准备临时副本并拉起更新器）。后者需要一个比已安装版本更高的已发布版本，本批次不验证，结果表中记为“未验证”。

## 第 5 阶段：需要人工判断的兼容性（UAC 项在虚拟机中再做一次）

在 `$run\candidate` 的一个副本中进行：`Copy-Item $run\candidate C:\AsterunWork\manual -Recurse`（下文称 `manual`；目录已存在时换一个名字）。

| 编号 | 操作 | 预期 |
|---|---|---|
| M-5 | 启动 `manual\Asterun.exe`，搜索一个开始菜单程序（例如 notepad）并回车 | 程序正常启动 |
| M-6 | 搜索“计算器”或 Calculator 并启动 | 打包应用正常启动 |
| M-7 | 打开设置窗口，切换几个开关；在快捷项管理中删除一项后在确认对话框中取消 | 窗口和对话框正常，控件为 Windows 主题样式（不是老式灰色样式） |
| M-8 | 设置中勾选“添加到‘发送到’菜单”；在资源管理器中右键 `C:\Windows\System32\notepad.exe` →“发送到 > Asterun” | 快捷项被添加，可以搜索并启动 |
| M-9（UAC） | 设置中启用“Everything 文件与文件夹”，按提示获取并启动 Everything | 下载和服务安装成功，状态显示 Everything 正在运行。首次启用时实际运行的通常是 `--repair-managed-everything-service`，也可能是 `--set-managed-everything-service enabled`，两者都算覆盖。需要联网。可选：按 PROCMON.md 第 6 节同时采集 |
| M-10（UAC） | 搜索一个文件名，确认返回 Everything 结果；然后关闭“Everything 文件与文件夹” | 结果正常；关闭成功，无错误提示 |
| M-12（UAC） | 退出 Asterun，复制 `manual` 为 `manual-keep`，运行其中的 Uninstall.exe，选择保留数据 | 卸载完成，没有错误。执行进程从 `%TEMP%\Asterun-Elevated.<随机>` 以提权方式运行。注意它会移除所有副本共用的 Everything 服务 |
| M-13（UAC） | 复制 `manual` 为 `manual-delete`，启动其中的 Asterun 一次后退出，运行 Uninstall.exe，选择删除数据 | 卸载完成，目录被清除，没有错误 |
| M-14 | 查看 `C:\Acceptance\logs\<run>\initialize.txt` 中两个版本 3 个 EXE 的大小 | 只作记录：静态运行库会让每个 EXE 增大几百 KB，由你判断是否可接受 |

## ARM64（可选）

有 ARM64 Windows 设备时，用 Artifacts 中的 `Asterun-ARM64` 和 v1.0.5 的 `Asterun-ARM64.zip`（E-2 中加 `-Arch ARM64 -BaselineSha256 ''`），重复第 1–5 阶段；探针自动使用 `probe-dll\out\arm64`，Process Monitor 使用 zip 中的 `Procmon64a.exe`。没有设备时，记为“ARM64：已编译并通过 PE 检查，未运行”。

## 清理

不需要手动清理：沙盒关闭后 `C:\AsterunWork`、`%TEMP%` 和 `%PUBLIC%\AsterunDllProbe` 都会被丢弃，宿主机上只留下 `logs`。使用虚拟机时，可在检查完 `logs` 后自行删除 `C:\AsterunWork\run-*`、`C:\AsterunWork\manual*` 和 `%PUBLIC%\AsterunDllProbe`；脚本自身不会删除这些目录。

## 交回

整个 `C:\AsterunAcceptance\logs` 文件夹（含 `<run>\summary.txt`、各场景报告、PML/CSV、截图）和下表。

| 编号 | 系统（Win10 / Win11，版本号）与环境（沙盒 / 虚拟机） | 结果（通过 / 不通过 / 未验证 / 不适用） | 证据文件 | 备注 |
|---|---|---|---|---|
| E-1 | | | E-1-environment.txt | |
| E-2 | | | `<run>\initialize.txt` | |
| E-3 | | | E-3-analyzer-selftest\summary.txt | |
| R-1 | | | 截图 | |
| R-2 | | | 输出 | |
| R-3 | | | 截图 | |
| R-4 | | | 输出 | |
| R-5 | | | 截图 | |
| A-1 | | | `<run>\summary.txt` 与各报告 | |
| U-1 | | | candidate-update-chain 报告 | |
| U-2 | | | U-2-protected.txt | |
| U-3 | | | 输出 | |
| 在线发现更新 | | 未验证（本批次不验证） | | |
| M-5 | | | | |
| M-6 | | | | |
| M-7 | | | 截图 | |
| M-8 | | | | |
| M-9 | | | 截图 | |
| M-10 | | | | |
| M-12 | | | | |
| M-13 | | | | |
| M-14 | | | initialize.txt | |
| ARM64 | | | | |
