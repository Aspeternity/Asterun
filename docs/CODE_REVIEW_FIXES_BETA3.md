# v0.8.0-beta.3 代码审查修复验收记录

本分支承接 v0.8.0-beta.2（`3db9b75`）的 18 项体检清单。应用行为和配置模式版本继续保持 11 / 2 / 2 / 22；智能数字键映射和搜索排序的比较规则不变。Windows GUI、UAC、Everything 签名及 x64/ARM64 打包需要在 Windows CI 和实机上完成最后验收。

| 清单项 | 修复位置 | 验收重点 |
| --- | --- | --- |
| H1、L1 配置恢复 | `src/core/ConfigIO.cpp`、`ConfigValidation.hpp`、三个 Store | 语义校验后才选择主文件/备份；坏主文件不覆盖好备份；无备份时只读隔离有效记录；报告主文件修复结果，禁止无效新内容写入。 |
| H2 回滚 | `src/updater/UpdaterTransaction.cpp`、`UpdaterMain.cpp` | 回滚逐项报告失败；不完整恢复保留备份和错误日志；新进程未退出时不回滚。 |
| H3 解压 | `src/core/ArchiveExtractor.cpp`、三个平台调用点 | miniz 同步读取，限制成员与展开大小，拒绝路径穿越/重解析点，校验 CRC；Everything 先暂存再发布；取消与失败清理。 |
| H4 服务源身份 | `src/platform/EverythingBootstrapper.cpp` | 受保护暂存副本通过 Windows Authenticode 链验证且签名组织为 voidtools，才替换服务宿主。需实机验证官方 x64/ARM64 签名。 |
| H5、M1 搜索热路径 | `src/app/App.cpp`、`SearchEngine.cpp`、`RelevancePolicy.cpp` | `{folder}` 视图按索引代次和目录复用；归一化字段/首字母预计算；Top K 保留原索引并列次序。 |
| H6、M3 使用记录 | `src/core/UsageStore.cpp`、`src/app/App.cpp` | 内存学习立即生效，后台写入按代次合并并在退出排空；临时写入失败保留内存状态并重试；永久删除的用户 ID 清理，缺席自动来源有一年宽限期。 |
| M2 拼音缓存 | `src/core/PinyinSearch.cpp` | 有容量上限的链表 LRU 替代满缓存线性扫描。 |
| M4、M5 后台任务 | `src/core/*Provider*`、`ProviderRegistry.cpp`、`src/app/AppUpdate.cpp`、`App.cpp` | 枚举批次检查停止令牌，取消后不发布部分索引；更新/Everything 工作线程异常转失败状态并通知 UI。 |
| M6 未用接口 | `src/app/App.hpp`、`src/core/CommandStore.hpp`、`ConfigIO.hpp` | 删除仓库内未使用的访问器及随之无用的计数字段，保留仍在内部使用的热键状态。 |
| M7、M11 发布与依赖 | `scripts/verify_release_contract.py`、`CMakeLists.txt` | 当前版本契约替代历史分支；未知版本失败；三项 FetchContent 有 SHA-256；miniz 许可证进入安装包。 |
| M8 重复基础代码 | `src/platform/SecureArchive.cpp`、`WindowsCommandLine.hpp`、`ArchiveExtractor.cpp` | 文件哈希、参数编码、归档提取各自只有一个底层实现；调用端仍维持自己的安全前置条件。 |
| M9、M10 结构和构建 | `src/app/AppUpdate.cpp`、`src/ui/*Layout.cpp`、`CMakeLists.txt` | 拆分更新协调和两个窗口布局；应用生产实现只编译一次，产品和真实窗口测试共用静态目标，入口/资源仍由各 EXE 持有。 |

本地 Release 构建及 32 项 CTest 全部通过。`scripts/verify_version.py`、`scripts/verify_release_contract.py`、发布脚本语法检查和 `git diff --check` 通过。Windows CI Build #716 的 x64、ARM64 构建与包契约、Windows 10 API 基线及 25 项兼容性测试、25 项桌面运行时烟测、核心测试全部通过。搜索微基准使用 1/100/1000/10000 条合成命令，10,000 条的稳态查询平均约 8.6 ms（旧路径）与 0.8 ms（预计算 + Top K）；这是 Linux 合成负载，不代表 Windows 输入到绘制耗时，也未计入一次性索引准备。

`docs/V0.8_BETA_VALIDATION.md` 新增坏配置、签名拒绝、慢速或损坏解压、受阻回滚、后台写入及搜索负载的手工验收点。Windows CI 不能代替官方 Everything x64/ARM64 签名、UAC 和真实桌面交互的实机验收；完成这些验收前不要合并或发布 beta.3。
