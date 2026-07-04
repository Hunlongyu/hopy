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
        {"Preview paging (mouse M4 down / M5 up)",
                            QStringLiteral("预览翻页（鼠标侧键 M4 下翻 / M5 上翻）")},
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
        {"Start on boot",   QStringLiteral("开机自启")},
        {"Storage",         QStringLiteral("存储")},
        {"Items shown",     QStringLiteral("显示条数")},
        {"Storage limit",   QStringLiteral("存储上限")},
        {"Clear all",       QStringLiteral("清空全部")},
        {"Clear history",   QStringLiteral("清空历史")},
        {"Clear all clipboard records? This cannot be undone.",
                            QStringLiteral("确定清空全部剪贴板记录吗？此操作不可撤销。")},
        {"Check for updates", QStringLiteral("检查更新")},
        // About
        {"About",           QStringLiteral("关于")},
        {"Version %1",      QStringLiteral("版本 %1")},
        {"A lightweight clipboard manager built with Qt 6 and SQLite.",
                            QStringLiteral("使用 Qt 6 与 SQLite 构建的轻量剪贴板管理器。")},
        {"MIT License",     QStringLiteral("MIT 开源协议")},
        // Tray + dialogs
        {"Show",            QStringLiteral("显示")},
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
