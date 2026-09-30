# Batch 4 — Show 刷新测量与局部去重

基线：Batch 3 `6c40786`，独立分支 / Draft PR #53。

## 先测再改

仅测试提交 `eec4420` 没有修改产品代码。Build #852（run 36708157179）在 windows-latest 和 windows-2022 上测量真实 Win32 EDIT / Launcher HWND，结果一致：

| 场景（Modern / Classic 均覆盖） | EN_CHANGE 次数 | 通知内刷新 | Show 显式刷新 | 同步刷新合计 |
| --- | ---: | ---: | ---: | ---: |
| 首次 Show，空 query | 1 | 1 | 1 | 2 |
| Hide → Show，之前 query 为空 | 1 | 1 | 1 | 2 |
| Hide → Show，之前 query 非空 | 1 | 1 | 1 | 2 |
| 直接 SetWindowTextW 清空已有空文本 | 1 | 1 | 0 | 1 |
| 直接 SetWindowTextW 清空非空文本 | 1 | 1 | 0 | 1 |

测试通过现有 searchGeneration 增量计数，并在 HWND subclass 中分离 EN_CHANGE 内的增量。Create / Hide 在测量区间外；有效 list 存在时 RefreshResults 每次增加 generation 并调用一次 App::Search。Dynamic 完成走 ApplyDynamicResults → RebuildVisibleResults，不调用 RefreshResults、不增加 generation。没有产品 instrumentation 或日志。

## 修改

只改 LauncherWindow::Show：在 SetWindowTextW 前后保存 generation，记住本次 reset 是否已刷新。Show 末尾仅在 reset 未刷新时调用原有显式 RefreshResults。

- 保留原生 EDIT 通知和 EN_CHANGE 处理，不改 WndProc，不引入持续 suppression 状态。
- 正常路径在显示窗口前仍由原有 EN_CHANGE 完成必要刷新；清选择、定位、首次 reveal、焦点及 Edit selection 顺序不变。
- 未发生通知时仍有原位置的显式刷新兜底。
- 每次普通 Show 从两次同步搜索减为一次；不将调用次数减少等同于总唤醒耗时降低 50%，本批未报告毫秒收益。
- 搜索、排序、拼音、Usage、数字键、Everything 查询、配置、协议与资源不改。

## 确定性回归

扩展既有 LauncherResourceRuntimeTests（复用现有 friend，不增加产品测试接口）：

- 两种样式的全部测量场景和重复空/非空唤醒，断言普通 Show 总刷新为 1；直接 Edit 修改仍正常刷新。
- 测试侧模拟无 EN_CHANGE 的 reset，断言显式兜底刷新 1 次。
- 带 Usage 的真实测试命令，Show 后结果 ID / score / usageScore 与直接搜索一致，选中首项。
- 普通逐字输入、替换 Edit selection、中文文本及 IME 起止消息，每次文本变化正常刷新。
- 上下键 / Tab 循环选中，不产生新搜索。
- 单结果自动执行决策：IME 中不执行，组合结束后允许执行；空 query 不执行。使用无效 command index 的合成结果观察执行入口，不启动外部程序。
- 两种样式复用 NumericIntentRuntimeFixture，覆盖数字键 continuation、超时、迟到 probe、快照、后续输入及 Escape 取消。
- Everything 使用缺席的专用测试 endpoint 验证查询 pending；在真实 UI 完成入口注入当前/过期 generation，验证异步结果合并、选择保留及跨 Show 迟到结果拒绝，完成本身不触发同步搜索。IPC/provider transport 继续由既有测试覆盖。

完整 CI 结果记录在 PR。原有资源 / DPI 回归继续执行。自动 IME 消息测试不替代实际中文输入法候选窗交互验收；Windows 10 API baseline 不是 Windows 10 实机，ARM64 本批保持构建与包验证。
