# hopy 内容感知打开 + 预览信息条 + 相对时间 + Home/End 设计文档

> 状态：已通过 brainstorming 评审，待写实现计划
> 日期：2026-07-04

## 目标

一组「代码量小、幸福感高」的交互增强：

1. **内容感知「打开」**：对当前/鼠标下的条目，按内容类型一键打开 —— URL→浏览器、email→邮箱、文件路径/文件→资源管理器。触发键可配置（默认鼠标右键 + 键盘 `O`，都生效）。
2. **预览窗口顶部信息条**：显示内容类型、字数/字符数、绝对时间戳、以及「按 X 可打开…」的操作提示。
3. **相对时间**：卡片 meta 行由绝对时间改为相对时间（刚刚 / N 分钟前 …）。
4. **Home / End**：跳到第一条 / 最后一条。
5. **帮助面板重排**：容纳新增快捷键，分组两列。

## 决策摘要（brainstorming 结论）

| 议题 | 决定 |
|---|---|
| 触发绑定 | 一个「打开」动作，带鼠标绑定（默认**右键**，可改中键/关闭）+ 键盘绑定（默认 **`O`**，可改/关闭），两者都默认生效、各自可配 |
| 右键语义 | 右键直接触发「打开」，**不是**右键菜单（消费事件，不弹默认菜单） |
| 目标条目 | 鼠标绑定→鼠标下的卡片；键盘绑定→当前选中条目 |
| 文件路径判定 | 纯文本须 `QFileInfo::exists()` 为真才算可打开 |
| 检测优先级 | URL → email → 存在的文件路径 → 无 |
| 资源管理器定位 | Windows `explorer /select,`（选中文件/打开文件夹）；mac/Linux 留空实现（`#ifdef`） |
| 相对时间 | 卡片 meta 用相对时间；绝对完整时间戳移到预览信息条，不丢信息 |
| 帮助面板 | 分组两列（导航 / 操作 / 窗口·预览） |

## 现状（代码事实）

- `AppSettings`（`src/config/Settings.h`）是扁平结构 + JSON 序列化（`fromJson`/`toJson`）；新增字段照现有字段模式加即可。
- `RecordDelegate::editorEvent` 已存在（上一提交新增，处理**左键**动作图标点击）；本功能的鼠标「打开」不改它，改走 MainWindow 的 `eventFilter`（见组件 4）。
- `MainWindow` 已在 `list_` 与 `list_->viewport()` 上 `installEventFilter(this)`（现有 `eventFilter` 处理 `Leave`/按键）。
- `RecordDelegate::paint` 的 meta 行当前渲染绝对时间戳 `yyyy-MM-dd HH:mm:ss`（`drawMeta`）。
- `PreviewPopup`（`src/ui/PreviewPopup.h`）：无边框预览，`showPreview(rec, anchor, leftSide)`；内部 `QScrollArea` + `QLabel content_`。顶部信息条加在 `content_` 之上。
- `HelpPanel`（`src/ui/panel/HelpPanel.cpp`）：静态 `Row{key, desc}` 数组 → `QGridLayout`。
- `MainWindow::keyPressEvent`：命令模式单键 F/T/D/Delete/Esc/`/`；`currentId()` 取当前选中条目 id。
- `QDesktopServices::openUrl` 已在用（`UpdateService`/`SettingsPanel`）。
- 内容检测有先例：`util/ColorDetect`（delegate 已用 `detectColor` 渲染色卡）。
- 平台差异模式：单 `.cpp` 内 `#if defined(Q_OS_WIN)`（见 `platform/Autostart.cpp`、`platform/UpdateInstaller.cpp`）。
- `ClipboardRecord`：文本内容在 `RawRole`（完整、未截断）；`ContentType` = Text/RichText/Image/Files；Files 条目的路径在 `RawRole`（换行分隔）。

## 架构与组件

### 1. 内容检测（纯函数，`src/util/OpenTarget.{h,cpp}`）

```cpp
enum class OpenKind { None, Url, Email, Path };
struct OpenTarget { OpenKind kind = OpenKind::None; QString value; };  // value: 规范化后的 URL / 邮箱 / 绝对路径

// 纯函数：按 URL → email → 存在的路径 的优先级检测一段纯文本。
// - URL: 去空白后以 http:// https:// ftp:// 开头(大小写不敏感)；或裸 www. 前缀 → 补 https://
// - Email: 单 token、匹配 ^[^\s@]+@[^\s@]+\.[^\s@]+$、且不含 scheme → value 为该地址(不含 mailto:)
// - Path: QFileInfo(trimmed).exists() 为真 → value 为其绝对路径
// 都不匹配 → {None, ""}
OpenTarget detectOpenTarget(const QString& text);
```

单测覆盖：各类型正例、优先级（含 `@` 的 URL 仍判 URL）、裸 `www.`、不存在路径判 None、空串。（路径存在性用测试临时目录/已知存在的系统路径断言，或对“明显不存在”的路径断言 None。）

### 2. 打开执行（`src/ui/OpenAction.{h,cpp}` 或并入现有 ui 工具）

```cpp
// 对一条记录执行内容感知打开。返回是否真的触发了打开(供调用方决定是否消费事件)。
// - Text/RichText: detectOpenTarget(raw) → Url/Email 用 QDesktopServices::openUrl(含 mailto:)；Path 用平台 reveal
// - Files: 取第一条路径 → 平台 reveal（存在才 reveal）
// - 其它/无可打开 → 返回 false
bool openRecord(const ClipboardRecord& rec);
```

### 3. 平台「资源管理器定位」（`src/platform/RevealInExplorer.{h,cpp}`，`#ifdef`）

```cpp
namespace hopy::platform {
// 在系统文件管理器中定位路径：文件则选中，目录则打开该目录。Windows 实现；
// 其它平台回退为打开所在目录(QDesktopServices)。路径不存在时不做任何事、返回 false。
bool revealInExplorer(const QString& path);
}
```
Windows：`QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(path)})`（目录则 `explorer <dir>`）。mac/Linux：`QDesktopServices::openUrl(QUrl::fromLocalFile(dirOf(path)))`。

### 4. 触发接线

> **为什么鼠标不走 `editorEvent`：** `QStyledItemDelegate::editorEvent` 对**左键**可靠（现有动作图标点击即靠它），但视图对右/中键是否转发给 delegate 不确定。因此鼠标绑定（右/中键）走 MainWindow **已存在**的 `eventFilter`（`list_->viewport()` 上已 `installEventFilter`），更可靠且不必给 delegate 加 setter。左键的动作图标点击仍留在 `editorEvent`。

- **鼠标**（`MainWindow::eventFilter`，`obj == list_->viewport()`）：在现有 `Leave` 处理旁，追加 `MouseButtonRelease` 分支：若 `me->button()` 等于配置的鼠标绑定（`right`/`middle`，`none` 则跳过），用 `list_->indexAt(me->pos())` 取索引，`model_->recordAt(row)->id` → `emit openRequested(id)`。QListView 未挂任何 `QMenu`，右键本就不会弹上下文菜单，无需额外抑制。
- **键盘**（`MainWindow::keyPressEvent`）：命令模式下，若按下的键等于配置的 `openKey`，对 `currentId()` `emit openRequested(id)`。
- MainWindow 持有 `openKey_`（`int` 键码，0=禁用）与 `openMouseButton_`（`Qt::MouseButton`，`NoButton`=禁用），由 `setSettings` 从 `AppSettings` 同步。鼠标与键盘触发都汇到同一个新信号 `openRequested(qint64 id)`。

> 打开执行放在 Application 层（能拿到 `repo_->getById(id)` 的完整记录），新增 `MainWindow::openRequested(qint64 id)` 信号，Application 连接后调用 `openRecord`。

### 5. 预览信息条（`PreviewPopup`）

在 `content_` 之上加一行信息条（`QLabel infoBar_`）。`showPreview` 时根据记录填充：
- 文本：`<类型> · <字符数> 字符 · <行数> 行 · <绝对时间> · <操作提示>`
  - 字符数按 Unicode 码点计（CJK 友好）；行数 = `count('\n')+1`
- 图片/文件：`<类型> · <绝对时间> · <操作提示>`
- 操作提示按 `detectOpenTarget`/Files 结果：`按 O / 右键 打开链接` / `…打开邮箱` / `…在资源管理器中显示`；不可打开则省略提示部分。
- 字数统计与相对/绝对时间格式化用 util 纯函数（可单测）。

### 6. 相对时间（`src/util/TimeFormat.{h,cpp}`，纯函数）

```cpp
// 相对时间(注入 now 便于测试)：<60s→"刚刚"；<60min→"N 分钟前"；<24h→"N 小时前"；
// 昨天→"昨天 HH:mm"；同年→"MM-DD"；否则→"yyyy-MM-DD"。英文侧给对应文案。
QString relativeTime(qint64 msecs, qint64 nowMsecs);
```
`RecordDelegate::paint` 的 `drawMeta` 改用 `relativeTime(ms, QDateTime::currentMSecsSinceEpoch())`。绝对时间戳移入预览信息条。

### 7. Home / End（`MainWindow::keyPressEvent`）

命令模式：`Home` → `setCurrentIndex(row 0)`；`End` → 最后一行。复用现有 `setCurrentIndex`/滚动到可见的逻辑。

### 8. 帮助面板重排（`HelpPanel`）

分三组，每组一个小标题 + 若干行，整体两列排布：
- **导航**：`/`、`↑ / ↓`、`← / → / Tab`、`Home / End`、`1 – 5`
- **操作**：`Enter`、`Shift+Enter`、`O / 右键`（打开链接·邮箱·文件）、`F`、`T`、`Delete / D`
- **窗口 · 预览**：`Esc`、`Space`、`M4 / M5`

### 9. 配置（`AppSettings` + `SettingsPanel`）

新增字段：
- `QString openKey = "O";`（键盘绑定；空串 = 禁用键盘触发）
- `QString openMouseButton = "right";`（`"right"` | `"middle"` | `"none"`）

`fromJson`/`toJson` 读写这两个字段（缺省时回退默认，保证旧配置兼容）。`SettingsPanel`「行为」区加：一个单键录制控件（openKey）+ 一个鼠标键下拉（右键/中键/关闭）。`MainWindow::setSettings` 时把二者同步给 delegate 与键盘处理。

## 错误处理

- 打开失败（URL 非法、路径消失、`openUrl` 返回 false）：`qWarning` 记录，不崩溃、不打扰用户（沉默失败或托盘无提示）。
- 检测不到可打开内容时，触发键**无动作**（预览信息条也不显示打开提示）。
- 遵循项目「IO 失败记录并优雅降级、绝不崩溃」约束。

## 测试

- **纯函数 QtTest**：
  - `detectOpenTarget`：url/email/path/none、优先级、裸 www.、不存在路径、含 `@` 的 url。
  - `relativeTime`：刚刚 / 分钟 / 小时 / 昨天 / 同年 / 跨年 各分桶（注入固定 now）。
  - 字数/行数统计：CJK 码点计数、多行行数、空串。
- **人工验证**：右键 / `O` 打开各类型；改绑定为中键后生效；预览信息条内容；相对时间显示；Home/End；帮助面板布局；改 openKey 后即时生效。

## 范围外（YAGNI）

- 右键菜单、中键删除（用户明确不做）。
- 打开前的确认弹窗、URL 安全校验、历史「最近打开」。
- 相对时间的实时刷新（每分钟重绘）——仅在面板显示/刷新时按当时 now 计算即可。

## 实现前置

- `openKey` 的单键录制：可用轻量捕获（`keyPressEvent` 取单键），或裁剪现有 `HotkeyEdit`；本计划用轻量单键录制控件，避免和主热键的和弦录制混淆。
