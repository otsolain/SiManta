#pragma once

#include <QString>

namespace LabMonitor {
namespace Styles {

// ── Color Constants ────────────────────────────────────────
namespace Colors {
    constexpr auto ToolbarGradientTop    = "#2D7A3A";
    constexpr auto ToolbarGradientBottom = "#1B5E28";
    constexpr auto ToolbarHover          = "#3A9C4E";
    constexpr auto ToolbarPressed        = "#164D20";
    constexpr auto ToolbarText           = "#FFFFFF";
    constexpr auto ToolbarSeparator      = "#3D8B4F";

    constexpr auto SidebarBg             = "#1A4D2A";
    constexpr auto SidebarHover          = "#236B35";
    constexpr auto SidebarActive         = "#27AE60";
    constexpr auto SidebarText           = "#8FBDA0";
    constexpr auto SidebarTextActive     = "#FFFFFF";

    constexpr auto MainBg                = "#E8F2EB";
    constexpr auto CardBg                = "#FFFFFF";
    constexpr auto CardBorder            = "#BDC7C0";
    constexpr auto CardBorderSelected    = "#27AE60";
    constexpr auto CardBgSelected        = "#EBF8EF";
    constexpr auto CardShadow            = "rgba(0,0,0,0.12)";

    constexpr auto TextPrimary           = "#2C3E35";
    constexpr auto TextSecondary         = "#7F8D84";
    constexpr auto TextMuted             = "#95A69A";

    constexpr auto StatusOnline          = "#27AE60";
    constexpr auto StatusOffline         = "#95A5A6";
    constexpr auto StatusAlert           = "#E74C3C";

    constexpr auto LessonHeaderBg        = "#2C4A38";
    constexpr auto LessonHeaderText      = "#FFFFFF";
    constexpr auto LessonBodyBg          = "#F8FAF9";
    constexpr auto LessonBorder          = "#DEE6E0";

    constexpr auto StatusBarBg           = "#ECF1ED";
    constexpr auto StatusBarText         = "#2C3E35";
    constexpr auto StatusBarBorder       = "#BDC7C0";

    constexpr auto SliderGroove          = "#BDC7C0";
    constexpr auto SliderHandle          = "#27AE60";
    constexpr auto SliderHandleHover     = "#2ECC71";

    constexpr auto TabHeaderBg           = "#D5DBD7";
    constexpr auto TabHeaderText         = "#2C3E35";
}

// ── Font Constants ─────────────────────────────────────────
namespace Fonts {
    constexpr auto Family         = "Segoe UI, Noto Sans, Ubuntu, sans-serif";
    constexpr int  ToolbarSize    = 9;
    constexpr int  StudentName    = 10;
    constexpr int  StudentUser    = 8;
    constexpr int  LessonLabel    = 9;
    constexpr int  LessonInput    = 9;
    constexpr int  StatusBar      = 9;
    constexpr int  TabHeader      = 10;
    constexpr int  EmptyMessage   = 12;
}

// ── Stylesheet: Main Window ────────────────────────────────
inline QString mainWindowStyle()
{
    return QStringLiteral(R"(
        QMainWindow {
            background-color: %1;
        }
    )").arg(Colors::MainBg);
}

// ── Stylesheet: Toolbar ────────────────────────────────────
inline QString toolbarStyle()
{
    return QStringLiteral(R"(
        #ToolbarWidget {
            background-color: #1B5E28;
            border-bottom: 2px solid #144D1E;
            padding: 6px 8px;
        }
    )");
}

inline QString toolbarButtonStyle()
{
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            color: %1;
            font-family: %2;
            font-size: %3pt;
            font-weight: bold;
            padding: 4px 6px;
        }
        QToolButton:hover {
            background: %4;
            border: 1px solid rgba(255,255,255,0.2);
        }
        QToolButton:pressed {
            background: %5;
        }
        QToolButton:disabled {
            color: rgba(255,255,255,0.4);
        }
    )").arg(Colors::ToolbarText,
            Fonts::Family,
            QString::number(Fonts::ToolbarSize),
            Colors::ToolbarHover,
            Colors::ToolbarPressed);
}

inline QString toolbarSeparatorStyle()
{
    return QStringLiteral(R"(
        QFrame {
            background: %1;
            max-width: 1px;
            min-width: 1px;
            margin: 4px 4px;
        }
    )").arg(Colors::ToolbarSeparator);
}

// ── Stylesheet: Sidebar ────────────────────────────────────
inline QString sidebarStyle()
{
    return QStringLiteral(R"(
        #SidebarWidget {
            background-color: %1;
            border-right: 1px solid #12381F;
            min-width: 48px;
            max-width: 48px;
        }
    )").arg(Colors::SidebarBg);
}

inline QString sidebarButtonStyle()
{
    return QStringLiteral(R"(
        QPushButton {
            background: transparent;
            border: none;
            border-left: 3px solid transparent;
            border-radius: 0px;
            padding: 12px 10px;
            min-height: 36px;
        }
        QPushButton:hover {
            background: %1;
        }
        QPushButton:checked {
            background: %2;
            border-left: 3px solid %3;
        }
    )").arg(Colors::SidebarHover,
            "rgba(39,174,96,0.3)",
            Colors::SidebarActive);
}

// ── Stylesheet: Student Tile ───────────────────────────────
inline QString studentTileStyle()
{
    return QStringLiteral(R"(
        #StudentTile {
            background: %1;
            border: 2px solid %2;
            border-radius: 6px;
        }
        #StudentTile:hover {
            border: 2px solid %3;
        }
        #StudentTile[selected="true"] {
            border: 2px solid %3;
            background: %4;
        }
    )").arg(Colors::CardBg,
            Colors::CardBorder,
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

// ── Stylesheet: Lesson Panel ───────────────────────────────
inline QString lessonHeaderStyle()
{
    return QStringLiteral(R"(
        #LessonHeader {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3D6E4A, stop:1 %1);
            border: 1px solid #2C4A38;
            border-radius: 0px;
            padding: 6px 12px;
        }
        #LessonHeader QLabel {
            color: %2;
            font-family: %3;
            font-size: %4pt;
            font-weight: bold;
            background: transparent;
            border: none;
        }
    )").arg(Colors::LessonHeaderBg,
            Colors::LessonHeaderText,
            Fonts::Family,
            QString::number(Fonts::LessonLabel));
}

inline QString lessonBodyStyle()
{
    return QStringLiteral(R"(
        #LessonBody {
            background: %1;
            border: 1px solid %2;
            border-top: none;
            padding: 10px 12px;
        }
        #LessonBody QLabel {
            color: %3;
            font-family: %4;
            font-size: %5pt;
            font-weight: bold;
            background: transparent;
            border: none;
        }
        #LessonBody QLineEdit, #LessonBody QTextEdit {
            background: white;
            border: 1px solid %2;
            border-radius: 3px;
            padding: 4px 6px;
            font-family: %4;
            font-size: %6pt;
            color: %3;
        }
        #LessonBody QLineEdit:focus, #LessonBody QTextEdit:focus {
            border: 1px solid %7;
        }
    )").arg(Colors::LessonBodyBg,
            Colors::LessonBorder,
            Colors::TextPrimary,
            Fonts::Family,
            QString::number(Fonts::LessonLabel),
            QString::number(Fonts::LessonInput),
            Colors::CardBorderSelected);
}

// ── Stylesheet: Status Bar ─────────────────────────────────
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
            border: 1px solid %2;
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
    )").arg(Colors::StatusBarBg,
            Colors::StatusBarBorder,
            Colors::StatusBarText,
            Fonts::Family,
            QString::number(Fonts::StatusBar),
            Colors::SliderGroove,
            Colors::SliderHandle,
            Colors::SliderHandleHover);
}

// ── Stylesheet: Tab Header ─────────────────────────────────
inline QString tabHeaderStyle()
{
    return QStringLiteral(R"(
        #TabHeader {
            background: %1;
            border-bottom: 1px solid %2;
            padding: 4px 12px;
            min-height: 28px;
            max-height: 28px;
        }
        #TabHeader QLabel {
            color: %3;
            font-family: %4;
            font-size: %5pt;
            font-weight: bold;
            background: transparent;
            border: none;
        }
    )").arg(Colors::TabHeaderBg,
            Colors::CardBorder,
            Colors::TabHeaderText,
            Fonts::Family,
            QString::number(Fonts::TabHeader));
}

// ── Stylesheet: ScrollArea ─────────────────────────────────
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
            background: %1;
            width: 8px;
            margin: 0;
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
            Colors::TextSecondary);
}

} // namespace Styles
} // namespace LabMonitor
