#include "imageloader.h"
#include "core/puzzlegenerator.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcImages, "imagepuzzle.io")

ImageLoadResult ImageLoader::load(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize sourceSize = reader.size();
    ImageLoadResult result;
    if (sourceSize.isValid() && qint64(sourceSize.width()) * sourceSize.height() > 100000000) {
        result.error = QCoreApplication::translate("ImageLoader", "Изображение слишком велико: предел — 100 миллионов пикселей.");
    } else {
        // Декодер по возможности уменьшает изображение до выделения полного буфера.
        if (sourceSize.width() > PuzzleGenerator::MaximumImageSide
            || sourceSize.height() > PuzzleGenerator::MaximumImageSide) {
            reader.setScaledSize(sourceSize.scaled(PuzzleGenerator::MaximumImageSide,
                                                   PuzzleGenerator::MaximumImageSide, Qt::KeepAspectRatio));
        }
        result.image = reader.read();
        if (result.image.isNull())
            result.error = QCoreApplication::translate("ImageLoader", "Не удалось открыть изображение: %1").arg(reader.errorString());
        else {
            if (result.image.width() > PuzzleGenerator::MaximumImageSide || result.image.height() > PuzzleGenerator::MaximumImageSide)
                result.image = result.image.scaled(PuzzleGenerator::MaximumImageSide, PuzzleGenerator::MaximumImageSide,
                                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
            result.image = result.image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
    }
    if (!result.error.isEmpty())
        qCWarning(lcImages) << "Image load failed:" << QFileInfo(path).fileName() << result.error;
    else
        qCInfo(lcImages) << "Image loaded:" << QFileInfo(path).fileName() << "size:" << result.image.size();
    return result;
}
