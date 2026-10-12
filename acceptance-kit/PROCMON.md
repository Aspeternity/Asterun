# Process Monitor 取证：方法与判定规则

正常情况下不需要手动操作 Process Monitor：`Run-Acceptance.ps1` 逐个调用 `Invoke-Scenario.ps1`，由它完成采集、导出和分析。本文说明脚本具体做了什么、怎样判定，以及可选的手动采集方法。

## 1. 每个场景的自动步骤（Invoke-Scenario.ps1）

1. 把被测包或对照包复制到工作区内一个新的场景目录 `C:\AsterunWork\<run>\scenarios\<role>-<场景>-<时间>`。卸载执行进程（worker）场景另外在 `%TEMP%\Asterun-Elevated.kit-<时间>` 中放一份 Uninstall.exe，与真实卸载的临时副本位置一致。
2. 植入探针：把探针 DLL 复制成目标进程的某个**静态导入**的名称，放在目标 EXE 所在的目录中，并记录完整路径。选用的名称都是 v1.0.5 与批次 A 都会导入、且不在常见 KnownDLLs 列表中的系统 DLL。它在当前系统上能否用于植入，由同名对照（v1.0.5）在每次运行中实测：

   | 目标 | 植入名称 | 位置 |
   |---|---|---|
   | Asterun.exe（主程序、服务子命令、更新后重启的新程序） | version.dll | Asterun.exe 所在目录 |
   | Update.exe（无参数、`--apply` 执行进程） | bcrypt.dll | Update.exe 所在目录（执行链中为 `Asterun-Elevated.updater-it-*`） |
   | Uninstall.exe（启动器、`--perform` 执行进程） | rstrtmgr.dll | Uninstall.exe 所在目录 |

   调查场景（`investigate-*`，结果只记录，不计入通过与否）在上表之外，再把探针复制成 `ncrypt.dll`、`ntasn1.dll` 放在同一目录。这两个名称是 CI 中观察到加固进程在 EXE 目录查找过的名称，用来检验这种查找能否变成加载（见 README 发现 F-1）。

3. 启动 Process Monitor 采集：`Procmon64.exe /AcceptEula /Quiet /Minimized /BackingFile <pml>`，再用 `/WaitForIdle` 等它就绪。采集全部进程，不设过滤器，分析时再按进程筛选，这样不会因为过滤器写错而漏记。
4. 运行场景，并做功能检查（结果写入报告的 `functional=` 行）：

   | 场景 | 运行方式 | 功能检查 |
   |---|---|---|
   | main | 启动 Asterun.exe | 等待 10 秒后进程仍在运行 |
   | update-noargs | 无参数运行 Update.exe | 退出码 2（参数无效，不做任何修改） |
   | update-chain | 运行 `tests\UpdaterIntegrationTests.ps1 -Mode Portable -PlantDll <探针>` | 测试退出码 0（见 CHECKLIST 第 4 阶段） |
   | service-command | `Asterun.exe --set-managed-everything-service disabled` | 退出码 0 |
   | uninstall-launcher | 启动 Uninstall.exe | 等待 10 秒后仍在运行（停在“是否保留数据”对话框） |
   | uninstall-worker | `Uninstall.exe --perform --parent-pid <已退出的进程> --install <场景目录> --delete-data 0` | 程序文件被删除，`data\` 中的标记文件保留 |

   对照（v1.0.5）不做功能判断：它加载了探针后通常会因为探针没有所需导出而启动失败，这是预期现象。
5. 结束场景中仍在运行的进程，`Procmon64.exe /Terminate` 停止采集，再用 `/OpenLog <pml> /SaveAs <csv>` 导出 CSV（默认列）。
6. 调用 `Analyze-Procmon.ps1` 生成报告 `<role>-<场景>-<时间>-report.txt`，并把运行记录附在报告末尾。PML、CSV 和报告都保存在 `C:\Acceptance\logs\<run>\`。

所有子进程继承 `SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX)`，所以对照组加载失败时不会弹出系统错误框而卡住。

## 2. 目标进程的识别（Analyze-Procmon.ps1）

只按“进程名”判断是不够的，因为普通 Asterun 主进程也可能访问各种 EXE 和 DLL。分析脚本对每个 PID 取两项信息：

- **命令行**：来自该 PID 的 `Process Start` 事件的 Detail（`Command line: ...`）；
- **映像路径**：来自该 PID 加载自身 EXE 的 `Load Image` 事件。

然后按场景同时检查 EXE 名称、映像位置和命令行：

| 场景 | 目标 |
|---|---|
| main | Asterun.exe，映像在被测目录下，命令行不含服务参数和 `--post-update-health-event` |
| update-noargs | Update.exe，映像在被测目录下，命令行不含 `--apply` |
| update-chain | ① Update.exe，映像在 `Asterun-Elevated.*` 目录，命令行含 `--apply`；② Asterun.exe，命令行含 `--post-update-health-event`。两者都必须出现 |
| service-command | Asterun.exe，映像在被测目录下，命令行含 `--set-managed-everything-service` 或 `--repair-managed-everything-service` |
| uninstall-launcher | Uninstall.exe，映像在被测目录下，命令行不含 `--perform` |
| uninstall-worker | Uninstall.exe，映像为 `%TEMP%\Asterun-Elevated.*\Uninstall.exe`，命令行含 `--perform` |

**采集无效（INVALID）**：任一必需目标没有出现在采集中，或者目标缺少 `Process Start` 或自身 `Load Image` 事件（例如采集开始得太晚），或者 CSV 缺少 Process Name、PID、Operation、Path、Result、Detail 中的任一列。其他同名进程不能替代目标：例如服务子命令场景中只有普通 Asterun 主进程时，结果是 INVALID，而不是 PASS。

## 3. DLL 事件的分类

只看目标 PID 的、路径以 `.dll` 结尾的事件：

| 类别 | 条件 | 含义 |
|---|---|---|
| LOADED | `Load Image` | DLL 已被映射进进程 |
| OPEN-EXECUTE | `CreateFile` 成功且 Desired Access 含 Execute | 为映射而打开（加载过程中） |
| SEARCH | 在目标 EXE **自身所在目录**中结果为 `NAME NOT FOUND` 或 `PATH NOT FOUND` | 加载器按默认顺序首先在这里查找，文件不存在，没有加载 |
| FILE-OP | 其他访问，包括在其他监视目录中的 `NAME NOT FOUND`（读取、查询属性、存在性检查、复制、删除等） | 程序主动的文件操作，例如更新器检查安装目录中的目标文件、复制，卸载器删除文件；只记录，本身不算失败 |

位置分为：WATCHED（被测目录、%TEMP%、目标 EXE 所在目录，含长路径和 8.3 短路径两种写法）、SYSTEM（%WINDIR%）、OTHER。

## 4. 判定规则

**对照（v1.0.5，`-Role control`）**：目标从自己目录加载了植入的探针 → `CONTROL-VALID name=<dll>`（证明在这个环境里，这种植入方式和这次采集能看到旧版本的问题）；否则 `CONTROL-INVALID`。

**被测（批次 A，`-Role candidate`）**，对每个目标：

1. 加固进程（Update.exe、Uninstall.exe、Asterun.exe 服务子命令）：任何 LOADED 或 OPEN-EXECUTE 位于 WATCHED → FAIL；只有 SEARCH 位于 WATCHED → REVIEW（列出明细，人工判断，不直接判失败）。
2. 主程序正常模式（Asterun.exe 主程序和更新后重启的新程序）：按设计不调用 SetDefaultDllDirectories，所以只有 C/C++ 运行库 DLL（vcruntime*、msvcp*、api-ms-win-crt-*、ucrtbase、concrt*）从 WATCHED 被 LOADED 或 OPEN-EXECUTE 才判 FAIL；其他活动作为 INFO 列出。
3. 植入：目标从 WATCHED 加载了植入的探针 → FAIL。
4. **只有同时满足下面三点才是 PASS**：
   - 与目标 EXE 同目录的植入名称，被目标从 System32 加载（Load Image 路径为证）；
   - 同一名称在本次运行的对照报告中为 CONTROL-VALID（有效对照）；
   - 功能检查通过。

   缺少其中任何一点而又没有触发 FAIL 或 REVIEW 时，判为 UNVERIFIED。没有植入、没有对照，或者功能检查没有运行的场景，都不会因为“采集里看起来很干净”而判通过。
5. 同一目标有多个 PID 时（例如执行链中成功用例和两个负面用例各有一个 Update.exe），每个 PID 都按第 1–3 条检查；第 4 条只要求其中至少一个 PID 得到验证。
6. 探针日志（`%PUBLIC%\AsterunDllProbe\probe-log.txt`）只作辅助证据：如果日志显示目标 PID 加载了某个探针，而采集中没有对应的 Load Image，说明采集漏记，判为 INVALID。对静态导入来说，没有日志不能证明没有加载：探针没有导出函数，旧版本绑定导入失败发生在 DllMain 之前，这时不会写日志。

总体结果取最严重的一项：INVALID > FAIL > REVIEW > UNVERIFIED > PASS。退出码：PASS 0、REVIEW 10、UNVERIFIED 11、FAIL 20、INVALID 30；对照 CONTROL-VALID 0、CONTROL-INVALID 31。

说明：加载器在 EXE 目录查找时使用的访问权限不含 Execute（CI 实测：Uninstall.exe 查找 ncrypt.dll 的记录不带 Execute，而在植入后同一名称被实际加载），所以不能用访问权限区分“查找”和“检查”；本套件按位置区分，并用植入调查验证查找能否变成加载。

规则本身由 `tests\Test-Analyzer.ps1` 用合成的采集数据自测（21 个用例，覆盖上面每一种结果）。报告中列出的每条事件都附带 Desired Access，便于核对分类。

## 5. 这套方法证明什么、不证明什么

- 证明：在当前系统上，批次 A 的三个 EXE 的静态导入不会从 EXE 所在目录加载同名 DLL（A2，DependentLoadFlags=0x800），而 v1.0.5 会；加固进程在场景运行期间没有从被测目录或 %TEMP% 加载或映射任何 DLL（A3 的效果）。
- 不证明：运行时 `LoadLibrary` 按名称加载的每一条路径都被覆盖。A3 的运行时行为由 CI 中的 `dll_search_hardening_tests`（进程内实际调用 LoadLibrary 验证）覆盖；这里只记录场景中实际发生的查找。
- 不证明：系统 DLL 自身依赖链也被保护。DependentLoadFlags 只作用于 EXE 的直接导入；非 KnownDLL 的系统 DLL 自己的静态依赖仍会先在 EXE 目录查找。CI 已实测到一例（Uninstall.exe → RstrtMgr.dll → ncrypt.dll，植入的 ncrypt.dll 被加载），见 README 发现 F-1。套件的 investigate-* 场景专门检查这一点。
- 主程序正常模式按设计不限制运行时 DLL 搜索路径（它需要加载第三方 Shell 扩展），这里只检查运行库和静态导入。

## 6. 可选：手动采集一个真实的 UAC 流程

`Run-Acceptance.ps1` 直接以提权方式运行服务子命令和卸载执行进程。如果还想取证“由 UAC 拉起”的真实进程（CHECKLIST 第 3 阶段 M-9、M-12），可以这样做（提权的 PowerShell，`$run` 为本次工作区）：

```powershell
$run  = 'C:\AsterunWork\run-YYYYMMDD-HHMMSS'
$logs = "C:\Acceptance\logs\$(Split-Path -Leaf $run)"
$pm   = (Get-ChildItem "$run\tools" -Filter 'Procmon64*.exe')[0].FullName
$app  = 'C:\AsterunWork\manual-uac'                 # 新建目录，复制 $run\candidate\* 进去
Copy-Item "$run\probe\asterun_dll_probe.dll" "$app\version.dll"
& $pm /AcceptEula /Quiet /Minimized /BackingFile "$logs\manual-service.pml"
& $pm /WaitForIdle
# 现在在 $app 中启动 Asterun.exe，启用或关闭“Everything 文件与文件夹”，同意 UAC，操作完成后：
& $pm /Terminate
Start-Process $pm -ArgumentList '/AcceptEula','/Quiet','/OpenLog',"$logs\manual-service.pml",'/SaveAs',"$logs\manual-service.csv" -Wait
$controls = (Get-ChildItem $logs -Filter 'control-*-report.txt').FullName -join ';'
powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Analyze-Procmon.ps1 -Csv "$logs\manual-service.csv" `
  -Scenario service-command -AppDir $app -PlantedPath "$app\version.dll" -ControlReport $controls `
  -FunctionalResult PASS -FunctionalDetail '界面显示启用/关闭成功' -Out "$logs\manual-service-report.txt"
```

`-FunctionalResult` 按你观察到的结果填写 PASS 或 FAIL。卸载执行进程同理：`-Scenario uninstall-worker`，植入 `rstrtmgr.dll`。真实卸载的临时副本目录是随机生成的，无法预先植入，所以该次手动采集的结果会是 UNVERIFIED（只能证明没有从 WATCHED 加载），这是预期的；植入后的验证由自动场景 uninstall-worker 完成。
