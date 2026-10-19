#include "selfcheck.h"
#include "core/puzzlegenerator.h"
#include "core/puzzlemodel.h"
#include "io/imageloader.h"
#include "io/sampleimages.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QDir>

Q_LOGGING_CATEGORY(lcSelfCheck, "imagepuzzle.check")

int SelfCheck::run(const QString &reportPath)
{
    int images = 0;
    int puzzles = 0;
    QJsonArray errors;
    const bool testImagesEmbedded = QDir(QStringLiteral(":/imagepuzzle/images")).exists();
    if (testImagesEmbedded)
        errors.append(QStringLiteral("Test image resources must not be embedded in the application"));
    const QStringList paths = SampleImages::filePaths();
    if (paths.isEmpty())
        errors.append(QStringLiteral("No sample resources"));
    for (const QString &path : paths) {
        const ImageLoadResult loaded = ImageLoader::load(path);
        if (!loaded.error.isEmpty()) {
            errors.append(path + QStringLiteral(": ") + loaded.error);
            continue;
        }
        ++images;
        if (PuzzleGenerator::maximumPieces(loaded.image.size()) < 17)
            continue;
        PuzzleModel model;
        QString error;
        if (!model.start(loaded.image, 17, 20261017u, &error)) {
            errors.append(error);
            continue;
        }
        // Используется тот же контракт отпускания, что и у игрового контроллера.
        for (int id = 0; id < model.pieces().size(); ++id) {
            model.rotate(id, -model.pieces().at(id).rotation / 90);
            model.place(id, model.pieces().at(id).source.topLeft(), 0);
        }
        if (model.placedCount() != 17)
            errors.append(path + QStringLiteral(": incomplete puzzle"));
        else
            ++puzzles;
    }
    if (puzzles == 0)
        errors.append(QStringLiteral("No playable sample images"));
    const QJsonObject report{{QStringLiteral("status"), errors.isEmpty() ? QStringLiteral("passed") : QStringLiteral("failed")},
                             {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
                             {QStringLiteral("imagesRead"), images}, {QStringLiteral("puzzlesSolved"), puzzles},
                             {QStringLiteral("piecesPerPuzzle"), 17}, {QStringLiteral("errors"), errors},
                             {QStringLiteral("testImageResourcesPresent"), testImagesEmbedded},
                             {QStringLiteral("imageResources"), QJsonArray::fromStringList(paths)}};
    if (!reportPath.isEmpty()) {
        QSaveFile file(reportPath);
        const QByteArray contents = QJsonDocument(report).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() || !file.commit()) {
            qCCritical(lcSelfCheck) << "Cannot save self-check report:" << file.errorString();
            return 1;
        }
    }
    qCInfo(lcSelfCheck) << "Self-check:" << images << "images," << puzzles << "puzzles, errors:" << errors.size();
    return errors.isEmpty() ? 0 : 1;
}
