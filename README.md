# 浮贴（FloatNote）

原生 Windows 剪贴板悬浮便签骨架。当前版本直接读取 Windows 剪贴板历史，不创建自己的历史数据库，也不把剪贴板内容写入磁盘。

## 当前骨架

- C++20 + Win32 单进程应用。
- C++/WinRT `Clipboard::GetHistoryItemsAsync()` 读取最多 25 条 Windows 历史。
- 订阅 `Clipboard::HistoryChanged`，系统历史变化后重新加载。
- 提供 `SetHistoryItemAsContent`、`DeleteItemFromHistory`、`ClearHistory` 的服务接口。
- Direct2D / DirectWrite 基础渲染。
- DWM 圆角和 Windows 11 系统背景。
- 置顶、无边框、可拖动窗口。
- 收缩态展示当前记录摘要，展开态展示系统历史并支持滚轮。
- JSON、HTTP/HTTPS 链接和基础 Markdown 文本识别。

## 尚未实现

- 图片缩略图和文件详细信息。
- 记录展开详情及完整操作按钮命中区域。
- JSON / Markdown 高保真格式化。
- 设置窗口、快捷键、暂停监听和视觉精修。
- 自动化测试工程。

## 依赖

不需要第三方库或包管理器。构建环境需要：

- Visual Studio Build Tools，包含 MSVC x64/x86 C++ 工具。
- Windows 11 SDK，包含 C++/WinRT 头文件。
- MSBuild。

当前开发机使用：

- MSVC `14.51.36231`，平台工具集 `v145`。
- Windows SDK `10.0.26100.0`。

Windows 剪贴板历史必须已启用。可按 `Win + V` 打开系统面板并启用。

## 构建

在 PowerShell 7 中执行：

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  'FloatNote.vcxproj' /p:Configuration=Debug /p:Platform=x64 /m
```

Release 构建：

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  'FloatNote.vcxproj' /p:Configuration=Release /p:Platform=x64 /m
```

输出路径：

- `build/x64/Debug/FloatNote.exe`
- `build/x64/Release/FloatNote.exe`

## 数据边界

- Windows 剪贴板历史是唯一数据源。
- 应用进程只保留用于绘制的轻量视图模型和系统历史项句柄。
- 应用退出时释放全部历史投影。
- 应用重启后重新读取 Windows 当前历史。
- 应用启动或退出时不会主动清空 Windows 历史。
