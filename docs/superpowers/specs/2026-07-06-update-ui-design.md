# 更新界面重做 — 设计（Update UI Redesign）

- 日期：2026-07-06
- 状态：已批准，进入实现
- 相关：`src/app/UpdateService.*`、`src/update/*`、`src/ui/TrayIcon.*`、`resources/themes/*.qss`

## 问题

当前更新流程用一串 `QMessageBox` + `QProgressDialog` 串起来：发现新版一个框、下载一个进度框、装好一个重启框、出错再一个框。缺点：

- 弹框太多，一步一个，割裂。
- 没有主题（亮/暗）、没有走 `T()` 多语言，和 app 观感不一致。
- 看不到更新说明（release notes）。
- `QProgressDialog` 程序化关闭会误触 `canceled()`，得靠 `dlg->disconnect()` 打补丁。

## 目标

- **单一对话框**承载整个更新流程，状态在框内切换，不再连续弹框。
- **固定尺寸 400 × 550**，与主窗口 (`MainWindow` `resize(400,550)`) 一致，竖版；**永不缩放**。
- 进入更新流程后**上半部（版本 + 更新内容滚动区）常驻**，只有底部一条操作区随状态切换 —— 下载时更新说明仍可见。
- 沿用 app 的**无边框圆角 + 亮/暗主题 + 中英双语**。
- **静默检查**只上小红点（托盘图标 + 设置齿轮），不自动弹框。

## 非目标

- 不做增量/差分更新、不做多平台（Windows-only，与现状一致）。
- 不做下载限速/断点续传/速度显示（后续可加）。

## 组件

### `UpdateDialog`（新增 `src/ui/UpdateDialog.h/.cpp`）

`QDialog`，无边框 + 圆角，复刻 `MainWindow` 的外观：`FramelessWindowHint`、`WA_TranslucentBackground`、内置 `#Root`（`WA_StyledBackground` + QSS `border-radius`）+ 16px 阴影留白 + `startSystemMove()` 拖动。父窗口为 `MainWindow`，非模态、置顶、每次开流程**新建实例**（自然拿到当前主题与语言，无需实时重译）。

结构：

```
Root(圆角)
 ├─ Header: 标题「检查更新」+ ✕(关闭)
 └─ Body = QStackedWidget
      ├─ page FLOW   （有版本信息的四态共用）
      │    版本块：  「发现新版本」/ 当前 vX → vY · 相对时间
      │    「更新内容」标签
      │    notes：   QTextBrowser（#UpdateNotes，flex 占满，openExternalLinks）
      │    actionStack = QStackedWidget（只有这一条随态切换）：
      │        ├─ 按钮行：  [查看发布页]        [稍后][立即更新]
      │        ├─ 下载行：  进度条 + 「下载中… 6.3/10.9 MB · 58%」[取消]
      │        ├─ 就绪行：  ✓ 已下载并校验通过   [稍后重启][立即重启]
      │        └─ 错误行：  ✗ 原因              [打开下载页][重试]
      └─ page SIMPLE （无版本信息时居中显示）
           居中：图标 + 标题 + 副标题 + 可选按钮
           用于：检查中(转圈) / 已最新(✓) / 检查失败·限流(✗ + 重试/打开下载页)
```

两个 page 都填满同一块固定 Body → 尺寸恒定。FLOW 内版本块 + notes 常驻，只有 `actionStack` 换页 → 下载时更新说明仍在。

公开 API（由 `UpdateService` 驱动）：

```cpp
void showChecking();
void showUpToDate(const QString& currentVersion);
void showAvailable(const QString& current, const ReleaseInfo& info, const QString& publishedRelative);
void setNotes(const QString& html);      // 异步填更新说明
void setNotesUnavailable();              // 拉取失败 → 占位 +「查看发布页」
void showDownloading(qint64 received, qint64 total);
void showVerifying();                    // 进度尾段「校验中…」
void showReady();
void showCheckError(const QString& title, const QString& detail, bool offerReleasesPage);
void showDownloadError(const QString& detail);
signals:
  void updateNow(); void cancelRequested(); void restartNow();
  void retry(); void openReleasesPage();   // 关闭走标准 QDialog::reject
```

### `UpdateService`（改写 `src/app/UpdateService.*`）

去掉全部 `QMessageBox` / `QProgressDialog`，改为持有并驱动一个 `UpdateDialog* dlg_`（每次流程新建）。保留节流（24h 静默检查戳 `dataDir()/last_update_check`）、`busy_`、`manual_`。新增 `enum Phase { Checking, Downloading }` 供「重试」判定。

- `checkManually()`：新建 `dlg_`→`showChecking()`→`show()`；清小红点；`checker_->check()`。
- `checkSilently()`：节流通过后静默 `check()`，**不建对话框**。
- `updateAvailable(info)`：manual → `dlg_->showAvailable(...)` 并 `checker_->fetchReleaseNotes(tag)`；连 `updateNow`→`startDownload`。silent → 存 `pending_` + `emit updateBadge(true)`。
- `upToDate`：manual → `dlg_->showUpToDate(cur)`。
- `rateLimited(reset)`：manual → `dlg_->showCheckError(T("检查太频繁"), 到点提示, offerReleasesPage=false)`。
- `failed(why)`：manual → `dlg_->showCheckError(T("检查失败"), why, offerReleasesPage=true)`。
- `releaseNotes(tag, html)`→`dlg_->setNotes(html)`；`notesFailed`→`dlg_->setNotesUnavailable()`。
- `startDownload(info)`：`Phase=Downloading`；连 `downloader progress→showDownloading`、`ready→applyUpdate→showReady()`、`failed→showDownloadError`。
- `dlg_` 信号：`cancelRequested→downloader_->cancel()` + 关框；`restartNow→restartApp+quit`；`retry→` 按 `Phase` 重新 `check()` 或 `startDownload(pending_)`；`openReleasesPage→QDesktopServices`。

### 更新说明来源（`UpdateChecker` + `UpdateConfig`）

检查仍走 `releases/latest` 302（排除 pre-release，拿 tag）。更新说明单独拉 `releases.atom`：

- `UpdateConfig.h` 加 `releasesAtomUrl()` = `https://github.com/<o>/<r>/releases.atom`（网页端，不受 API 限流）。
- `UpdateChecker` 加 `void fetchReleaseNotes(const QString& tag)`，成功 `emit releaseNotes(tag, html)`，失败 `emit notesFailed()`。
- **纯函数** `QString notesFromAtom(const QByteArray& atomXml, const QString& tag)`：用 `QXmlStreamReader` 找 `<link href>` 以 `/tag/<tag>` 结尾的 `<entry>`，返回其 `<content>`（HTML）；找不到返回空。**单测覆盖**（`tests/test_update_notes.cpp`）。

仅在「确有更新且用户在看对话框」时才拉 atom → 额外请求最小。

### 小红点（`TrayIcon` + `MainWindow`）

- `TrayIcon::setUpdateBadge(bool)`：在 `:/logo.ico` 右下角合成一个红点 `QPixmap` → `tray_->setIcon()`；关掉恢复原图。存基图。
- `MainWindow::setUpdateBadge(bool)`：在设置齿轮 `setBtn_` 右上角放一个小圆点子 `QLabel`，`raise()`，随按钮几何定位，pending 时显示。
- `Application` 连 `UpdateService::updateBadge(bool)` → 同时点亮/熄灭两处。
- 用户看到小红点 → 进设置点「检查更新」→ 正常手动流程（对话框：检查中→发现新版），任一结果后小红点熄灭。`pending_` 仅用于徽标/提示，手动检查一律重新请求以保正确。

### 主题 / i18n

- `resources/themes/{light,dark}.qss` 各加一小段：`#UpdateRoot`(圆角 bg)、`#UpdateNotes`(卡片 bg+边框+圆角)、`#UpdatePrimary`(主色按钮 + hover)、`QProgressBar`/`::chunk`(轨道 + 主色块)、标题/副标题色。
- 所有文案走 `T()`；新增词条加入 `I18n.cpp` 的 `zh()` 表（发现新版本 / 当前 / 更新内容 / 正在检查更新… / 已是最新版本 / 正在下载 / 校验中… / 已下载并校验通过 / 立即更新 / 稍后 / 立即重启 / 稍后重启 / 取消 / 重试 / 打开下载页 / 查看发布页 / 检查太频繁 / 下载失败 等）。

## 错误处理

- 检查限流 → SIMPLE 错误态，「检查太频繁，请在 HH:mm 后重试」+ 重试（不提供下载页，纯瞬时）。
- 检查失败（网络/解析）→ SIMPLE 错误态 + 重试 + 打开下载页。
- 下载/校验失败 → FLOW 错误行（更新说明仍在上方）+ 重试（重新下载）+ 打开下载页。
- 安装失败（`applyUpdate != Ok`）→ 复用下载错误行，提示打开下载页手动更新。
- 更新说明拉取失败 → 不阻断流程，notes 区显示占位 +「查看发布页」。

## 测试

- 保留 `tests/test_update_parse.cpp`（`releaseFromRedirect`）。
- 新增 `tests/test_update_notes.cpp`（`notesFromAtom`）：匹配 tag 取 content、多 entry 取对的那条、无匹配返回空、pre-release 首条不误取。
- 纯函数用普通字符串字面量（不用 `R"()"`，moc 会误解析 `//`）。

## 涉及文件

新增：`src/ui/UpdateDialog.h/.cpp`、`tests/test_update_notes.cpp`。
修改：`src/app/UpdateService.h/.cpp`、`src/update/UpdateChecker.h/.cpp`、`src/update/UpdateConfig.h`、`src/update/UpdateDownloader.h/.cpp`（加 `cancel()`）、`src/ui/TrayIcon.h/.cpp`、`src/ui/MainWindow.h/.cpp`、`src/app/Application.cpp`、`resources/themes/{light,dark}.qss`、`src/util/I18n.cpp`、`CMakeLists.txt`（加源文件与测试；**不提交本地临时的 version 0.1.0**）。

## 分步实现顺序

1. `notesFromAtom` 纯函数 + 单测（红→绿）。
2. `UpdateChecker::fetchReleaseNotes` + `UpdateConfig::releasesAtomUrl` + `UpdateDownloader::cancel`。
3. `UpdateDialog`（结构 + 状态 + 信号；先不接线，手动塞假数据自检）。
4. QSS（亮/暗）+ i18n 词条。
5. 改写 `UpdateService` 驱动 `UpdateDialog`。
6. 小红点：`TrayIcon`/`MainWindow`/`Application` 接线。
7. 编译（静态单文件）、跑测试、本地跑一遍完整流程（本地保持 0.1.0 以命中线上 v0.2.0）。
