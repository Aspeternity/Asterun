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
| E-2 | `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Initialize-Workspace.ps1 -BaselineZip C:\Acceptance\packages\Asterun-x64-v1.0.5.zip -CandidateZip C:\Acceptance\packages\Asterun-x64-candidate.zip -Tools C:\Acceptance\tools -Kit C:\Acceptance\kit -Logs C:\Acceptance\logs` | 最后两行为 `RESULT: PASS` 和 `WORKSPACE=C:\AsterunWork\run-...`。`candidate input=... sha256=` 与 README 中 artifact 摘要相同。记下这个路径，下文写作 `$run`（在 PowerShell 中执行 `$run = 'C:\AsterunWork\run-...'`）。对照包哈希与正式发布一致、Procmon 签名为 Valid、两个包各有 3 个 EXE |
| E-3 | `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\tests\Test-Analyzer.ps1 -Out C:\Acceptance\logs\E-3-analyzer-selftest` | `RESULT: all 31 cases as expected` |

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
| A-1 | `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Run-Acceptance.ps1 -Work $run -Logs C:\Acceptance\logs` | 末行 `RESULT: all steps as expected`（F-1 已修复）。每一行的预期见下表；请如实记录你环境中的结果 |

| 步骤 | 内容 | 预期 |
|---|---|---|
| probe-self-test | 探针在当前系统上能被加载并写日志 | PASS |
| control-main | v1.0.5 Asterun.exe，目录中植入 version.dll | CONTROL-VALID |
| control-update-noargs | v1.0.5 Update.exe，目录中植入 bcrypt.dll | CONTROL-VALID |
| control-uninstall-launcher | v1.0.5 Uninstall.exe，目录中植入 rstrtmgr.dll | CONTROL-VALID |
| control-uninstall-launcher-ncrypt | v1.0.5 Uninstall.exe 启动器，植入 ncrypt.dll（F-1 复现） | CONTROL-VALID |
| control-uninstall-worker-ncrypt | v1.0.5 卸载执行进程，植入 ncrypt.dll（F-1 复现） | CONTROL-VALID |
| candidate-main | 批次 A Asterun.exe 主程序 | PASS |
| candidate-update-noargs | 批次 A Update.exe 无参数 | PASS |
| candidate-update-chain | 批次 A Update.exe 完整执行链（见第 4 阶段） | PASS |
| candidate-service-command | 批次 A `Asterun.exe --set-managed-everything-service disabled` | PASS |
| candidate-uninstall-launcher | 批次 A Uninstall.exe 启动器（rstrtmgr 延迟加载后不再加载它） | UNVERIFIED（干净启动器不触发 RM，没有可核对的 System32 加载；见 README） |
| candidate-uninstall-worker | 批次 A Uninstall.exe `--perform`，从 `%TEMP%\Asterun-Elevated.*` 运行（无锁，正常完成） | UNVERIFIED（同上） |
| candidate-uninstall-rm-worker-ncrypt | **决定性 F-1 回归**：锁住 VERSION 迫使执行进程调用 Restart Manager，植入 ncrypt.dll | PASS：rstrtmgr.dll 从 System32 加载（路径确实执行）、植入的 ncrypt.dll 未从执行目录加载 |
| candidate-uninstall-launcher-ncrypt / candidate-uninstall-worker-ncrypt | 修复后启动器/干净执行进程植入 ncrypt.dll | UNVERIFIED（这些路径不加载它；若被加载则 FAIL） |
| investigate-uninstall-worker-ntasn1 | 调查项：强制 RM 下植入 ntasn1.dll | 记录（可能根本不加载） |
| investigate-main-ncrypt | 调查项：Asterun.exe 主程序植入 ncrypt.dll | 记录：FAIL——主程序正常模式在运行时加载了植入的 ncrypt.dll（单独登记，未修复；见 README） |

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

## 第 4b 阶段：F-1 真实提权链核对（CI 无法覆盖，必须实机）

**只在一次性虚拟机里、对一套独立的测试安装进行；不要在日常电脑上做，不修改任何产品代码。** 开启 UAC，用普通管理员账户（拆分令牌）。

### 产品代码的真实行为（已审查源码，作为本阶段的依据）

- 执行进程的临时副本目录由**未提权**的启动器创建：`CreateSecuredTemporaryExecutableCopy` 取 `%TEMP%`（即启动它的普通用户的 TEMP），用 `CreateDirectoryW(dir, nullptr)`（**不设显式安全描述符**）创建 `%TEMP%\Asterun-Elevated.<随机令牌>`，把 Uninstall.exe 复制进去，**然后**才用 `ShellExecuteExW` 的 `runas` 动词触发 UAC、以提权身份运行执行进程。
- 因此该目录由普通用户创建于其自己的 `%TEMP%` 下，继承默认 DACL（归属该用户、该用户可写）。这正是 F-1 的攻击面：提权进程从一个普通用户可写的目录运行。
- 卸载结束后，`CleanupSelfLater` 用 `MoveFileExW(..., MOVEFILE_DELAY_UNTIL_REBOOT)` 安排该副本与目录在**下次重启时**删除。所以卸载完成后、重启之前，该目录**仍然存在**——可以事后从容取证，**无需预测随机令牌、无需抢时间**。

### 取证前准备

- 只在一次性虚拟机里，对一套独立的测试安装操作。日志统一放到 `C:\Acceptance\logs\u4\`（先 `New-Item -ItemType Directory C:\Acceptance\logs\u4`）。
- 把本套件被测包安装到 `C:\Program Files\Asterun`（真实走提权卸载的位置），启动一次确认可用（U-4a）。
- 卸载一律直接运行 `C:\Program Files\Asterun\Uninstall.exe`，不从 Asterun 界面发起，避免多出一个 Asterun 进程。
- Process Monitor 用工作区中的 `$run\tools\Procmon64.exe`（下文写作 `$pm`）。

### 三项验证分开记录

| 类别 | 编号 | 证明什么 | 不证明什么 |
|---|---|---|---|
| 正常卸载功能（真实 UAC） | U-4b | 真实拆分令牌提权下卸载完成：程序文件删除、`data\` 保留 | 不一定触发 Restart Manager，所以不证明 ncrypt.dll 的来源 |
| 真实执行进程的 DLL 观察 | U-4c / U-4e | 由采集确认的执行进程实际从哪里加载了 rstrtmgr.dll、ncrypt.dll（逐项完整路径） | 无植入、无对照：只报告观察结果（OBSERVED），不是植入回归的 PASS |
| 执行目录的所有者与可写性 | U-4d | 本次执行进程所在的**确切目录**的 Owner/Sddl/Access/icacls，以及未提权账户能否写入 | 不在该目录做植入（见“明确未覆盖”） |

| 编号 | 操作 | 预期/记录 |
|---|---|---|
| U-4a | 用普通管理员账户（UAC 开启）安装到 `C:\Program Files\Asterun`，启动一次后从托盘退出 Asterun。 | 安装成功、可运行 |
| U-4b（正常卸载，真实提权，先采集） | 在**提权**的 PowerShell 中：`& $pm /AcceptEula /Quiet /Minimized /BackingFile C:\Acceptance\logs\u4\u4b.pml`，再 `& $pm /AcceptEula /WaitForIdle`。确认采集已开始后，在**未提权**的资源管理器中双击 `C:\Program Files\Asterun\Uninstall.exe`，选择保留数据，UAC 弹出时同意，等待完成提示。然后 `& $pm /AcceptEula /Terminate`，`Start-Process $pm -ArgumentList '/AcceptEula','/Quiet','/OpenLog','C:\Acceptance\logs\u4\u4b.pml','/SaveAs','C:\Acceptance\logs\u4\u4b.csv' -Wait`。 | 卸载完成提示；`C:\Program Files\Asterun` 中程序文件已删除、`data\` 保留。记为“正常卸载功能”的结果 |
| U-4c（观察执行进程的 DLL 来源） | `powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Analyze-Procmon.ps1 -Csv C:\Acceptance\logs\u4\u4b.csv -Scenario uninstall-worker -Role observation -AppDir "C:\Program Files\Asterun" -TempDir $env:TEMP -ReportDll rstrtmgr.dll,ncrypt.dll -Out C:\Acceptance\logs\u4\u4c-report.txt` | 报告按“映像在 `Asterun-Elevated.*` 下 + 命令行含 `--perform`”确认执行进程 PID，并给出 `TARGET-IMAGE-DIR` 与每个名称的 `DLL-LOAD ... class=... path=...`。逐项记录：`SYSTEM32`（来自 `%WINDIR%\System32`）、`WINDOWS-OTHER`（%WINDIR% 下但不是 System32）、`WATCHED`（来自执行目录或安装目录，结果 FAIL）、`OTHER`、`NOT-OBSERVED`（本次没有加载——正常卸载未触发 Restart Manager 时属正常，**不是**通过也不是失败）、`CAPTURE-INVALID`（采集里没有执行进程，本次无效，需重做）。结果行为 `OBSERVED`（退出码 12），不要求 PASS |
| U-4d（确切执行目录的所有者与可写性） | 卸载完成后、**重启之前**：① 任意 PowerShell：`powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Get-WorkerDirEvidence.ps1 -Report C:\Acceptance\logs\u4\u4c-report.txt -Out C:\Acceptance\logs\u4\u4d-acl.txt` ② **未提权**的 Windows PowerShell：同一命令加 `-WriteTest`，输出到 `C:\Acceptance\logs\u4\u4d-write.txt` | 脚本只使用报告中 `TARGET-IMAGE-DIR` 给出的确切目录，记录 Owner、Group、Sddl、Access 逐条、icacls，并注明“所有者不等于创建者”。写入测试记录账户、是否提权、完整性级别，写后删除测试文件；只有 ② 输出 `write test validity: VALID ... (not elevated ...)` 才算“普通用户可写”的结论，提权进程运行会标为 NOT VALID（退出码 13）。若输出 `DIRECTORY-GONE`（退出码 32，例如已重启），如实记录“无法取证”，**不要改测其他目录** |
| U-4e（强制 Restart Manager，诊断用，与 U-4b 分开记录） | 先重新安装一次（U-4a）。① 在一个**未提权**的 PowerShell 窗口锁住 VERSION 并保持窗口打开：`powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Hold-FileLock.ps1 -Path "C:\Program Files\Asterun\VERSION"`（与 CI 回归相同：`FileShare.None` 独占句柄；启动器和执行进程只用 GetFileAttributes 检查 VERSION，不受影响，只有删除 VERSION 会失败）。② 按 U-4b 开始采集（文件名用 `u4e.pml`/`u4e.csv`）并运行卸载。③ 执行进程报告卸载错误后，点确定，停止并导出采集。④ **释放锁**：回到锁窗口按 Enter（关闭该窗口同样释放）。⑤ 分析：同 U-4c，但加 `-RequireSystemLoad rstrtmgr.dll`，输出 `u4e-report.txt`；可再按 U-4d 对该报告取证。⑥ 清理：锁释放后再运行一次 `C:\Program Files\Asterun\Uninstall.exe` 完成卸载（或直接丢弃虚拟机）。 | 这次卸载**预期不完整**（VERSION 被故意锁住），不计入正常卸载功能。报告中 `required load of rstrtmgr.dll : SYSTEM32 (...)` 证明 Restart Manager 路径实际执行，且 `DLL-LOAD ... ncrypt.dll class=SYSTEM32 path=C:\Windows\System32\ncrypt.dll`。若为 `NOT-OBSERVED`，结果为 INVALID（路径没执行，需检查锁是否生效后重做）；若 rstrtmgr/ncrypt 来自 System32 以外，结果为 REVIEW；来自执行目录则 FAIL |

### 明确未覆盖（如实记为未验证）

- **在真实 `Asterun-Elevated.<随机>` 目录中植入 DLL、再看是否被忽略**：目录名含随机令牌，由未提权启动器在提权前创建；要在执行进程加载前放入副本必须与令牌赛跑，本流程**不做**。“植入副本被忽略”这一属性只由 CI 中的强制 RM 产品回归在测试自建目录中证明（DLL 加载属性）；真实提权链下的植入记为**未验证**。
- **安装目录植入不能代替临时执行目录植入**：执行进程不从安装目录运行，修复后启动器初始化也不加载 rstrtmgr/ncrypt，安装目录植入观察不到执行进程的行为。
- U-4c 若为 `NOT-OBSERVED`，只说明本次正常卸载没有执行 Restart Manager 路径，不能写成该路径“已验证通过”；该路径的真实提权观察以 U-4e 为准。

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
| U-4a | | | 截图 | 独立测试安装 |
| U-4b | | | u4b.pml / u4b.csv + 截图 | 正常卸载功能（真实提权）：程序文件删除、data 保留 |
| U-4c | | | u4c-report.txt | 观察：执行进程 rstrtmgr/ncrypt 的类别与完整路径（OBSERVED / NOT-OBSERVED / CAPTURE-INVALID） |
| U-4d | | | u4d-acl.txt + u4d-write.txt | 确切执行目录的 Owner/Sddl/Access/icacls；未提权写入（账户、完整性级别）；或 DIRECTORY-GONE |
| U-4e | | | u4e-report.txt + 锁窗口输出 | 强制 RM 诊断：rstrtmgr/ncrypt 来源；卸载预期不完整，单独记录 |
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
