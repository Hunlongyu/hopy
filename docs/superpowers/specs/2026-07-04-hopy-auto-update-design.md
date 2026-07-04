# hopy 自动更新（检查更新）设计文档

> 状态：已通过 brainstorming 评审，待写实现计划
> 日期：2026-07-04

## 目标

为 hopy 实现完整的**联网检查更新 + 全自动更新（下载 → 校验 → 自替换 → 重启）**功能，替换当前设置面板里那个「检查更新」死按钮。同时修正硬编码版本号与错误的 GitHub 链接。

仓库：`https://github.com/Hunlongyu/hopy`

## 决策摘要（brainstorming 结论）

| 议题 | 决定 |
|---|---|
| 更新深度 | 全自动更新：下载 + 校验 + 自替换正在运行的 exe + 重启 |
| 平台范围 | 先实现 **Windows**，把「下载/校验/替换」抽象成平台接口，mac/Linux 留空实现 |
| 检查时机 | 启动时静默自动检查 + 设置面板手动按钮 |
| 完整性校验 | **SHA-256 校验和**（release 附 checksums 文件，下载后比对） |
| 自替换机制 | **方案 A：重命名自身 + 重启**（无辅助进程/脚本，契合单文件目标） |
| 结果交互 | 对话框（非设置面板内联） |
| 失败降级 | 任一步失败统一降级为「打开 Releases 页」，绝不崩溃 |

## 现状（代码事实）

- **版本号硬编码**两处：`src/ui/panel/AboutPanel.cpp`（`"Version 0.1.0"`）、`src/ui/panel/SettingsPanel.cpp`（About 卡片）。`src/util/BuildInfo.h` 仅是占位。
- **GitHub 链接**曾硬编码为 `https://github.com`，本次已改为 `https://github.com/Hunlongyu/hopy`（`SettingsPanel.cpp`）。
- **「检查更新」是死按钮**：`SettingsPanel::checkUpdateRequested` → `MainWindow::updateRequested`（信号），app 层无任何接收者，点击无反应。
- **`Qt6::Network` 已在 CMake 依赖中**（`find_package` + `target_link_libraries` 均已包含），无需新增模块，但需确认 TLS 后端。
- `project(hopy LANGUAGES CXX)` 尚未设 `VERSION`。

## 架构

沿用现有 `hopy::` 分层。新增 `update/` 目录 + 一个平台接口实现。

```
版本号来源:  CMake project(VERSION) → 生成 Version.h → HOPY_VERSION_STRING
                                                      │
SettingsPanel「检查更新」┐                            │
启动延迟触发           ├──► app/UpdateService ──► update/UpdateChecker (通用)
                        │         │                    └─ GET GitHub releases/latest, 比对版本
                        │         ├──────────────► update/UpdateDownloader (通用)
                        │         │                    └─ 下载 exe + checksums, SHA-256 校验
                        │         └──────────────► platform/IUpdateInstaller
                        │                              ├─ WindowsUpdateInstaller: 重命名自身+重启
                        │                              └─ mac/Linux: 返回 NotSupported
                        └──► 失败任一步 ──► 降级: 打开 Releases 页
```

### 组件职责

**1. `Version`（`src/util/Version.{h,cpp}`）**
- CMake `project(hopy VERSION 0.1.0)` → `configure_file` 生成 `Version.h`，暴露 `HOPY_VERSION_STRING`（如 `"0.1.0"`）。
- 纯函数 `int compareVersions(const QString& a, const QString& b)`：解析 `major.minor.patch`（容忍前缀 `v`），返回 -1/0/1。忽略非数字后缀。
- 替换 `AboutPanel.cpp` / `SettingsPanel.cpp` 两处硬编码，改用 `HOPY_VERSION_STRING`。

**2. `UpdateChecker`（`src/update/UpdateChecker.{h,cpp}`，通用）**
- 依赖注入 `QNetworkAccessManager*`（便于测试与复用）。
- `void check()`：GET `https://api.github.com/repos/Hunlongyu/hopy/releases/latest`，Header 带 `Accept: application/vnd.github+json` 与 `User-Agent: hopy`。
- 解析 JSON → `ReleaseInfo{ tagName, htmlUrl, exeAsset{name,url,size}, checksumAsset{name,url} }`。
- 与 `HOPY_VERSION_STRING` 比对：
  - `signals: void upToDate();`
  - `void updateAvailable(ReleaseInfo);`
  - `void failed(QString reason);`
- JSON 解析逻辑单独拆成纯函数 `parseLatestRelease(QByteArray)` 便于单测。

**3. `UpdateDownloader`（`src/update/UpdateDownloader.{h,cpp}`，通用）**
- `void download(ReleaseInfo)`：
  1. 下载 checksums 文件（文本），解析出目标 exe 的 SHA-256。
  2. 下载 exe 到 `QStandardPaths::TempLocation`/临时文件，边下边发 `progress(qint64 received, qint64 total)`。
  3. 用 `QCryptographicHash::Sha256` 算下载文件哈希，与期望值比对。
  4. `signals: void progress(qint64,qint64); void ready(QString localPath); void failed(QString);`
- SHA-256 解析（从 `checksums.txt` 文本按文件名取哈希）拆成纯函数便于单测。

**4. `IUpdateInstaller`（`src/platform/UpdateInstaller.h` 接口）+ Windows 实现**
- 接口：`enum class InstallResult { Ok, NotSupported, PermissionDenied, Failed };`
  `InstallResult apply(const QString& newExePath, QString* errorOut);`
  `void restart();`（启动新 exe 并请求退出）
- **Windows 实现（方案 A）**：
  1. 定位当前 exe：`QCoreApplication::applicationFilePath()`。
  2. 把当前 `hopy.exe` 重命名为同目录 `hopy_old.exe`（Windows 允许对运行中 exe 重命名；若 `hopy_old.exe` 已存在先删）。
  3. 把已校验的新 exe 拷贝/移动到原 `hopy.exe` 路径。
  4. `restart()`：`QProcess::startDetached(newHopyExe)` 后 `qApp->quit()`。
  5. 任一步失败（尤其重命名/拷贝因权限失败）→ 回滚（把 `hopy_old.exe` 改回）并返回 `PermissionDenied`/`Failed`。
- **启动清理**：`Application` 启动早期删除同目录残留的 `hopy_old.exe`（更新成功后的旧版本）。
- **mac/Linux 实现**：返回 `NotSupported`（由 UpdateService 降级为打开下载页）。

**5. `UpdateService`（`src/app/UpdateService.{h,cpp}`，编排）**
- 由 `Application` 持有，接 `MainWindow::updateRequested`（手动）与启动延迟定时器（自动）。
- 编排 Checker → 对话框确认 → Downloader（进度对话框）→ Installer → 重启对话框。
- 区分**手动**与**自动**触发：
  - 自动：`upToDate` / `failed` 静默（不打扰）；仅 `updateAvailable` 弹「发现新版本」对话框。
  - 手动：两种结果都反馈（`upToDate` → 「已是最新版本」提示）。
- 任一步 `failed` 或 `NotSupported`/`PermissionDenied` → 弹「无法自动更新，前往下载页？」→ `QDesktopServices::openUrl(releases 页)`。

### 交互流程

1. **启动**：延迟 ~5s（不阻塞启动）静默 `check()`。仅有新版才弹小对话框：「发现新版本 vX.Y.Z」+「现在更新」/「稍后」。
2. **手动**：设置面板点「检查更新」→ 无新版提示「已是最新」；有新版走同一对话框。
3. 确认更新 → 进度对话框（下载 + 校验，可取消）→ 成功后「更新完成，重启生效」+「立即重启」/「稍后」。

## 错误处理与降级

- **统一降级**：检查/下载/校验/替换任一步失败，弹提示并提供「打开 Releases 页」按钮 → `https://github.com/Hunlongyu/hopy/releases`，绝不崩溃。
- **TLS 后端（最大风险，需实现前先验证）**：GitHub 全 HTTPS。静态 Qt 若无 TLS 后端则所有请求失败。
  - 首选 Windows 原生 **Schannel**（无需额外 DLL）。需在 CMake `qt_import_plugins` 里确认导入 TLS 后端插件（`INCLUDE_BY_TYPE tls Qt6::QSchannelBackendPlugin`，具体目标名以该静态 Qt 包为准）。
  - 运行时用 `QSslSocket::supportsSsl()` 探测；不支持时直接降级打开下载页，并 `qWarning` 记录。
- **权限不足**（装在 `Program Files` 等）：自替换失败 → 降级打开下载页（本设计不引入 UAC 提权）。
- **网络超时 / API 限流**（GitHub 未认证 60 次/小时，足够）：归入 `failed` 降级路径。
- 所有 IO/网络错误 `qWarning` 记录，遵循项目「绝不崩溃」约束。

## 测试

沿用项目已有的 QtTest。

- **单元测试（纯函数）**：
  - `compareVersions`：`v0.2.0 > 0.1.0`、相等、patch 差异、`v` 前缀容忍、非法输入。
  - `parseLatestRelease(json)`：用固定的 GitHub API 响应 fixture，断言 tag/assets 解析正确；缺失 asset 的容错。
  - checksums 解析：从 `checksums.txt` 文本正确取出指定文件的 SHA-256。
  - SHA-256 校验：已知内容 → 已知哈希，匹配/不匹配。
- **网络 + 自替换**：保持薄接口，人工验证（真实点击检查、模拟一个更高版本 release 走完整流程）。

## 范围外（YAGNI）

- 数字签名验证、增量/差分更新、多语言更新日志渲染。
- mac/Linux 的实际自替换（接口留空，后续单独计划）。
- UAC 提权安装。

## 实现前置检查清单

1. 确认该静态 Qt 6.11.1 包是否含 Schannel（或 OpenSSL）TLS 后端；在 CMake `qt_import_plugins` 补 `tls` 后端导入。
2. `project(hopy ... VERSION 0.1.0)` + `configure_file` 生成 `Version.h`。
