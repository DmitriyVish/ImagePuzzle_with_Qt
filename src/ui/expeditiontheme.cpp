#include "expeditiontheme.h"
#include <QApplication>
#include <QFontDatabase>
#include <QLoggingCategory>
#include <QPalette>
#include <QWidget>
#include <QIcon>

Q_LOGGING_CATEGORY(lcTheme, "imagepuzzle.ui.theme")

QColor ExpeditionTheme::ground() { return QColor(QStringLiteral("#EAE5DA")); }
QColor ExpeditionTheme::outline() { return QColor(QStringLiteral("#C9C8BD")); }
void ExpeditionTheme::apply(QWidget *window)
{
    // Ресурсный ICO содержит отдельные размеры для заголовка, панели задач и высокого DPI.
    const QIcon icon(QStringLiteral(":/imagepuzzle/assets/icons/imagepuzzle.ico"));
    if (icon.isNull())
        qCWarning(lcTheme) << "Application icon unavailable";
    else {
        QApplication::setWindowIcon(icon);
        window->setWindowIcon(icon);
    }
    // Один шрифт разделяется всеми окнами; системный увеличенный размер сохраняется.
    static const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/imagepuzzle/assets/fonts/Geologica.ttf"));
    QFont font = QApplication::font();
    if (fontId >= 0)
        font.setFamily(QFontDatabase::applicationFontFamilies(fontId).value(0, font.family()));
    else
        qCWarning(lcTheme) << "Geologica unavailable; using system font";
    font.setPointSizeF(qMax(14.0, font.pointSizeF()));
    // Qt Style Sheets берёт исходный шрифт контролов из QApplication, включая файловые диалоги.
    QApplication::setFont(font);
    window->setFont(font);
    QPalette palette = window->palette();
    palette.setColor(QPalette::Window, QColor("#F7F4EE"));
    palette.setColor(QPalette::WindowText, QColor("#24332D"));
    palette.setColor(QPalette::Text, QColor("#24332D"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, ground());
    palette.setColor(QPalette::Button, Qt::white);
    palette.setColor(QPalette::ButtonText, QColor("#24332D"));
    palette.setColor(QPalette::Highlight, QColor("#A64B20"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Mid, outline());
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#74796F"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#74796F"));
    QApplication::setPalette(palette);
    window->setPalette(palette);
    window->setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#centralwidget { background: #F7F4EE; }
        QLabel { background: transparent; }
        QLabel[role="eyebrow"], QLabel[role="muted"] { color: #57635C; }
        QFrame#brandHeader { border-bottom: 1px solid #D8D9CE; }
        QPushButton { background: #FFFFFF; color: #24332D; border: 1px solid #D1D3C8;
                      border-radius: 14px; padding: 8px 14px; }
        QPushButton:hover { background: #F0EADF; border-color: #A64B20; }
        QPushButton:pressed { background: #E7D5C3; }
        QPushButton:focus, QComboBox:focus, QSpinBox:focus, QCheckBox:focus { border: 2px solid #27624C; }
        QPushButton[role="primary"] { background: #A64B20; color: white; border-color: #A64B20; }
        QPushButton[role="primary"]:hover { background: #8D3D18; }
        QPushButton[role="primary"]:pressed { background: #713115; }
        QPushButton:disabled { background: #E5E4DC; color: #74796F; border-color: #D1D3C8; }
        QPushButton:checked { background: #24332D; color: white; border-color: #24332D; }
        QSpinBox, QComboBox { background: white; color: #24332D; border: 1px solid #D1D3C8;
                             border-radius: 12px; padding: 8px 12px; selection-background-color: #A64B20; }
        QComboBox::drop-down { border: 0; width: 28px; }
        QComboBox QAbstractItemView { background: white; color: #24332D; selection-background-color: #A64B20;
                                    selection-color: white; padding: 4px; }
        QSpinBox::up-button, QSpinBox::down-button { width: 28px; }
        QProgressBar { background: #E4DED2; border: 0; border-radius: 5px; }
        QProgressBar::chunk { background: #27624C; border-radius: 5px; }
        QLabel#completionLabel { background: #E2EDDF; color: #204E3A; border-radius: 14px; padding: 12px; }
        QGraphicsView { border: 1px solid #D1D3C8; border-radius: 16px; background: #EAE5DA; }
        QCheckBox { color: #57635C; spacing: 8px; padding: 6px; }
        QCheckBox::indicator { width: 20px; height: 20px; }
        QStatusBar { background: #F7F4EE; color: #57635C; }
    )"));
    qCInfo(lcTheme) << "Northern Expedition theme applied, font:" << font.family() << font.pointSizeF();
}
