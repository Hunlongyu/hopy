# hopy 内容感知打开 + 预览信息条 + 相对时间 + Home/End Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给 hopy 增加一组小而高频的交互增强：内容感知「打开」（URL/email/路径/文件，右键+`O` 触发、可配置）、预览窗口顶部信息条（类型/字数/绝对时间/操作提示）、卡片相对时间、Home/End 跳转、帮助面板重排。

**Architecture:** 纯逻辑（内容检测 `detectOpenTarget`、`relativeTime`、字数统计）放 `util/` 并 QtTest 单测；「资源管理器定位」按平台放 `platform/`（单文件 `#ifdef`）；打开执行放薄函数 `openRecord`；触发接线在 `MainWindow`（键盘 `O` 走 `handleNavKey`、鼠标走 `eventFilter`）汇成新信号 `openRequested(id)` 由 `Application` 连到 `openRecord`；预览信息条与相对时间是 UI 消费纯函数。

**Tech Stack:** Qt 6.8+（Core/Gui/Widgets/Network/Test）、C++20、`QDesktopServices`、`QProcess`、`QFileInfo`、`QRegularExpression`、QtTest。

## Global Constraints

- **Qt 6.8 下限；C++20。** 所有源码在 `hopy::` 命名空间（平台代码 `hopy::platform`）。
- **错误处理**：打开失败/IO 失败 `qWarning` 记录并静默降级，**绝不崩溃**。
- **单文件静态构建**：不引入新运行时 DLL；平台差异写在**单个 `.cpp` 内 `#if defined(Q_OS_WIN)`**（参照 `platform/Autostart.cpp`），不建 `_win.cpp`。
- **UI 文案**：英文常量走 `T("...")`，中文加到 `src/util/I18n.cpp` 的 `zh()` 映射，key 与英文源串**逐字节一致**（否则静默回退英文）。
- **配置兼容**：`AppSettings` 新字段在 `fromJson` 缺省时回退默认值，旧 config 不报错。
- **测试**：纯函数 QtTest，`tests/CMakeLists.txt` 用 `hopy_add_test(<name>)` 注册，文件尾 `QTEST_APPLESS_MAIN(<Class>)` + `#include "<name>.moc"`。
- **构建/测试命令（本机静态 Qt，非通用 `cmake --build build`）**：在 MSVC 2026 vcvars 内用 preset —— 见调度时提供的 build-context 文件（构建目录 `build/win-static-release`）。新增 `Q_OBJECT` 类首次构建若报 `metaObject/qt_metacall` 未解析，删 `hopy_lib_autogen` 与 `CMakeFiles/hopy_lib.dir/hopy_lib_autogen.dir` 重构；带 `Q_OBJECT` 的文件里禁用含 `//` 的原始字符串 `R"(...)"`（moc 会误解析）。新增源文件后先 `cmake --preset win-static-release` 重扫 glob。

---

### Task 1: 纯逻辑 util —— 内容检测 + 相对时间 + 字数统计

**Files:**
- Create: `src/util/OpenTarget.h`, `src/util/OpenTarget.cpp`
- Create: `src/util/TextInfo.h`, `src/util/TextInfo.cpp`
- Modify: `src/util/I18n.cpp`（相对时间的中文文案）
- Test: `tests/test_open_target.cpp`, `tests/test_text_info.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `enum class hopy::OpenKind { None, Url, Email, Path };`
  - `struct hopy::OpenTarget { OpenKind kind = OpenKind::None; QString value; };`
  - `hopy::OpenTarget detectOpenTarget(const QString& text);`
  - `int hopy::charCount(const QString& text);`（Unicode 码点数）
  - `int hopy::lineCount(const QString& text);`（`\n` 数 +1；空串 → 0）
  - `QString hopy::relativeTime(qint64 msecs, qint64 nowMsecs);`

- [ ] **Step 1: 写失败测试** `tests/test_open_target.cpp`

```cpp
#include <QtTest>
#include <QTemporaryFile>
#include "util/OpenTarget.h"
using namespace hopy;

class TestOpenTarget : public QObject {
    Q_OBJECT
private slots:
    void detectsHttpUrl() {
        const auto r = detectOpenTarget("https://example.com");
        QCOMPARE(r.kind, OpenKind::Url);
        QCOMPARE(r.value, QStringLiteral("https://example.com"));
    }
    void bareWwwGetsHttpsPrefix() {
        const auto r = detectOpenTarget("www.example.com");
        QCOMPARE(r.kind, OpenKind::Url);
        QCOMPARE(r.value, QStringLiteral("https://www.example.com"));
    }
    void urlWithAtIsStillUrl() {
        QCOMPARE(detectOpenTarget("https://a@b.com").kind, OpenKind::Url);
    }
    void detectsEmail() {
        const auto r = detectOpenTarget("  user@example.com  ");
        QCOMPARE(r.kind, OpenKind::Email);
        QCOMPARE(r.value, QStringLiteral("user@example.com"));
    }
    void emailNeedsADot() {
        QCOMPARE(detectOpenTarget("user@host").kind, OpenKind::None);
    }
    void plainWordIsNone() {
        QCOMPARE(detectOpenTarget("hello").kind, OpenKind::None);
    }
    void nonexistentPathIsNone() {
        QCOMPARE(detectOpenTarget("Z:/no/such/path_xyz123").kind, OpenKind::None);
    }
    void existingFileIsPath() {
        QTemporaryFile f;
        QVERIFY(f.open());
        const auto r = detectOpenTarget(f.fileName());
        QCOMPARE(r.kind, OpenKind::Path);
        QVERIFY(!r.value.isEmpty());
    }
};
QTEST_APPLESS_MAIN(TestOpenTarget)
#include "test_open_target.moc"
```

- [ ] **Step 2: 写失败测试** `tests/test_text_info.cpp`

```cpp
#include <QtTest>
#include <QDateTime>
#include "util/TextInfo.h"
using namespace hopy;

static qint64 ms(int y, int mo, int d, int h, int mi) {
    return QDateTime(QDate(y, mo, d), QTime(h, mi, 0)).toMSecsSinceEpoch();
}

class TestTextInfo : public QObject {
    Q_OBJECT
    const qint64 now = ms(2026, 7, 4, 12, 0);
private slots:
    void countsCodePointsNotUtf16() {
        QCOMPARE(charCount(QStringLiteral("中文a")), 3);
        QCOMPARE(charCount(QString::fromUcs4(U"\U0001D518", 1)), 1);  // non-BMP => 1 code point
    }
    void countsLines() {
        QCOMPARE(lineCount("a\nb\nc"), 3);
        QCOMPARE(lineCount("x"), 1);
        QCOMPARE(lineCount(""), 0);
    }
    void justNowUnderAMinute() { QCOMPARE(relativeTime(now - 30'000, now), QStringLiteral("just now")); }
    void minutesAgo()          { QCOMPARE(relativeTime(now - 5*60'000, now), QStringLiteral("5 min ago")); }
    void hoursAgoSameDay()     { QCOMPARE(relativeTime(now - 3*3600'000, now), QStringLiteral("3 h ago")); }
    void yesterdayShowsClock() {
        QCOMPARE(relativeTime(ms(2026, 7, 3, 20, 0), now), QStringLiteral("Yesterday 20:00"));
    }
    void sameYearShowsMonthDay() {
        QCOMPARE(relativeTime(ms(2026, 3, 1, 9, 0), now), QStringLiteral("03-01"));
    }
    void otherYearShowsFullDate() {
        QCOMPARE(relativeTime(ms(2025, 12, 1, 9, 0), now), QStringLiteral("2025-12-01"));
    }
};
QTEST_APPLESS_MAIN(TestTextInfo)
#include "test_text_info.moc"
```

- [ ] **Step 3: 注册测试并确认失败**

`tests/CMakeLists.txt` 末尾加：
```cmake
hopy_add_test(test_open_target)
hopy_add_test(test_text_info)
```
Run（见 build-context 的 preset 命令）：构建 `test_open_target`。
Expected: 编译失败（缺 `util/OpenTarget.h`）。

- [ ] **Step 4: 实现** `src/util/OpenTarget.h`

```cpp
#pragma once
#include <QString>

namespace hopy {

enum class OpenKind { None, Url, Email, Path };
struct OpenTarget { OpenKind kind = OpenKind::None; QString value; };

// Detect an openable target in a record's plain text, priority URL → email →
// existing file path. `value` is the normalized URL / email / absolute path.
// Returns {None, ""} when nothing openable is found.
OpenTarget detectOpenTarget(const QString& text);

} // namespace hopy
```

- [ ] **Step 5: 实现** `src/util/OpenTarget.cpp`

```cpp
#include "util/OpenTarget.h"
#include <QFileInfo>
#include <QRegularExpression>

namespace hopy {

OpenTarget detectOpenTarget(const QString& text) {
    const QString s = text.trimmed();
    if (s.isEmpty()) return {};

    const QString lower = s.toLower();
    if (lower.startsWith("http://") || lower.startsWith("https://") || lower.startsWith("ftp://"))
        return {OpenKind::Url, s};
    if (lower.startsWith("www.") && !s.contains(' '))
        return {OpenKind::Url, QStringLiteral("https://") + s};

    static const QRegularExpression email(QStringLiteral("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$"));
    if (email.match(s).hasMatch())
        return {OpenKind::Email, s};

    // A file path must look path-like (absolute or containing a separator) and
    // actually exist on disk — this avoids treating a bare word as a path.
    if (s.contains('/') || s.contains('\\') || QFileInfo(s).isAbsolute()) {
        const QFileInfo fi(s);
        if (fi.exists()) return {OpenKind::Path, fi.absoluteFilePath()};
    }
    return {};
}

} // namespace hopy
```

- [ ] **Step 6: 实现** `src/util/TextInfo.h`

```cpp
#pragma once
#include <QString>

namespace hopy {

// Unicode code-point count (CJK-friendly; not UTF-16 units).
int charCount(const QString& text);
// Number of lines: count('\n') + 1; empty string => 0.
int lineCount(const QString& text);
// Human relative time; `nowMsecs` is injected for testability. UI strings via T().
// Buckets: <60s "just now"; <60m "N min ago"; same-day "N h ago";
// yesterday "Yesterday HH:mm"; same-year "MM-dd"; else "yyyy-MM-dd".
QString relativeTime(qint64 msecs, qint64 nowMsecs);

} // namespace hopy
```

- [ ] **Step 7: 实现** `src/util/TextInfo.cpp`

```cpp
#include "util/TextInfo.h"
#include "util/I18n.h"
#include <QDateTime>

namespace hopy {

int charCount(const QString& text) { return static_cast<int>(text.toUcs4().size()); }

int lineCount(const QString& text) {
    if (text.isEmpty()) return 0;
    return static_cast<int>(text.count(QLatin1Char('\n'))) + 1;
}

QString relativeTime(qint64 msecs, qint64 nowMsecs) {
    const qint64 diff = (nowMsecs - msecs) / 1000;      // seconds; negative => clock skew
    if (diff < 60)   return T("just now");
    if (diff < 3600) return T("%1 min ago").arg(diff / 60);

    const QDateTime t   = QDateTime::fromMSecsSinceEpoch(msecs);
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(nowMsecs);
    if (t.date() == now.date())            return T("%1 h ago").arg(diff / 3600);
    if (t.date() == now.date().addDays(-1)) return T("Yesterday %1").arg(t.toString("HH:mm"));
    if (t.date().year() == now.date().year()) return t.toString("MM-dd");
    return t.toString("yyyy-MM-dd");
}

} // namespace hopy
```

- [ ] **Step 8: 相对时间中文文案**（`src/util/I18n.cpp`，`zh()` map 内加）

```cpp
        {"just now",        QStringLiteral("刚刚")},
        {"%1 min ago",      QStringLiteral("%1 分钟前")},
        {"%1 h ago",        QStringLiteral("%1 小时前")},
        {"Yesterday %1",    QStringLiteral("昨天 %1")},
```

- [ ] **Step 9: 运行两个测试确认通过**

Run: 构建并 ctest `test_open_target` 与 `test_text_info`。
Expected: 全部 PASS。

- [ ] **Step 10: 提交**

```bash
git add src/util/OpenTarget.h src/util/OpenTarget.cpp src/util/TextInfo.h src/util/TextInfo.cpp \
        src/util/I18n.cpp tests/test_open_target.cpp tests/test_text_info.cpp tests/CMakeLists.txt
git commit -m "feat(util): content detection + relative time + text stats (pure, tested)"
```

---

### Task 2: 平台「资源管理器定位」+ 打开执行

**Files:**
- Create: `src/platform/RevealInExplorer.h`, `src/platform/RevealInExplorer.cpp`
- Create: `src/ui/OpenAction.h`, `src/ui/OpenAction.cpp`
- Test: `tests/test_reveal.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `detectOpenTarget`（Task 1）、`hopy::ClipboardRecord`（`src/storage/Record.h`：`content`, `type`, `imagePath`；`ContentType` = Text/RichText/Image/Files；Files 的路径在 `content`，`\n` 分隔）
- Produces:
  - `bool hopy::platform::revealInExplorer(const QString& path);`（不存在的路径直接返回 false 且不启动任何进程）
  - `bool hopy::openRecord(const ClipboardRecord& rec);`（触发了打开返回 true）

- [ ] **Step 1: 写失败测试** `tests/test_reveal.cpp`

```cpp
#include <QtTest>
#include "platform/RevealInExplorer.h"
using namespace hopy::platform;

class TestReveal : public QObject {
    Q_OBJECT
private slots:
    void nonexistentPathReturnsFalse() {
        QVERIFY(!revealInExplorer("Z:/definitely/not/here_9f8a7b"));
    }
    void emptyPathReturnsFalse() {
        QVERIFY(!revealInExplorer(QString()));
    }
};
QTEST_APPLESS_MAIN(TestReveal)
#include "test_reveal.moc"
```

- [ ] **Step 2: 注册并确认失败**

`tests/CMakeLists.txt` 加：
```cmake
hopy_add_test(test_reveal)
```
Run: 构建 `test_reveal`。Expected: 编译失败（缺头文件）。

- [ ] **Step 3: 实现** `src/platform/RevealInExplorer.h`

```cpp
#pragma once
#include <QString>

namespace hopy::platform {

// Reveal `path` in the system file manager: select the file, or open the
// directory. Windows-native; other platforms open the containing folder.
// Returns false (doing nothing) when the path is empty or does not exist.
bool revealInExplorer(const QString& path);

} // namespace hopy::platform
```

- [ ] **Step 4: 实现** `src/platform/RevealInExplorer.cpp`

```cpp
#include "platform/RevealInExplorer.h"
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>

namespace hopy::platform {

bool revealInExplorer(const QString& path) {
    if (path.isEmpty()) return false;
    const QFileInfo fi(path);
    if (!fi.exists()) return false;

#if defined(Q_OS_WIN)
    const QString native = QDir::toNativeSeparators(fi.absoluteFilePath());
    if (fi.isDir())
        return QProcess::startDetached(QStringLiteral("explorer"), {native});
    // "/select," must reach explorer as a single argument glued to the path.
    return QProcess::startDetached(QStringLiteral("explorer"),
                                   {QStringLiteral("/select,") + native});
#else
    const QString dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
#endif
}

} // namespace hopy::platform
```

- [ ] **Step 5: 实现** `src/ui/OpenAction.h`

```cpp
#pragma once
#include "storage/Record.h"

namespace hopy {

// Content-aware "open" for a record: URL → browser, email → mail client,
// file path / Files record → file manager. Returns true if an open was
// triggered, false when there is nothing openable. Never throws.
bool openRecord(const ClipboardRecord& rec);

} // namespace hopy
```

- [ ] **Step 6: 实现** `src/ui/OpenAction.cpp`

```cpp
#include "ui/OpenAction.h"
#include "util/OpenTarget.h"
#include "platform/RevealInExplorer.h"
#include <QDesktopServices>
#include <QUrl>

namespace hopy {

bool openRecord(const ClipboardRecord& rec) {
    if (rec.type == ContentType::Files) {
        const QString first = rec.content.section(QLatin1Char('\n'), 0, 0).trimmed();
        return platform::revealInExplorer(first);
    }
    if (rec.type == ContentType::Text || rec.type == ContentType::RichText) {
        const OpenTarget t = detectOpenTarget(rec.content);
        switch (t.kind) {
            case OpenKind::Url:   return QDesktopServices::openUrl(QUrl(t.value));
            case OpenKind::Email: return QDesktopServices::openUrl(QUrl(QStringLiteral("mailto:") + t.value));
            case OpenKind::Path:  return platform::revealInExplorer(t.value);
            case OpenKind::None:  return false;
        }
    }
    return false;
}

} // namespace hopy
```

- [ ] **Step 7: 运行测试确认通过**

Run: 构建并 ctest `test_reveal`；再构建 `hopy` 确认 `openRecord` 编译链接。
Expected: `test_reveal` PASS；app 链接成功。

- [ ] **Step 8: 提交**

```bash
git add src/platform/RevealInExplorer.h src/platform/RevealInExplorer.cpp \
        src/ui/OpenAction.h src/ui/OpenAction.cpp tests/test_reveal.cpp tests/CMakeLists.txt
git commit -m "feat(open): content-aware openRecord + Windows reveal-in-explorer"
```

---

### Task 3: 配置字段 openKey / openMouseButton

**Files:**
- Modify: `src/config/Settings.h`（`AppSettings` 加两字段）
- Modify: `src/config/Settings.cpp`（`fromJson`/`toJson` + 校验）
- Test: `tests/test_settings.cpp`（追加用例）

**Interfaces:**
- Produces（`AppSettings` 新成员）：
  - `QString openKey = "O";`（单个键，空串=禁用键盘触发）
  - `QString openMouseButton = "right";`（`"right"` | `"middle"` | `"none"`）

- [ ] **Step 1: 追加失败测试**（在 `tests/test_settings.cpp` 的 class 内加 private slots）

```cpp
    void openDefaultsWhenAbsent() {
        const AppSettings s = Settings::fromJson("{}");
        QCOMPARE(s.openKey, QStringLiteral("O"));
        QCOMPARE(s.openMouseButton, QStringLiteral("right"));
    }
    void openRoundTrips() {
        AppSettings s; s.openKey = "G"; s.openMouseButton = "middle";
        const AppSettings r = Settings::fromJson(Settings::toJson(s));
        QCOMPARE(r.openKey, QStringLiteral("G"));
        QCOMPARE(r.openMouseButton, QStringLiteral("middle"));
    }
    void openMouseButtonInvalidFallsBack() {
        const AppSettings s = Settings::fromJson(R"({"openMouseButton":"bogus"})");
        QCOMPARE(s.openMouseButton, QStringLiteral("right"));
    }
```

- [ ] **Step 2: 运行确认失败**

Run: 构建并 ctest `test_settings`。Expected: 新用例 FAIL（字段不存在 → 编译失败）。

- [ ] **Step 3: 加字段** `src/config/Settings.h`（在 `windowOpacity` 后）

```cpp
    QString openKey = "O";            // content-aware open: keyboard key ("" = disabled)
    QString openMouseButton = "right"; // "right" | "middle" | "none"
```

- [ ] **Step 4: 读写 + 校验** `src/config/Settings.cpp`

`fromJson` 中，在 `windowOpacity` 读取之后加：
```cpp
    if (o.contains("openKey"))         s.openKey = o["openKey"].toString(s.openKey);
    if (o.contains("openMouseButton")) s.openMouseButton = o["openMouseButton"].toString(s.openMouseButton);
```
并在校验段（`windowOpacity` clamp 之后、`return s;` 之前）加：
```cpp
    if (s.openMouseButton != "right" && s.openMouseButton != "middle" && s.openMouseButton != "none")
        s.openMouseButton = "right";
```
`toJson` 中，`windowOpacity` 之后加：
```cpp
    o["openKey"] = s.openKey;
    o["openMouseButton"] = s.openMouseButton;
```

- [ ] **Step 5: 运行确认通过**

Run: 构建并 ctest `test_settings`。Expected: PASS。

- [ ] **Step 6: 提交**

```bash
git add src/config/Settings.h src/config/Settings.cpp tests/test_settings.cpp
git commit -m "feat(config): openKey / openMouseButton settings with validation"
```

---

### Task 4: 触发接线（键盘 O + 鼠标 + Home/End）+ openRequested 信号 + Application

**Files:**
- Modify: `src/ui/MainWindow.h`（信号 + 成员）
- Modify: `src/ui/MainWindow.cpp`（`handleNavKey`、`eventFilter`、`setSettings`）
- Modify: `src/app/Application.cpp`（连接 + 调用 `openRecord`）

**Interfaces:**
- Consumes: `hopy::openRecord`（Task 2）、`AppSettings::openKey/openMouseButton`（Task 3）、现有 `MainWindow::currentId()`、`RecordListModel::recordAt(row)`、`repo_->getById(id)`
- Produces: `MainWindow` 新信号 `void openRequested(qint64 id);`

- [ ] **Step 1: 加信号与成员** `src/ui/MainWindow.h`

在 signals 区（与 `deleteRequested` 并列）加：
```cpp
    void openRequested(qint64 id);
```
在私有成员区（与 `delegate_` 并列）加：
```cpp
    int openKey_ = Qt::Key_O;              // 0 = keyboard open disabled
    Qt::MouseButton openMouseButton_ = Qt::RightButton;  // Qt::NoButton = mouse open disabled
```
顶部若无 `#include <QtGlobal>`/`<Qt>` 对 `Qt::MouseButton` 的可见性问题，`Qt::MouseButton` 由 `<QtCore>` 提供，MainWindow.h 已含 Qt 头，无需额外 include。

- [ ] **Step 2: 从设置同步绑定** `src/ui/MainWindow.cpp`（`setSettings` 内，任意集中处）

在 `MainWindow::setSettings(const AppSettings& s)` 中加入解析（把字符串映射到键码/按钮）：
```cpp
    // Content-aware open bindings.
    openKey_ = s.openKey.isEmpty()
        ? 0
        : QKeySequence(s.openKey)[0].key();   // single-key string -> key code
    openMouseButton_ = s.openMouseButton == "middle" ? Qt::MiddleButton
                     : s.openMouseButton == "none"   ? Qt::NoButton
                                                     : Qt::RightButton;
```
（`MainWindow.cpp` 顶部确认 `#include <QKeySequence>`；若无则加。）

- [ ] **Step 3: 键盘触发 + Home/End** `src/ui/MainWindow.cpp`（`handleNavKey`）

在 `handleNavKey` 首个 `switch` 里，`Key_Delete` 之后加 Home/End：
```cpp
        case Qt::Key_Home:    if (model_->rowCount()) { list_->setCurrentIndex(model_->index(0, 0)); showPreviewRow(0); } return true;
        case Qt::Key_End:     { const int last = model_->rowCount() - 1;
                                if (last >= 0) { list_->setCurrentIndex(model_->index(last, 0)); showPreviewRow(last); } return true; }
```
在 `if (!searchMode_)` 的第二个 `switch` 里，`Key_T` 之后加键盘「打开」：
```cpp
            default: break;
        }
        if (openKey_ && key == openKey_) { const qint64 id = currentId(); if (id) emit openRequested(id); return true; }
    }
```
（即：把 openKey 判断放在第二个 switch 之后、`if (!searchMode_) {` 块的末尾，避免与固定单键冲突；若 `openKey_` 恰好等于某固定键如 F，则固定键先命中——可接受，设置里避免这么绑即可。）

> 若 `showPreviewRow` 私有且签名为 `void showPreviewRow(int row)`（现有），直接调用即可；仅当 `spacePreview_`/`hoverPreview_` 逻辑要求时它内部已判断。Home/End 调用 `showPreviewRow` 只为跟随预览，如担心副作用可省略该调用只保留 `setCurrentIndex`。

- [ ] **Step 4: 鼠标触发** `src/ui/MainWindow.cpp`（`eventFilter`）

在 `eventFilter` 的 `Leave` 分支之前加鼠标释放分支：
```cpp
    if (ev->type() == QEvent::MouseButtonRelease && obj == list_->viewport()) {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (openMouseButton_ != Qt::NoButton && me->button() == openMouseButton_) {
            const QModelIndex ix = list_->indexAt(me->pos());
            if (ix.isValid()) {
                if (const ClipboardRecord* r = model_->recordAt(ix.row())) {
                    emit openRequested(r->id);
                    return true;   // consume; QListView has no context menu to suppress
                }
            }
        }
    }
```
（`MainWindow.cpp` 顶部确认 `#include <QMouseEvent>`；现有已用 `QMouseEvent`，无需新增。`me->pos()` 在 viewport 坐标系。）

- [ ] **Step 5: Application 连接执行** `src/app/Application.cpp`

顶部加 `#include "ui/OpenAction.h"`。在 `start()` 里其它 `connect(window_, ...)` 附近加：
```cpp
    connect(window_, &MainWindow::openRequested, this, [this](qint64 id) {
        if (auto rec = repo_->getById(id)) openRecord(*rec);
    });
```

- [ ] **Step 6: 构建 + 冒烟**

Run: 构建 `hopy`（先 `Stop-Process hopy`）。Expected: 链接成功。运行全量 `ctest` 确认无回归。

- [ ] **Step 7: 提交**

```bash
git add src/ui/MainWindow.h src/ui/MainWindow.cpp src/app/Application.cpp
git commit -m "feat(open): wire keyboard O / mouse button open + Home/End"
```

---

### Task 5: 卡片相对时间

**Files:**
- Modify: `src/ui/RecordDelegate.cpp`（`paint` 的 meta 行）

**Interfaces:**
- Consumes: `hopy::relativeTime`（Task 1）

- [ ] **Step 1: 替换 meta 时间戳** `src/ui/RecordDelegate.cpp`

顶部加 `#include "util/TextInfo.h"`。把 `paint` 中：
```cpp
    const qint64 ms = idx.data(RecordListModel::CreatedAtRole).toLongLong();
    const QString ts = QDateTime::fromMSecsSinceEpoch(ms).toString("yyyy-MM-dd HH:mm:ss");
    const QString meta = QString::number(num) + "   " + ts;
```
改为：
```cpp
    const qint64 ms = idx.data(RecordListModel::CreatedAtRole).toLongLong();
    const QString ts = relativeTime(ms, QDateTime::currentMSecsSinceEpoch());
    const QString meta = QString::number(num) + "   " + ts;
```

- [ ] **Step 2: 构建 + 目视**

Run: 构建 `hopy`。Expected: 链接成功；卡片时间显示为「刚刚 / N 分钟前 …」。

- [ ] **Step 3: 提交**

```bash
git add src/ui/RecordDelegate.cpp
git commit -m "feat(ui): show relative time on record cards"
```

---

### Task 6: 预览窗口顶部信息条

**Files:**
- Modify: `src/ui/PreviewPopup.h`（信息条成员 + `setOpenKeysLabel`）
- Modify: `src/ui/PreviewPopup.cpp`（构造加信息条；`showPreview` 填充）
- Modify: `src/ui/MainWindow.cpp`（`setSettings` 里把当前绑定标签传给预览）
- Modify: `src/util/I18n.cpp`（信息条与提示文案）

**Interfaces:**
- Consumes: `charCount`/`lineCount`（Task 1）、`detectOpenTarget`（Task 1）
- Produces: `void PreviewPopup::setOpenKeysLabel(const QString& label);`

- [ ] **Step 1: 头文件** `src/ui/PreviewPopup.h`

在 public 加：
```cpp
    void setOpenKeysLabel(const QString& label);   // e.g. "O / 右键", shown in the open hint
```
在 private 加成员：
```cpp
    QLabel* info_ = nullptr;
    QString openKeysLabel_;
```

- [ ] **Step 2: 构造信息条** `src/ui/PreviewPopup.cpp`（`lay->addWidget(scroll_)` 之前）

```cpp
    info_ = new QLabel(card);
    info_->setObjectName("PreviewInfo");
    info_->setWordWrap(false);
    {
        QFont f = info_->font();
        f.setPixelSize(11);
        info_->setFont(f);
    }
    lay->addWidget(info_);   // sits above the scroll area
```

并加实现：
```cpp
void PreviewPopup::setOpenKeysLabel(const QString& label) { openKeysLabel_ = label; }
```
（顶部补 `#include "util/TextInfo.h"`、`#include "util/OpenTarget.h"`。）

- [ ] **Step 3: 填充信息条** `src/ui/PreviewPopup.cpp`（`showPreview` 开头，`switch (rec.type)` 之前）

```cpp
    // Top info bar: type · counts · absolute time · open hint.
    const QString typeLabel =
        rec.type == ContentType::Image ? T("Image")
      : rec.type == ContentType::Files ? T("Files")
                                       : T("Text");
    const QString absTime =
        QDateTime::fromMSecsSinceEpoch(rec.createdAt).toString("yyyy-MM-dd HH:mm:ss");
    QStringList parts{typeLabel};
    if (rec.type == ContentType::Text || rec.type == ContentType::RichText)
        parts << T("%1 chars").arg(charCount(rec.content))
              << T("%1 lines").arg(lineCount(rec.content));
    parts << absTime;

    QString verb;
    if (rec.type == ContentType::Files) {
        verb = T("reveal in file manager");
    } else if (rec.type == ContentType::Text || rec.type == ContentType::RichText) {
        switch (detectOpenTarget(rec.content).kind) {
            case OpenKind::Url:   verb = T("open link"); break;
            case OpenKind::Email: verb = T("open email"); break;
            case OpenKind::Path:  verb = T("reveal in file manager"); break;
            case OpenKind::None:  break;
        }
    }
    if (!verb.isEmpty() && !openKeysLabel_.isEmpty())
        parts << (openKeysLabel_ + QStringLiteral(" ") + verb);
    info_->setText(parts.join(QStringLiteral("  ·  ")));
```
（`showPreview` 已 `#include <QDateTime>`? 若无则在 `.cpp` 顶部加 `#include <QDateTime>`、`#include <QStringList>`。）

- [ ] **Step 4: MainWindow 传当前绑定标签** `src/ui/MainWindow.cpp`（`setSettings`，Task 4 的绑定解析之后）

```cpp
    // Build the hint label from the active bindings, e.g. "O / 右键".
    QStringList keys;
    if (!s.openKey.isEmpty())      keys << s.openKey;
    if (s.openMouseButton == "right")  keys << T("Right-click");
    else if (s.openMouseButton == "middle") keys << T("Middle-click");
    if (preview_) preview_->setOpenKeysLabel(keys.join(QStringLiteral(" / ")));
```
（确认成员名为 `preview_`；若不同按实际改。`#include <QStringList>` 现有 MainWindow 已具备。）

- [ ] **Step 5: 文案** `src/util/I18n.cpp`（`zh()` map 内加）

```cpp
        {"%1 chars",   QStringLiteral("%1 字符")},
        {"%1 lines",   QStringLiteral("%1 行")},
        {"open link",  QStringLiteral("打开链接")},
        {"open email", QStringLiteral("打开邮箱")},
        {"reveal in file manager", QStringLiteral("在资源管理器中定位")},
        {"Right-click", QStringLiteral("右键")},
        {"Middle-click", QStringLiteral("中键")},
```
（`Text`/`Image`/`Files` 已有译文，勿重复添加。）

- [ ] **Step 6: 构建 + 目视**

Run: 构建 `hopy`。Expected: 悬浮/空格预览时顶部显示信息条（类型 · 字数 · 行数 · 绝对时间 · 打开提示）。

- [ ] **Step 7: 提交**

```bash
git add src/ui/PreviewPopup.h src/ui/PreviewPopup.cpp src/ui/MainWindow.cpp src/util/I18n.cpp
git commit -m "feat(ui): preview info bar (type / counts / time / open hint)"
```

---

### Task 7: 设置面板 —— openKey 录制 + 鼠标键下拉

**Files:**
- Modify: `src/ui/panel/SettingsPanel.h`（控件成员）
- Modify: `src/ui/panel/SettingsPanel.cpp`（Behavior 区加两行；`setSettings`/`emitChange`）
- Modify: `src/util/I18n.cpp`（标签文案）

**Interfaces:**
- Consumes: `AppSettings::openKey/openMouseButton`（Task 3）

- [ ] **Step 1: 控件成员** `src/ui/panel/SettingsPanel.h`

在私有控件区加：
```cpp
    class QComboBox* openMouse_ = nullptr;   // right / middle / off
    class QComboBox* openKey_ = nullptr;     // single-key picker: Off / A..Z
```
（用 `QComboBox` 做单键选择，避免和主热键的和弦录制器混淆：Off + A–Z 足够，简单可靠。）

- [ ] **Step 2: Behavior 区加两行** `src/ui/panel/SettingsPanel.cpp`（`bhCard` 的 `addRow` 群里，`autostart` 之前）

在构造控件处（`autostart_ = new QCheckBox();` 附近）加：
```cpp
    openMouse_ = new QComboBox();
    openMouse_->addItem(T("Right-click"), "right");
    openMouse_->addItem(T("Middle-click"), "middle");
    openMouse_->addItem(T("Off"), "none");
    openMouse_->setFixedWidth(120);
    openKey_ = new QComboBox();
    openKey_->addItem(T("Off"), "");
    for (char c = 'A'; c <= 'Z'; ++c) openKey_->addItem(QString(QChar(c)), QString(QChar(c)));
    openKey_->setFixedWidth(120);
```
在 `bhCard` 的 `addRow` 群里加两行：
```cpp
    addRow(bh, T("Open with mouse"), openMouse_, true);
    addRow(bh, T("Open with key"), openKey_, true);
```

- [ ] **Step 3: 读写设置** `src/ui/panel/SettingsPanel.cpp`

`setSettings` 里加（在 `loading_ = true;` 之后的赋值群）：
```cpp
    openMouse_->setCurrentIndex(qMax(0, openMouse_->findData(s.openMouseButton)));
    openKey_->setCurrentIndex(qMax(0, openKey_->findData(s.openKey)));
```
`emitChange` 里加（组装 `AppSettings s` 处）：
```cpp
    s.openMouseButton = openMouse_->currentData().toString();
    s.openKey = openKey_->currentData().toString();
```
并把两个 combo 接到 `emitChange`（在其它 `connect(..., &QComboBox::currentIndexChanged, ...)` 群里加）：
```cpp
    connect(openMouse_, &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
    connect(openKey_,   &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
```

- [ ] **Step 4: 文案** `src/util/I18n.cpp`（`zh()` 内加；`Right-click`/`Middle-click` 已在 Task 6 加，勿重复）

```cpp
        {"Off",              QStringLiteral("关闭")},
        {"Open with mouse",  QStringLiteral("鼠标打开键")},
        {"Open with key",    QStringLiteral("键盘打开键")},
```

- [ ] **Step 5: 构建 + 目视**

Run: 构建 `hopy`。Expected: 设置「行为」区出现两个下拉；改为中键后中键点卡片可打开、右键失效；改 openKey 后对应键生效。

- [ ] **Step 6: 提交**

```bash
git add src/ui/panel/SettingsPanel.h src/ui/panel/SettingsPanel.cpp src/util/I18n.cpp
git commit -m "feat(settings): configure content-aware open key / mouse button"
```

---

### Task 8: 帮助面板重排（分组）

**Files:**
- Modify: `src/ui/panel/HelpPanel.cpp`
- Modify: `src/util/I18n.cpp`（新行文案）

**Interfaces:** 无（纯 UI 重排）

- [ ] **Step 1: 分组行数据** `src/ui/panel/HelpPanel.cpp`

把现有单一 `rows[]` 改为三组，并在网格里插入组标题。用现有 `Row{key, desc}` 结构，新增一个分组渲染。替换构造中 `rows[]` 与其渲染循环为：
```cpp
    struct Row { const char* key; const char* desc; };
    struct Group { const char* title; QList<Row> rows; };
    const Group groups[] = {
        {"Navigation", {
            {"/",            "Focus the search box"},
            {"↑ / ↓",        "Select previous / next"},
            {"← / → / Tab",  "Cycle content type"},
            {"Home / End",   "Jump to first / last"},
            {"1 – 5",        "Pick item N"},
        }},
        {"Actions", {
            {"Enter",        "Confirm selection"},
            {"Shift+Enter",  "Confirm as plain text"},
            {"O / Right-click", "Open link / email / file"},
            {"F",            "Toggle favorite"},
            {"T",            "Toggle pin"},
            {"Delete / D",   "Delete selection"},
        }},
        {"Window & preview", {
            {"Esc",          "Hide window"},
            {"Space",        "Preview current item"},
            {"M4 / M5",      "Preview paging (mouse M4 down / M5 up)"},
        }},
    };

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(8);
    int r = 0;
    for (const Group& g : groups) {
        auto* gt = new QLabel(T(g.title));
        gt->setObjectName("HelpGroup");
        grid->addWidget(gt, r++, 0, 1, 2);
        for (const Row& row : g.rows) {
            auto* k = new QLabel(QString::fromUtf8(row.key));
            k->setObjectName("KeyCap");
            k->setAlignment(Qt::AlignCenter);
            k->setFixedHeight(26);
            grid->addWidget(k, r, 0);
            grid->addWidget(new QLabel(T(row.desc)), r, 1, Qt::AlignVCenter);
            ++r;
        }
    }
```
（保留其后把 `grid` 居中放入 `tableHost` 的现有代码。）

- [ ] **Step 2: 文案** `src/util/I18n.cpp`（`zh()` 内加新键；已存在的行文案勿重复）

```cpp
        {"Navigation",              QStringLiteral("导航")},
        {"Actions",                 QStringLiteral("操作")},
        {"Window & preview",        QStringLiteral("窗口与预览")},
        {"Jump to first / last",    QStringLiteral("跳到第一条 / 最后一条")},
        {"Open link / email / file", QStringLiteral("打开链接 / 邮箱 / 文件")},
        {"Preview current item",    QStringLiteral("预览当前项")},
```

- [ ] **Step 3: 构建 + 目视**

Run: 构建 `hopy`。Expected: 帮助面板按「导航 / 操作 / 窗口与预览」分组显示，含新增 O/右键、Home/End 行。

- [ ] **Step 4: 提交**

```bash
git add src/ui/panel/HelpPanel.cpp src/util/I18n.cpp
git commit -m "feat(ui): regroup help panel; add open + Home/End rows"
```

---

### Task 9: 人工验证

自动化覆盖纯逻辑；打开执行、鼠标/键盘触发、预览条、设置、帮助面板需人工走一遍。

- [ ] **Step 1: 内容感知打开**：复制一个 `https://` URL、一个 `a@b.com`、一个存在的文件路径、以及在文件管理器里复制一个文件（Files 类型）。分别：右键卡片 / 选中后按 `O` → URL 开浏览器、email 开邮箱、路径与文件在资源管理器中定位。
- [ ] **Step 2: 不可打开**：普通文本条目右键 / 按 `O` → 无反应、不崩。
- [ ] **Step 3: 重绑定**：设置里把「鼠标打开键」改中键 → 中键可打开、右键失效；把「键盘打开键」改成别的字母或关闭 → 生效。
- [ ] **Step 4: 预览信息条**：悬浮/空格预览 → 顶部显示 类型 · 字符 · 行 · 绝对时间 · 「<键> 打开…」；改绑定后提示里的键随之更新。
- [ ] **Step 5: 相对时间**：卡片显示「刚刚 / N 分钟前 …」。
- [ ] **Step 6: Home/End**：命令模式按 Home/End 跳首尾。
- [ ] **Step 7: 帮助面板**：分组显示、含新行、中英文均正确。

---

## Self-Review

- **Spec coverage**：内容检测+打开(T1/T2)、配置(T3)、键鼠+Home/End 触发(T4)、相对时间(T5)、预览信息条(T6)、设置 UI(T7)、帮助面板(T8)、人工验证(T9) —— spec 各节均有对应任务。i18n 文案分散在引入它的任务中。
- **平台约束**：`revealInExplorer` 单文件 `#ifdef`，mac/Linux 回退打开目录。
- **降级**：`openRecord`/`revealInExplorer` 对空/不存在/非法输入返回 false 不崩；触发键无可打开内容时无动作。
- **类型一致性**：`OpenKind`/`OpenTarget`/`detectOpenTarget`(T1) 被 T2/T6 一致引用；`relativeTime`/`charCount`/`lineCount`(T1) 被 T5/T6 一致引用；`openRecord`(T2) 被 T4 调用；`openRequested(qint64)`(T4) 被 Application 连接；`AppSettings::openKey/openMouseButton`(T3) 被 T4/T7 读写。
- **已知交互取舍**：键盘 `openKey_` 判断置于固定单键之后，若用户把 openKey 绑成 F/T/D 等已占用键则固定键优先（设置里避免即可）——已在 T4 注明。
