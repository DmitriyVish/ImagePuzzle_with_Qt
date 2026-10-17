#include "core/puzzlegenerator.h"
#include "core/puzzlemodel.h"
#include <QSignalSpy>
#include <QTest>
#include <limits>
#include <QPainter>
#include <QLineF>

/** @brief Проверяет геометрию, ограничения, закрепление и сброс независимо от интерфейса. */
class PuzzleTest : public QObject
{
    Q_OBJECT
private slots:
    /** @brief Набор обычных, предельных, простых n и вытянутых размеров. */
    void coverage_data();
    /** @brief Доказывает покрытие каждого пикселя ровно одной деталью. */
    void coverage();
    /** @brief Проверяет воспроизводимость и отказ на некорректных параметрах. */
    void seedAndValidation();
    /** @brief Проверяет отпускание, запрет повторного закрепления, победу и повтор игры. */
    void placementAndRestart();
    /** @brief Проверяет компактность лотка, отсутствие перекрытий и обратное уменьшение детали. */
    void compactTray();
    /** @brief Проверяет покрытие фигурными деталями, совпадение границ и плавность кубических стыков. */
    void curvedCoverage();
    /** @brief Задаёт нечисловые координаты, недопустимые id и допуски. */
    void invalidDrops_data();
    /** @brief Проверяет атомарный отказ без изменения модели и сигналов. */
    void invalidDrops();
    /** @brief Проверяет точную границу евклидова допуска и отсутствие повторного прогресса. */
    void snapToleranceBoundary();
};

void PuzzleTest::invalidDrops_data()
{
    QTest::addColumn<int>("id");
    QTest::addColumn<QPointF>("position");
    QTest::addColumn<qreal>("tolerance");
    const qreal nan = std::numeric_limits<qreal>::quiet_NaN();
    const qreal inf = std::numeric_limits<qreal>::infinity();
    QTest::newRow("negative-id") << -1 << QPointF() << qreal(0);
    QTest::newRow("past-last-id") << 4 << QPointF() << qreal(0);
    QTest::newRow("nan-x") << 0 << QPointF(nan, 0) << qreal(0);
    QTest::newRow("nan-y") << 0 << QPointF(0, nan) << qreal(0);
    QTest::newRow("infinite-x") << 0 << QPointF(inf, 0) << qreal(0);
    QTest::newRow("infinite-y") << 0 << QPointF(0, -inf) << qreal(0);
    QTest::newRow("negative-tolerance") << 0 << QPointF() << qreal(-1);
    QTest::newRow("nan-tolerance") << 0 << QPointF() << nan;
    QTest::newRow("infinite-tolerance") << 0 << QPointF() << inf;
    QTest::newRow("outside-workspace") << 0 << QPointF(-10000, -10000) << qreal(0);
}
void PuzzleTest::invalidDrops()
{
    QFETCH(int, id);
    QFETCH(QPointF, position);
    QFETCH(qreal, tolerance);
    QImage image(320, 240, QImage::Format_RGB32);
    image.fill(Qt::green);
    PuzzleModel model;
    QVERIFY(model.start(image, 4, 42));
    const auto before = model.pieces();
    QSignalSpy changes(&model, &PuzzleModel::pieceChanged);
    QSignalSpy progress(&model, &PuzzleModel::progressChanged);
    QSignalSpy completion(&model, &PuzzleModel::completed);
    QVERIFY(!model.place(id, position, tolerance));
    QCOMPARE(model.placedCount(), 0);
    QCOMPARE(changes.size(), 0);
    QCOMPARE(progress.size(), 0);
    QCOMPARE(completion.size(), 0);
    for (int index = 0; index < before.size(); ++index) {
        QCOMPARE(model.pieces().at(index).position, before.at(index).position);
        QCOMPARE(model.pieces().at(index).scale, before.at(index).scale);
        QVERIFY(!model.pieces().at(index).placed);
    }
}
void PuzzleTest::snapToleranceBoundary()
{
    QImage image(320, 240, QImage::Format_RGB32);
    image.fill(Qt::green);
    PuzzleModel model;
    QVERIFY(model.start(image, 4, 42));
    const QPointF target = model.pieces().front().source.topLeft();
    // Смещение 3–4–5 проверяет круговой допуск, а не прямоугольную область.
    QPointF delta(3, 4);
    if (target.x() > 0) delta.setX(-3);
    if (target.y() > 0) delta.setY(-4);
    QSignalSpy progress(&model, &PuzzleModel::progressChanged);
    QVERIFY(model.place(0, target + delta, 4.999));
    QVERIFY(!model.pieces().front().placed);
    QCOMPARE(progress.size(), 0);
    QVERIFY(model.place(0, target + delta, 5.0));
    QVERIFY(model.pieces().front().placed);
    QCOMPARE(model.pieces().front().position, target);
    QCOMPARE(progress.size(), 1);
    QVERIFY(!model.place(0, target, 5));
    QCOMPARE(progress.size(), 1);
}
void PuzzleTest::coverage_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<int>("count");
    for (int count = 4; count <= 20; ++count)
        QTest::newRow(qPrintable(QString::number(count))) << QSize(640, 480) << count;
    QTest::newRow("wide") << QSize(1600, 32) << 17;
    QTest::newRow("tall") << QSize(32, 1600) << 17;
    QTest::newRow("capacity") << QSize(32, 32) << 4;
    QTest::newRow("small") << QSize(16, 64) << 4;
}
void PuzzleTest::coverage()
{
    QFETCH(QSize, size);
    QFETCH(int, count);
    const PuzzleLayout layout = PuzzleGenerator::generate(size, count, 12345);
    QVERIFY2(layout.error.isEmpty(), qPrintable(layout.error));
    QCOMPARE(layout.fragments.size(), count);
    QVector<quint8> pixels(size.width() * size.height(), 0);
    for (const QRect &rectangle : layout.fragments) {
        QVERIFY(QRect(QPoint(), size).contains(rectangle));
        QVERIFY(rectangle.width() >= PuzzleGenerator::MinimumSide);
        QVERIFY(rectangle.height() >= PuzzleGenerator::MinimumSide);
        for (int y = rectangle.top(); y <= rectangle.bottom(); ++y) {
            for (int x = rectangle.left(); x <= rectangle.right(); ++x) {
                quint8 &coverage = pixels[y * size.width() + x];
                QVERIFY2(coverage == 0, "Fragments overlap.");
                ++coverage;
            }
        }
    }
    for (quint8 coverage : pixels)
        QCOMPARE(coverage, quint8(1));
}
void PuzzleTest::seedAndValidation()
{
    const auto first = PuzzleGenerator::generate(QSize(640, 480), 17, 42);
    QCOMPARE(first.fragments, PuzzleGenerator::generate(QSize(640, 480), 17, 42).fragments);
    QCOMPARE(first.shapes, PuzzleGenerator::generate(QSize(640, 480), 17, 42).shapes);
    QVERIFY(first.fragments != PuzzleGenerator::generate(QSize(640, 480), 17, 43).fragments);
    QCOMPARE(PuzzleGenerator::maximumPieces(QSize(640, 480)), 20);
    QCOMPARE(PuzzleGenerator::maximumPieces(QSize(32, 32)), 4);
    for (int count : {-1, 0, 1, 2, 3, 21, 1000}) {
        const auto invalid = PuzzleGenerator::generate(QSize(640, 480), count, 42);
        QVERIFY(invalid.fragments.isEmpty());
        QVERIFY(!invalid.error.isEmpty());
    }
    QVERIFY(!PuzzleGenerator::generate(QSize(16, 48), 4, 0).error.isEmpty());
    QVERIFY(!PuzzleGenerator::generate(QSize(), 4, 0).error.isEmpty());
    QVERIFY(!PuzzleGenerator::generate(QSize(2000, 2000), 4, 0).error.isEmpty());
}
void PuzzleTest::placementAndRestart()
{
    QImage image(320, 240, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::green);
    PuzzleModel model;
    QSignalSpy victory(&model, &PuzzleModel::completed);
    QVERIFY(model.start(image, 17, 42));
    const auto original = model.pieces();
    for (int i = 0; i < original.size(); ++i) {
        const QRectF rectangle(original.at(i).position, QSizeF(original.at(i).source.size()) * original.at(i).scale);
        QVERIFY(model.trayRect().contains(rectangle));
        for (int j = 0; j < i; ++j)
            QVERIFY(!rectangle.intersects(QRectF(original.at(j).position, QSizeF(original.at(j).source.size()) * original.at(j).scale)));
    }
    QVERIFY(!model.place(-1, {}, 5));
    QVERIFY(!model.place(0, QPointF(-1000, -1000), 5));
    QVERIFY(!model.place(0, QPointF(std::numeric_limits<double>::quiet_NaN(), 0), 5));
    QCOMPARE(model.pieces().at(0).position, original.at(0).position);
    for (int id = 0; id < model.pieces().size(); ++id) {
        // Угловая деталь должна закрепляться даже при отпускании чуть снаружи поля.
        const QPointF nearTarget = QPointF(model.pieces().at(id).source.topLeft()) - QPointF(1, 1);
        QVERIFY(model.place(id, nearTarget, 3));
        QVERIFY(model.pieces().at(id).placed);
        QCOMPARE(model.pieces().at(id).position, QPointF(model.pieces().at(id).source.topLeft()));
    }
    QCOMPARE(model.placedCount(), 17);
    QCOMPARE(victory.size(), 1);
    QVERIFY(!model.place(0, original.at(0).position, 3));
    QCOMPARE(victory.size(), 1);
    model.restart(7);
    QCOMPARE(model.placedCount(), 0);
    QCOMPARE(model.seed(), quint32(42));
    bool moved = false;
    for (int i = 0; i < original.size(); ++i) {
        QCOMPARE(model.pieces().at(i).source, original.at(i).source);
        QVERIFY(!model.pieces().at(i).placed);
        moved |= model.pieces().at(i).position != original.at(i).position;
    }
    QVERIFY(moved);
    QString error;
    QVERIFY(!model.start({}, 17, 1, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(model.pieces().size(), 17);
    for (int count : {3, 21}) {
        QVERIFY(!model.start(image, count, 1, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(model.pieces().size(), 17);
    }
}
void PuzzleTest::compactTray()
{
    for (const QSize size : {QSize(1280, 853), QSize(1024, 1024), QSize(480, 900), QSize(1600, 32), QSize(32, 1600), QSize(32, 32), QSize(16, 64)}) {
        QImage image(size, QImage::Format_RGB32);
        image.fill(Qt::green);
        for (int count = 4; count <= PuzzleGenerator::maximumPieces(size); ++count) {
            for (quint32 seed = 0; seed < 12; ++seed) {
                PuzzleModel model;
                QVERIFY(model.start(image, count, seed));
                QVERIFY(model.trayRect().height() > 0 && model.trayRect().height() <= size.height());
                if (size.width() >= 64)
                    QVERIFY(model.trayRect().width() <= size.width() * 0.55);
                const auto pieces = model.pieces();
                for (int i = 0; i < pieces.size(); ++i) {
                    const auto &piece = pieces.at(i);
                    QVERIFY(piece.scale > 0 && piece.scale <= 0.42);
                    const QRectF rectangle(piece.position, QSizeF(piece.source.size()) * piece.scale);
                    QVERIFY(model.trayRect().adjusted(-0.00001, -0.00001, 0.00001, 0.00001).contains(rectangle));
                    for (int j = 0; j < i; ++j)
                        QVERIFY(!rectangle.intersects(QRectF(pieces.at(j).position, QSizeF(pieces.at(j).source.size()) * pieces.at(j).scale)));
                }
                const auto &first = pieces.front();
                const QPointF sourceCenter = QRectF(QPointF(), first.source.size()).center();
                QVERIFY(model.place(0, model.trayRect().center() - sourceCenter, 0));
                QVERIFY(model.pieces().front().scale < 1);
                QVERIFY(!model.pieces().front().placed);
                QVERIFY(model.place(0, first.source.topLeft(), 0));
                QCOMPARE(model.pieces().front().scale, 1.0);
                QVERIFY(model.pieces().front().placed);
            }
        }
    }
}
void PuzzleTest::curvedCoverage()
{
    for (const QSize size : {QSize(160, 120), QSize(320, 240), QSize(16, 64), QSize(32, 32), QSize(1600, 32), QSize(32, 1600)}) {
        for (int count : {4, 7, 17, 20}) {
            if (count > PuzzleGenerator::maximumPieces(size)) continue;
            for (quint32 seed = 0; seed < 8; ++seed) {
                const auto layout = PuzzleGenerator::generate(size, count, seed);
                QCOMPARE(layout.shapes.size(), count);
                QImage coverage(size, QImage::Format_RGB32);
                coverage.fill(Qt::black);
                QPainter painter(&coverage);
                painter.setRenderHint(QPainter::Antialiasing, false);
                painter.setCompositionMode(QPainter::CompositionMode_Plus);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(1, 0, 0));
                int curves = 0;
                QVector<QVector<QPointF>> cubics;
                for (int id = 0; id < count; ++id) {
                    const auto &shape = layout.shapes.at(id);
                    QVERIFY(QRectF(QPointF(), size).adjusted(-0.00001, -0.00001, 0.00001, 0.00001).contains(shape.boundingRect()));
                    QVERIFY(shape.contains(QRectF(layout.fragments.at(id)).center()));
                    painter.drawPath(shape);
                    for (int i = 0; i + 2 < shape.elementCount(); ++i) {
                        if (!shape.elementAt(i).isCurveTo()) continue;
                        ++curves;
                        cubics.append({QPointF(shape.elementAt(i - 1)), QPointF(shape.elementAt(i)),
                                       QPointF(shape.elementAt(i + 1)), QPointF(shape.elementAt(i + 2))});
                        if (i + 3 >= shape.elementCount() || !shape.elementAt(i + 3).isCurveTo()) continue;
                        const QPointF end = shape.elementAt(i + 2);
                        const QPointF before = end - QPointF(shape.elementAt(i + 1));
                        const QPointF after = QPointF(shape.elementAt(i + 3)) - end;
                        QVERIFY(QLineF(before, after).length() < 0.00001);
                    }
                }
                painter.end();
                QVERIFY(curves > 0);
                for (int i = 0; i < cubics.size(); ++i) {
                    int reversed = 0;
                    for (int j = 0; j < cubics.size(); ++j) {
                        if (i == j) continue;
                        if (cubics[i][0] == cubics[j][3] && cubics[i][1] == cubics[j][2]
                            && cubics[i][2] == cubics[j][1] && cubics[i][3] == cubics[j][0]) ++reversed;
                    }
                    QCOMPARE(reversed, 1);
                }
                // Qt округляет кривую и её обратный обход к пикселям немного по-разному.
                // Для спорного растрового пикселя проверяем принадлежность настоящим контурам,
                // чтобы отличить погрешность растеризации от геометрического пропуска/наложения.
                for (int y = 0; y < size.height(); ++y) {
                    const auto *row = reinterpret_cast<const QRgb *>(coverage.constScanLine(y));
                    for (int x = 0; x < size.width(); ++x) {
                        if (qRed(row[x]) == 1) continue;
                        int containing = 0;
                        for (const auto &shape : layout.shapes) containing += shape.contains(QPointF(x + 0.5, y + 0.5));
                        QVERIFY2(containing == 1, qPrintable(QStringLiteral("Geometric coverage %1 at %2,%3; %4x%5 n=%6 seed=%7")
                            .arg(containing).arg(x).arg(y).arg(size.width()).arg(size.height()).arg(count).arg(seed)));
                    }
                }
            }
        }
    }
}
QTEST_GUILESS_MAIN(PuzzleTest)
#include "tst_puzzle.moc"
