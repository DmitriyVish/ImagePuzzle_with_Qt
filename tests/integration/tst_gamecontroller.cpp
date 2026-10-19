#include "app/gamecontroller.h"
#include <QPointer>
#include <QSignalSpy>
#include <QTest>

/** @brief Проверяет совместную работу модели, игрового контроллера и автомата состояний. */
class GameControllerTest : public QObject
{
    Q_OBJECT
private slots:
    /** @brief Задаёт все разрешённые количества деталей. */
    void completesAllCounts_data();
    /** @brief Проверяет отложенную подготовку, прогресс и единственное завершение. */
    void completesAllCounts();
    /** @brief Задаёт некорректные изображения и количества. */
    void rejectsPreparation_data();
    /** @brief Проверяет возврат из ошибочной подготовки и возможность следующего старта. */
    void rejectsPreparation();
    /** @brief Проверяет отмену старого запроса до запуска нового. */
    void cancellationAndReplacement();
    /** @brief Проверяет повтор геометрии, новое разбиение и запрет действий вне игры. */
    void restartRegenerateAndGuards();
    /** @brief Проверяет удаление контроллера до срабатывания его таймера. */
    void destructionDuringPreparation();
    /** @brief Проверяет, что команды вращения проходят к модели только в состоянии Playing. */
    void rotationStateGuards();
private:
    /** @brief Создаёт небольшую рабочую фотографию. @return Непустое изображение. */
    static QImage image();
};

QImage GameControllerTest::image()
{
    QImage result(320, 240, QImage::Format_RGB32);
    result.fill(Qt::cyan);
    return result;
}

void GameControllerTest::rotationStateGuards()
{
    AppController flow;
    GameController game(&flow);
    auto *model = game.model();
    QSignalSpy rotations(model, &PuzzleModel::pieceRotated);
    game.rotatePiece(0, 1);
    QCOMPARE(rotations.size(), 0);
    QVERIFY(game.start(image(), 4, 42));
    game.rotatePiece(0, 1);
    QCOMPARE(rotations.size(), 0);
    QTRY_COMPARE(flow.state(), AppController::State::Playing);
    game.rotatePiece(0, -5);
    QCOMPARE(rotations.size(), 1);
    QCOMPARE(rotations.front().at(1).toInt(), -1);
    game.restart();
    const int angle = model->pieces().front().rotation;
    game.rotatePiece(0, 1);
    QCOMPARE(rotations.size(), 1);
    QCOMPARE(model->pieces().front().rotation, angle);
    QTRY_COMPARE(flow.state(), AppController::State::Playing);
    game.cancel();
    const auto before = model->pieces();
    game.rotatePiece(0, 1);
    QCOMPARE(rotations.size(), 1);
    QCOMPARE(model->pieces().front().rotation, before.front().rotation);
    QCOMPARE(model->pieces().front().position, before.front().position);
    QVERIFY(game.start(image(), 4, 43));
    QTRY_COMPARE(flow.state(), AppController::State::Playing);
    for (int id = 0; id < model->pieces().size(); ++id) {
        game.rotatePiece(id, -model->pieces().at(id).rotation / 90);
        game.dropPiece(id, model->pieces().at(id).source.topLeft(), 0);
    }
    QCOMPARE(flow.state(), AppController::State::Completed);
    rotations.clear();
    game.rotatePiece(0, 1);
    QCOMPARE(rotations.size(), 0);
    QCOMPARE(model->pieces().front().rotation, 0);
}
void GameControllerTest::completesAllCounts_data()
{
    QTest::addColumn<int>("count");
    for (int count = 4; count <= 20; ++count)
        QTest::newRow(qPrintable(QString::number(count))) << count;
}
void GameControllerTest::completesAllCounts()
{
    QFETCH(int, count);
    AppController flow;
    GameController game(&flow);
    auto *model = game.model();
    QSignalSpy states(&flow, &AppController::stateChanged);
    QSignalSpy progress(model, &PuzzleModel::progressChanged);
    QSignalSpy victory(model, &PuzzleModel::completed);
    QSignalSpy errors(&game, &GameController::preparationFailed);
    QVERIFY(game.start(image(), count, 123));
    QCOMPARE(flow.state(), AppController::State::Preparing);
    QVERIFY(model->pieces().isEmpty());
    QVERIFY(!game.start(image(), 4, 999));
    QTRY_COMPARE(flow.state(), AppController::State::Playing);
    QCOMPARE(model->seed(), quint32(123));
    QCOMPARE(model->pieces().size(), count);
    QCOMPARE(progress.size(), 1);
    QCOMPARE(progress.front().at(0).toInt(), 0);
    for (int id = count - 1; id >= 0; --id) {
        game.rotatePiece(id, -model->pieces().at(id).rotation / 90);
        game.dropPiece(id, model->pieces().at(id).source.topLeft(), 0);
        QCOMPARE(model->placedCount(), count - id);
        QCOMPARE(flow.state(), id == 0 ? AppController::State::Completed : AppController::State::Playing);
    }
    QCOMPARE(victory.size(), 1);
    QCOMPARE(progress.size(), count + 1);
    for (int index = 0; index < progress.size(); ++index) {
        QCOMPARE(progress.at(index).at(0).toInt(), index);
        QCOMPARE(progress.at(index).at(1).toInt(), count);
    }
    game.dropPiece(0, {}, 1000);
    game.rotatePiece(0, 1);
    QCOMPARE(model->pieces().front().rotation, 0);
    QVERIFY(!game.start(image(), 4, 42));
    QCOMPARE(victory.size(), 1);
    QCOMPARE(states.size(), 3);
    QCOMPARE(errors.size(), 0);
}
void GameControllerTest::rejectsPreparation_data()
{
    QTest::addColumn<QImage>("input");
    QTest::addColumn<int>("count");
    QTest::newRow("empty") << QImage() << 4;
    QTest::newRow("below-minimum") << image() << 3;
    QTest::newRow("above-maximum") << image() << 21;
    QTest::newRow("insufficient-size") << QImage(16, 16, QImage::Format_RGB32) << 4;
}
void GameControllerTest::rejectsPreparation()
{
    QFETCH(QImage, input);
    QFETCH(int, count);
    AppController flow;
    GameController game(&flow);
    QSignalSpy errors(&game, &GameController::preparationFailed);
    QSignalSpy resets(game.model(), &PuzzleModel::puzzleReset);
    QVERIFY(game.start(input, count, 10));
    QTRY_COMPARE(errors.size(), 1);
    QVERIFY(!errors.front().front().toString().isEmpty());
    QCOMPARE(flow.state(), AppController::State::Configuring);
    QCOMPARE(resets.size(), 0);
    QVERIFY(game.model()->pieces().isEmpty());
    QVERIFY(game.start(image(), 4, 11));
    QTRY_COMPARE(flow.state(), AppController::State::Playing);
    QCOMPARE(resets.size(), 1);
    QCOMPARE(errors.size(), 1);
}
void GameControllerTest::cancellationAndReplacement()
{
    AppController flow;
    GameController game(&flow);
    QSignalSpy resets(game.model(), &PuzzleModel::puzzleReset);
    QSignalSpy errors(&game, &GameController::preparationFailed);
    QVERIFY(game.start(image(), 4, 1));
    game.cancel();
    QCoreApplication::processEvents();
    QCOMPARE(flow.state(), AppController::State::Configuring);
    QCOMPARE(resets.size(), 0);
    // Новый запрос не должен получить параметры отменённого таймера.
    QVERIFY(game.start(image(), 20, 2));
    QTRY_COMPARE(flow.state(), AppController::State::Playing);
    QCOMPARE(game.model()->pieces().size(), 20);
    QCOMPARE(game.model()->seed(), quint32(2));
    QCOMPARE(resets.size(), 1);
    QCOMPARE(errors.size(), 0);
}
void GameControllerTest::restartRegenerateAndGuards()
{
    AppController flow;
    GameController game(&flow);
    game.restart(); game.regenerate(); game.dropPiece(0, {}, 10);
    QCOMPARE(flow.state(), AppController::State::Configuring);
    QVERIFY(game.start(image(), 17, 42));
    QTRY_COMPARE(flow.state(), AppController::State::Playing);
    auto *model = game.model();
    const auto geometry = model->pieces();
    game.rotatePiece(0, -model->pieces().front().rotation / 90);
    game.dropPiece(0, geometry.front().source.topLeft(), 0);
    QCOMPARE(model->placedCount(), 1);
    game.restart();
    QCOMPARE(flow.state(), AppController::State::Preparing);
    const int angle = model->pieces().at(1).rotation;
    game.rotatePiece(1, 1);
    QCOMPARE(model->pieces().at(1).rotation, angle);
    game.dropPiece(1, geometry.at(1).source.topLeft(), 0);
    QCOMPARE(model->placedCount(), 1);
    QTRY_COMPARE(flow.state(), AppController::State::Playing);
    QCOMPARE(model->placedCount(), 0);
    QCOMPARE(model->seed(), quint32(42));
    for (int id = 0; id < geometry.size(); ++id) {
        QCOMPARE(model->pieces().at(id).source, geometry.at(id).source);
        QCOMPARE(model->pieces().at(id).shape, geometry.at(id).shape);
    }
    game.regenerate();
    QTRY_COMPARE(flow.state(), AppController::State::Playing);
    QVERIFY(model->seed() != 42);
    QCOMPARE(model->pieces().size(), 17);
    game.cancel();
    game.dropPiece(0, model->pieces().front().source.topLeft(), 0);
    QCOMPARE(model->placedCount(), 0);
    game.restart(); game.regenerate();
    QCOMPARE(flow.state(), AppController::State::Configuring);
}
void GameControllerTest::destructionDuringPreparation()
{
    AppController flow;
    auto *game = new GameController(&flow);
    QPointer<PuzzleModel> model = game->model();
    QSignalSpy states(&flow, &AppController::stateChanged);
    QVERIFY(game->start(image(), 4, 8));
    delete game;
    QVERIFY(model.isNull());
    QTest::qWait(30);
    QCOMPARE(states.size(), 1);
    QCOMPARE(flow.state(), AppController::State::Preparing);
}
QTEST_GUILESS_MAIN(GameControllerTest)
#include "tst_gamecontroller.moc"
