#include "util/I18n.h"
#include <QLocale>
#include <QHash>

namespace hopy {
namespace {

bool g_chinese = false;

const QHash<QByteArray, QString>& zh() {
    static const QHash<QByteArray, QString> m = {
        // Header / toolbar / filters
        {"Shortcuts",       QStringLiteral("快捷键说明")},
        {"Settings",        QStringLiteral("设置")},
        {"Case sensitive",  QStringLiteral("区分大小写")},
        {"Whole word",      QStringLiteral("全词匹配")},
        {"Text",            QStringLiteral("文本")},
        {"Image",           QStringLiteral("图片")},
        {"Files",           QStringLiteral("文件")},
        {"Favorites only",  QStringLiteral("仅收藏")},
        {"All",             QStringLiteral("全部")},
        // Preview
        {"(image missing)", QStringLiteral("（图片缺失）")},
        {"%1 chars",   QStringLiteral("%1 字符")},
        {"%1 lines",   QStringLiteral("%1 行")},
        {"open link",  QStringLiteral("打开链接")},
        {"open email", QStringLiteral("打开邮箱")},
        {"reveal in file manager", QStringLiteral("在资源管理器中定位")},
        {"Right-click", QStringLiteral("右键")},
        {"Middle-click", QStringLiteral("中键")},
        // Relative time
        {"just now",        QStringLiteral("刚刚")},
        {"%1 min ago",      QStringLiteral("%1 分钟前")},
        {"%1 h ago",        QStringLiteral("%1 小时前")},
        {"Yesterday %1",    QStringLiteral("昨天 %1")},
        // Hotkey recorder
        {"Click, then press a shortcut", QStringLiteral("点击后按下快捷键")},
        {"Press Ctrl / Alt / Win plus a key; Backspace clears, Esc cancels",
                            QStringLiteral("按下 Ctrl / Alt / Win 加一个键；Backspace 清空，Esc 取消")},
        {"Clear shortcut",  QStringLiteral("清空快捷键")},
        {"Needs Ctrl / Alt / Win", QStringLiteral("需要 Ctrl / Alt / Win")},
        // Help panel
        {"Focus the search box",   QStringLiteral("聚焦搜索框")},
        {"Select previous / next", QStringLiteral("选择上一项 / 下一项")},
        {"Cycle content type",     QStringLiteral("切换内容类型（循环）")},
        {"Pick item N",            QStringLiteral("快速选中第 N 项")},
        {"Confirm selection",      QStringLiteral("确认选中项")},
        {"Confirm as plain text",  QStringLiteral("以纯文本确认选中项")},
        {"Toggle favorite",        QStringLiteral("切换收藏")},
        {"Toggle pin",             QStringLiteral("切换置顶")},
        {"Delete selection",       QStringLiteral("删除选中项")},
        {"Hide window",            QStringLiteral("隐藏窗口")},
        {"Preview paging",         QStringLiteral("预览翻页")},
        {"Jump to first / last",    QStringLiteral("跳到第一条 / 最后一条")},
        {"Open link / email / file", QStringLiteral("打开链接 / 邮箱 / 文件")},
        {"Preview current item",    QStringLiteral("预览当前项")},
        // Settings panel
        {"Dark",            QStringLiteral("深色")},
        {"Light",           QStringLiteral("浅色")},
        {"Follow caret",    QStringLiteral("跟随光标")},
        {"Screen center",   QStringLiteral("屏幕中央")},
        {"Left",            QStringLiteral("左侧")},
        {"Right",           QStringLiteral("右侧")},
        {"Appearance",      QStringLiteral("外观")},
        {"Language",        QStringLiteral("语言")},
        {"Auto",            QStringLiteral("自动")},
        {"Theme",           QStringLiteral("主题")},
        {"Window position", QStringLiteral("显示位置")},
        {"Preview side",    QStringLiteral("预览位置")},
        {"Opacity",         QStringLiteral("不透明度")},
        {"Behavior",        QStringLiteral("行为")},
        {"Activation hotkey",          QStringLiteral("激活快捷键")},
        {"Paste immediately on confirm", QStringLiteral("确认后立即粘贴")},
        {"Hover preview",   QStringLiteral("悬停预览")},
        {"Space preview",   QStringLiteral("空格预览")},
        {"Off",              QStringLiteral("关闭")},
        {"Open with mouse",  QStringLiteral("鼠标打开键")},
        {"Open with key",    QStringLiteral("键盘打开键")},
        {"Start on boot",   QStringLiteral("开机自启")},
        {"Suppress hotkey in fullscreen", QStringLiteral("全屏时屏蔽快捷键")},
        {"Mask sensitive content", QStringLiteral("遮蔽敏感内容")},
        {"Storage",         QStringLiteral("存储")},
        {"Items shown",     QStringLiteral("显示条数")},
        {"Storage limit",   QStringLiteral("存储上限")},
        {"Clear all",       QStringLiteral("清空全部")},
        {"Clear history",   QStringLiteral("清空历史")},
        {"Clear all clipboard records? This cannot be undone.",
                            QStringLiteral("确定清空全部剪贴板记录吗？此操作不可撤销。")},
        {"Check for updates", QStringLiteral("检查更新")},
        {"You're on the latest version.", QStringLiteral("已是最新版本。")},
        {"Checked too often — please try again later.", QStringLiteral("检查太频繁，请稍后再试。")},
        {"Checked too often — please try again after %1.", QStringLiteral("检查太频繁，请在 %1 后重试。")},
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
        // Update dialog (redesigned single-window flow)
        {"What's new",                    QStringLiteral("更新内容")},
        {"Release page",                  QStringLiteral("查看发布页")},
        {"Downloads page",                QStringLiteral("打开下载页")},
        {"Later",                         QStringLiteral("稍后")},
        {"Update now",                    QStringLiteral("立即更新")},
        {"Retry",                         QStringLiteral("重试")},
        {"Close",                         QStringLiteral("关闭")},
        {"Restart now",                   QStringLiteral("立即重启")},
        {"Restart later",                 QStringLiteral("稍后重启")},
        {"Checking for updates…",         QStringLiteral("正在检查更新…")},
        {"You're on the latest version",  QStringLiteral("已是最新版本")},
        {"A new version is available",    QStringLiteral("发现新版本")},
        {"New version %1",                QStringLiteral("发现新版本 %1")},
        {"Current %1",                    QStringLiteral("当前 %1")},
        {"Loading release notes…",        QStringLiteral("正在加载更新说明…")},
        {"Release notes unavailable — see the release page.",
                                          QStringLiteral("更新说明获取失败，请查看发布页。")},
        {"Verifying…",                    QStringLiteral("正在校验…")},
        {"Downloaded and verified",       QStringLiteral("已下载并校验通过")},
        {"Checked too often",             QStringLiteral("检查太频繁")},
        {"Update check failed",           QStringLiteral("检查更新失败")},
        {"Download failed",               QStringLiteral("下载失败")},
        {"Couldn't install the update",   QStringLiteral("无法安装更新")},
        // About
        {"About",           QStringLiteral("关于")},
        {"Version %1",      QStringLiteral("版本 %1")},
        {"A lightweight clipboard manager built with Qt 6 and SQLite.",
                            QStringLiteral("使用 Qt 6 与 SQLite 构建的轻量剪贴板管理器。")},
        {"MIT License",     QStringLiteral("MIT 开源协议")},
        // Tray + dialogs
        {"Show",            QStringLiteral("显示")},
        {"Show main window", QStringLiteral("显示主界面")},
        {"Pause monitoring", QStringLiteral("暂停监听")},
        {"Launch at startup", QStringLiteral("开机自启")},
        {"Quit",            QStringLiteral("退出")},
        {"Yes",             QStringLiteral("是")},
        {"No",              QStringLiteral("否")},
        {"Hotkey",          QStringLiteral("快捷键")},
        {"Could not register the shortcut %1 — it may already be in use by another program. Please pick another.",
                            QStringLiteral("快捷键 %1 注册失败，可能已被其他程序占用，请换一个。")},
    };
    return m;
}

} // namespace

void initLanguage() {
    g_chinese = (QLocale::system().language() == QLocale::Chinese);
}

void setLanguage(const QString& lang) {
    if (lang == "zh")      g_chinese = true;
    else if (lang == "en") g_chinese = false;
    else                   g_chinese = (QLocale::system().language() == QLocale::Chinese);   // "auto"
}

QString T(const char* english) {
    if (g_chinese) {
        const auto it = zh().constFind(QByteArray::fromRawData(english, int(qstrlen(english))));
        if (it != zh().constEnd()) return it.value();
    }
    return QString::fromUtf8(english);
}

} // namespace hopy
