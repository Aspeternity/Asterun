# Process Monitor 取证步骤

目的是用 Process Monitor 记录 Asterun.exe、Update.exe、Uninstall.exe 实际在哪些目录查找或加载了 DLL，然后用 `Summarize-Procmon.ps1` 生成可核对的清单。

## 1. 准备

- 从微软 Sysinternals 下载 Process Monitor：https://learn.microsoft.com/sysinternals/downloads/procmon 。解压出 `Procmon64.exe`，放到宿主机的 `C:\AsterunAcceptance\tools\`。ARM64 设备用 `Procmon64a.exe`。
- 在沙盒或虚拟机内，以管理员身份运行 `C:\Acceptance\tools\Procmon64.exe`，首次运行时接受许可协议。它需要管理员权限，才能看到提权后的 Update.exe、Uninstall.exe 和服务子命令进程。

## 2. 过滤器（Filter > Filter...，Ctrl+L）

先点 Reset，然后逐条添加以下条件，每条添加后点 Add：

| 列 | 关系 | 值 | 动作 |
|---|---|---|---|
| Process Name | is | Asterun.exe | Include |
| Process Name | is | Update.exe | Include |
| Process Name | is | Uninstall.exe | Include |
| Path | ends with | .dll | Include |
| Path | ends with | .exe | Include |
| Operation | is | CreateFile | Include |
| Operation | is | QueryOpen | Include |
| Operation | is | Load Image | Include |

保留默认的 Exclude 条目（Procmon、System 等）。同一列的多个 Include 条件之间是“或”，不同列之间是“与”，所以这组条件的含义是：三个进程之一、路径以 .dll 或 .exe 结尾、操作为三者之一。.exe 事件用于覆盖率对照：汇总脚本要求在被监视目录下看到被测进程自己的 EXE，否则判定这次采集无效。

再通过 Options > Select Columns 勾选 “Command Line”，导出的 CSV 才会带上命令行，用来区分提权的服务子命令进程。

工具栏上保持“显示文件系统活动”和“显示进程与线程活动”开启。后者是 Load Image 事件的来源。

## 3. 每个场景的采集方法

1. Edit > Clear Display（Ctrl+X）清空。
2. 确认 File > Capture Events（Ctrl+E）处于开启状态。
3. 按 CHECKLIST.md 执行该场景的操作，直到操作结束、对话框关闭或进程退出。
4. 再按一次 Ctrl+E 停止采集。
5. 导出：File > Save...，选择“Events displayed using current filter”，格式选 “Comma-Separated Values (CSV)”，文件名按 CHECKLIST.md 中的编号，例如 `C:\Acceptance\logs\P3-candidate-update.csv`。
6. 同时另存一份原生格式（PML）。需要时可以完整复查。

## 4. 汇总

在 PowerShell 中运行（`-Watch` 填被测目录和当前用户的临时目录，用分号分隔）：

```powershell
powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Summarize-Procmon.ps1 `
  -Csv C:\Acceptance\logs\P3-candidate-update.csv `
  -Watch "C:\Acceptance\work\candidate-batch-a;$env:TEMP" `
  -Out C:\Acceptance\logs\P3-candidate-update-summary.txt
```

输出分三部分：

- `COVERAGE`：被测进程自身的 EXE 在被监视目录下出现的次数。为 0 时脚本输出 `RESULT: INVALID` 并以退出码 3 结束，说明 `-Watch` 路径或过滤条件不对，需要重做，不能当作“0 条即通过”。脚本会自动同时匹配目录的长路径和 8.3 短路径形式（沙盒中 %TEMP% 可能是 `WDAGUT~1` 这样的短路径）。
- `CHECKED`：这三个进程在被测目录或临时目录下对 *.dll 的每一次访问。提权后的临时副本就运行在临时目录里。
- `INFO`：从 Windows 目录以外加载的其他 DLL，例如 Program Files 下第三方 Shell 扩展的完整路径加载。这一部分不算失败。

如果脚本报告“no events from Asterun.exe, Update.exe or Uninstall.exe”，说明这次采集没有覆盖测试，需要重做。

## 5. 怎样判读

| 结果 | 含义 |
|---|---|
| 基线 v1.0.5 的 CHECKED 中出现 `VCRUNTIME140.dll`、`MSVCP140.dll` 等，结果为 NAME NOT FOUND 或 SUCCESS | 加载器在应用目录中查找静态导入的运行库，这是批次 A 要消除的行为 |
| 被测版本的 Update.exe、Uninstall.exe 和服务子命令的 CHECKED 为 0 | 通过：静态导入（A2）和运行时加载（A3）都没有在这些目录中查找 |
| 被测版本的上述进程 CHECKED 不为 0 | 不通过：把汇总文件和 PML 一起交回，由开发方定位原因 |
| 被测版本主程序正常模式的 CHECKED 中出现运行库名称（vcruntime、msvcp、api-ms-win-crt） | 不通过（A1 或 A2 失效） |
| 被测版本主程序正常模式的 CHECKED 中出现其他名称 | 仅作记录。主程序按设计不调用 SetDefaultDllDirectories，交回清单即可，不作为失败 |
