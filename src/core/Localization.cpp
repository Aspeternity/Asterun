#include "Localization.hpp"

namespace altrun {

std::wstring_view LocalizedText(TextId id, Language language) {
    const bool zh = language == Language::ZhCN;

    switch (id) {
    case TextId::CreateWindowFailed:
        return zh ? L"无法创建 ALTRun Next 启动器窗口。" : L"Unable to create ALTRun Next launcher window.";
    case TextId::UnableToLaunch:
        return zh ? L"无法启动：" : L"Unable to launch:";
    case TextId::UnableToCopy:
        return zh ? L"无法复制到剪贴板。" : L"Unable to copy to the clipboard.";
    case TextId::CopyTextAction:
        return zh ? L"复制文本" : L"Copy text";
    case TextId::HotkeyBusy:
        return zh
            ? L"Alt+Space 已被其他程序占用。\n\nALTRun Next 会继续在系统托盘运行；后续版本将支持自定义快捷键。"
            : L"Alt+Space is already in use by another application.\n\nALTRun Next will keep running in the tray; hotkey customization will be added next.";
    case TextId::SearchPlaceholder:
        return zh ? L"输入快捷词、程序名或路径…" : L"Type a keyword, app name, or path...";
    case TextId::ClassicHint:
        return zh ? L"按下Shift+Tab键显示上一项快捷项" : L"Press Shift+Tab to show the previous item";
    case TextId::CommandPrefix:
        return zh ? L"命令：" : L"Command: ";
    case TextId::TrayShow:
        return zh ? L"显示\tAlt+Space" : L"Show\tAlt+Space";
    case TextId::TrayReload:
        return zh ? L"重新加载快捷项" : L"Reload commands";
    case TextId::TrayAppearance:
        return zh ? L"界面" : L"Appearance";
    case TextId::TrayClassic:
        return zh ? L"经典 ALTRun" : L"Classic ALTRun";
    case TextId::TrayModern:
        return zh ? L"现代紧凑" : L"Modern Compact";
    case TextId::TrayLanguage:
        return zh ? L"语言" : L"Language";
    case TextId::TrayChinese:
        return L"简体中文";
    case TextId::TrayEnglish:
        return L"English";
    case TextId::TrayExit:
        return zh ? L"退出" : L"Exit";
    }

    return L"";
}

} // namespace altrun
