#include "ui/mainwindow.h"
#include "ui/loadpage.h"
#include "ui/puzzleview.h"
#include "app/gamecontroller.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTouchEvent>
#include <QWindow>
#include <QScreen>
#include <QCheckBox>
#include <QGraphicsOpacityEffect>
#include <QSettings>
#include <QPainter>
#include <QPainterPathStroker>
#include <QParallelAnimationGroup>
#include <QVariantAnimation>
#include <QPointer>
#include <QIcon>

/** @brief Сохраняет маршрут касаний для диагностики автоматических тестов без реального экрана. */
class TouchTrace : public QObject
{
public:
    QStringList messages; ///< Получатели и координаты событий касания.
    QObject *target = nullptr; ///< Проверяемый viewport.
    bool targetReceived = false; ///< Подтверждает настоящий TouchBegin вместо эмуляции мышью.
protected:
    /** @brief Записывает события, не вмешиваясь в доставку. @param object Получатель.
     * @param event Событие. @return false, чтобы продолжить обычную обработку. */
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd) {
            if (event->type() == QEvent::TouchBegin && object == target)
                targetReceived = true;
            const auto *touch = static_cast<QTouchEvent *>(event);
            for (const auto &point : touch->points())
                messages.append(QStringLiteral("%1 %2 type=%3 local=%4,%5 global=%6,%7")
                                    .arg(QString::fromLatin1(object->metaObject()->className()), object->objectName())
                                    .arg(int(event->type())).arg(point.position().x()).arg(point.position().y())
                                    .arg(point.globalPosition().x()).arg(point.globalPosition().y()));
        }
        return false;
    }
};

/** @brief Проверяет полный пользовательский сценарий настоящими событиями Qt. */
class MainWindowTest : public QObject
{
    Q_OBJECT
private slots:
    /** @brief Настраивает шрифт headless Windows и виртуальное сенсорное устройство. */
    void initTestCase();
    /** @brief Запрещает старт до выбора изображения. */
    void startsDisabled();
    /** @brief Проверяет границы 4–20 и запрет игры для слишком маленького изображения. */
    void pieceCountLimits();
    /** @brief Проверяет музейные ресурсы, готовые сложности и сохранение отключённого движения. */
    void expeditionControls();
    /** @brief Собирает пазл мышью и проверяет повтор, новую геометрию и возврат. */
    void mouseCompletesAndRestarts();
    /** @brief Проверяет перенос пальцем без дублирования мышью. */
    void touchPlacesPiece();
    /** @brief Проверяет дополнительные пальцы и отмену касания. */
    void touchCancellationAndExtraFinger();
    /** @brief Проверяет восстановление детали при потере активности окна. */
    void deactivationCancelsDrag();
    /** @brief Проверяет выбор файла, отмену диалога и сохранение картинки при ошибке. */
    void fileDialogAndErrors();
    /** @brief Проверяет доступность большого пазла, масштаб и сохранение позиций при resize. */
    void zoomResizeAndLargeCount();
    /** @brief Проверяет долю игрового поля, точку захвата миниатюры и возврат в лоток. */
    void compactTrayInteraction();
    /** @brief Проверяет прозрачные выемки, выбор по контуру и правильные пиксели текстурной заливки. */
    void curvedPieceRendering();
    /** @brief Проверяет притягивание, краткую подсветку и затухание швов при переходе к фотографии. */
    void assemblyEffects();
    /** @brief Проверяет отключение эффектов и отмену отложенного перехода при новом раунде. */
    void effectsCancellation();
    /** @brief Задаёт минимальный и максимальный пазл для сборки касаниями. */
    void touchCompletesWithEffects_data();
    /** @brief Собирает весь пазл касаниями и проверяет единственное завершение эффектов. */
    void touchCompletesWithEffects();
    /** @brief Проверяет отключение движения при закреплении и в отложенном завершении. */
    void reducedMotionDuringPlacementAndDelay();
    /** @brief Проверяет реальные пиксели свечения, пустую внешнюю рамку и итог без швов. */
    void completionGlowRendering();
    /** @brief Задаёт удаление сцены во время закрепления, задержки и свечения. */
    void effectsLifetime_data();
    /** @brief Проверяет отсутствие отложенных сигналов после уничтожения сцены и её деталей. */
    void effectsLifetime();
    /** @brief Проверяет навигацию клавиатурой и увеличенный системный шрифт. */
    void keyboardAndLargeText();
    /** @brief Отменяет подготовку и убеждается, что поле не откроется от старого таймера. */
    void cancelsPreparation();
private:
    /** @brief Выбирает встроенную фотографию действиями пользователя и открывает пазл.
     * @param window Окно. @param count Запрошенное количество, ограничиваемое допустимым диапазоном. */
    void prepare(MainWindow &window, int count = 4);
    /** @brief Переносит одну деталь мышью. @param window Окно. @param id Номер детали. */
    void mousePlace(MainWindow &window, int id);
    QPointingDevice *m_device = nullptr; ///< Виртуальное устройство принадлежит QApplication.
    std::unique_ptr<QTemporaryDir> m_preferences; ///< Изолирует настройки тестового процесса от пользователя.
};
void MainWindowTest::initTestCase()
{
    m_preferences = std::make_unique<QTemporaryDir>(QDir(QCoreApplication::applicationDirPath()).filePath("ui-preferences-XXXXXX"));
    QVERIFY(m_preferences->isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_preferences->path());
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        int id = -1;
        for (const QString &directory : QStandardPaths::standardLocations(QStandardPaths::FontsLocation)) {
            id = QFontDatabase::addApplicationFont(QDir(directory).filePath("segoeui.ttf"));
            if (id >= 0) break;
        }
        QVERIFY(id >= 0);
        QApplication::setFont(QFont(QFontDatabase::applicationFontFamilies(id).front(), 12));
    }
#endif
    m_device = QTest::createTouchDevice();
    m_device->setParent(qApp);
}
void MainWindowTest::prepare(MainWindow &window, int count)
{
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *samples = window.findChild<QComboBox *>("sampleImagesCombo");
    auto *use = window.findChild<QPushButton *>("sampleLoadButton");
    auto *start = window.findChild<QPushButton *>("startButton");
    auto *number = window.findChild<QSpinBox *>("pieceCountSpinBox");
    QVERIFY(number->font().pointSizeF() >= 14);
    QVERIFY(samples && use && start && number);
    samples->setFocus();
    QTest::keyClick(samples, Qt::Key_Home);
    QTest::keyClick(samples, Qt::Key_Down);
    QTest::mouseClick(use, Qt::LeftButton);
    QTRY_VERIFY(start->isEnabled());
    const int expected = qMin(count, number->maximum());
    number->setFocus();
    QTest::keyClick(number, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClicks(number, QString::number(expected));
    QTest::keyClick(number, Qt::Key_Return);
    QCOMPARE(number->value(), expected);
    QTest::mouseClick(start, Qt::LeftButton, {}, {}, 0);
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    auto *view = window.findChild<PuzzleView *>();
    QVERIFY(view);
    view->fitAll();
}
void MainWindowTest::mousePlace(MainWindow &window, int id)
{
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    const PuzzlePiece piece = model->pieces().at(id);
    const QPointF offset = QRectF(QPointF(), piece.source.size()).center();
    const QPoint start = view->mapFromScene(view->puzzleScene()->pieceItem(piece.id)->mapToScene(offset));
    const QPoint end = view->mapFromScene(QPointF(piece.source.topLeft()) + offset);
    QVERIFY(view->viewport()->rect().contains(start));
    QVERIFY(view->viewport()->rect().contains(end));
    QTest::mousePress(view->viewport(), Qt::LeftButton, {}, start, 0);
    QTest::mouseMove(view->viewport(), end, 0);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, end, 0);
    QVERIFY(model->pieces().at(id).placed);
}
void MainWindowTest::startsDisabled()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *start = window.findChild<QPushButton *>("startButton");
    QVERIFY(start && !start->isEnabled());
    QCOMPARE(window.state(), AppController::State::Configuring);
    QVERIFY(window.close());
}
void MainWindowTest::pieceCountLimits()
{
    LoadPage page;
    auto *number = page.findChild<QSpinBox *>("pieceCountSpinBox");
    auto *start = page.findChild<QPushButton *>("startButton");
    QCOMPARE(number->minimum(), 4);
    QCOMPARE(number->maximum(), 20);
    QImage image(320, 240, QImage::Format_RGB32);
    image.fill(Qt::green);
    page.setImage(image, QStringLiteral("normal"));
    number->setValue(3);
    QCOMPARE(number->value(), 4);
    number->setValue(21);
    QCOMPARE(number->value(), 20);
    QVERIFY(start->isEnabled());
    // Вместимость картинки может сузить диапазон, но не снизить нижнюю границу.
    page.setImage(image.copy(0, 0, 32, 32), QStringLiteral("minimum"));
    QCOMPARE(number->minimum(), 4);
    QCOMPARE(number->maximum(), 4);
    QVERIFY(start->isEnabled());
    page.setImage(image.copy(0, 0, 16, 48), QStringLiteral("too-small"));
    QCOMPARE(number->value(), 4);
    QVERIFY(!start->isEnabled());
    page.setImage(image, QStringLiteral("normal"));
    QCOMPARE(number->maximum(), 20);
    QVERIFY(start->isEnabled());
}
void MainWindowTest::expeditionControls()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("ImagePuzzle"), QStringLiteral("NorthernExpedition"));
    const QVariant previous = settings.value(QStringLiteral("reducedMotion"));
    const auto restore = qScopeGuard([&] {
        if (previous.isValid()) settings.setValue(QStringLiteral("reducedMotion"), previous);
        else settings.remove(QStringLiteral("reducedMotion"));
    });
    settings.setValue(QStringLiteral("reducedMotion"), false);
    settings.sync();
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *samples = window.findChild<QComboBox *>("sampleImagesCombo");
    QVERIFY(samples->itemData(1).toString().contains(QStringLiteral("/assets/museum/")));
    QCOMPARE(samples->count(), 4); // Подсказка и три музейные фотографии, без тестовой коллекции.
    for (int index = 1; index < samples->count(); ++index)
        QVERIFY(samples->itemData(index).toString().startsWith(QStringLiteral(":/imagepuzzle/assets/museum/")));
    QVERIFY(QFile::exists(QStringLiteral(":/imagepuzzle/assets/fonts/Geologica.ttf")));
    QVERIFY(!window.windowIcon().isNull());
    QVERIFY(!QApplication::windowIcon().isNull());
    for (int side : {16, 20, 24, 32, 40, 48, 64, 128, 256})
        QVERIFY(!window.windowIcon().pixmap(QSize(side, side)).isNull());
    QCOMPARE(window.palette().color(QPalette::Window), QColor("#F7F4EE"));
    auto *number = window.findChild<QSpinBox *>("pieceCountSpinBox");
    QTest::mouseClick(window.findChild<QPushButton *>("countPreset20"), Qt::LeftButton);
    QCOMPARE(number->value(), 20);
    number->setValue(7);
    for (int preset : {4, 8, 12, 16, 20})
        QVERIFY(!window.findChild<QPushButton *>(QStringLiteral("countPreset%1").arg(preset))->isChecked());
    auto *motion = window.findChild<QCheckBox *>("reducedMotionCheckBox");
    QTest::mouseClick(motion, Qt::LeftButton);
    QVERIFY(motion->isChecked());
    for (auto *effect : window.findChildren<QGraphicsOpacityEffect *>()) QCOMPARE(effect->opacity(), 1.0);
    MainWindow second;
    QVERIFY(second.findChild<QCheckBox *>("reducedMotionCheckBox")->isChecked());
    window.resize(640, 480);
    QCoreApplication::processEvents();
    for (const char *name : {"startButton", "chooseImageButton", "pieceCountSpinBox"}) {
        auto *control = window.findChild<QWidget *>(QString::fromLatin1(name));
        QVERIFY(window.rect().contains(QRect(control->mapTo(&window, QPoint()), control->size())));
    }
}
void MainWindowTest::mouseCompletesAndRestarts()
{
    MainWindow window;
    prepare(window);
    QCOMPARE(window.state(), AppController::State::Playing);
    auto *model = window.findChild<GameController *>()->model();
    const auto geometry = model->pieces();
    for (int id = 0; id < model->pieces().size(); ++id)
        mousePlace(window, id);
    QCOMPARE(window.state(), AppController::State::Completed);
    QVERIFY(window.findChild<QLabel *>("completionLabel")->isVisible());
    QTest::mouseClick(window.findChild<QPushButton *>("restartButton"), Qt::LeftButton);
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    QCOMPARE(model->placedCount(), 0);
    for (int id = 0; id < geometry.size(); ++id)
        QCOMPARE(model->pieces().at(id).source, geometry.at(id).source);
    QVERIFY(!window.findChild<QLabel *>("completionLabel")->isVisible());
    const quint32 seed = model->seed();
    QTest::mouseClick(window.findChild<QPushButton *>("regenerateButton"), Qt::LeftButton);
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    QVERIFY(model->seed() != seed);
    QTest::mouseClick(window.findChild<QPushButton *>("backButton"), Qt::LeftButton);
    QCOMPARE(window.state(), AppController::State::Configuring);
    QCOMPARE(window.findChild<QSpinBox *>("pieceCountSpinBox")->value(), 4);
    QVERIFY(window.findChild<LoadPage *>()->hasImage());
}
void MainWindowTest::touchPlacesPiece()
{
    TouchTrace trace;
    qApp->installEventFilter(&trace);
    MainWindow window;
    prepare(window);
    QCOMPARE(window.state(), AppController::State::Playing);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    const auto piece = model->pieces().front();
    const QPointF offset = QRectF(QPointF(), piece.source.size()).center();
    const QPoint start = view->mapFromScene(view->puzzleScene()->pieceItem(piece.id)->mapToScene(offset));
    const QPoint end = view->mapFromScene(QPointF(piece.source.topLeft()) + offset);
    QSignalSpy progress(model, &PuzzleModel::progressChanged);
    trace.target = view->viewport();
    trace.messages.append(QStringLiteral("screen=%1x%2 window=%3x%4 start=%5,%6 end=%7,%8")
                              .arg(window.screen()->geometry().width()).arg(window.screen()->geometry().height())
                              .arg(window.width()).arg(window.height()).arg(start.x()).arg(start.y()).arg(end.x()).arg(end.y()));
    // Указываем QWindow явно: QWidget-вариант Qt Test теряет доставку дочернему viewport при DPR != 1.
    auto sequence = QTest::touchEvent(window.windowHandle(), m_device, false);
    sequence.press(0, view->viewport()->mapTo(&window, start), window.windowHandle()).commit();
    sequence.move(0, view->viewport()->mapTo(&window, end), window.windowHandle()).commit();
    sequence.release(0, view->viewport()->mapTo(&window, end), window.windowHandle()).commit();
    QVERIFY2(trace.targetReceived, qPrintable(trace.messages.join(QLatin1Char('\n'))));
    QVERIFY2(model->pieces().front().placed, qPrintable(trace.messages.join(QLatin1Char('\n'))));
    QCOMPARE(model->placedCount(), 1);
    QCOMPARE(progress.size(), 1);
}
void MainWindowTest::touchCancellationAndExtraFinger()
{
    MainWindow window;
    prepare(window);
    QCOMPARE(window.state(), AppController::State::Playing);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    const auto original = model->pieces().front();
    const QPointF offset = QRectF(QPointF(), original.source.size()).center();
    const QPoint start = view->mapFromScene(view->puzzleScene()->pieceItem(original.id)->mapToScene(offset));
    const QPoint end = view->mapFromScene(QPointF(original.source.topLeft()) + offset);
    const auto windowPoint = [view, &window](QPoint point) { return view->viewport()->mapTo(&window, point); };
    auto sequence = QTest::touchEvent(window.windowHandle(), m_device, false);
    sequence.press(0, windowPoint(start), window.windowHandle()).commit();
    const QPointF draggingPosition = view->puzzleScene()->pieceItem(0)->pos();
    QCOMPARE(view->puzzleScene()->pieceItem(0)->scale(), 1.0);
    sequence.stationary(0).press(1, windowPoint(QPoint(10, 10)), window.windowHandle()).commit();
    sequence.stationary(0).move(1, windowPoint(end), window.windowHandle()).commit();
    QCOMPARE(view->puzzleScene()->pieceItem(0)->pos(), draggingPosition);
    sequence.stationary(0).release(1, windowPoint(end), window.windowHandle()).commit();
    sequence.move(0, windowPoint(end), window.windowHandle()).commit();
    QVERIFY(view->puzzleScene()->pieceItem(0)->pos() != original.position);
    QTouchEvent cancel(QEvent::TouchCancel, m_device);
    QCoreApplication::sendEvent(view->viewport(), &cancel);
    QCOMPARE(view->puzzleScene()->pieceItem(0)->pos(), original.position);
    QCOMPARE(view->puzzleScene()->pieceItem(0)->scale(), original.scale);
    QCOMPARE(model->placedCount(), 0);
    sequence.release(0, windowPoint(end), window.windowHandle()).commit();
    QCOMPARE(model->placedCount(), 0);
}
void MainWindowTest::deactivationCancelsDrag()
{
    MainWindow window;
    prepare(window);
    QCOMPARE(window.state(), AppController::State::Playing);
    auto *view = window.findChild<PuzzleView *>();
    const auto piece = window.findChild<GameController *>()->model()->pieces().front();
    const QPoint start = view->mapFromScene(view->puzzleScene()->pieceItem(piece.id)->mapToScene(QRectF(QPointF(), piece.source.size()).center()));
    QTest::mousePress(view->viewport(), Qt::LeftButton, {}, start, 0);
    QTest::mouseMove(view->viewport(), start - QPoint(30, 20), 0);
    QEvent deactivate(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(view, &deactivate);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, start - QPoint(30, 20), 0);
    QCOMPARE(view->puzzleScene()->pieceItem(0)->pos(), piece.position);
    QCOMPARE(view->puzzleScene()->pieceItem(0)->scale(), piece.scale);
}
void MainWindowTest::fileDialogAndErrors()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *choose = window.findChild<QPushButton *>("chooseImageButton");
    auto *start = window.findChild<QPushButton *>("startButton");
    QVERIFY(choose && start);
    QTimer::singleShot(0, &window, [&window] {
        if (auto *dialog = window.findChild<QFileDialog *>()) dialog->reject();
    });
    QTest::mouseClick(choose, Qt::LeftButton);
    QVERIFY(!start->isEnabled());
    QTemporaryDir directory(QDir(QCoreApplication::applicationDirPath()).filePath("dialog-XXXXXX"));
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("selected.jpg");
    QVERIFY(QFile::copy(QStringLiteral(":/imagepuzzle/images/cat1.jpg"), path));
    QTimer::singleShot(0, &window, [&window, path] {
        if (auto *dialog = window.findChild<QFileDialog *>()) {
            dialog->selectFile(path);
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
        }
    });
    QTest::mouseClick(choose, Qt::LeftButton);
    QTRY_VERIFY(start->isEnabled());
    QFile invalid(directory.filePath("broken.jpg"));
    QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("broken"); invalid.close();
    window.loadImage(invalid.fileName());
    auto *message = window.findChild<QLabel *>("loadStatusLabel");
    QTRY_VERIFY(message->text().contains(QStringLiteral("Не удалось")));
    QVERIFY(start->isEnabled());
    QTest::mouseClick(start, Qt::LeftButton);
    QTRY_COMPARE(window.state(), AppController::State::Playing);
}
void MainWindowTest::zoomResizeAndLargeCount()
{
    MainWindow window;
    prepare(window, 20);
    QCOMPARE(window.state(), AppController::State::Playing);
    auto *model = window.findChild<GameController *>()->model();
    auto *view = window.findChild<PuzzleView *>();
    QCOMPARE(model->pieces().size(), 20);
    const auto original = model->pieces();
    const qreal scale = view->transform().m11();
    QTest::mouseClick(window.findChild<QPushButton *>("zoomInButton"), Qt::LeftButton);
    QVERIFY(view->transform().m11() > scale);
    window.resize(640, 480);
    QCoreApplication::processEvents();
    for (int i = 0; i < original.size(); ++i)
        QCOMPARE(model->pieces().at(i).position, original.at(i).position);
    QTest::mouseClick(window.findChild<QPushButton *>("fitButton"), Qt::LeftButton);
    auto *back = window.findChild<QPushButton *>("backButton");
    QVERIFY(window.rect().contains(QRect(back->mapTo(&window, QPoint()), back->size())));
    QTest::mouseClick(back, Qt::LeftButton);
    QCOMPARE(window.state(), AppController::State::Configuring);
}
void MainWindowTest::compactTrayInteraction()
{
    MainWindow window;
    prepare(window, 20);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    const QRect board = view->mapFromScene(model->boardRect()).boundingRect();
    const QRect tray = view->mapFromScene(model->trayRect()).boundingRect();
    QVERIFY(board.width() > tray.width() * 1.8);
    for (const auto &data : model->pieces()) {
        const QRect miniature = view->mapFromScene(view->puzzleScene()->pieceItem(data.id)->sceneBoundingRect()).boundingRect();
        QVERIFY(view->viewport()->rect().contains(miniature));
    }
    auto *item = view->puzzleScene()->pieceItem(0);
    const QPointF oldPosition = item->pos();
    const qreal oldScale = item->scale();
    const QPointF offset(item->boundingRect().width() * 0.45, item->boundingRect().height() * 0.55);
    const QPoint start = view->mapFromScene(item->mapToScene(offset));
    const QPointF grabbed = item->mapFromScene(view->mapToScene(start));
    QTest::mousePress(view->viewport(), Qt::LeftButton, {}, start, 0);
    QCOMPARE(item->scale(), 1.0);
    // Округление координат экрана допускает максимум один пиксель отклонения точки захвата.
    const QPoint held = view->mapFromScene(item->mapToScene(grabbed));
    QVERIFY((held - start).manhattanLength() <= 2);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, start, 0);
    QCOMPARE(item->scale(), oldScale);
    QVERIFY(QLineF(item->pos(), oldPosition).length() < 0.001);
    QVERIFY(!model->pieces().at(0).placed);
    const QPoint end = view->mapFromScene(QPointF(model->pieces().at(0).source.topLeft()) + grabbed);
    QTest::mousePress(view->viewport(), Qt::LeftButton, {}, start, 0);
    QTest::mouseMove(view->viewport(), end, 0);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, end, 0);
    QVERIFY(model->pieces().at(0).placed);
    QCOMPARE(item->scale(), 1.0);
    window.resize(640, 480);
    QCoreApplication::processEvents();
    view->fitAll();
    QVERIFY(view->mapFromScene(model->boardRect()).boundingRect().width()
            > view->mapFromScene(model->trayRect()).boundingRect().width() * 1.8);
    QVERIFY(view->mapFromScene(model->boardRect()).boundingRect().width() >= 250);
}
void MainWindowTest::curvedPieceRendering()
{
    QImage image(160, 120, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) image.setPixel(x, y, qRgb(x, y, (x + y) % 256));
    PuzzleModel model;
    QVERIFY(model.start(image, 7, 42));
    int transparent = 0;
    for (const auto &piece : model.pieces()) {
        PieceItem item(image, piece);
        item.setLocked(true);
        QImage rendered(piece.source.size(), QImage::Format_ARGB32_Premultiplied);
        rendered.fill(Qt::transparent);
        QPainter painter(&rendered);
        item.paint(&painter, nullptr, nullptr);
        painter.end();
        QPainterPathStroker stroker;
        stroker.setWidth(4);
        const auto edge = stroker.createStroke(piece.shape);
        for (int y = 0; y < rendered.height(); ++y) {
            for (int x = 0; x < rendered.width(); ++x) {
                const QPointF point(x + 0.5, y + 0.5);
                if (edge.contains(point)) continue;
                if (piece.shape.contains(point))
                    QCOMPARE(rendered.pixel(x, y), image.pixel(piece.source.x() + x, piece.source.y() + y));
                else {
                    QVERIFY(!item.contains(point));
                    QCOMPARE(qAlpha(rendered.pixel(x, y)), 0);
                    ++transparent;
                }
            }
        }
    }
    QVERIFY(transparent > 0);
}
void MainWindowTest::assemblyEffects()
{
    MainWindow window;
    prepare(window);
    window.findChild<QCheckBox *>("reducedMotionCheckBox")->setChecked(false);
    auto *model = window.findChild<GameController *>()->model();
    auto *view = window.findChild<PuzzleView *>();
    auto *scene = view->puzzleScene();
    auto *item = scene->pieceItem(0);
    const QPointF center = item->boundingRect().center();
    const QPoint start = view->mapFromScene(item->mapToScene(center));
    const QPointF grabbed = item->mapFromScene(view->mapToScene(start));
    const QPointF target = model->pieces().at(0).source.topLeft();
    const QPoint end = view->mapFromScene(target + grabbed) + QPoint(8, 0);
    QTest::mousePress(view->viewport(), Qt::LeftButton, {}, start, 0);
    QTest::mouseMove(view->viewport(), end, 0);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, end, 0);
    QVERIFY(model->pieces().at(0).placed);
    auto *placement = item->findChild<QParallelAnimationGroup *>("placementAnimation");
    QVERIFY(placement);
    QCOMPARE(placement->state(), QAbstractAnimation::Running);
    QVERIFY(QLineF(item->pos(), target).length() > 0.1);
    QTRY_COMPARE(placement->state(), QAbstractAnimation::Stopped);
    QCOMPARE(item->pos(), target);
    QCOMPARE(item->placementGlow(), 0.0);
    QSignalSpy presentation(scene, &PuzzleScene::completionPresentationChanged);
    QSignalSpy finished(scene, &PuzzleScene::completionEffectsFinished);
    for (int id = 1; id < model->pieces().size(); ++id) mousePlace(window, id);
    QCOMPARE(window.state(), AppController::State::Completed);
    QVERIFY(!scene->completionPresented());
    QTRY_COMPARE(presentation.size(), 1);
    auto findLayer = [scene](const QString &name) -> QGraphicsItem * {
        for (auto *layer : scene->items()) if (layer->data(0).toString() == name) return layer;
        return nullptr;
    };
    auto *glow = findLayer(QStringLiteral("completionSeamGlow"));
    auto *photo = findLayer(QStringLiteral("completedPhoto"));
    QVERIFY(glow && photo);
    QVERIFY(glow->shape().isEmpty());
    QTRY_VERIFY(glow->opacity() > 0.05);
    QVERIFY(photo->opacity() > 0 && photo->opacity() <= 1);
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(photo->opacity(), 1.0);
    QCOMPARE(glow->opacity(), 0.0);
    QVERIFY(!glow->isVisible());
}
void MainWindowTest::effectsCancellation()
{
    MainWindow window;
    prepare(window);
    auto *game = window.findChild<GameController *>();
    auto *model = game->model();
    auto *scene = window.findChild<PuzzleView *>()->puzzleScene();
    auto *motion = window.findChild<QCheckBox *>("reducedMotionCheckBox");
    motion->setChecked(false);
    QPointer<PieceItem> old = scene->pieceItem(0);
    for (int id = 0; id < model->pieces().size(); ++id) game->dropPiece(id, model->pieces().at(id).source.topLeft(), 0);
    QVERIFY(!scene->completionPresented());
    game->restart();
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    QVERIFY(old.isNull());
    QTest::qWait(750);
    QVERIFY(!scene->completionPresented());
    QCOMPARE(model->placedCount(), 0);
    for (int id = 0; id < model->pieces().size(); ++id) game->dropPiece(id, model->pieces().at(id).source.topLeft(), 0);
    QTRY_VERIFY(scene->completionPresented());
    auto *animation = scene->findChild<QVariantAnimation *>("completionAnimation");
    QVERIFY(animation);
    motion->setChecked(true);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    for (auto *layer : scene->items()) {
        if (layer->data(0).toString() == QStringLiteral("completionSeamGlow")) QCOMPARE(layer->opacity(), 0.0);
        if (layer->data(0).toString() == QStringLiteral("completedPhoto")) QCOMPARE(layer->opacity(), 1.0);
    }
    game->restart();
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    for (int id = 0; id < model->pieces().size(); ++id) game->dropPiece(id, model->pieces().at(id).source.topLeft(), 0);
    QVERIFY(scene->completionPresented());
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    for (auto *piece : scene->items()) {
        QVERIFY(piece->data(0).toString() != QStringLiteral("completionSeamGlow"));
        if (auto *fragment = qgraphicsitem_cast<PieceItem *>(piece)) QCOMPARE(fragment->placementGlow(), 0.0);
    }
    motion->setChecked(false);
}
void MainWindowTest::touchCompletesWithEffects_data()
{
    QTest::addColumn<int>("count");
    QTest::newRow("minimum-4") << 4;
    QTest::newRow("maximum-20") << 20;
}
void MainWindowTest::touchCompletesWithEffects()
{
    QFETCH(int, count);
    MainWindow window;
    prepare(window, count);
    window.findChild<QCheckBox *>("reducedMotionCheckBox")->setChecked(false);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    QSignalSpy progress(model, &PuzzleModel::progressChanged);
    QSignalSpy victory(model, &PuzzleModel::completed);
    QSignalSpy finished(view->puzzleScene(), &PuzzleScene::completionEffectsFinished);
    for (int id = 0; id < count; ++id) {
        auto *item = view->puzzleScene()->pieceItem(id);
        const QPointF offset = item->boundingRect().center();
        const QPoint start = view->mapFromScene(item->mapToScene(offset));
        const QPoint end = view->mapFromScene(QPointF(model->pieces().at(id).source.topLeft()) + offset);
        auto sequence = QTest::touchEvent(window.windowHandle(), m_device, false);
        sequence.press(0, view->viewport()->mapTo(&window, start), window.windowHandle()).commit();
        sequence.move(0, view->viewport()->mapTo(&window, end), window.windowHandle()).commit();
        sequence.release(0, view->viewport()->mapTo(&window, end), window.windowHandle()).commit();
        QCOMPARE(model->placedCount(), id + 1);
    }
    QCOMPARE(window.state(), AppController::State::Completed);
    QCOMPARE(victory.size(), 1);
    QCOMPARE(progress.size(), count);
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(view->puzzleScene()->completionPresented());
    QCOMPARE(model->placedCount(), count);
}
void MainWindowTest::reducedMotionDuringPlacementAndDelay()
{
    MainWindow window;
    prepare(window);
    auto *motion = window.findChild<QCheckBox *>("reducedMotionCheckBox");
    motion->setChecked(false);
    auto *game = window.findChild<GameController *>();
    auto *model = game->model();
    auto *scene = window.findChild<PuzzleView *>()->puzzleScene();
    auto *item = scene->pieceItem(0);
    const QPointF target = model->pieces().front().source.topLeft();
    item->setPos(target + QPointF(3, 0));
    game->dropPiece(0, target, 0);
    auto *placement = item->findChild<QParallelAnimationGroup *>("placementAnimation");
    QVERIFY(placement);
    QCOMPARE(placement->state(), QAbstractAnimation::Running);
    motion->setChecked(true);
    QCOMPARE(placement->state(), QAbstractAnimation::Stopped);
    QCOMPARE(item->pos(), target);
    QCOMPARE(item->placementGlow(), 0.0);
    QCOMPARE(model->placedCount(), 1);
    motion->setChecked(false);
    QSignalSpy presented(scene, &PuzzleScene::completionPresentationChanged);
    QSignalSpy finished(scene, &PuzzleScene::completionEffectsFinished);
    for (int id = 1; id < model->pieces().size(); ++id)
        game->dropPiece(id, model->pieces().at(id).source.topLeft(), 0);
    QVERIFY(!scene->completionPresented());
    motion->setChecked(true);
    QVERIFY(scene->completionPresented());
    QCOMPARE(presented.size(), 1);
    QCOMPARE(finished.size(), 1);
    QTest::qWait(750);
    QCOMPARE(presented.size(), 1);
    QCOMPARE(finished.size(), 1);
    for (auto *layer : scene->items())
        QVERIFY(layer->data(0).toString() != QStringLiteral("completionSeamGlow"));
    motion->setChecked(false);
}
void MainWindowTest::completionGlowRendering()
{
    QImage original(320, 240, QImage::Format_RGB32);
    original.fill(QColor(32, 45, 61));
    PuzzleModel model;
    PuzzleScene scene(&model);
    QVERIFY(model.start(original, 17, 42));
    for (int id = 0; id < model.pieces().size(); ++id)
        QVERIFY(model.place(id, model.pieces().at(id).source.topLeft(), 0));
    QTRY_VERIFY(scene.completionPresented());
    auto *timeline = scene.findChild<QVariantAnimation *>("completionAnimation");
    QVERIFY(timeline);
    // Управляем временем, чтобы сравнение растров не зависело от загрузки машины.
    timeline->pause();
    timeline->setCurrentTime(timeline->duration() / 2);
    QGraphicsItem *glow = nullptr;
    for (auto *layer : scene.items())
        if (layer->data(0).toString() == QStringLiteral("completionSeamGlow")) glow = layer;
    QVERIFY(glow);
    QVERIFY(glow->acceptedMouseButtons() == Qt::NoButton);
    QImage rendered(original.size(), QImage::Format_ARGB32_Premultiplied);
    rendered.fill(Qt::transparent);
    QPainter painter(&rendered);
    scene.render(&painter, QRectF(original.rect()), model.boardRect());
    painter.end();
    int litPixels = 0;
    for (int y = 10; y < original.height() - 10; ++y)
        for (int x = 10; x < original.width() - 10; ++x)
            if (rendered.pixelColor(x, y).red() > original.pixelColor(x, y).red() + 10) ++litPixels;
    QVERIFY(litPixels > 100);
    // Между точками выхода внутренних швов внешний край должен оставаться тёмным.
    int unchangedBorder = 0;
    for (int x = 10; x < original.width() - 10; ++x) {
        unchangedBorder += rendered.pixelColor(x, 1) == original.pixelColor(x, 1);
        unchangedBorder += rendered.pixelColor(x, original.height() - 2) == original.pixelColor(x, original.height() - 2);
    }
    QVERIFY(unchangedBorder > original.width());
    timeline->resume();
    QSignalSpy finished(&scene, &PuzzleScene::completionEffectsFinished);
    timeline->setCurrentTime(timeline->duration());
    QCOMPARE(finished.size(), 1);
    rendered.fill(Qt::transparent);
    painter.begin(&rendered);
    scene.render(&painter, QRectF(original.rect()), model.boardRect());
    painter.end();
    QCOMPARE(rendered.convertToFormat(QImage::Format_RGB32), original);
}
void MainWindowTest::effectsLifetime_data()
{
    QTest::addColumn<int>("stage");
    QTest::newRow("placement") << 0;
    QTest::newRow("completion-delay") << 1;
    QTest::newRow("seam-glow") << 2;
}
void MainWindowTest::effectsLifetime()
{
    QFETCH(int, stage);
    QImage image(320, 240, QImage::Format_RGB32);
    image.fill(Qt::blue);
    PuzzleModel model;
    auto *scene = new PuzzleScene(&model);
    QVERIFY(model.start(image, 4, 42));
    QPointer<PieceItem> piece = scene->pieceItem(0);
    QSignalSpy finished(scene, &PuzzleScene::completionEffectsFinished);
    const int count = stage == 0 ? 1 : 4;
    for (int id = 0; id < count; ++id)
        QVERIFY(model.place(id, model.pieces().at(id).source.topLeft(), 0));
    if (stage == 2) QTRY_VERIFY(scene->completionPresented());
    else QVERIFY(!scene->completionPresented());
    delete scene;
    QVERIFY(piece.isNull());
    QTest::qWait(750);
    QCOMPARE(finished.size(), 0);
    // Модель продолжает жить: удалённая сцена не должна получать её сигналы.
    model.restart(123);
    QCOMPARE(model.placedCount(), 0);
}
void MainWindowTest::keyboardAndLargeText()
{
    const QFont previous = QApplication::font();
    const auto restore = qScopeGuard([previous] { QApplication::setFont(previous); });
    QFont font = previous; font.setPointSize(18); QApplication::setFont(font);
    MainWindow window;
    window.resize(900, 700);
    prepare(window);
    QCOMPARE(window.state(), AppController::State::Playing);
    auto *back = window.findChild<QPushButton *>("backButton");
    QVERIFY(back && back->font().pointSizeF() >= 18);
    back->setFocus();
    QTest::keyClick(back, Qt::Key_Space);
    QCOMPARE(window.state(), AppController::State::Configuring);
    auto *number = window.findChild<QSpinBox *>("pieceCountSpinBox");
    auto *start = window.findChild<QPushButton *>("startButton");
    number->setFocus(); QTest::keyClick(number, Qt::Key_Tab);
    QVERIFY(start->hasFocus());
    QTest::keyClick(start, Qt::Key_Space);
    QTRY_COMPARE(window.state(), AppController::State::Playing);
}
void MainWindowTest::cancelsPreparation()
{
    MainWindow window;
    prepare(window);
    QCOMPARE(window.state(), AppController::State::Playing);
    QTest::mouseClick(window.findChild<QPushButton *>("backButton"), Qt::LeftButton);
    auto *start = window.findChild<QPushButton *>("startButton");
    QTest::mouseClick(start, Qt::LeftButton, {}, {}, 0);
    QCOMPARE(window.state(), AppController::State::Preparing);
    QTest::mouseClick(window.findChild<QPushButton *>("backButton"), Qt::LeftButton, {}, {}, 0);
    QCoreApplication::processEvents();
    QCOMPARE(window.state(), AppController::State::Configuring);
}
QTEST_MAIN(MainWindowTest)
#include "tst_mainwindow.moc"
