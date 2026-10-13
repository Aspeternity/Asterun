# 批次 C 接手状态与测试规格修订

状态：未完成，不能验收或合并。基线 main 为 0adf6c511f58d89b01d494738616e56864c5d405，接手提交为 125baa3。

原始交接全文保存在 handoff-125baa3.md，仅作为历史记录；以下修订优先于历史规格。

## 已核对

运行 38301093470 已完成：x64/ARM64 打包构建成功，Linux 测试成功，ARM64 测试编译成功。两个 Windows 测试作业失败，跨平台清单检查跳过。编译通过不能代替更新失败收尾验证。

QueryPathState 仅将明确不存在作为允许清理的条件；异常类型和查询失败保留。接手后的产品修改仅为下述健康事件错误码保存；未修改更新协议、工作流、VERSION 或发布状态。

## 记录隔离修订

每个用例采用独立的带 GUID 根目录，子目录为 install、records。用户数据在 install/data 内。假主程序从自己的实际映像路径确定同级 records 目录，读取 records/mode，追加 records/launches.txt；不再使用共享的 TEMP/asterun-updater-ft 路径。

records 必须由测试预先创建，不能是重解析点。受保护用例需保证被启动的用户具有所需读写权限，仅允许设置本次测试目录的权限。完整性级别仍记录，不宣称已验证真实 UAC。

快照排除 records，因为它位于安装目录外。每个用例使用自己的记录文件；不能通过清空共享文件来避免串扰。

## 补回场景 4

场景：建立受保护工作目录失败。只在测试自己创建的安装根目录上设置并保存可恢复的权限，拒绝创建子目录，保持旧程序可读和可启动；不得改动真实安装。

前置证据：父进程已退出、归档校验通过、测试进程已提权。故障应发生在创建受保护工作根处，而不是更早的参数或归档校验。

断言：返回 2；失败阶段为准备受保护目录；程序和用户数据快照不变；归档及暂存保留；旧版无参数启动恰好一次；只出现一个预期提示；没有删除既有目录。检查后恢复本次设置的权限，再清理本次创建的目录。

状态：规格已补回，未实现、未运行。若无法稳定构造，报告阻碍，不自行记为通过或延期。

## 当前缺口

- UpdaterFailureTests.ps1、两个负向用例迁移、CTest 注册和旧版对比尚未完成。
- 场景 6 的共享读占用 README.md 已在事务层验证（见下），失败收尾端到端仍未验证，仍是验收必需项。
- 场景 12、13 按既定授权未自动验证；新版无法停止及重启查询失败也未覆盖，不计作通过，不因本记录而获得额外延期批准。
- 原会话生成提示框自动化曾被分类器阻断，原始文字见历史交接。不重建被拦截的内容，不通过换会话或改写重复尝试；由平台澄清，或由用户另行批准实质不同的验证范围。
- 不删除旧测试、不降低断言、不以跳过测试制造全绿。

## 后续验收边界

最终需要候选提交的完整 CI、场景结果、备份内容及用户数据核对和旧版对比。Windows 实机失败更新继续延期。所有未验证项逐项公开，合并和 dev-latest 发布须另获批准。

## 接手后的独立验证增量

新增 updater_fake_main_tests：直接运行测试辅助程序，检查不同独立根目录下的同名用例不会共享记录，零码/非零码退出和持续存活符合预期。它不启动 Update.exe、不处理任何提示框，不验证 RestartPreviousVersion 的判定。

update_runtime_tests 新增 Windows 专属事务用例：共享读占用 README.md，要求 ApplyPackage 已替换 Asterun.exe、备份 README.md 后在覆盖处失败；Rollback 必须报告该文件共享冲突、恢复其他文件，备份内容和用户数据不变；释放锁后回滚完成。这仅验证场景 6 的触发条件及事务层结果，不覆盖 FinishFailedUpdate、提示、重启抑制或恢复材料收尾策略。场景 6 端到端缺口保持。

本地用 g++ 直接编译运行 update_runtime_tests 的 Linux 路径通过；Windows 增量结果以对应提交的 CI 为准。没有删除或跳过尚未迁移的旧负向用例。

## CI 38304045926 的实际结果

提交 cef1d51298f4230ede8d3a260577e82d8da4bc82：两套 Windows 作业各 60/62 通过。update_runtime_tests 通过，包含上述共享读锁事务场景；受保护模式更新成功流程也通过。Linux、ARM64 测试编译及两种架构打包作业通过。

两个失败分别是：尚未迁移的 updater_portable_integration_tests 在错误哈希场景等待旧的静默退出而超时；新增 updater_fake_main_tests 无法加载 Get-FileHash。这第二项是新增测试自身的问题，不能归为产品预期失败。本次修正复用现有更新集成脚本的 Windows PowerShell 模块路径初始化，修正后的结果待 CI 验证。

本记录不把事务层验证或辅助程序自测视作统一失败收尾验证；不把部分作业成功写成完整 CI 通过。

## 健康事件错误码复核

发现 CreateHealthEvent 仅保存了 CreateEventW 的失败码；令牌查询和安全描述符失败分支先清理再返回，调用方随后读取的 last-error 不可靠。此外 SetEntriesInAclW 直接返回错误码，原实现丢弃了该值。

改为显式 DWORD 输出参数：在失败 API 处保存错误码，清理后直接传给失败收尾；SetEntriesInAclW 使用返回值。返回码仍为 4，创建事件的权限和成功流程不变。产品变动仅在 UpdaterMain.cpp，无测试入口。这些罕见失败分支目前仅代码审查覆盖，不能写成运行验证通过。

API 依据（2026-10-13 核对）：
- https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-setentriesinaclw
- https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror

## 辅助程序自测的命令行断言修正

运行 38331123280 的 Windows 基线作业确认 Get-FileHash 问题已消失，退出码 0/3、启动记录和 PID 等检查通过；随后失败于把整条命令行去掉引号后与路径字符串直接比较。原输出未记录命令行，不能据此断言具体是短路径、引号还是空白差异。

改用 Windows CommandLineToArgvW 解析实际记录，严格要求 argc 为 1 且程序名为 Asterun.exe；进程归属继续由本次启动的 PID 和各自记录目录核对。增加三个解析对照：带空格的程序路径及尾部空白允许，额外空参数和普通参数都必须检出；不删除无参数要求。新的输出记录实际命令行和参数数目。此自测仍不启动 Update.exe，也不操作提示框。

API 依据：https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw
