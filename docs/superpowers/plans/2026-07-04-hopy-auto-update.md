# hopy 自动更新 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 hopy 实现联网检查更新 + 全自动更新（下载 → SHA-256 校验 → 自替换正在运行的 exe → 重启），Windows 优先并预留跨平台接口。

**Architecture:** 新增 `src/update/`（通用的检查器/下载器 + 可单测的纯解析函数）与 `platform/UpdateInstaller`（单文件内 `#ifdef` 分平台，Windows 走「重命名自身 + 重启」，mac/Linux 返回 `NotSupported`）。`app/UpdateService` 编排全流程并接到设置面板的手动按钮与启动时的静默定时器。任一步失败统一降级为打开 GitHub Releases 页，绝不崩溃。

**Tech Stack:** Qt 6.8+（Core/Gui/Widgets/Network/Test 已在依赖中）、C++20、`QNetworkAccessManager`、`QCryptographicHash::Sha256`、`QProcess`、QtTest。

## Global Constraints

- **Qt 版本下限 6.8；C++20**（`CMAKE_CXX_STANDARD 20`）。
- **命名空间**：所有源码置于 `hopy::`。
- **错误处理**：网络/文件 IO 失败必须 `qWarning` 记录并优雅降级，**绝不崩溃**。
- **单文件静态构建**：不得引入新的运行时 DLL 依赖；新用到的 Qt 静态插件必须经 `qt_import_plugins` 导入（本计划涉及 TLS 后端）。
- **UI 文案**：英文常量走 `T("...")`，中文在 `src/util/I18n.cpp` 的 `zh()` 哈希表补对应项。
- **平台差异**：写在单个 `.cpp` 内用 `#if defined(Q_OS_WIN)` 分支（沿用 `Autostart.cpp` 模式），不建 `_win.cpp` 独立文件（源码 `GLOB_RECURSE` 会把它编到所有平台）。
- **仓库**：`https://github.com/Hunlongyu/hopy`（owner `Hunlongyu`，repo `hopy`）。
- **测试**：纯函数用 QtTest，经 `tests/CMakeLists.txt` 的 `hopy_add_test(<name>)` 注册；测试文件尾部 `QTEST_APPLESS_MAIN(<Class>)` + `#include "<name>.moc"`。

**构建/测试前提**：仓库已按 `docs`/构建记忆配置好 `build/` 目录（静态 Qt 前缀、MSVC 2026、Release）。下文命令假定 `build/` 已存在；如未配置，先按构建记忆 configure 一次。

---

### Task 1: 版本号单一来源（CMake VERSION + 生成头 + compareVersions）

消除硬编码的 `"0.1.0"`，让版本号来自 CMake `project(VERSION)`，并提供可单测的 semver 比较函数。

**Files:**
- Modify: `CMakeLists.txt`（`project()` 加 VERSION；`configure_file` 生成版本头；加 include 目录）
- Create: `src/util/BuildVersion.h.in`
- Create: `src/util/Version.h`
- Create: `src/util/Version.cpp`
- Test: `tests/test_version.cpp`
- Modify: `tests/CMakeLists.txt`（注册 `test_version`）
- Modify: `src/ui/panel/AboutPanel.cpp`、`src/ui/panel/SettingsPanel.cpp`、`src/util/I18n.cpp`

**Interfaces:**
- Produces:
  - `hopy::currentVersion() -> QString`（如 `"0.1.0"`，来自 `HOPY_VERSION_STRING`）
  - `hopy::compareVersions(const QString& a, const QString& b) -> int`（a<b 返回 -1，相等 0，a>b 返回 1；容忍前缀 `v`，缺省段按 0，忽略 `-` 之后的后缀）

- [ ] **Step 1: 写失败测试** `tests/test_version.cpp`

```cpp
#include <QtTest>
#include "util/Version.h"
using namespace hopy;

class TestVersion : public QObject {
    Q_OBJECT
private slots:
    void equalVersions() {
        QCOMPARE(compareVersions("0.1.0", "0.1.0"), 0);
        QCOMPARE(compareVersions("v0.1.0", "0.1.0"), 0);   // 'v' 前缀被容忍
    }
    void ordersByMajorMinorPatch() {
        QCOMPARE(compareVersions("0.1.0", "0.2.0"), -1);
        QCOMPARE(compareVersions("0.2.0", "0.1.9"), 1);
        QCOMPARE(compareVersions("1.0.0", "0.9.9"), 1);
        QCOMPARE(compareVersions("0.1.1", "0.1.0"), 1);
    }
    void missingSegmentsAreZero() {
        QCOMPARE(compareVersions("1", "1.0.0"), 0);
        QCOMPARE(compareVersions("1.2", "1.2.0"), 0);
    }
    void ignoresPreReleaseSuffix() {
        QCOMPARE(compareVersions("v1.2.3-beta", "1.2.3"), 0);
    }
    void currentIsNonEmpty() {
        QVERIFY(!currentVersion().isEmpty());
    }
};
QTEST_APPLESS_MAIN(TestVersion)
#include "test_version.moc"
```

- [ ] **Step 2: 注册测试并确认它编译失败**

在 `tests/CMakeLists.txt` 末尾 `hopy_add_test(...)` 列表里加一行：

```cmake
hopy_add_test(test_version)
```

Run: `cmake --build build --target test_version`
Expected: 编译失败（找不到 `util/Version.h`）。

- [ ] **Step 3: 创建生成头模板** `src/util/BuildVersion.h.in`

```cpp
#pragma once
// Generated from CMake project(VERSION ...). Do not edit the generated copy.
#define HOPY_VERSION_STRING "@PROJECT_VERSION@"
```

- [ ] **Step 4: 接线 CMake**（`CMakeLists.txt`）

把顶部：
```cmake
project(hopy LANGUAGES CXX)
```
改为：
```cmake
project(hopy VERSION 0.1.0 LANGUAGES CXX)
```

在 `add_library(hopy_lib STATIC ...)` 之后、`target_link_libraries(hopy_lib ...)` 之前插入：
```cmake
# Single-source the app version: generate a header from project(VERSION ...).
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/BuildVersion.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/BuildVersion.h @ONLY)
target_include_directories(hopy_lib PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated)
```

- [ ] **Step 5: 声明** `src/util/Version.h`

```cpp
#pragma once
#include <QString>

namespace hopy {

// The running app's version, e.g. "0.1.0" (sourced from CMake project(VERSION)).
QString currentVersion();

// Compare two dotted versions. Tolerates a leading 'v', treats missing
// segments as 0, ignores any '-suffix'. Returns -1 if a<b, 0 if equal, 1 if a>b.
int compareVersions(const QString& a, const QString& b);

} // namespace hopy
```

- [ ] **Step 6: 实现** `src/util/Version.cpp`

```cpp
#include "util/Version.h"
#include "BuildVersion.h"
#include <array>

namespace hopy {

QString currentVersion() {
    return QStringLiteral(HOPY_VERSION_STRING);
}

namespace {
// Parse "v1.2.3-beta" → {1,2,3}, missing segments default to 0.
std::array<int, 3> parts(const QString& in) {
    QString s = in.trimmed();
    if (s.startsWith('v') || s.startsWith('V')) s.remove(0, 1);
    const int dash = s.indexOf('-');
    if (dash >= 0) s = s.left(dash);
    std::array<int, 3> out{0, 0, 0};
    const QStringList segs = s.split('.');
    for (int i = 0; i < 3 && i < segs.size(); ++i) out[i] = segs[i].toInt();
    return out;
}
} // namespace

int compareVersions(const QString& a, const QString& b) {
    const auto pa = parts(a);
    const auto pb = parts(b);
    for (int i = 0; i < 3; ++i) {
        if (pa[i] < pb[i]) return -1;
        if (pa[i] > pb[i]) return 1;
    }
    return 0;
}

} // namespace hopy
```

- [ ] **Step 7: 运行测试确认通过**

Run: `cmake --build build --target test_version && ctest --test-dir build -R test_version --output-on-failure`
Expected: PASS。

- [ ] **Step 8: 用动态版本替换硬编码**

`src/ui/panel/AboutPanel.cpp` —— 顶部加 `#include "util/Version.h"`，把：
```cpp
    auto* ver = new QLabel(T("Version 0.1.0"));
```
改为：
```cpp
    auto* ver = new QLabel(T("Version %1").arg(currentVersion()));
```

`src/ui/panel/SettingsPanel.cpp` —— 顶部加 `#include "util/Version.h"`，把：
```cpp
    auto* nameVer = new QLabel(QStringLiteral("Hopy · ") + T("Version 0.1.0"));
```
改为：
```cpp
    auto* nameVer = new QLabel(QStringLiteral("Hopy · ") + T("Version %1").arg(currentVersion()));
```

`src/util/I18n.cpp` —— 把：
```cpp
        {"Version 0.1.0",   QStringLiteral("版本 0.1.0")},
```
改为：
```cpp
        {"Version %1",      QStringLiteral("版本 %1")},
```

- [ ] **Step 9: 构建 app 确认无回归**

Run: `cmake --build build --target hopy`
Expected: 链接成功。

- [ ] **Step 10: 提交**

```bash
git add CMakeLists.txt src/util/BuildVersion.h.in src/util/Version.h src/util/Version.cpp \
        tests/test_version.cpp tests/CMakeLists.txt \
        src/ui/panel/AboutPanel.cpp src/ui/panel/SettingsPanel.cpp src/util/I18n.cpp
git commit -m "feat(update): single-source app version from CMake project(VERSION)"
```

---

### Task 2: 更新配置常量 + GitHub 链接修正

集中放更新相关的 URL 常量，并把设置面板里指错的 GitHub 按钮改到本仓库。

**Files:**
- Create: `src/update/UpdateConfig.h`
- Modify: `src/ui/panel/SettingsPanel.cpp`

**Interfaces:**
- Produces（`namespace hopy::update`，均为 `inline constexpr` / `inline` 常量函数）：
  - `kRepoOwner = "Hunlongyu"`, `kRepoName = "hopy"`
  - `latestReleaseApiUrl() -> QString` = `https://api.github.com/repos/Hunlongyu/hopy/releases/latest`
  - `releasesPageUrl() -> QString` = `https://github.com/Hunlongyu/hopy/releases`

- [ ] **Step 1: 创建常量头** `src/update/UpdateConfig.h`

```cpp
#pragma once
#include <QString>

namespace hopy::update {

inline constexpr const char* kRepoOwner = "Hunlongyu";
inline constexpr const char* kRepoName  = "hopy";

inline QString latestReleaseApiUrl() {
    return QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
        .arg(QLatin1String(kRepoOwner), QLatin1String(kRepoName));
}

inline QString releasesPageUrl() {
    return QStringLiteral("https://github.com/%1/%2/releases")
        .arg(QLatin1String(kRepoOwner), QLatin1String(kRepoName));
}

} // namespace hopy::update
```

- [ ] **Step 2: 修正 GitHub 按钮链接**（`src/ui/panel/SettingsPanel.cpp`）

顶部加 `#include "update/UpdateConfig.h"`。把：
```cpp
    connect(gh, &QToolButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com"))); });
```
改为：
```cpp
    connect(gh, &QToolButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(update::releasesPageUrl())); });
```

- [ ] **Step 3: 构建确认**

Run: `cmake --build build --target hopy`
Expected: 成功。

- [ ] **Step 4: 提交**

```bash
git add src/update/UpdateConfig.h src/ui/panel/SettingsPanel.cpp
git commit -m "feat(update): repo URL constants; fix GitHub link to Hunlongyu/hopy"
```

---

### Task 3: UpdateChecker —— 解析 latest release + 版本比对

**Files:**
- Create: `src/update/ReleaseInfo.h`
- Create: `src/update/UpdateChecker.h`
- Create: `src/update/UpdateChecker.cpp`
- Test: `tests/test_update_parse.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `hopy::update::latestReleaseApiUrl()`（Task 2）、`hopy::compareVersions/currentVersion`（Task 1）
- Produces:
  - `struct ReleaseAsset { QString name; QString url; qint64 size = 0; };`
  - `struct ReleaseInfo { QString tagName; QString htmlUrl; ReleaseAsset exeAsset; ReleaseAsset checksumAsset; bool hasExe() const; bool hasChecksum() const; };`
  - 纯函数 `parseLatestRelease(const QByteArray& json, ReleaseInfo* out) -> bool`（JSON 有效且含 `tag_name` 时返回 true）
  - `class UpdateChecker : QObject`，`explicit UpdateChecker(QNetworkAccessManager* nam, QObject* parent=nullptr)`，`void check()`；signals：`upToDate()`、`updateAvailable(hopy::ReleaseInfo)`、`failed(QString reason)`

- [ ] **Step 1: 写失败测试** `tests/test_update_parse.cpp`

```cpp
#include <QtTest>
#include "update/UpdateChecker.h"
using namespace hopy;

static QByteArray sampleJson() {
    return R"({
      "tag_name": "v0.2.0",
      "html_url": "https://github.com/Hunlongyu/hopy/releases/tag/v0.2.0",
      "assets": [
        {"name": "hopy.exe", "browser_download_url": "https://example/hopy.exe", "size": 1234},
        {"name": "checksums.txt", "browser_download_url": "https://example/checksums.txt", "size": 80}
      ]
    })";
}

class TestUpdateParse : public QObject {
    Q_OBJECT
private slots:
    void parsesTagAndAssets() {
        ReleaseInfo info;
        QVERIFY(parseLatestRelease(sampleJson(), &info));
        QCOMPARE(info.tagName, QStringLiteral("v0.2.0"));
        QVERIFY(info.hasExe());
        QVERIFY(info.hasChecksum());
        QCOMPARE(info.exeAsset.name, QStringLiteral("hopy.exe"));
        QCOMPARE(info.exeAsset.url, QStringLiteral("https://example/hopy.exe"));
        QCOMPARE(info.exeAsset.size, qint64(1234));
        QCOMPARE(info.checksumAsset.name, QStringLiteral("checksums.txt"));
    }
    void rejectsInvalidJson() {
        ReleaseInfo info;
        QVERIFY(!parseLatestRelease("not json", &info));
    }
    void missingAssetsStillParsesTag() {
        ReleaseInfo info;
        QVERIFY(parseLatestRelease(R"({"tag_name":"v0.3.0","assets":[]})", &info));
        QCOMPARE(info.tagName, QStringLiteral("v0.3.0"));
        QVERIFY(!info.hasExe());
        QVERIFY(!info.hasChecksum());
    }
};
QTEST_APPLESS_MAIN(TestUpdateParse)
#include "test_update_parse.moc"
```

- [ ] **Step 2: 注册并确认失败**

`tests/CMakeLists.txt` 加：
```cmake
hopy_add_test(test_update_parse)
```
Run: `cmake --build build --target test_update_parse`
Expected: 编译失败（缺 `update/UpdateChecker.h`）。

- [ ] **Step 3: 数据结构** `src/update/ReleaseInfo.h`

```cpp
#pragma once
#include <QString>

namespace hopy {

struct ReleaseAsset {
    QString name;
    QString url;
    qint64  size = 0;
};

struct ReleaseInfo {
    QString      tagName;
    QString      htmlUrl;
    ReleaseAsset exeAsset;
    ReleaseAsset checksumAsset;

    bool hasExe() const { return !exeAsset.url.isEmpty(); }
    bool hasChecksum() const { return !checksumAsset.url.isEmpty(); }
};

} // namespace hopy
```

- [ ] **Step 4: 声明** `src/update/UpdateChecker.h`

```cpp
#pragma once
#include "update/ReleaseInfo.h"
#include <QObject>
#include <QByteArray>

class QNetworkAccessManager;

namespace hopy {

// Pure: parse a GitHub "releases/latest" JSON payload. Picks the first asset
// whose name ends with ".exe" as the installer, and the first ending in
// ".sha256" or named "checksums.txt" (case-insensitive) as the checksum file.
// Returns false only when the JSON is invalid or has no tag_name.
bool parseLatestRelease(const QByteArray& json, ReleaseInfo* out);

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QNetworkAccessManager* nam, QObject* parent = nullptr);
    void check();

signals:
    void upToDate();
    void updateAvailable(hopy::ReleaseInfo info);
    void failed(QString reason);

private:
    QNetworkAccessManager* nam_;
};

} // namespace hopy
```

- [ ] **Step 5: 实现** `src/update/UpdateChecker.cpp`

```cpp
#include "update/UpdateChecker.h"
#include "update/UpdateConfig.h"
#include "util/Version.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

namespace hopy {

bool parseLatestRelease(const QByteArray& json, ReleaseInfo* out) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonObject root = doc.object();
    const QString tag = root.value("tag_name").toString();
    if (tag.isEmpty()) return false;

    out->tagName = tag;
    out->htmlUrl = root.value("html_url").toString();
    out->exeAsset = {};
    out->checksumAsset = {};

    for (const QJsonValue v : root.value("assets").toArray()) {
        const QJsonObject a = v.toObject();
        const QString name = a.value("name").toString();
        const QString url  = a.value("browser_download_url").toString();
        const qint64  size = static_cast<qint64>(a.value("size").toDouble());
        const QString lower = name.toLower();
        if (out->exeAsset.url.isEmpty() && lower.endsWith(".exe"))
            out->exeAsset = {name, url, size};
        else if (out->checksumAsset.url.isEmpty() &&
                 (lower.endsWith(".sha256") || lower == "checksums.txt"))
            out->checksumAsset = {name, url, size};
    }
    return true;
}

UpdateChecker::UpdateChecker(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), nam_(nam) {}

void UpdateChecker::check() {
    QNetworkRequest req{QUrl(update::latestReleaseApiUrl())};
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "hopy-updater");
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning("update check failed: %s", qPrintable(reply->errorString()));
            emit failed(reply->errorString());
            return;
        }
        ReleaseInfo info;
        if (!parseLatestRelease(reply->readAll(), &info)) {
            emit failed(QStringLiteral("could not parse release info"));
            return;
        }
        if (compareVersions(currentVersion(), info.tagName) < 0)
            emit updateAvailable(info);
        else
            emit upToDate();
    });
}

} // namespace hopy
```

- [ ] **Step 6: 运行解析测试**

Run: `cmake --build build --target test_update_parse && ctest --test-dir build -R test_update_parse --output-on-failure`
Expected: PASS。

- [ ] **Step 7: 提交**

```bash
git add src/update/ReleaseInfo.h src/update/UpdateChecker.h src/update/UpdateChecker.cpp \
        tests/test_update_parse.cpp tests/CMakeLists.txt
git commit -m "feat(update): UpdateChecker with pure release-JSON parser"
```

---

### Task 4: UpdateDownloader —— 下载 + SHA-256 校验

**Files:**
- Create: `src/update/UpdateDownloader.h`
- Create: `src/update/UpdateDownloader.cpp`
- Test: `tests/test_update_checksum.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `hopy::ReleaseInfo`（Task 3）
- Produces:
  - 纯函数 `parseChecksumFor(const QByteArray& text, const QString& fileName) -> QString`（返回小写 hex 的 SHA-256；支持两种格式：多行 `<hex>  <name>`（`sha256sum` 风格）取匹配文件名的那行；或整个文件只有一个 64 位 hex token 时直接返回。找不到返回空串）
  - `class UpdateDownloader : QObject`，`explicit UpdateDownloader(QNetworkAccessManager* nam, QObject* parent=nullptr)`，`void download(const hopy::ReleaseInfo& info)`；signals：`progress(qint64 received, qint64 total)`、`ready(QString localExePath)`、`failed(QString reason)`

- [ ] **Step 1: 写失败测试** `tests/test_update_checksum.cpp`

```cpp
#include <QtTest>
#include "update/UpdateDownloader.h"
using namespace hopy;

class TestUpdateChecksum : public QObject {
    Q_OBJECT
private slots:
    void picksHashByFileNameFromSha256sumFormat() {
        const QByteArray text =
            "aaaa1111  other.zip\n"
            "bbbb2222  hopy.exe\n";
        QCOMPARE(parseChecksumFor(text, "hopy.exe"), QStringLiteral("bbbb2222"));
    }
    void singleTokenFileIsUsedDirectlyAndLowercased() {
        // A ".sha256" file often contains just the hash on its own.
        const QByteArray text = "ABCDEF0123456789\n";
        QCOMPARE(parseChecksumFor(text, "hopy.exe"), QStringLiteral("abcdef0123456789"));
    }
    void returnsEmptyWhenNotFound() {
        QCOMPARE(parseChecksumFor("ffff9999  somethingelse.exe\n", "hopy.exe"), QString());
    }
    void isCaseInsensitiveOnName() {
        QCOMPARE(parseChecksumFor("dead  HOPY.EXE\n", "hopy.exe"), QStringLiteral("dead"));
    }
};
QTEST_APPLESS_MAIN(TestUpdateChecksum)
#include "test_update_checksum.moc"
```

- [ ] **Step 2: 注册并确认失败**

`tests/CMakeLists.txt` 加：
```cmake
hopy_add_test(test_update_checksum)
```
Run: `cmake --build build --target test_update_checksum`
Expected: 编译失败。

- [ ] **Step 3: 声明** `src/update/UpdateDownloader.h`

```cpp
#pragma once
#include "update/ReleaseInfo.h"
#include <QObject>
#include <QByteArray>
#include <QString>

class QNetworkAccessManager;

namespace hopy {

// Pure: extract the SHA-256 for `fileName` from a checksums payload.
// Supports "<hex>  <name>" lines (sha256sum style; name matched case-insensitively)
// and a single-token file (the whole content is one hash). Returns lowercase hex,
// or an empty string if not found.
QString parseChecksumFor(const QByteArray& text, const QString& fileName);

class UpdateDownloader : public QObject {
    Q_OBJECT
public:
    explicit UpdateDownloader(QNetworkAccessManager* nam, QObject* parent = nullptr);
    void download(const ReleaseInfo& info);

signals:
    void progress(qint64 received, qint64 total);
    void ready(QString localExePath);
    void failed(QString reason);

private:
    QNetworkAccessManager* nam_;
};

} // namespace hopy
```

- [ ] **Step 4: 实现** `src/update/UpdateDownloader.cpp`

```cpp
#include "update/UpdateDownloader.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUrl>

namespace hopy {

QString parseChecksumFor(const QByteArray& text, const QString& fileName) {
    const QString content = QString::fromUtf8(text).trimmed();
    // Single-token file: the whole content is one hash.
    if (!content.contains('\n') && !content.contains(' ')) return content.toLower();
    const QString wanted = fileName.toLower();
    for (const QString& raw : content.split('\n')) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        const QStringList cols = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (cols.size() < 2) continue;
        if (cols.last().toLower() == wanted) return cols.first().toLower();
    }
    return QString();
}

UpdateDownloader::UpdateDownloader(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), nam_(nam) {}

void UpdateDownloader::download(const ReleaseInfo& info) {
    if (!info.hasExe() || !info.hasChecksum()) {
        emit failed(QStringLiteral("release is missing the exe or checksum asset"));
        return;
    }
    // Step 1: fetch checksum file (small).
    QNetworkRequest sumReq{QUrl(info.checksumAsset.url)};
    sumReq.setRawHeader("User-Agent", "hopy-updater");
    QNetworkReply* sumReply = nam_->get(sumReq);
    connect(sumReply, &QNetworkReply::finished, this, [this, sumReply, info] {
        sumReply->deleteLater();
        if (sumReply->error() != QNetworkReply::NoError) {
            emit failed(sumReply->errorString());
            return;
        }
        const QString expected = parseChecksumFor(sumReply->readAll(), info.exeAsset.name);
        if (expected.isEmpty()) {
            emit failed(QStringLiteral("checksum for %1 not found").arg(info.exeAsset.name));
            return;
        }
        // Step 2: download the exe with progress.
        QNetworkRequest exeReq{QUrl(info.exeAsset.url)};
        exeReq.setRawHeader("User-Agent", "hopy-updater");
        QNetworkReply* exeReply = nam_->get(exeReq);
        connect(exeReply, &QNetworkReply::downloadProgress, this, &UpdateDownloader::progress);
        connect(exeReply, &QNetworkReply::finished, this, [this, exeReply, expected, info] {
            exeReply->deleteLater();
            if (exeReply->error() != QNetworkReply::NoError) {
                emit failed(exeReply->errorString());
                return;
            }
            const QByteArray bytes = exeReply->readAll();
            const QString got = QString::fromLatin1(
                QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
            if (got.compare(expected, Qt::CaseInsensitive) != 0) {
                qWarning("update checksum mismatch: got %s expected %s",
                         qPrintable(got), qPrintable(expected));
                emit failed(QStringLiteral("downloaded file failed checksum verification"));
                return;
            }
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            const QString path = QDir(dir).filePath(QStringLiteral("hopy-update.exe"));
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size()) {
                emit failed(QStringLiteral("could not write the downloaded update"));
                return;
            }
            f.close();
            emit ready(path);
        });
    });
}

} // namespace hopy
```

> 头文件补 `#include <QRegularExpression>` 于 `.cpp`。

- [ ] **Step 5: 运行校验测试**

Run: `cmake --build build --target test_update_checksum && ctest --test-dir build -R test_update_checksum --output-on-failure`
Expected: PASS。

- [ ] **Step 6: 提交**

```bash
git add src/update/UpdateDownloader.h src/update/UpdateDownloader.cpp \
        tests/test_update_checksum.cpp tests/CMakeLists.txt
git commit -m "feat(update): UpdateDownloader with SHA-256 verification"
```

---

### Task 5: 平台安装器 —— Windows 重命名自身 + 重启

**Files:**
- Create: `src/platform/UpdateInstaller.h`
- Create: `src/platform/UpdateInstaller.cpp`
- Test: `tests/test_update_installer.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces（`namespace hopy::platform`）：
  - `enum class InstallResult { Ok, NotSupported, PermissionDenied, Failed };`
  - 纯函数 `QString sideBySideOldPath(const QString& exePath)` —— 同目录下把文件名改成 `<base>_old.<ext>`（如 `.../hopy.exe` → `.../hopy_old.exe`）
  - `InstallResult applyUpdate(const QString& newExePath, const QString& currentExePath, QString* errorOut)` —— 把 `currentExePath` 改名为其 `sideBySideOldPath`，再把 `newExePath` 落到 `currentExePath`；失败回滚
  - `void restartApp(const QString& exePath)` —— `QProcess::startDetached`
  - `void cleanupOldBinary(const QString& currentExePath)` —— 删除残留的 `sideBySideOldPath`（启动早期调用）

- [ ] **Step 1: 写失败测试** `tests/test_update_installer.cpp`

```cpp
#include <QtTest>
#include "platform/UpdateInstaller.h"
using namespace hopy::platform;

class TestUpdateInstaller : public QObject {
    Q_OBJECT
private slots:
    void oldPathInsertsSuffixKeepingExtension() {
        QCOMPARE(sideBySideOldPath("C:/apps/hopy.exe"), QStringLiteral("C:/apps/hopy_old.exe"));
        QCOMPARE(sideBySideOldPath("/opt/hopy/hopy"),   QStringLiteral("/opt/hopy/hopy_old"));
    }
};
QTEST_APPLESS_MAIN(TestUpdateInstaller)
#include "test_update_installer.moc"
```

- [ ] **Step 2: 注册并确认失败**

`tests/CMakeLists.txt` 加：
```cmake
hopy_add_test(test_update_installer)
```
Run: `cmake --build build --target test_update_installer`
Expected: 编译失败。

- [ ] **Step 3: 声明** `src/platform/UpdateInstaller.h`

```cpp
#pragma once
#include <QString>

namespace hopy::platform {

enum class InstallResult { Ok, NotSupported, PermissionDenied, Failed };

// Pure: given ".../hopy.exe" return ".../hopy_old.exe" (extension preserved).
QString sideBySideOldPath(const QString& exePath);

// Swap the running binary for `newExePath` using the rename-self strategy.
// Windows only; other platforms return NotSupported. On failure the original
// binary is restored and *errorOut is set.
InstallResult applyUpdate(const QString& newExePath, const QString& currentExePath,
                          QString* errorOut);

// Launch `exePath` detached (call right before quitting to complete the update).
void restartApp(const QString& exePath);

// Delete a leftover "<name>_old.<ext>" next to the current binary, if present.
void cleanupOldBinary(const QString& currentExePath);

} // namespace hopy::platform
```

- [ ] **Step 4: 实现** `src/platform/UpdateInstaller.cpp`

```cpp
#include "platform/UpdateInstaller.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QProcess>

namespace hopy::platform {

QString sideBySideOldPath(const QString& exePath) {
    const QFileInfo fi(exePath);
    const QString base = fi.completeBaseName();          // "hopy"
    const QString ext  = fi.suffix();                    // "exe" or ""
    const QString name = ext.isEmpty() ? base + QStringLiteral("_old")
                                       : base + QStringLiteral("_old.") + ext;
    return fi.absolutePath() + QLatin1Char('/') + name;
}

#if defined(Q_OS_WIN)
InstallResult applyUpdate(const QString& newExePath, const QString& currentExePath,
                          QString* errorOut) {
    const QString oldPath = sideBySideOldPath(currentExePath);
    QFile::remove(oldPath);                              // clear any stale copy

    // Windows allows renaming a running exe (but not overwriting/deleting it).
    if (!QFile::rename(currentExePath, oldPath)) {
        if (errorOut) *errorOut = QStringLiteral("could not rename the running binary");
        return InstallResult::PermissionDenied;
    }
    if (!QFile::copy(newExePath, currentExePath)) {
        QFile::rename(oldPath, currentExePath);          // roll back
        if (errorOut) *errorOut = QStringLiteral("could not place the new binary");
        return InstallResult::Failed;
    }
    return InstallResult::Ok;
}
#else
InstallResult applyUpdate(const QString&, const QString&, QString* errorOut) {
    if (errorOut) *errorOut = QStringLiteral("self-update is not supported on this platform yet");
    return InstallResult::NotSupported;
}
#endif

void restartApp(const QString& exePath) {
    QProcess::startDetached(exePath, {});
}

void cleanupOldBinary(const QString& currentExePath) {
    const QString oldPath = sideBySideOldPath(currentExePath);
    if (QFile::exists(oldPath)) QFile::remove(oldPath);
}

} // namespace hopy::platform
```

- [ ] **Step 5: 运行测试**

Run: `cmake --build build --target test_update_installer && ctest --test-dir build -R test_update_installer --output-on-failure`
Expected: PASS。

- [ ] **Step 6: 启动早期清理残留旧二进制**（`src/app/Application.cpp`）

顶部加 `#include "platform/UpdateInstaller.h"` 与 `#include <QCoreApplication>`。在 `Application::start()` 的第一行 `settings_ = Settings::load();` **之前**插入：
```cpp
    platform::cleanupOldBinary(QCoreApplication::applicationFilePath());  // remove leftover *_old.exe from a prior update
```

- [ ] **Step 7: 构建 app 确认**

Run: `cmake --build build --target hopy`
Expected: 成功。

- [ ] **Step 8: 提交**

```bash
git add src/platform/UpdateInstaller.h src/platform/UpdateInstaller.cpp \
        tests/test_update_installer.cpp tests/CMakeLists.txt src/app/Application.cpp
git commit -m "feat(update): Windows rename-self installer + startup cleanup"
```

---

### Task 6: CMake 导入 TLS 后端插件

静态 Qt 下 HTTPS 需要 TLS 后端插件被显式导入，否则所有 `https://` 请求失败。

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 在 `qt_import_plugins(hopy ...)` 里加入 TLS 后端**

把：
```cmake
  qt_import_plugins(hopy
    INCLUDE Qt6::QWindowsIntegrationPlugin
    INCLUDE_BY_TYPE sqldrivers Qt6::QSQLiteDriverPlugin
    INCLUDE_BY_TYPE imageformats Qt6::QICOPlugin Qt6::QJpegPlugin)
```
改为（Windows 用原生 Schannel，无需外部 DLL）：
```cmake
  qt_import_plugins(hopy
    INCLUDE Qt6::QWindowsIntegrationPlugin
    INCLUDE_BY_TYPE sqldrivers Qt6::QSQLiteDriverPlugin
    INCLUDE_BY_TYPE imageformats Qt6::QICOPlugin Qt6::QJpegPlugin
    INCLUDE_BY_TYPE tls Qt6::QSchannelBackendPlugin)
```

- [ ] **Step 2: 构建 app**

Run: `cmake --build build --target hopy`
Expected: 链接成功。
若报找不到 `Qt6::QSchannelBackendPlugin`：说明该静态 Qt 未含 Schannel 后端。改查 `Qt6::QOpenSSLBackendPlugin` 是否存在（`ls <QtPrefix>/plugins/tls/`）；有则改用之，并在运行期确认 `QSslSocket::supportsSsl()`。二者皆无时，本功能只能走「打开下载页」降级（Task 7 已覆盖），此时跳过本步并在 spec/plan 记录该限制。

- [ ] **Step 3: 提交**

```bash
git add CMakeLists.txt
git commit -m "build(update): statically import Schannel TLS backend for HTTPS"
```

---

### Task 7: UpdateService —— 编排 + 接线 + 对话框 + 降级

把 checker / downloader / installer 串成用户可见的流程，接到设置面板手动按钮与启动定时器；任一步失败降级为打开 Releases 页。

**Files:**
- Create: `src/app/UpdateService.h`
- Create: `src/app/UpdateService.cpp`
- Modify: `src/app/Application.h`（持有 `UpdateService*`）
- Modify: `src/app/Application.cpp`（构造并接线）
- Modify: `src/util/I18n.cpp`（新增中文文案）

**Interfaces:**
- Consumes: `UpdateChecker`、`UpdateDownloader`、`platform::applyUpdate/restartApp`、`ReleaseInfo`、`update::releasesPageUrl()`、`MainWindow::updateRequested`（signal，已存在于 `src/ui/MainWindow.h`，由 `SettingsPanel::checkUpdateRequested` 转发，见 `src/ui/MainWindow.cpp:188`）
- Produces:
  - `class UpdateService : QObject`
  - `explicit UpdateService(QWidget* dialogParent, QObject* parent=nullptr)`
  - `void checkManually()`（用户点按钮：两种结果都反馈）
  - `void checkSilently()`（启动触发：仅有新版才打扰）

- [ ] **Step 1: 声明** `src/app/UpdateService.h`

```cpp
#pragma once
#include "update/ReleaseInfo.h"
#include <QObject>

class QWidget;
class QNetworkAccessManager;

namespace hopy {

class UpdateChecker;
class UpdateDownloader;

// Orchestrates check → confirm → download+verify → self-replace → restart.
// Any failure degrades to offering the GitHub Releases page. Never crashes.
class UpdateService : public QObject {
    Q_OBJECT
public:
    explicit UpdateService(QWidget* dialogParent, QObject* parent = nullptr);

    void checkManually();   // user-initiated: report both "up to date" and "available"
    void checkSilently();   // startup: only surface when an update is available

private:
    void onUpdateAvailable(const ReleaseInfo& info);
    void startDownload(const ReleaseInfo& info);
    void offerReleasesPage(const QString& reason);   // degrade path

    QWidget*               parent_;
    QNetworkAccessManager* nam_;
    UpdateChecker*         checker_;
    UpdateDownloader*      downloader_;
    bool                   manual_ = false;
    ReleaseInfo            pending_;
};

} // namespace hopy
```

- [ ] **Step 2: 实现** `src/app/UpdateService.cpp`

```cpp
#include "app/UpdateService.h"
#include "update/UpdateChecker.h"
#include "update/UpdateDownloader.h"
#include "update/UpdateConfig.h"
#include "platform/UpdateInstaller.h"
#include "util/I18n.h"
#include <QNetworkAccessManager>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QUrl>

namespace hopy {

UpdateService::UpdateService(QWidget* dialogParent, QObject* parent)
    : QObject(parent), parent_(dialogParent) {
    nam_        = new QNetworkAccessManager(this);
    checker_    = new UpdateChecker(nam_, this);
    downloader_ = new UpdateDownloader(nam_, this);

    connect(checker_, &UpdateChecker::updateAvailable, this, &UpdateService::onUpdateAvailable);
    connect(checker_, &UpdateChecker::upToDate, this, [this] {
        if (manual_) QMessageBox::information(parent_, T("Check for updates"),
                                              T("You're on the latest version."));
    });
    connect(checker_, &UpdateChecker::failed, this, [this](const QString& why) {
        if (manual_) offerReleasesPage(why);   // silent checks fail quietly
    });
}

void UpdateService::checkManually() { manual_ = true;  checker_->check(); }
void UpdateService::checkSilently() { manual_ = false; checker_->check(); }

void UpdateService::onUpdateAvailable(const ReleaseInfo& info) {
    pending_ = info;
    const auto btn = QMessageBox::question(
        parent_, T("Update available"),
        T("A new version %1 is available. Update now?").arg(info.tagName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (btn == QMessageBox::Yes) startDownload(info);
}

void UpdateService::startDownload(const ReleaseInfo& info) {
    auto* dlg = new QProgressDialog(T("Downloading update…"), T("Cancel"), 0, 100, parent_);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setAutoClose(false);
    dlg->setValue(0);

    connect(downloader_, &UpdateDownloader::progress, dlg, [dlg](qint64 rec, qint64 total) {
        if (total > 0) dlg->setMaximum(100), dlg->setValue(int(rec * 100 / total));
    });
    connect(dlg, &QProgressDialog::canceled, this, [this] {
        // Best-effort: leave the running binary untouched; nothing has been swapped yet.
        offerReleasesPage(T("Update canceled."));
    });
    connect(downloader_, &UpdateDownloader::failed, dlg, [this, dlg](const QString& why) {
        dlg->close(); dlg->deleteLater();
        offerReleasesPage(why);
    });
    connect(downloader_, &UpdateDownloader::ready, dlg, [this, dlg](const QString& localExe) {
        dlg->close(); dlg->deleteLater();
        const QString cur = QCoreApplication::applicationFilePath();
        QString err;
        const auto r = platform::applyUpdate(localExe, cur, &err);
        if (r != platform::InstallResult::Ok) { offerReleasesPage(err); return; }
        const auto restart = QMessageBox::question(
            parent_, T("Update ready"),
            T("Update installed. Restart now to apply?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (restart == QMessageBox::Yes) {
            platform::restartApp(cur);
            QCoreApplication::quit();
        }
    });

    downloader_->download(info);
}

void UpdateService::offerReleasesPage(const QString& reason) {
    qWarning("update degraded: %s", qPrintable(reason));
    const auto btn = QMessageBox::warning(
        parent_, T("Check for updates"),
        T("Couldn't update automatically. Open the downloads page?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (btn == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl(update::releasesPageUrl()));
}

} // namespace hopy
```

- [ ] **Step 3: 在 Application 头里持有 UpdateService**（`src/app/Application.h`）

在私有成员区（与 `window_`、`tray_` 等并列）加：
```cpp
    class UpdateService* updater_ = nullptr;
```
（若该文件已用前向声明风格则照此；否则在 `namespace hopy {` 内顶部加 `class UpdateService;` 前向声明后，成员写 `UpdateService* updater_ = nullptr;`）

- [ ] **Step 4: 接线**（`src/app/Application.cpp`）

顶部加：
```cpp
#include "app/UpdateService.h"
```
在 `Application::start()` 中，`window_ = new MainWindow();` 之后、`refreshWindow();`（函数末尾）之前，插入：
```cpp
    updater_ = new UpdateService(window_, this);
    connect(window_, &MainWindow::updateRequested, updater_, &UpdateService::checkManually);
    QTimer::singleShot(5000, updater_, &UpdateService::checkSilently);  // silent auto-check, non-blocking
```
（`QTimer` 已 `#include`。）

- [ ] **Step 5: 中文文案**（`src/util/I18n.cpp`）

在 `zh()` 的 map 里补：
```cpp
        {"You're on the latest version.", QStringLiteral("已是最新版本。")},
        {"Update available",              QStringLiteral("发现新版本")},
        {"A new version %1 is available. Update now?",
                                          QStringLiteral("发现新版本 %1，现在更新吗？")},
        {"Downloading update…",           QStringLiteral("正在下载更新…")},
        {"Cancel",                        QStringLiteral("取消")},
        {"Update canceled.",              QStringLiteral("已取消更新。")},
        {"Update ready",                  QStringLiteral("更新就绪")},
        {"Update installed. Restart now to apply?",
                                          QStringLiteral("更新已安装，立即重启生效吗？")},
        {"Couldn't update automatically. Open the downloads page?",
                                          QStringLiteral("无法自动更新，打开下载页吗？")},
```

- [ ] **Step 6: 构建 app**

Run: `cmake --build build --target hopy`
Expected: 链接成功。

- [ ] **Step 7: 全量测试回归**

Run: `ctest --test-dir build --output-on-failure`
Expected: 全绿。

- [ ] **Step 8: 提交**

```bash
git add src/app/UpdateService.h src/app/UpdateService.cpp \
        src/app/Application.h src/app/Application.cpp src/util/I18n.cpp
git commit -m "feat(update): UpdateService orchestration wired to settings + startup"
```

---

### Task 8: 人工验证（真实流程）

自动化测不到网络 + 自替换 + 重启，需人工走一遍。

- [ ] **Step 1: 无新版路径**：确保本地版本 = 最新 release。运行 hopy → 设置 → 点「检查更新」→ 应弹「已是最新版本」。
- [ ] **Step 2: 有新版路径（模拟）**：临时把 `CMakeLists.txt` 的 `project(hopy VERSION 0.1.0)` 降到一个**低于**当前最新 release 的版本（如 `0.0.1`），重建 → 点「检查更新」→ 应弹「发现新版本 vX.Y.Z」→「现在更新」→ 进度条 → 下载校验通过 → 「更新已安装,立即重启」→ 重启后版本更新、同目录 `hopy_old.exe` 在下次启动被清除。验证后把版本改回。
- [ ] **Step 3: 降级路径**：断网后点「检查更新」→ 应弹「无法自动更新,打开下载页?」→ 点是打开 `github.com/Hunlongyu/hopy/releases`。
- [ ] **Step 4: TLS 确认**：若联网检查始终失败,在检查前加临时日志 `qWarning("ssl=%d", QSslSocket::supportsSsl());`（`#include <QSslSocket>`）确认 TLS 后端是否可用;不可用回到 Task 6 Step 2 的排查。

> 说明:发布流程需保证每个 GitHub Release 附带 `hopy.exe` 与其 SHA-256（`checksums.txt` 或 `hopy.exe.sha256`）；否则检查器会因缺 asset 走降级路径。此为发布约定,不在本代码计划内。

---

## Self-Review

- **Spec coverage**：版本单一来源(T1)、GitHub 链接(T2)、检查器+版本比对(T3)、下载+SHA-256(T4)、Windows 自替换+启动清理(T5)、TLS 后端(T6)、编排+对话框+启动/手动触发+降级(T7)、人工验证(T8) —— spec 各节均有对应任务。mac/Linux 经 `applyUpdate` 的 `#else` 分支返回 `NotSupported`（预留接口）。
- **降级**：检查/下载/校验/安装任一失败都汇入 `offerReleasesPage`，符合「绝不崩溃」。
- **类型一致性**：`ReleaseInfo`/`ReleaseAsset` 在 T3 定义,T4/T7 一致引用;`InstallResult` 在 T5 定义,T7 用 `platform::InstallResult::Ok` 一致;`compareVersions`/`currentVersion` 在 T1 定义,T3 一致引用。
- **已知外部依赖**：发布需附带 exe + 校验文件（T8 注明）;静态 Qt 的 TLS 后端可用性（T6 有排查与降级兜底）。
