#include "sampleimages.h"

namespace SampleImages {

QString directoryPath()
{
    return QStringLiteral(":/imagepuzzle/assets/museum");
}

QStringList filePaths()
{
    // Список посетителя не зависит от наличия фикстур в тестовом процессе.
    return {QStringLiteral(":/imagepuzzle/assets/museum/kunitsina-estate.jpg"),
                      QStringLiteral(":/imagepuzzle/assets/museum/korely-summer.jpg"),
                      QStringLiteral(":/imagepuzzle/assets/museum/korely-winter.jpg")};
}

}
