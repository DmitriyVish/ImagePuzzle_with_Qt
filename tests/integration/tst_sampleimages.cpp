#include "core/puzzlegenerator.h"
#include "core/puzzlemodel.h"
#include "io/imageloader.h"
#include "io/sampleimages.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QImageReader>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

/** @brief Объединяет музейные фотографии и встроенные только в тесты фикстуры. @return Пути для полного прогона декодеров. */
static QStringList testImagePaths()
{
    QStringList paths = SampleImages::filePaths();
    const QDir fixtures(QStringLiteral(":/imagepuzzle/images"));
    for (const QString &name : fixtures.entryList(QDir::Files, QDir::Name))
        paths.append(fixtures.filePath(name));
    return paths;
}

/** @brief Проверяет реальные декодеры, EXIF и полный цикл модели на тестовых фотографиях. */
class ImageIntegrationTest : public QObject
{
    Q_OBJECT
private slots:
    /** @brief Проверяет независимость ресурсов от рабочего каталога. */
    void resourcePaths();
    /** @brief Набор всех встроенных изображений, включая PNG. */
    void readsSamples_data();
    /** @brief Читает реальные изображения через рабочий загрузчик. */
    void readsSamples();
    /** @brief Форматы, для которых локальный Qt предоставляет запись и чтение. */
    void readsFormats_data();
    /** @brief Создаёт файл выбранного формата и читает его. */
    void readsFormats();
    /** @brief Проверяет JPEG с поворотом EXIF и ограничение разрешения. */
    void orientationAndSize();
    /** @brief Проверяет отсутствующий, повреждённый и анимированный файл. */
    void invalidAndGif();
    /** @brief Выбирает фотографии для полного цикла. */
    void solvesPhotos_data();
    /** @brief Загружает фотографию, создаёт разные n и собирает каждый пазл. */
    void solvesPhotos();
};
void ImageIntegrationTest::resourcePaths()
{
    const QStringList paths = SampleImages::filePaths();
    QCOMPARE(paths.size(), 3);
    for (const QString &path : paths)
        QVERIFY(path.startsWith(SampleImages::directoryPath() + QLatin1Char('/')));
    QVERIFY(QFile::exists(QStringLiteral(":/imagepuzzle/images/cat1.jpg")));
    QVERIFY(testImagePaths().size() > paths.size());
    QTemporaryDir directory(QDir(QCoreApplication::applicationDirPath()).filePath("cwd-XXXXXX"));
    QVERIFY(directory.isValid());
    const QString previous = QDir::currentPath();
    const auto restore = qScopeGuard([previous] { QDir::setCurrent(previous); });
    QVERIFY(QDir::setCurrent(directory.path()));
    QCOMPARE(SampleImages::filePaths(), paths);
    QVERIFY(!ImageLoader::load(paths.front()).image.isNull());
}
void ImageIntegrationTest::readsSamples_data()
{
    QTest::addColumn<QString>("path");
    for (const QString &path : testImagePaths())
        QTest::newRow(qPrintable(path)) << path;
}
void ImageIntegrationTest::readsSamples()
{
    QFETCH(QString, path);
    const auto result = ImageLoader::load(path);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY(!result.image.isNull());
    QVERIFY(result.image.width() <= 1600 && result.image.height() <= 1600);
}
void ImageIntegrationTest::readsFormats_data()
{
    QTest::addColumn<QByteArray>("format");
    const auto supported = QImageWriter::supportedImageFormats();
    for (const QByteArray &format : {QByteArray("png"), QByteArray("jpg"), QByteArray("bmp"), QByteArray("webp"), QByteArray("tiff"), QByteArray("ppm")}) {
        if (supported.contains(format))
            QTest::newRow(format.constData()) << format;
    }
}
void ImageIntegrationTest::readsFormats()
{
    QFETCH(QByteArray, format);
    QTemporaryDir directory(QDir(QCoreApplication::applicationDirPath()).filePath("format-XXXXXX"));
    QVERIFY(directory.isValid());
    QImage image(64, 48, QImage::Format_ARGB32);
    image.fill(qRgba(240, 20, 150, 128));
    const QString path = directory.filePath(QStringLiteral("sample.") + QString::fromLatin1(format));
    QVERIFY(image.save(path, format.constData()));
    const auto result = ImageLoader::load(path);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.image.size(), image.size());
    if (format == "png")
        QCOMPARE(result.image.pixelColor(0, 0).alpha(), 128);
}
void ImageIntegrationTest::orientationAndSize()
{
    QTemporaryDir directory(QDir(QCoreApplication::applicationDirPath()).filePath("orientation-XXXXXX"));
    QVERIFY(directory.isValid());
    QImage image(80, 40, QImage::Format_RGB32);
    image.fill(Qt::red);
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(image.save(&buffer, "JPEG"));
    // IFD Orientation=6 задаёт поворот на 90 градусов по часовой стрелке.
    const QByteArray exif = QByteArray::fromHex("45786966000049492a0008000000010012010300010000000600000000000000");
    QByteArray segment("\xff\xe1", 2);
    const int length = int(exif.size()) + 2;
    segment.append(char(length >> 8)); segment.append(char(length & 255)); segment += exif;
    jpeg.insert(2, segment);
    QFile file(directory.filePath("rotated.jpg"));
    QVERIFY(file.open(QIODevice::WriteOnly)); QVERIFY(file.write(jpeg) == jpeg.size()); file.close();
    QCOMPARE(ImageLoader::load(file.fileName()).image.size(), QSize(40, 80));
    QImage large(2400, 1200, QImage::Format_RGB32);
    large.fill(Qt::blue);
    QVERIFY(large.save(directory.filePath("large.png")));
    QCOMPARE(ImageLoader::load(directory.filePath("large.png")).image.size(), QSize(1600, 800));
    const int sof = jpeg.indexOf(QByteArray("\xff\xc0", 2));
    QVERIFY(sof >= 0);
    jpeg[sof + 5] = char(10000 >> 8); jpeg[sof + 6] = char(10000 & 255);
    jpeg[sof + 7] = char(11000 >> 8); jpeg[sof + 8] = char(11000 & 255);
    QFile huge(directory.filePath("huge.jpg"));
    QVERIFY(huge.open(QIODevice::WriteOnly)); huge.write(jpeg); huge.close();
    QVERIFY(ImageLoader::load(huge.fileName()).error.contains(QStringLiteral("100")));
}
void ImageIntegrationTest::invalidAndGif()
{
    QVERIFY(!ImageLoader::load(QStringLiteral(":/missing-image.png")).error.isEmpty());
    QTemporaryDir directory(QDir(QCoreApplication::applicationDirPath()).filePath("bad-XXXXXX"));
    QVERIFY(directory.isValid());
    QFile file(directory.filePath("bad.jpg"));
    QVERIFY(file.open(QIODevice::WriteOnly)); file.write("not an image"); file.close();
    QVERIFY(!ImageLoader::load(file.fileName()).error.isEmpty());
    QFile gif(directory.filePath("frame.gif"));
    QVERIFY(gif.open(QIODevice::WriteOnly));
    gif.write(QByteArray::fromBase64("R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7")); gif.close();
    QCOMPARE(ImageLoader::load(gif.fileName()).image.size(), QSize(1, 1));
    QVERIFY(gif.open(QIODevice::WriteOnly | QIODevice::Truncate));
    // Два кадра 1x1, сначала чёрный, затем белый: загрузчик должен выбрать первый.
    gif.write(QByteArray::fromHex("47494638396101000100800000000000ffffff"
                                "21f904000a0000002c0000000001000100000202440100"
                                "21f904000a0000002c00000000010001000002024c01003b"));
    gif.close();
    QImageReader animation(gif.fileName());
    const QImage first = animation.read();
    const QImage second = animation.read();
    QVERIFY(!first.isNull() && !second.isNull());
    QVERIFY(first.pixelColor(0, 0) != second.pixelColor(0, 0));
    QCOMPARE(ImageLoader::load(gif.fileName()).image.pixelColor(0, 0), first.pixelColor(0, 0));
}
void ImageIntegrationTest::solvesPhotos_data()
{
    QTest::addColumn<QString>("path");
    for (const QString &path : testImagePaths()) {
        if (QFileInfo(path).fileName().startsWith("cat") || path.contains(QStringLiteral("/museum/")))
            QTest::newRow(qPrintable(path)) << path;
    }
}
void ImageIntegrationTest::solvesPhotos()
{
    QFETCH(QString, path);
    const auto loaded = ImageLoader::load(path);
    QVERIFY(!loaded.image.isNull());
    const int capacity = PuzzleGenerator::maximumPieces(loaded.image.size());
    for (int count : {4, 17, capacity}) {
        PuzzleModel model;
        QSignalSpy completed(&model, &PuzzleModel::completed);
        QVERIFY(model.start(loaded.image, count, 20261017u));
        QCOMPARE(model.pieces().size(), count);
        for (int id = 0; id < count; ++id)
            QVERIFY(model.place(id, model.pieces().at(id).source.topLeft(), 0));
        QCOMPARE(model.placedCount(), count);
        QCOMPARE(completed.size(), 1);
    }
}
QTEST_GUILESS_MAIN(ImageIntegrationTest)
#include "tst_sampleimages.moc"
