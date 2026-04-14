#pragma once

#include <QString>

namespace LabMonitor {
namespace Styles {
namespace Colors {
    constexpr auto BgPrimary         = "#0D1117";
    constexpr auto BgSecondary       = "#161B22";
    constexpr auto BgTertiary        = "#1C2128";
    constexpr auto BgOverlay         = "#21262D";
    constexpr auto BorderSubtle      = "rgba(255,255,255,0.06)";
    constexpr auto BorderDefault     = "rgba(255,255,255,0.1)";
    constexpr auto BorderMuted       = "rgba(255,255,255,0.15)";
    constexpr auto BorderActive      = "#1F6FEB";
    constexpr auto ToolbarBg         = "#0F2A44";
    constexpr auto ToolbarHover      = "rgba(88,166,255,0.15)";
    constexpr auto ToolbarPressed    = "rgba(88,166,255,0.08)";
    constexpr auto ToolbarText       = "#E6EDF3";
    constexpr auto ToolbarSeparator  = "rgba(255,255,255,0.12)";
    constexpr auto SidebarBg         = "#081E33";
    constexpr auto SidebarHover      = "rgba(88,166,255,0.12)";
    constexpr auto SidebarActive     = "#1F6FEB";
    constexpr auto SidebarText       = "#8B949E";
    constexpr auto SidebarTextActive = "#E6EDF3";
    constexpr auto CardBg            = "#161B22";
    constexpr auto CardBorder        = "rgba(255,255,255,0.08)";
    constexpr auto CardBorderHover   = "rgba(255,255,255,0.2)";
    constexpr auto CardBorderSelected= "#1F6FEB";
    constexpr auto CardBgSelected    = "rgba(31,111,235,0.08)";
    constexpr auto CardBgHover       = "#1C2128";
    constexpr auto MainBg            = "#0D1117";
    constexpr auto TextPrimary       = "#E6EDF3";
    constexpr auto TextSecondary     = "#8B949E";
    constexpr auto TextMuted         = "#484F58";
    constexpr auto TextLink          = "#58A6FF";
    constexpr auto StatusOnline      = "#3FB950";
    constexpr auto StatusOffline     = "#484F58";
    constexpr auto StatusAlert       = "#F85149";
    constexpr auto AccentBlue        = "#1F6FEB";
    constexpr auto AccentGreen       = "#1F6FEB";
    constexpr auto AccentGreenHover  = "#388BFD";
    constexpr auto AccentRed         = "#DA3633";
    constexpr auto LessonHeaderBg    = "#161B22";
    constexpr auto LessonHeaderText  = "#E6EDF3";
    constexpr auto LessonBodyBg      = "#0D1117";
    constexpr auto LessonBorder      = "rgba(255,255,255,0.06)";
    constexpr auto StatusBarBg       = "#081E33";
    constexpr auto StatusBarText     = "#B0C4DE";
    constexpr auto StatusBarBorder   = "rgba(255,255,255,0.06)";
    constexpr auto SliderGroove      = "#21262D";
    constexpr auto SliderHandle      = "#1F6FEB";
    constexpr auto SliderHandleHover = "#388BFD";
    constexpr auto TabHeaderBg       = "#161B22";
    constexpr auto TabHeaderText     = "#E6EDF3";
}
namespace Fonts {
    constexpr auto Family         = "Segoe UI";
    constexpr int  ToolbarSize    = 9;
    constexpr int  StudentName    = 10;
    constexpr int  StudentUser    = 8;
    constexpr int  LessonLabel    = 9;
    constexpr int  LessonInput    = 9;
    constexpr int  StatusBar      = 9;
    constexpr int  TabHeader      = 10;
    constexpr int  EmptyMessage   = 13;
}
inline QString mainWindowStyle()
{
    return QStringLiteral(R"(
        QMainWindow {
            background-color: %1;
        }
    )").arg(Colors::BgPrimary);
}
inline QString toolbarStyle()
{
    return QStringLiteral(R"(
        #ToolbarWidget {
            background-color: %1;
            border-bottom: 1px solid %2;
            padding: 6px 8px;
        }
    )").arg(Colors::ToolbarBg, Colors::BorderSubtle);
}

inline QString toolbarButtonStyle()
{
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 6px;
            color: %1;
            font-family: %2;
            font-size: %3pt;
            font-weight: 500;
            padding: 4px 6px;
        }
        QToolButton:hover {
            background: %4;
            border: 1px solid %5;
        }
        QToolButton:pressed {
            background: %6;
        }
        QToolButton:disabled {
            color: %7;
        }
    )").arg(Colors::ToolbarText,
            Fonts::Family,
            QString::number(Fonts::ToolbarSize),
            Colors::ToolbarHover,
            Colors::BorderSubtle,
            Colors::ToolbarPressed,
            Colors::TextMuted);
}

inline QString toolbarSeparatorStyle()
{
    return QStringLiteral(R"(
        QFrame {
            background: %1;
            max-width: 1px;
            min-width: 1px;
            margin: 6px 6px;
        }
    )").arg(Colors::ToolbarSeparator);
}
inline QString sidebarStyle()
{
    return QStringLiteral(R"(
        #SidebarWidget {
            background-color: %1;
            border-right: 1px solid %2;
            min-width: 50px;
            max-width: 50px;
        }
    )").arg(Colors::SidebarBg, Colors::BorderSubtle);
}

inline QString sidebarButtonStyle()
{
    return QStringLiteral(R"(
        QPushButton {
            background: transparent;
            border: none;
            border-left: 2px solid transparent;
            border-radius: 0px;
            padding: 12px 12px;
            min-height: 36px;
        }
        QPushButton:hover {
            background: %1;
        }
        QPushButton:checked {
            background: rgba(31,111,235,0.1);
            border-left: 2px solid %2;
        }
    )").arg(Colors::SidebarHover, Colors::SidebarActive);
}
inline QString studentTileStyle()
{
    return QStringLiteral(R"(
        #StudentTile {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
        #StudentTile:hover {
            border: 1px solid %3;
            background: %4;
        }
        #StudentTile[selected="true"] {
            border: 1px solid %5;
            background: %6;
        }
    )").arg(Colors::CardBg,
            Colors::CardBorder,
            Colors::CardBorderHover,
            Colors::CardBgHover,
            Colors::CardBorderSelected,
            Colors::CardBgSelected);
}

inline QString studentNameStyle()
{
    return QStringLiteral(R"(
        QLabel {
            color: %1;
            font-family: %2;
            font-size: %3pt;
            font-weight: 600;
            padding: 0px;
            background: transparent;
            border: none;
        }
    )").arg(Colors::TextPrimary,
            Fonts::Family,
            QString::number(Fonts::StudentName));
}

inline QString studentUserStyle()
{
    return QStringLiteral(R"(
        QLabel {
            color: %1;
            font-family: %2;
            font-size: %3pt;
            font-weight: normal;
            padding: 0px;
            background: transparent;
            border: none;
        }
    )").arg(Colors::TextSecondary,
            Fonts::Family,
            QString::number(Fonts::StudentUser));
}
inline QString lessonHeaderStyle()
{
    return QStringLiteral(R"(
        #LessonHeader {
            background: %1;
            border-top: 1px solid %2;
            border-bottom: 1px solid %2;
            padding: 6px 12px;
        }
        #LessonHeader QLabel {
            color: %3;
            font-family: %4;
            font-size: %5pt;
            font-weight: bold;
            background: transparent;
            border: none;
        }
    )").arg(Colors::LessonHeaderBg,
            Colors::BorderSubtle,
            Colors::LessonHeaderText,
            Fonts::Family,
            QString::number(Fonts::LessonLabel));
}

inline QString lessonBodyStyle()
{
    return QStringLiteral(R"(
        #LessonBody {
            background: %1;
            border-bottom: 1px solid %2;
            padding: 10px 12px;
        }
        #LessonBody QLabel {
            color: %3;
            font-family: %4;
            font-size: %5pt;
            font-weight: 500;
            background: transparent;
            border: none;
        }
        #LessonBody QLineEdit, #LessonBody QTextEdit {
            background: %6;
            border: 1px solid %2;
            border-radius: 6px;
            padding: 6px 10px;
            font-family: %4;
            font-size: %7pt;
            color: %3;
        }
        #LessonBody QLineEdit:focus, #LessonBody QTextEdit:focus {
            border: 1px solid %8;
        }
    )").arg(Colors::LessonBodyBg,
            Colors::BorderDefault,
            Colors::TextPrimary,
            Fonts::Family,
            QString::number(Fonts::LessonLabel),
            Colors::BgSecondary,
            QString::number(Fonts::LessonInput),
            Colors::AccentBlue);
}
inline QString statusBarStyle()
{
    return QStringLiteral(R"(
        #StatusBarWidget {
            background: %1;
            border-top: 1px solid %2;
            padding: 4px 12px;
            min-height: 32px;
            max-height: 32px;
        }
        #StatusBarWidget QLabel {
            color: %3;
            font-family: %4;
            font-size: %5pt;
            background: transparent;
            border: none;
        }
        #StatusBarWidget QSlider::groove:horizontal {
            border: none;
            height: 4px;
            background: %6;
            border-radius: 2px;
        }
        #StatusBarWidget QSlider::handle:horizontal {
            background: %7;
            border: none;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        #StatusBarWidget QSlider::handle:horizontal:hover {
            background: %8;
        }
        #StatusBarWidget QSlider::sub-page:horizontal {
            background: %7;
            border-radius: 2px;
        }
    )").arg(Colors::StatusBarBg,
            Colors::StatusBarBorder,
            Colors::StatusBarText,
            Fonts::Family,
            QString::number(Fonts::StatusBar),
            Colors::SliderGroove,
            Colors::SliderHandle,
            Colors::SliderHandleHover);
}
inline QString tabHeaderStyle()
{
    return QStringLiteral(R"(
        #TabHeader {
            background: %1;
            border-bottom: 1px solid %2;
            padding: 4px 12px;
            min-height: 30px;
            max-height: 30px;
        }
        #TabHeader QLabel {
            color: %3;
            font-family: %4;
            font-size: %5pt;
            font-weight: 600;
            background: transparent;
            border: none;
        }
    )").arg(Colors::TabHeaderBg,
            Colors::BorderSubtle,
            Colors::TabHeaderText,
            Fonts::Family,
            QString::number(Fonts::TabHeader));
}
inline QString scrollAreaStyle()
{
    return QStringLiteral(R"(
        QScrollArea {
            background: %1;
            border: none;
        }
        QScrollArea > QWidget > QWidget {
            background: %1;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 4px 0;
        }
        QScrollBar::handle:vertical {
            background: %2;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: %3;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            height: 0;
        }
    )").arg(Colors::MainBg,
            Colors::SliderGroove,
            Colors::TextMuted);
}
inline QString dialogStyle()
{
    return QStringLiteral(R"(
        QDialog {
            background-color: %1;
            color: %2;
        }
        QLabel {
            color: %2;
            background: transparent;
        }
        QTextEdit {
            color: %2;
            background-color: %3;
            border: 1px solid %4;
            border-radius: 8px;
        }
        QLineEdit {
            color: %2;
            background-color: %3;
            border: 1px solid %4;
            border-radius: 8px;
        }
        QCheckBox {
            color: %5;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 4px;
            border: 1px solid %4;
            background: %3;
        }
        QCheckBox::indicator:checked {
            background: %6;
            border: 1px solid %6;
        }
        QTableWidget {
            color: %2;
            background-color: %3;
            border: 1px solid %4;
            border-radius: 8px;
            gridline-color: %4;
        }
        QTableWidget::item {
            padding: 4px 8px;
        }
        QTableWidget::item:selected {
            background: rgba(31,111,235,0.2);
        }
        QHeaderView::section {
            background: %7;
            color: %2;
            padding: 6px 8px;
            font-weight: bold;
            border: none;
            border-bottom: 1px solid %4;
        }
    )").arg(Colors::BgPrimary,
            Colors::TextPrimary,
            Colors::BgSecondary,
            Colors::BorderDefault,
            Colors::TextSecondary,
            Colors::AccentBlue,
            Colors::BgTertiary);
}
inline QString primaryButtonStyle()
{
    return QStringLiteral(R"(
        QPushButton {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 %1, stop:1 #1158C7);
            color: white;
            padding: 8px 24px;
            border-radius: 8px;
            font-weight: bold;
            font-size: 10pt;
            border: none;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 %2, stop:1 %1);
        }
        QPushButton:pressed {
            background: #1158C7;
        }
    )").arg(Colors::AccentBlue, Colors::SliderHandleHover);
}
inline QString secondaryButtonStyle()
{
    return QStringLiteral(R"(
        QPushButton {
            background: %1;
            color: %2;
            padding: 8px 24px;
            border-radius: 8px;
            font-weight: bold;
            font-size: 10pt;
            border: 1px solid %3;
        }
        QPushButton:hover {
            background: %4;
            border: 1px solid %5;
        }
    )").arg(Colors::BgSecondary,
            Colors::TextPrimary,
            Colors::BorderDefault,
            Colors::BgTertiary,
            Colors::BorderMuted);
}

}
}
