#include "app/appcontroller.h"

#include <QSignalSpy>
#include <QTest>

/** @brief Проверяет порядок состояний и контракт уведомлений без создания интерфейса. */
class AppControllerTest : public QObject
{
    Q_OBJECT

private slots:
    /** @brief Проверяет полный жизненный цикл и точный порядок сигналов. */
    void followsLifecycle();
    /** @brief Запрещает пропуск подготовки, повторные и обратные переходы. */
    void rejectsInvalidTransitions();
    /** @brief Проверяет отмену подготовки и последующее открытие новой игры. */
    void cancelsAndRestarts();
};

void AppControllerTest::followsLifecycle()
{
    AppController controller;
    QSignalSpy spy(&controller, &AppController::stateChanged);
    QVERIFY(spy.isValid());
    QCOMPARE(controller.state(), AppController::State::Configuring);
    QVERIFY(controller.beginPreparation());
    QVERIFY(controller.finishPreparation());
    QVERIFY(controller.finishGame());
    QVERIFY(controller.returnToSetup());
    QCOMPARE(controller.state(), AppController::State::Configuring);
    QCOMPARE(spy.size(), 4);
    QCOMPARE(spy.at(0).at(0).value<AppController::State>(), AppController::State::Preparing);
    QCOMPARE(spy.at(1).at(0).value<AppController::State>(), AppController::State::Playing);
    QCOMPARE(spy.at(2).at(0).value<AppController::State>(), AppController::State::Completed);
    QCOMPARE(spy.at(3).at(0).value<AppController::State>(), AppController::State::Configuring);
}

void AppControllerTest::rejectsInvalidTransitions()
{
    AppController controller;
    QSignalSpy spy(&controller, &AppController::stateChanged);
    QVERIFY(!controller.returnToSetup());
    QVERIFY(!controller.finishPreparation());
    QVERIFY(!controller.finishGame());
    QCOMPARE(spy.size(), 0);
    QCOMPARE(controller.state(), AppController::State::Configuring);

    QVERIFY(controller.beginPreparation());
    QVERIFY(!controller.beginPreparation());
    QVERIFY(!controller.finishGame());
    QCOMPARE(controller.state(), AppController::State::Preparing);
    QCOMPARE(spy.size(), 1);

    QVERIFY(controller.finishPreparation());
    QVERIFY(!controller.beginPreparation());
    QVERIFY(!controller.finishPreparation());
    QCOMPARE(controller.state(), AppController::State::Playing);
    QCOMPARE(spy.size(), 2);

    QVERIFY(controller.finishGame());
    QVERIFY(!controller.beginPreparation());
    QVERIFY(!controller.finishPreparation());
    QVERIFY(!controller.finishGame());
    QCOMPARE(controller.state(), AppController::State::Completed);
    QCOMPARE(spy.size(), 3);
}

void AppControllerTest::cancelsAndRestarts()
{
    AppController controller;
    QVERIFY(controller.beginPreparation());
    QVERIFY(controller.returnToSetup());
    QVERIFY(!controller.finishPreparation());
    QCOMPARE(controller.state(), AppController::State::Configuring);
    QVERIFY(controller.beginPreparation());
    QVERIFY(controller.finishPreparation());
    QVERIFY(controller.returnToSetup());
    QCOMPARE(controller.state(), AppController::State::Configuring);
}

QTEST_GUILESS_MAIN(AppControllerTest)
#include "tst_appcontroller.moc"
