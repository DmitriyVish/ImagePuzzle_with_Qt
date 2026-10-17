#ifndef EXPEDITIONTHEME_H
#define EXPEDITIONTHEME_H
#include <QColor>
class QWidget;

/** @brief Общие цвета и типографика музейного оформления «Северная экспедиция». */
namespace ExpeditionTheme {
/** @brief Применяет иконку, общие шрифт и палитру QApplication и стили окна в GUI-потоке. @param window Окно-владелец. */
void apply(QWidget *window);
/** @brief Возвращает нейтральную подложку игровой сцены. @return Цвет фона. */
QColor ground();
/** @brief Возвращает цвет тонкой границы поверхностей. @return Цвет границы. */
QColor outline();
}
#endif
