# UpdateManager synchronous WinHTTP cancellation race

状态：单独记录，Batch 2 未修复、未修改 UpdateManager。基于 v1.0.1 / Batch 1 源码。

严重程度：High（静态审查确认并发模式违反 WinHTTP 契约，尚无实机崩溃复现）。

位置：`src/platform/UpdateManager.cpp` 的 `InternetHandle`（约第 51 行）、同步 session 创建（约第 228 行），以及 OpenRequest / DownloadText / DownloadPackage 中的 stop_callback（约第 312、438、641 行）。

现状：session 未设置 WINHTTP_FLAG_ASYNC。停止回调通过原子 exchange 取得并关闭 request；与此同时下载线程可能还在 WinHttpSendRequest、ReceiveResponse 或 ReadData 内。也可能已取得 Get() 返回的 handle，随后在取消关闭后才调用下一次 API。

问题：原子 exchange 保证了 Close 的唯一性，但不能同步 WinHTTP 调用本身。微软要求同步操作返回前不能由其他线程对同一 request 操作/关闭；关闭后的 handle 值也可能被系统复用。因此存在无效句柄访问/竞态风险，不能把当前实现作为 Everything 修复模板。

建议另行批准一个独立批次：在更新器自己的 HTTP transport 内采用局部异步适配，下载线程拥有唯一关闭路径，停止回调只发信号，状态/在途 buffer 保留至 HANDLE_CLOSING。保持现有发布来源、协议、清单验证、哈希/签名、更新 watchdog、安装和回滚逻辑；不要顺带合并两个下载器或改通用框架。

专项验收：metadata 和 package 各网络阶段取消；watchdog 与完成同时发生；关闭后迟到回调；对象析构；重复检查/下载/取消；失败恢复与完整升级回归。验证后再讨论复用代码，不能仅凭原子指针测试就认定消除了 WinHTTP 竞态。

依据：
- https://learn.microsoft.com/en-us/windows/win32/winhttp/concurrency-in-winhttp
- https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpclosehandle
