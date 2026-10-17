#ifndef SAMPLEIMAGES_H
#define SAMPLEIMAGES_H

#include <QString>
#include <QStringList>

namespace SampleImages {

/** @brief Возвращает каталог встроенных музейных фотографий. @return Путь Qt-ресурса. */
QString directoryPath();
/** @brief Перечисляет только фотографии музея в порядке показа посетителю. @return Пути Qt-ресурсов. */
QStringList filePaths();

}

#endif // SAMPLEIMAGES_H
