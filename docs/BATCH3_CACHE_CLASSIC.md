# Batch 3 — 失效搜索缓存释放与 Classic 延迟加载

基线：Batch 2.5 `4d8b4ed`。只处理 B2 / B1，不涉及 B3 或 HTTP warning 清理。

## B2

App 在命令修改、Reload、Provider cache 发布后调用 `ReleaseStaleSearchCaches`，因此 Launcher 不存在或隐藏时也能释放旧代际。Search 入口同样做常数时间的代际检查，包含 IndexSearchable 为 false 的提前返回路径。

- contextual cache 的代际过期，或最后一个 folder 模板移除后，整体交换为空对象，释放 commands / indices / prepared 的容量及 folder / generation。
- base prepared index 的代际过期时交换为空 vector，generation 重置。它在添加 folder 模板后也不会长期保留旧索引。
- 同一代际仍有效的缓存不清理；隐藏 Launcher 不清缓存。上下文 folder 改变时仍使用原来的重建逻辑。
- 不改匹配、排序、拼音、Usage 或数字键逻辑，不改 Provider 发布时机。

## B1

`EnsureClassicResources` 将原有位图加载路径移动到首次 Classic 使用：Classic 启动时由 Create 检查，Modern 启动不加载；样式首次切到 Classic 时由 ApplyAppearance 调用。DC 只在全部位图和背景信息加载成功后创建，作为已完整加载的标志。

返回 Modern 后保留 Classic 位图/DC，随后再切 Classic 直接复用。析构统一释放。任何部分加载、背景信息读取或 DC 创建失败，都释放已分配资源并清空成员，允许后续重试；Classic 首次创建失败仍返回 false。运行中分配失败使用现有绘制空资源保护，下一次 ApplyAppearance 可重试。

没有改变任何位图、资源 ID、LoadImage 参数、绘制代码、DPI 资源选择、布局或默认设置。

## 验证

- 扩展 AppLifecycleRuntimeTests：24 轮添加/移除 folder 参数，删除最后一个模板、Reload 代际更新；检查 vector capacity 为零；同代际重复搜索的结果/分数与缓存地址保持；Hide 不释放缓存；base prepared index 过期释放。
- LauncherResourceRuntimeTests：逐个位图加载失败、GetObject/DC 失败的 GDI 清理和重试；Modern 初始零 Classic 位图/DC；首次进入 Classic 加载一次，此后保留相同 handle。
- 21 轮 Modern → Classic → Modern → Classic 和窗口销毁；100/125/150/200/250% DPI 的实际 WM_DPICHANGED 路径，比较背景/shortcut/close 绘制像素与 Classic-first 初始化一致。
- 记录 Private Bytes、Working Set、GDI Objects、USER Objects、Process Handles。预热后对重复操作资源数量做差分检查，不设置绝对内存 MB 阈值。
- 完整 CI 继续验证核心测试、Windows desktop/runtime、Windows 10 API baseline、x64/ARM64 构建和包契约。实际结果填写在 PR。

验证边界：DPI 自动测试比较确定性的 Classic 位图绘制路径，不替代真实显示器上的整窗视觉与交互验收；Win10 API baseline Runner 不是 Win10 实机；ARM64 保持构建和包验证。
