#ifndef SELFCHECK_H
#define SELFCHECK_H
#include <QString>
/** @brief Проверка установленной сборки без Qt Test и без показа окна. */
namespace SelfCheck {
/** @brief Проверяет отсутствие тестовых ресурсов, читает музейные фотографии и собирает пазлы через модель.
 * @param reportPath Необязательный путь отчёта JSON. @return 0 при успехе, 1 при ошибке. */
int run(const QString &reportPath);
}
#endif
