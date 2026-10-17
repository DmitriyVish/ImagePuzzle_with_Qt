#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include <QImage>
#include <QString>

/** @brief Результат фонового чтения; при неудаче image пуст, error заполнен. */
struct ImageLoadResult {
    QImage image; ///< Рабочее изображение с применённой ориентацией.
    QString error; ///< Описание ошибки для пользователя.
};

/** @brief Потокобезопасная загрузка растровых файлов и ресурсов без доступа к виджетам. */
class ImageLoader
{
public:
    /** @brief Читает первый кадр и ограничивает рабочее разрешение.
     * @param path Путь файла или ресурса Qt. @return Изображение либо описание ошибки. */
    static ImageLoadResult load(const QString &path);
};
#endif
