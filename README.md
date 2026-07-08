<div align="center">

# hopy

**轻量 · 跨平台剪贴板管理器** — 窗口在文本光标处弹出，永不抢焦点，回贴精准落回原位。

<a href="https://github.com/Hunlongyu/hopy/releases/latest"><img alt="Release" src="https://img.shields.io/github/v/release/Hunlongyu/hopy?style=flat-square&color=6b8cff&label=release"></a>
<a href="https://github.com/Hunlongyu/hopy/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/Hunlongyu/hopy/total?style=flat-square&color=6b8cff"></a>
<a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/Hunlongyu/hopy?style=flat-square&color=6b8cff"></a>
<img alt="Platform" src="https://img.shields.io/badge/platform-Windows-6b8cff?style=flat-square">
<img alt="Qt" src="https://img.shields.io/badge/Qt-6-6b8cff?style=flat-square&logo=qt&logoColor=white">
<img alt="C++" src="https://img.shields.io/badge/C%2B%2B-20-6b8cff?style=flat-square&logo=cplusplus&logoColor=white">

<img src="docs/showcase.png" alt="hopy 界面总览：颜色值卡片 / 图片卡片 / 鼠标悬浮 / 实时预览" width="100%">

</div>

## ✨ 亮点

- **🎯 插入符跟随 · 永不抢焦点** —— 窗口就在编辑器的**文本光标处**弹出，全程不夺取焦点，原窗口的插入符始终存活，回贴精准落回原光标位置。
- **🌈 颜色值卡片** —— 自动识别 `#hex`（3/4/6/8 位）、`rgb()` / `rgba()`、裸 hex，在行首直接画出对应色块。
- **🔗 内容感知打开** —— 对 URL / 邮箱 / 文件路径，按一个键或点一下即可用浏览器 / 默认邮箱 / 资源管理器打开。
- **🔒 隐私遮罩** —— 自动遮蔽被系统标记为敏感的内容（如从密码管理器复制的条目），列表与预览里都不泄露。
- **👁️ 实时预览 + 侧键翻页** —— 悬停或空格弹出预览，信息条显示**类型 · 字数 · 时间**，长文可用鼠标侧键 `M4 / M5` 翻页。
- **📦 静态单文件** —— Windows 下编译为**单个无依赖 exe**，不带任何 Qt DLL，拷贝即用。

> 此外还有：全键盘操作 · 搜索与筛选 · 置顶 / 收藏 · 亮暗双主题 · 中英双语 · 自动检查更新。

## 📥 下载

前往 **[Releases](https://github.com/Hunlongyu/hopy/releases/latest)** 下载最新的 `hopy-*-windows-x64.exe`，双击即用，免安装。

## 🌗 亮 / 暗双主题

<div align="center">
  <img src="docs/showcase-themes.png" alt="hopy 亮色与暗色主题并排对比" width="100%">
</div>

## ⌨️ 快捷键

| 按键 | 功能 |
| --- | --- |
| `Alt + C` | 全局唤起窗口（默认，可自定义） |
| `双击卡片` / `Enter` | 回贴选中项到原窗口 |
| `Shift + Enter` | 以纯文本回贴 |
| `1 – 5` | 选取并回贴第 N 条 |
| `↑ / ↓` · `Home / End` | 上下移动 · 跳首尾 |
| `← / →` · `Tab / Shift + Tab` | 切换内容类型（筛选） |
| `/` | 聚焦搜索框 |
| `O` / `右键` | 打开链接 / 邮箱 / 文件（可自定义触发键） |
| `F` · `T` · `Delete` / `D` | 收藏 · 置顶 · 删除 |
| `Space`（按住） · `M4 / M5` | 预览当前项 · 预览翻页 |
| `Esc` | 退出搜索 / 隐藏窗口 |

## 🔨 构建（Windows，静态单文件）

需要 `CMakePresets.json` 所引用前缀处的静态 Qt 6.11.1（MSVC 2026 x64），外加 Ninja 与 MSVC 2026 工具链。

```powershell
cmake --preset win-static-release
cmake --build --preset win-static-release
ctest --preset win-static-release
```

产物：`build/win-static-release/hopy.exe`。

## 🧱 技术栈

Qt 6 Widgets（自绘卡片 · QSS 主题）· SQLite 持久化 · 平台层（全局热键 / 低级输入钩子 / 插入符探测 / 前台回贴）· CMake + Ninja。面向跨平台设计，当前在 Windows / MSVC 2026 x64 静态链接。

## 📄 许可

[MIT](LICENSE) © 2026 Hunlongyu
