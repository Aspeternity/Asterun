# Asterun 批次 A 验收套件（第 2 版）

本套件只用于验收，不属于 Asterun 产品，不进入安装包，也不会合并进 main。它验证 PR #64（批次 A：静态运行库与 DLL 加载加固）中需要在真实 Windows 上运行的部分。

## 被测版本

| 项 | 值 |
|---|---|
| 被测提交 | `56e1990d329796e0c0a07478777552a4bc915b11`（PR #64 当前 head） |
| 基线 | main `eefa61a7131e0d770d0e5455c97ba5466dfff450`；对照包用正式版 v1.0.5 |
| PR CI 运行 | https://github.com/Aspeternity/Asterun/actions/runs/38224531489 |
| 被测包 | 上述运行页面底部 Artifacts 中的 `Asterun-x64`（artifact id 11736510046）。GitHub 记录的下载文件（外层 zip）摘要为 `sha256:89382911ed57ce50c44b417cb20ce33eb6b9e29055b5a76ac0c27d1eced4ec83`，E-2 输出的 `candidate input=... sha256=` 应与之相同。外层 zip 里面是 `Asterun-x64.zip`，E-2 会自动解开并记录它的 SHA-256。ARM64 为 `Asterun-ARM64`（id 11735800861，摘要 `sha256:38ac1d0eb97662e6ec03bdd39e3c122f7b12c5d64194db9ca29a89b837182cdb`） |
| 套件验证运行中的被测包 | 验证工作流从同一产品源码独立构建，`Asterun-x64.zip` 的 SHA-256 为 `b4ece99eeac3e39c3df1e5873102bd897a5fc95748d5beb8135be436d73a65be`。它和 PR CI 的包不是同一次构建，哈希不要求相同 |
| 套件验证运行 | 分支 `acceptance/batch-a-kit-validation`，提交 `bf6c7bae27e56afa45278c17d943164ae4be1bf2`（= PR head + `acceptance-kit/` + 验证工作流，不合并）：https://github.com/Aspeternity/Asterun/actions/runs/38229202635 。验证之后只改了文档措辞，以 `[skip ci]` 提交，脚本与该次运行完全相同 |
| 对照包 | https://github.com/Aspeternity/Asterun/releases/download/v1.0.5/Asterun-x64.zip ，SHA-256 `2bf93316a54d84fa7721016d3b0bf48b6a6ddec92f58acde9c94d7401321bd23` |
| 探针 DLL | `probe-dll/out/x64/asterun_dll_probe.dll`，SHA-256 `e01b82a8da56ea4721ccaeb84d3bc64a4ee98cd18d2c3cba129c09e63cf74e82`；ARM64 见 `probe-dll/SHA256SUMS.txt` |

## 分工

### CI 已完成（在 Windows runner 上实际运行，不是语法检查）

| 内容 | 位置 | 结果 |
|---|---|---|
| x64、ARM64 全部目标编译；PE 导入白名单、无 CRT DLL、DependentLoadFlags=0x800 | PR CI | 通过（x64、ARM64 两个 build-windows 作业，PE 契约 x64/ARM64 均通过） |
| 自动化测试：Linux 33；Windows x64 两套各 56（含 dll_search_hardening_tests、updater_portable/protected_integration_tests）；ARM64 只编译 | PR CI | 通过：Linux 33/33；Windows x64（MSVC）56/56；Windows 10 API baseline 56/56；ARM64 56 个只编译，未运行 |
| 更新器执行链（便携目录 + 受保护目录，见 CHECKLIST 第 4 阶段） | PR CI 的 CTest | 通过：两套 Windows 作业中 Portable 和 Protected 两个测试都通过 |
| 本套件全部脚本：环境检查、新建工作区（重复运行必须停止且不改动任何文件）、分析器自测、探针自测（套件内 LLVM 版和 CI 中用 MSVC 现编的版本）、Process Monitor 场景（v1.0.5 对照、被测场景、F-1 的基线复现与强制 Restart Manager 回归、调查场景）、产品回归脚本的前后对比、用真实采集做的 INVALID 负向检查 | 套件验证运行 | 已实际运行，与文档记载预期 0 处差异：F-1 基线对照 CONTROL-VALID（复现），强制 RM 回归 PASS（修复有效），产品脚本在 v1.0.5 退出 1、修复版退出 0；主程序 ncrypt 运行时加载 FAIL（单独登记）。详见下文 F-1 |

CI 的局限：runner 装有 VC++ 运行库（Check-Environment 输出 NOT clean），UAC 关闭，没有 Windows Sandbox，也没有 ARM64 运行环境。所以下面几项只能由你在实机上做。

### 需要你在实机上做（CHECKLIST.md）

| 内容 | 清单编号 |
|---|---|
| 无运行库系统上的启动 | R-1 至 R-5 |
| 在干净系统上重跑自动取证（一条命令） | A-1 |
| 受保护目录的更新执行链，及 UAC 拆分令牌下的跳过行为 | U-2、U-3 |
| 主程序、打包应用、对话框主题、SendTo、Everything 服务启用和关闭、卸载（保留数据 / 删除数据） | M-5 至 M-13 |
| ARM64 实际运行 | ARM64（可选） |

### 本批次不验证

客户端在线发现更新的完整流程（读取更新清单、比较版本、下载、提示、拉起更新器）。它需要一个版本号更高的已发布版本，不在本次授权内。更新器本身的执行链已用本地包验证，两者不能互相代替。

## F-1（已修复并验证）

**原问题**：批次 A 的 Uninstall.exe 曾静态导入 RstrtMgr.dll（非 KnownDLL），RstrtMgr.dll 又静态导入 ncrypt.dll。这条链在进程初始化阶段解析，早于 wWinMain 的 SetDefaultDllDirectories，所以植入在 EXE 目录的 ncrypt.dll 会被加载进（提权的、用户可写的临时副本）执行进程。

**修复**（提交 57dea2a，已在 PR #64 中）：

1. Uninstall.exe 改为延迟加载 rstrtmgr.dll（`/DELAYLOAD:rstrtmgr.dll` + delayimp）。进程初始化不再解析它及其依赖链。
2. 唯一的 Restart Manager 调用点 `RestartManagerLockOwners`（删除失败时报告锁定者的诊断路径）在首次调用前用 `LoadLibraryExW(L"rstrtmgr.dll", LOAD_LIBRARY_SEARCH_SYSTEM32)` 显式从 System32 加载；加载不到就跳过诊断、**绝不回退到应用目录搜索**。
3. verify_pe_contract.py 断言 Uninstall.exe 不再静态导入 rstrtmgr.dll、且 rstrtmgr.dll 作为延迟导入存在；延迟导入也要过系统 DLL 白名单。

**验证**（真实 Windows runner，验证运行 38253897057）：

- 基线对照 control-uninstall-launcher-ncrypt / control-uninstall-worker-ncrypt：v1.0.5 从 EXE 目录加载了植入的 ncrypt.dll（CONTROL-VALID，复现）。
- 决定性回归 candidate-uninstall-rm-worker-ncrypt：锁住 VERSION 迫使执行进程真正调用 Restart Manager，rstrtmgr.dll 从 System32 加载（证明路径执行），植入的 ncrypt.dll **没有**从执行目录加载（从 System32 解析）→ PASS。
- 修复后的标准 launcher/worker 场景不再加载 rstrtmgr.dll，因此判为 UNVERIFIED（诚实：那条会加载它的路径在这里没有执行，决定性证明在强制 RM 场景）。
- 产品级正式回归 `tests/UninstallDllPlantTests.ps1`（随 PR 每次构建运行）在验证分支 step 7b 对两种构建各跑一遍：v1.0.5 退出码 1（失败=复现），修复版退出码 0（通过）。

**证据层级（不夸大）**：以上都是直接运行 Uninstall.exe 二进制、在一个已提权的 runner 上、从测试自建的 `Asterun-Elevated.*` 目录运行；它验证的是二进制的 DLL 加载属性，**不**等于真实 UAC 拆分令牌提权链、真实临时目录的创建方式与 ACL 已验证——后者由实机清单的 U-4（见 CHECKLIST）核对。

**仍未修复、单独登记**：主程序正常模式（Asterun.exe 启动器）在运行时经 crypt32/schannel 的延迟导入链从应用目录加载植入的 ncrypt.dll（investigate-main-ncrypt = FAIL，记录项）。主程序按设计不限制运行时 DLL 搜索（它在进程内加载第三方 Shell 扩展），便携目录本身对用户可写；本轮未授权修改主程序加载策略。


## 套件内容

| 文件 | 用途 |
|---|---|
| `CHECKLIST.md` | 实机步骤、预期结果和结果记录表 |
| `PROCMON.md` | 自动采集做了什么、目标进程识别、事件分类、判定规则、可选的手动采集 |
| `asterun-acceptance.wsb` | Windows Sandbox 配置：packages、kit、tools 只读映射，logs 可写 |
| `scripts/Check-Environment.ps1` | 记录系统版本、完整性级别，判断是否装有 VC++ 运行库 |
| `scripts/Initialize-Workspace.ps1` | 在沙盒内新建工作区，复制并解压两个包，校验哈希和 Procmon 签名；目录已存在就停止 |
| `scripts/Invoke-Scenario.ps1` | 运行一个场景：复制、植入探针、采集、功能检查、导出、分析 |
| `scripts/Run-Acceptance.ps1` | 按顺序运行探针自测和全部场景，生成 summary.txt |
| `scripts/Analyze-Procmon.ps1` | 按场景识别目标进程并判定（规则见 PROCMON.md 第 4 节） |
| `tests/Test-Analyzer.ps1`、`tests/analyzer-fixtures/` | 分析器规则自测（21 个用例） |
| `tests/UpdaterIntegrationTests.ps1` | 与仓库中 `tests/UpdaterIntegrationTests.ps1` 相同的副本（更新器执行链测试） |
| `probe-dll/` | 探针源码、构建脚本、预构建 x64/ARM64 二进制、自检脚本 |

## 与上一版套件相比的变更

1. **Procmon 覆盖检查**：不再只按进程名汇总。每个场景按 EXE 名称、映像路径（自身 Load Image）和命令行（Process Start）识别目标 PID；服务子命令、`Asterun-Elevated.*` 中的更新器和卸载执行进程分别作为独立目标，普通 Asterun 主进程不能替代它们。缺少目标、缺少 Process Start / Load Image 记录或缺少必要的 CSV 列时，结果为 INVALID。
2. **DLL 判定标准**：区分 LOADED、OPEN-EXECUTE、SEARCH（在 EXE 自身目录中查找未找到）、FILE-OP（包括在其他目录中的存在性检查，例如更新器检查安装目录），只有加载或为执行而打开才判失败，查找只判 REVIEW，主动文件操作只记录。PASS 必须同时具备“目标 PID 的 Load Image 显示植入名称来自 System32 + 同名有效对照（v1.0.5 从自身目录加载了植入文件）+ 功能检查通过”；探针日志只作辅助（只能把结果判为 INVALID，不能单独判通过）；没有有效对照的为 UNVERIFIED。
3. **目录隔离**：packages、kit、tools 只读映射，logs 是唯一可写的映射目录；所有运行都在沙盒内部新建的 `C:\AsterunWork\run-<时间>` 中进行；`Initialize-Workspace.ps1` 替换了会递归删除已有目录的 `Prepare-Packages.ps1`，目录已存在就停止。旧的 `Summarize-Procmon.ps1` 由 `Analyze-Procmon.ps1` 替换。
4. **更新器执行链**：撤回“必须修改 VERSION 并发布才能验证”的结论；新增 `tests/UpdaterIntegrationTests.ps1`（已进入 PR #64，CTest 在 Windows CI 中运行），并作为 candidate-update-chain 场景纳入取证。明确区分更新器执行链与在线发现更新。
5. **先在 CI 验证**：新增不合并的验证分支和工作流，在 Windows runner 上实际运行了套件中的全部脚本。

## 已知限制

- 探针 DLL 不导出任何函数。作为静态依赖被加载时进程会启动失败（v1.0.5 对照组如此，这是预期现象；F-1 中的卸载执行进程也如此）。作为运行时依赖被加载时进程未必失败（investigate-main-ncrypt 中主程序仍在运行），所以判定以 Load Image 为准，功能检查只是补充。
- 植入只覆盖静态导入（A2）。运行时 `LoadLibrary` 的限制（A3）由 CTest `dll_search_hardening_tests` 在进程内实际验证；采集中只记录场景里实际发生的查找。
- 主程序正常模式按设计不调用 SetDefaultDllDirectories（要加载第三方 Shell 扩展），运行中的按名称查找保持原样，只作记录。
- 系统 DLL 自身的静态依赖不受 DependentLoadFlags 保护，已在 CI 中实测确认（F-1）。套件只调查了 CI 中实际观察到的名称（ncrypt.dll、ntasn1.dll）；其他系统、其他版本可能有别的名称，A-1 报告中的 SEARCH 列表就是线索。
