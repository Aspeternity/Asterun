# Asterun 批次 A 验收套件（第 2 版）

本套件只用于验收，不属于 Asterun 产品，不进入安装包，也不会合并进 main。它验证 PR #64（批次 A：静态运行库与 DLL 加载加固）中需要在真实 Windows 上运行的部分。

## 被测版本

| 项 | 值 |
|---|---|
| 被测提交 | `@@PR_SHA@@`（PR #64 当前 head） |
| 基线 | main `eefa61a7131e0d770d0e5455c97ba5466dfff450`；对照包用正式版 v1.0.5 |
| PR CI 运行 | @@PR_RUN_URL@@ |
| 被测包 | 上述运行页面底部 Artifacts 中的 `Asterun-x64`。下载得到的是外层 zip，里面是 `Asterun-x64.zip`；E-2 会自动解开外层并记录两层的 SHA-256。用 `-CandidateSha256` 校验的是**内层** `Asterun-x64.zip`（见下行） |
| 被测包内层 zip 的 SHA-256 | 本会话无法下载 PR CI 的 artifact，内层 zip 的哈希由 E-2 在你的环境中计算并记录。套件验证运行（下一行）从同一提交独立构建的包：@@KV_CAND_SHA@@，与 PR CI 的包不是同一次构建，哈希不要求相同 |
| 套件验证运行 | 分支 `acceptance/batch-a-kit-validation`，提交 `@@KV_SHA@@`（= PR head + `acceptance-kit/` + 验证工作流，不合并）：@@KV_RUN_URL@@ |
| 对照包 | https://github.com/Aspeternity/Asterun/releases/download/v1.0.5/Asterun-x64.zip ，SHA-256 `2bf93316a54d84fa7721016d3b0bf48b6a6ddec92f58acde9c94d7401321bd23` |
| 探针 DLL | `probe-dll/out/x64/asterun_dll_probe.dll`，SHA-256 `e01b82a8da56ea4721ccaeb84d3bc64a4ee98cd18d2c3cba129c09e63cf74e82`；ARM64 见 `probe-dll/SHA256SUMS.txt` |

## 分工

### CI 已完成（在 Windows runner 上实际运行，不是语法检查）

| 内容 | 位置 | 结果 |
|---|---|---|
| x64、ARM64 全部目标编译；PE 导入白名单、无 CRT DLL、DependentLoadFlags=0x800 | PR CI | @@R_BUILD@@ |
| 自动化测试：Linux 33；Windows x64 两套各 @@N_WIN@@（含 dll_search_hardening_tests、updater_portable/protected_integration_tests）；ARM64 只编译 | PR CI | @@R_TESTS@@ |
| 更新器执行链（便携目录 + 受保护目录，见 CHECKLIST 第 4 阶段） | PR CI 的 CTest | @@R_UPDATER@@ |
| 本套件全部脚本：环境检查、新建工作区（重复运行必须停止且不改动任何文件）、分析器自测、探针自测（套件内 LLVM 版和 CI 中用 MSVC 现编的版本）、10 个 Process Monitor 场景（3 个 v1.0.5 对照 + 6 个被测场景 + 探针自测）、用真实采集数据做的 INVALID 负向检查 | 套件验证运行 | @@R_KIT@@ |

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
| `tests/Test-Analyzer.ps1`、`tests/analyzer-fixtures/` | 分析器规则自测（19 个用例） |
| `tests/UpdaterIntegrationTests.ps1` | 与仓库中 `tests/UpdaterIntegrationTests.ps1` 相同的副本（更新器执行链测试） |
| `probe-dll/` | 探针源码、构建脚本、预构建 x64/ARM64 二进制、自检脚本 |

## 与上一版套件相比的变更

1. **Procmon 覆盖检查**：不再只按进程名汇总。每个场景按 EXE 名称、映像路径（自身 Load Image）和命令行（Process Start）识别目标 PID；服务子命令、`Asterun-Elevated.*` 中的更新器和卸载执行进程分别作为独立目标，普通 Asterun 主进程不能替代它们。缺少目标、缺少 Process Start / Load Image 记录或缺少必要的 CSV 列时，结果为 INVALID。
2. **DLL 判定标准**：区分 LOADED、OPEN-EXECUTE、SEARCH、FILE-OP，只有加载或为执行而打开才判失败，查找只判 REVIEW，主动文件操作只记录。PASS 必须同时具备“目标 PID 的 Load Image 显示植入名称来自 System32 + 同名有效对照（v1.0.5 从自身目录加载了植入文件）+ 功能检查通过”；探针日志只作辅助（只能把结果判为 INVALID，不能单独判通过）；没有有效对照的为 UNVERIFIED。
3. **目录隔离**：packages、kit、tools 只读映射，logs 是唯一可写的映射目录；所有运行都在沙盒内部新建的 `C:\AsterunWork\run-<时间>` 中进行；`Initialize-Workspace.ps1` 替换了会递归删除已有目录的 `Prepare-Packages.ps1`，目录已存在就停止。旧的 `Summarize-Procmon.ps1` 由 `Analyze-Procmon.ps1` 替换。
4. **更新器执行链**：撤回“必须修改 VERSION 并发布才能验证”的结论；新增 `tests/UpdaterIntegrationTests.ps1`（已进入 PR #64，CTest 在 Windows CI 中运行），并作为 candidate-update-chain 场景纳入取证。明确区分更新器执行链与在线发现更新。
5. **先在 CI 验证**：新增不合并的验证分支和工作流，在 Windows runner 上实际运行了套件中的全部脚本。

## 已知限制

- 探针 DLL 不导出任何函数。v1.0.5 加载它后会启动失败，这对对照组是预期现象；被测组如果加载了它，同样会启动失败，因此功能检查本身也是一道防线。
- 植入只覆盖静态导入（A2）。运行时 `LoadLibrary` 的限制（A3）由 CTest `dll_search_hardening_tests` 在进程内实际验证；采集中只记录场景里实际发生的查找。
- 主程序正常模式按设计不调用 SetDefaultDllDirectories（要加载第三方 Shell 扩展），运行中的按名称查找保持原样，只作记录。
- 系统 DLL 自身依赖链是否受 DependentLoadFlags 影响，只记录，不断言。
