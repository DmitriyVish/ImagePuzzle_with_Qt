#include "ui/mainwindow.h"
#include "ui/loadpage.h"
#include "ui/puzzleview.h"
#include "ui/gamehelpdialog.h"
#include "ui/gamepage.h"
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
#include <QPinchGesture>
#include <QGestureEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <cmath>

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
    /** @brief Проверяет первый показ для изображения, закрытие, повторное открытие и смену снимка. */
    void helpFirstGameAndReopen();
    /** @brief Проверяет прокрутку, фокус, отмену переноса и мгновенное отключение анимации подсказки. */
    void helpLayoutAndMotion();
    /** @brief Задаёт завершённое, отключённое и прерванное прокруткой появление правил. */
    void helpScrollKeepsText_data();
    /** @brief Сравнивает текст после прокрутки вниз/вверх с отрисовкой без эффектов прозрачности. */
    void helpScrollKeepsText();
    /** @brief Проверяет первый показ правил после отменённой подготовки нового изображения. */
    void helpAfterCancelledPreparation();
    /** @brief Проверяет повторное появление анимации и сброс прокрутки при открытии памятки. */
    void helpReopensAfterScroll();
    /** @brief Собирает пазл мышью и проверяет повтор, новую геометрию и возврат. */
    void mouseCompletesAndRestarts();
    /** @brief Проверяет перенос пальцем без дублирования мышью. */
    void touchPlacesPiece();
    /** @brief Проверяет двойной клик, направление, полный оборот и запрет поворота закреплённой детали. */
    void mouseRotatesPiece();
    /** @brief Задаёт направления и переходы через полный оборот для визуального поворота. */
    void rotationAnimation_data();
    /** @brief Проверяет промежуточный угол, неизменный центр, направление и точное завершение поворота. */
    void rotationAnimation();
    /** @brief Проверяет зелёные пиксели вихря и отсутствие его влияния на выбор детали. */
    void rotationVortexRendering();
    /** @brief Проверяет быстрые команды, захват во время поворота и отключение движения. */
    void rotationRetargetAndDrag();
    /** @brief Проверяет удаление вращающихся деталей при повторе и уничтожении сцены. */
    void rotationEffectsLifetime();
    /** @brief Проверяет закрепление и завершение игры во время визуального доворачивания. */
    void rotationDuringPlacement();
    /** @brief Задаёт направления реального вращения двумя пальцами. */
    void pinchRotatesPiece_data();
    /** @brief Проверяет распознавание касаний, шаг вращения и отсутствие отпускания после жеста. */
    void pinchRotatesPiece();
    /** @brief Задаёт направления полного оборота двумя пальцами. */
    void pinchFullTurn_data();
    /** @brief Проверяет переход угла через ±180° реальными касаниями без ложного отпускания детали. */
    void pinchFullTurn();
    /** @brief Проверяет порог pinch, дрожание, разворот и отсутствие масштабирования. */
    void pinchThresholdAndCancellation();
    /** @brief Задаёт ненулевые углы для проверки точки захвата. */
    void rotatedDrag_data();
    /** @brief Проверяет захват вне центра, увеличение и возврат повёрнутой миниатюры. */
    void rotatedDrag();
    /** @brief Проверяет дополнительные пальцы и отмену касания. */
    void touchCancellationAndExtraFinger();
    /** @brief Проверяет восстановление детали при потере активности окна. */
    void deactivationCancelsDrag();
    /** @brief Проверяет выбор файла, отмену диалога и сохранение картинки при ошибке. */
    void fileDialogAndErrors();
    /** @brief Проверяет доступность большого пазла, масштаб и сохранение позиций при resize. */
    void zoomResizeAndLargeCount();
    /** @brief Задаёт разворачивание окна и полноэкранный режим с разными способами обзора. */
    void windowStateResizesWorkspace_data();
    /** @brief Проверяет вписывание поля и лотка при разворачивании, включая экран меньше окна. */
    void windowStateResizesWorkspace();
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
     * @param window Окно. @param count Запрошенное количество, ограничиваемое допустимым диапазоном.
     * @param dismissHelp Закрыть автоматически показанные правила перед проверкой игрового ввода. */
    void prepare(MainWindow &window, int count = 4, bool dismissHelp = true);
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
void MainWindowTest::prepare(MainWindow &window, int count, bool dismissHelp)
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
    auto *help = window.findChild<GameHelpDialog *>();
    QVERIFY(help);
    QTRY_VERIFY(help->isVisible());
    if (dismissHelp) {
        QTest::mouseClick(help->findChild<QPushButton *>("helpUnderstoodButton"), Qt::LeftButton);
        QTRY_VERIFY(!help->isVisible());
        QVERIFY(QTest::qWaitForWindowActive(&window));
    }
    auto *view = window.findChild<PuzzleView *>();
    QVERIFY(view);
    view->fitAll();
    // Эти сценарии проверяют перенос и эффекты; вращение отдельно проверяется событиями ввода.
    auto *game = window.findChild<GameController *>();
    view->puzzleScene()->setReducedMotion(true);
    for (const auto &piece : game->model()->pieces())
        game->rotatePiece(piece.id, -piece.rotation / 90);
    view->puzzleScene()->setReducedMotion(window.findChild<QCheckBox *>("reducedMotionCheckBox")->isChecked());
}
void MainWindowTest::mousePlace(MainWindow &window, int id)
{
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    const PuzzlePiece piece = model->pieces().at(id);
    view->puzzleScene()->pieceItem(id)->setZValue(3);
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
void MainWindowTest::helpFirstGameAndReopen()
{
    MainWindow window;
    prepare(window, 4, false);
    auto *help = window.findChild<GameHelpDialog *>();
    auto *understood = help->findChild<QPushButton *>("helpUnderstoodButton");
    auto *button = window.findChild<QPushButton *>("howToPlayButton");
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    QVERIFY(view->toolTip().isEmpty());
    QVERIFY(help->isModal());
    QTRY_VERIFY(understood->hasFocus());
    QSignalSpy progress(model, &PuzzleModel::progressChanged);
    QTest::mouseClick(understood, Qt::LeftButton);
    QTRY_VERIFY(!help->isVisible());
    QTest::mouseClick(button, Qt::LeftButton);
    QTRY_VERIFY(help->isVisible());
    QCOMPARE(window.findChildren<GameHelpDialog *>().size(), 1);
    QTest::keyClick(understood, Qt::Key_Return);
    QTRY_VERIFY(!help->isVisible());
    QCOMPARE(progress.size(), 0);
    for (const char *name : {"restartButton", "regenerateButton"}) {
        QTest::mouseClick(window.findChild<QPushButton *>(name), Qt::LeftButton);
        QTRY_COMPARE(window.state(), AppController::State::Playing);
        QVERIFY(!help->isVisible());
    }
    auto *back = window.findChild<QPushButton *>("backButton");
    auto *start = window.findChild<QPushButton *>("startButton");
    auto *samples = window.findChild<QComboBox *>("sampleImagesCombo");
    const QString original = samples->currentData().toString();
    QTest::mouseClick(back, Qt::LeftButton);
    window.loadImage(original);
    QTRY_VERIFY(start->isEnabled());
    QTest::mouseClick(start, Qt::LeftButton);
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    QVERIFY(!help->isVisible());
    QTest::mouseClick(back, Qt::LeftButton);
    // Ошибка чтения оставляет прежнюю фотографию и не создаёт новую идентичность подсказки.
    window.loadImage(QStringLiteral(":/missing-help-image.jpg"));
    QTRY_VERIFY(start->isEnabled());
    QTest::mouseClick(start, Qt::LeftButton);
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    QVERIFY(!help->isVisible());
    QTest::mouseClick(back, Qt::LeftButton);
    samples->setCurrentIndex(2);
    QTest::mouseClick(window.findChild<QPushButton *>("sampleLoadButton"), Qt::LeftButton);
    QTRY_VERIFY(start->isEnabled());
    QTest::mouseClick(start, Qt::LeftButton);
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    QTRY_VERIFY(help->isVisible());
    // Возврат программным действием закрывает карточку и её анимации до смены страницы.
    window.findChild<GameController *>()->cancel();
    QVERIFY(!help->isVisible());
    window.loadImage(original);
    QTRY_VERIFY(start->isEnabled());
    QTest::mouseClick(start, Qt::LeftButton);
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    QVERIFY(!help->isVisible());
}

void MainWindowTest::helpLayoutAndMotion()
{
    MainWindow window;
    auto *motion = window.findChild<QCheckBox *>("reducedMotionCheckBox");
    const bool previous = motion->isChecked();
    const auto restore = qScopeGuard([&] { motion->setChecked(previous); });
    motion->setChecked(false);
    prepare(window, 4, false);
    auto *help = window.findChild<GameHelpDialog *>();
    auto *animation = help->findChild<QParallelAnimationGroup *>("helpRevealAnimation");
    auto *understood = help->findChild<QPushButton *>("helpUnderstoodButton");
    QVERIFY(animation);
    QCOMPARE(animation->state(), QAbstractAnimation::Running);
    motion->setChecked(true);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    for (auto *effect : help->findChildren<QGraphicsOpacityEffect *>())
        QCOMPARE(effect->opacity(), 1.0);
    QVERIFY(help->width() <= help->screen()->availableGeometry().width());
    QVERIFY(help->height() <= help->screen()->availableGeometry().height());
    QVERIFY(help->rect().contains(QRect(understood->mapTo(help, QPoint()), understood->size())));
    QTest::mouseClick(understood, Qt::LeftButton);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    auto *item = view->puzzleScene()->pieceItem(0);
    item->setZValue(3);
    const PuzzlePiece original = model->pieces().front();
    const QPoint at = view->mapFromScene(item->mapToScene(item->boundingRect().center()));
    QTest::mousePress(view->viewport(), Qt::LeftButton, {}, at, 0);
    QCOMPARE(item->scale(), 1.0);
    window.findChild<GamePage *>()->showHelp();
    QVERIFY(help->isVisible());
    QCOMPARE(item->pos(), original.position);
    QCOMPARE(item->scale(), original.scale);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    QTest::mouseClick(understood, Qt::LeftButton);
    motion->setChecked(false);
    QTest::mouseClick(window.findChild<QPushButton *>("howToPlayButton"), Qt::LeftButton);
    QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);
    QVERIFY(help->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("help-preview.png")));
    // Компактная карточка сохраняет доступную кнопку и позволяет добраться до последнего шага прокруткой.
    help->resize(520, 380);
    auto *scroll = help->findChild<QScrollArea *>("helpScrollArea");
    auto *last = help->findChild<QWidget *>("helpStep4");
    scroll->ensureWidgetVisible(last);
    QCoreApplication::processEvents();
    QVERIFY(scroll->viewport()->rect().intersects(QRect(last->mapTo(scroll->viewport(), QPoint()), last->size())));
    QVERIFY(help->rect().contains(QRect(understood->mapTo(help, QPoint()), understood->size())));
    QVERIFY(help->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("help-preview-compact.png")));
    QTest::mouseClick(understood, Qt::LeftButton);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
}

void MainWindowTest::helpScrollKeepsText_data()
{
    QTest::addColumn<bool>("reduced");
    QTest::addColumn<bool>("interrupt");
    QTest::newRow("after-reveal") << false << false;
    QTest::newRow("reduced-motion") << true << false;
    QTest::newRow("during-reveal") << false << true;
}

void MainWindowTest::helpScrollKeepsText()
{
    QFETCH(bool, reduced);
    QFETCH(bool, interrupt);
    MainWindow window;
    prepare(window);
    auto *help = window.findChild<GameHelpDialog *>();
    help->setReducedMotion(reduced);
    QTest::mouseClick(window.findChild<QPushButton *>("howToPlayButton"), Qt::LeftButton);
    help->resize(520, 380);
    auto *scroll = help->findChild<QScrollArea *>("helpScrollArea");
    auto *bar = scroll->verticalScrollBar();
    auto *animation = help->findChild<QParallelAnimationGroup *>("helpRevealAnimation");
    QTRY_VERIFY(bar->maximum() > 0);
    if (interrupt) {
        // Фиксируем промежуточную прозрачность, чтобы проверка не зависела от скорости машины.
        animation->pause();
        animation->setCurrentTime(100);
        bar->setValue(bar->maximum() / 2);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    } else {
        QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);
    }
    // Неполные шаги заставляют QScrollArea переносить уже нарисованные пиксели и дорисовывать открывшиеся полосы.
    const auto effects = help->findChildren<QGraphicsOpacityEffect *>();
    for (int position : {bar->maximum(), bar->maximum() / 2, 0, bar->maximum(), 0}) {
        bar->setValue(position);
        QCoreApplication::processEvents();
        const QImage actual = scroll->viewport()->grab().toImage();
        QVector<bool> enabled;
        for (auto *effect : effects) {
            enabled.append(effect->isEnabled());
            effect->setEnabled(false);
        }
        const QImage reference = scroll->viewport()->grab().toImage();
        for (int i = 0; i < effects.size(); ++i)
            effects.at(i)->setEnabled(enabled.at(i));
        QCOMPARE(actual, reference);
    }
    QCOMPARE(window.findChild<GameController *>()->model()->placedCount(), 0);
    QTest::mouseClick(help->findChild<QPushButton *>("helpUnderstoodButton"), Qt::LeftButton);
}

void MainWindowTest::helpAfterCancelledPreparation()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.findChild<QComboBox *>("sampleImagesCombo")->setCurrentIndex(1);
    QTest::mouseClick(window.findChild<QPushButton *>("sampleLoadButton"), Qt::LeftButton);
    auto *start = window.findChild<QPushButton *>("startButton");
    auto *help = window.findChild<GameHelpDialog *>();
    QTRY_VERIFY(start->isEnabled());
    QTest::mouseClick(start, Qt::LeftButton, {}, {}, 0);
    QCOMPARE(window.state(), AppController::State::Preparing);
    window.findChild<GameController *>()->cancel();
    QCoreApplication::processEvents();
    QCOMPARE(window.state(), AppController::State::Configuring);
    QVERIFY(!help->isVisible());
    QTest::mouseClick(start, Qt::LeftButton, {}, {}, 0);
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    QTRY_VERIFY(help->isVisible());
    QTest::mouseClick(help->findChild<QPushButton *>("helpUnderstoodButton"), Qt::LeftButton);
}

void MainWindowTest::helpReopensAfterScroll()
{
    MainWindow window;
    prepare(window);
    auto *help = window.findChild<GameHelpDialog *>();
    help->setReducedMotion(false);
    auto *button = window.findChild<QPushButton *>("howToPlayButton");
    auto *understood = help->findChild<QPushButton *>("helpUnderstoodButton");
    auto *animation = help->findChild<QParallelAnimationGroup *>("helpRevealAnimation");
    QTest::mouseClick(button, Qt::LeftButton);
    QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);
    help->resize(520, 380);
    auto *bar = help->findChild<QScrollArea *>("helpScrollArea")->verticalScrollBar();
    QTRY_VERIFY(bar->maximum() > 0);
    bar->setValue(bar->maximum());
    QTest::mouseClick(understood, Qt::LeftButton);
    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(bar->value(), 0);
    QCOMPARE(animation->state(), QAbstractAnimation::Running);
    bar->setValue(1);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    for (auto *effect : help->findChildren<QGraphicsOpacityEffect *>()) {
        QCOMPARE(effect->opacity(), 1.0);
        QVERIFY(!effect->isEnabled());
    }
    QTest::mouseClick(understood, Qt::LeftButton);
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
    view->puzzleScene()->pieceItem(0)->setZValue(3);
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

void MainWindowTest::mouseRotatesPiece()
{
    MainWindow window;
    prepare(window);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    auto *item = view->puzzleScene()->pieceItem(0);
    item->setZValue(3);
    const int initial = model->pieces().front().rotation;
    QSignalSpy drops(view, &PuzzleView::pieceDropped);
    for (int step = 1; step <= 4; ++step) {
        const QPoint at = view->mapFromScene(item->mapToScene(item->boundingRect().center()));
        const qreal scale = item->scale();
        QTest::mouseDClick(view->viewport(), Qt::LeftButton, {}, at, 0);
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, at, 0);
        QCOMPARE(model->pieces().front().rotation, (initial + step * 90) % 360);
        QTRY_COMPARE(item->findChild<QVariantAnimation *>("rotationAnimation")->state(), QAbstractAnimation::Stopped);
        const QRectF expected = model->pieces().front().boundsAtScale(item->scale()).translated(item->pos());
        QVERIFY(QLineF(item->sceneBoundingRect().topLeft(), expected.topLeft()).length() < 0.001);
        QVERIFY(QLineF(item->sceneBoundingRect().bottomRight(), expected.bottomRight()).length() < 0.001);
        QVERIFY(item->scale() <= scale);
        item->setZValue(3);
    }
    QCOMPARE(drops.size(), 0);
    const QPoint at = view->mapFromScene(item->mapToScene(item->boundingRect().center()));
    QTest::mouseDClick(view->viewport(), Qt::RightButton, {}, at, 0);
    QCOMPARE(model->pieces().front().rotation, initial);
    mousePlace(window, 0);
    item->finishPlacement();
    const QPoint locked = view->mapFromScene(item->mapToScene(item->boundingRect().center()));
    QTest::mouseDClick(view->viewport(), Qt::LeftButton, {}, locked, 0);
    QCOMPARE(model->pieces().front().rotation, 0);
    QVERIFY(model->pieces().front().placed);
}

void MainWindowTest::rotationAnimation_data()
{
    QTest::addColumn<int>("initial");
    QTest::addColumn<int>("turns");
    QTest::newRow("clockwise") << 0 << 1;
    QTest::newRow("counterclockwise-wrap") << 0 << -1;
    QTest::newRow("clockwise-wrap") << 270 << 1;
    QTest::newRow("half-turn-left") << 90 << -2;
    QTest::newRow("three-turns-right") << 180 << 3;
}

void MainWindowTest::rotationAnimation()
{
    QFETCH(int, initial);
    QFETCH(int, turns);
    QImage image(400, 400, QImage::Format_RGB32);
    image.fill(Qt::gray);
    PuzzleModel model;
    PuzzleScene scene(&model);
    scene.setReducedMotion(true);
    QVERIFY(model.start(image, 4, 42));
    model.rotate(0, (initial - model.pieces().front().rotation) / 90);
    auto *item = scene.pieceItem(0);
    QVERIFY(model.place(0, model.boardRect().center() - item->boundingRect().center(), 0));
    scene.setReducedMotion(false);
    const QPointF center = item->mapToScene(item->boundingRect().center());
    QSignalSpy progress(&model, &PuzzleModel::progressChanged);
    QVERIFY(model.rotate(0, turns));
    const int target = (initial + turns * 90 + 360) % 360;
    QCOMPARE(model.pieces().front().rotation, target);
    QCOMPARE(item->orientation(), target);
    auto *animation = item->findChild<QVariantAnimation *>("rotationAnimation");
    QCOMPARE(animation->state(), QAbstractAnimation::Running);
    animation->pause();
    animation->setCurrentTime(animation->duration() / 3);
    const qreal delta = item->displayedAngle() - initial;
    QVERIFY(delta * turns > 0);
    QVERIFY(qAbs(delta) < qAbs(turns * 90));
    QVERIFY(QLineF(item->mapToScene(item->boundingRect().center()), center).length() < 0.00001);
    QVERIFY(item->rotationProgress() > 0 && item->rotationProgress() < 1);
    QCOMPARE(scene.itemAt(center, QTransform()), static_cast<QGraphicsItem *>(item));
    QCOMPARE(progress.size(), 0);
    animation->setCurrentTime(animation->duration());
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    QCOMPARE(item->displayedAngle(), qreal(target));
    QCOMPARE(item->rotationProgress(), 0.0);
    QCOMPARE(item->transform(), model.pieces().front().rotationTransform());
    for (auto *child : item->childItems()) {
        if (child->data(0).toString() == QStringLiteral("rotationVortex"))
            QVERIFY(!child->isVisible());
    }
}

void MainWindowTest::rotationVortexRendering()
{
    QImage image(160, 120, QImage::Format_RGB32);
    image.fill(QColor(52, 52, 52));
    PuzzlePiece piece;
    piece.id = 0;
    piece.source = image.rect();
    piece.shape.addRoundedRect(QRectF(QPointF(), image.size()), 12, 12);
    QGraphicsScene scene;
    scene.setSceneRect(-40, -60, 240, 240);
    auto *item = new PieceItem(image, piece);
    scene.addItem(item);
    item->rotateTo(90, 1, true);
    auto *animation = item->findChild<QVariantAnimation *>("rotationAnimation");
    animation->pause();
    animation->setCurrentTime(animation->duration() / 5);
    QGraphicsItem *vortex = nullptr;
    for (auto *child : item->childItems())
        if (child->data(0).toString() == QStringLiteral("rotationVortex")) vortex = child;
    QVERIFY(vortex && vortex->isVisible());
    QVERIFY(vortex->shape().isEmpty());
    QImage rendered(480, 480, QImage::Format_ARGB32_Premultiplied);
    rendered.fill(QColor("#F7F4EE"));
    QPainter painter(&rendered);
    scene.render(&painter, QRectF(rendered.rect()), scene.sceneRect());
    painter.end();
    int green = 0;
    for (int y = 0; y < rendered.height(); ++y) {
        for (int x = 0; x < rendered.width(); ++x) {
            const QColor color = rendered.pixelColor(x, y);
            green += color.alpha() > 40 && color.green() > color.red() + 20 && color.green() > color.blue() + 10;
        }
    }
    QVERIFY(green > 150);
    QVERIFY(rendered.save(QDir(QCoreApplication::applicationDirPath()).filePath("rotation-vortex-preview.png")));
    item->finishRotation();
    QVERIFY(!vortex->isVisible());
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
}

void MainWindowTest::rotationRetargetAndDrag()
{
    MainWindow window;
    prepare(window);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    auto *scene = view->puzzleScene();
    scene->setReducedMotion(false);
    auto *item = scene->pieceItem(0);
    QVERIFY(model->place(0, model->boardRect().center() - item->boundingRect().center(), 0));
    auto *animation = item->findChild<QVariantAnimation *>("rotationAnimation");
    QVERIFY(model->rotate(0, 1));
    animation->pause();
    animation->setCurrentTime(animation->duration() / 4);
    const qreal first = item->displayedAngle();
    QVERIFY(model->rotate(0, 1));
    QCOMPARE(item->displayedAngle(), first);
    animation->pause();
    animation->setCurrentTime(animation->duration() / 2);
    QVERIFY(item->displayedAngle() > first && item->displayedAngle() < 180);
    QVERIFY(model->rotate(0, -1));
    animation->pause();
    animation->setCurrentTime(animation->duration());
    QCOMPARE(item->displayedAngle(), 90.0);
    // Четыре быстрых команды должны показать полный оборот в выбранную сторону.
    model->rotate(0, -1);
    item->finishRotation();
    for (int i = 0; i < 4; ++i) QVERIFY(model->rotate(0, 1));
    animation->pause();
    animation->setCurrentTime(animation->duration() * 3 / 4);
    QVERIFY(item->displayedAngle() > 270 && item->displayedAngle() < 360);
    animation->setCurrentTime(animation->duration());
    QCOMPARE(item->displayedAngle(), 0.0);
    QVERIFY(model->rotate(0, 1));
    animation->pause();
    animation->setCurrentTime(animation->duration() / 3);
    const QPoint at = view->mapFromScene(item->mapToScene(item->boundingRect().center() + QPointF(3, 2)));
    const QPointF grabbed = item->mapFromScene(view->mapToScene(at));
    QTest::mousePress(view->viewport(), Qt::LeftButton, {}, at, 0);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    QCOMPARE(item->displayedAngle(), 90.0);
    QVERIFY((view->mapFromScene(item->mapToScene(grabbed)) - at).manhattanLength() <= 2);
    view->cancelPointer();
    QVERIFY(model->rotate(0, -1));
    scene->setReducedMotion(true);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    QCOMPARE(item->displayedAngle(), 0.0);
    QVERIFY(model->rotate(0, -1));
    QCOMPARE(item->displayedAngle(), 270.0);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    QCOMPARE(model->placedCount(), 0);
}

void MainWindowTest::rotationEffectsLifetime()
{
    QImage image(400, 400, QImage::Format_RGB32);
    image.fill(Qt::gray);
    PuzzleModel model;
    auto *scene = new PuzzleScene(&model);
    QVERIFY(model.start(image, 4, 42));
    QPointer<PieceItem> item = scene->pieceItem(0);
    QPointer<QVariantAnimation> animation = item->findChild<QVariantAnimation *>("rotationAnimation");
    QVERIFY(model.rotate(0, 1));
    model.restart(99);
    QVERIFY(item.isNull());
    QVERIFY(animation.isNull());
    item = scene->pieceItem(0);
    animation = item->findChild<QVariantAnimation *>("rotationAnimation");
    QVERIFY(model.rotate(0, -1));
    delete scene;
    QVERIFY(item.isNull());
    QVERIFY(animation.isNull());
    QTest::qWait(450);
}

void MainWindowTest::rotationDuringPlacement()
{
    QImage image(400, 400, QImage::Format_RGB32);
    image.fill(Qt::gray);
    PuzzleModel model;
    PuzzleScene scene(&model);
    scene.setReducedMotion(true);
    QVERIFY(model.start(image, 4, 42));
    for (const auto &piece : model.pieces()) model.rotate(piece.id, -piece.rotation / 90);
    auto *item = scene.pieceItem(0);
    const QPointF centerPosition = model.boardRect().center() - item->boundingRect().center();
    QVERIFY(model.place(0, centerPosition, 0));
    scene.setReducedMotion(false);
    QSignalSpy progress(&model, &PuzzleModel::progressChanged);
    QSignalSpy completed(&model, &PuzzleModel::completed);
    QVERIFY(model.rotate(0, 1));
    auto *rotation = item->findChild<QVariantAnimation *>("rotationAnimation");
    rotation->pause();
    rotation->setCurrentTime(rotation->duration() / 3);
    QVERIFY(model.place(0, centerPosition, 0));
    QVERIFY(!model.pieces().front().placed);
    QCOMPARE(progress.size(), 0);
    QVERIFY(model.rotate(0, -1));
    rotation->pause();
    rotation->setCurrentTime(rotation->duration() / 3);
    QVERIFY(item->displayedAngle() > 0 && item->displayedAngle() < 90);
    // Модель уже подтвердила ориентацию 0°; закрепление должно закончить визуальное доворачивание.
    QVERIFY(model.place(0, model.pieces().front().source.topLeft(), 0));
    QCOMPARE(item->displayedAngle(), 0.0);
    QCOMPARE(rotation->state(), QAbstractAnimation::Stopped);
    QVERIFY(item->locked());
    for (auto *child : item->childItems())
        if (child->data(0).toString() == QStringLiteral("rotationVortex")) QVERIFY(!child->isVisible());
    auto *placement = item->findChild<QParallelAnimationGroup *>("placementAnimation");
    QVERIFY(placement);
    QCOMPARE(placement->state(), QAbstractAnimation::Running);
    for (int id = 1; id < model.pieces().size(); ++id)
        QVERIFY(model.place(id, model.pieces().at(id).source.topLeft(), 0));
    QCOMPARE(progress.size(), 4);
    QCOMPARE(completed.size(), 1);
    scene.setReducedMotion(true);
    QVERIFY(scene.completionPresented());
    QVERIFY(!model.rotate(0, 1));
    QCOMPARE(completed.size(), 1);
}

void MainWindowTest::pinchFullTurn_data() { pinchRotatesPiece_data(); }

void MainWindowTest::pinchFullTurn()
{
    QFETCH(int, direction);
    MainWindow window;
    prepare(window);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    QImage image(480, 480, QImage::Format_RGB32);
    image.fill(Qt::gray);
    view->puzzleScene()->setReducedMotion(true);
    QVERIFY(model->start(image, 4, 42));
    model->rotate(0, -model->pieces().front().rotation / 90);
    auto *item = view->puzzleScene()->pieceItem(0);
    QVERIFY(model->place(0, model->boardRect().center() - item->boundingRect().center(), 0));
    view->puzzleScene()->setReducedMotion(false);
    view->fitAll();
    const QPoint center = view->mapFromScene(item->mapToScene(item->boundingRect().center()));
    window.move(window.pos() + window.screen()->geometry().center() - view->viewport()->mapToGlobal(center));
    QCoreApplication::processEvents();
    const auto at = [&](QPoint point) { return view->viewport()->mapTo(&window, point); };
    QSignalSpy rotations(model, &PuzzleModel::pieceRotated);
    QSignalSpy drops(view, &PuzzleView::pieceDropped);
    auto sequence = QTest::touchEvent(window.windowHandle(), m_device, false);
    sequence.press(0, at(center), window.windowHandle()).commit();
    sequence.stationary(0).press(1, at(center + QPoint(50, 0)), window.windowHandle()).commit();
    // Шаги по 45° проходят обе стороны разрыва угла QPinchGesture у ±180°.
    for (int step = 1; step <= 8; ++step) {
        const qreal angle = direction * step * 3.141592653589793 / 4;
        const QPoint offset(qRound(50 * std::cos(angle)), qRound(50 * std::sin(angle)));
        sequence.stationary(0).move(1, at(center + offset), window.windowHandle()).commit();
    }
    QCOMPARE(rotations.size(), 4);
    for (const auto &arguments : rotations) QCOMPARE(arguments.at(1).toInt(), direction);
    QCOMPARE(model->pieces().front().rotation, 0);
    auto *animation = item->findChild<QVariantAnimation *>("rotationAnimation");
    animation->pause();
    animation->setCurrentTime(animation->duration() * 3 / 4);
    QVERIFY(item->displayedAngle() * direction > 270);
    animation->setCurrentTime(animation->duration());
    QCOMPARE(item->displayedAngle(), 0.0);
    sequence.stationary(0).release(1, at(center + QPoint(50, 0)), window.windowHandle()).commit();
    sequence.release(0, at(center), window.windowHandle()).commit();
    QCOMPARE(drops.size(), 0);
    QCOMPARE(model->placedCount(), 0);
}

void MainWindowTest::pinchRotatesPiece_data()
{
    QTest::addColumn<int>("direction");
    QTest::newRow("clockwise") << 1;
    QTest::newRow("counterclockwise") << -1;
}

void MainWindowTest::pinchRotatesPiece()
{
    QFETCH(int, direction);
    MainWindow window;
    prepare(window);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    auto *item = view->puzzleScene()->pieceItem(0);
    // Центр поля оставляет запас для обоих пальцев независимо от случайной позиции в лотке.
    QVERIFY(model->place(0, model->boardRect().center() - item->boundingRect().center(), 0));
    item->setZValue(3);
    const int initial = model->pieces().front().rotation;
    const QPoint center = view->mapFromScene(item->mapToScene(item->boundingRect().center()));
    // Qt ищет получателя pinch через экранную hotSpot. При DPR=2 виртуальный экран
    // offscreen меньше окна: переносим точку касания в его видимую часть.
    window.move(window.pos() + window.screen()->geometry().center() - view->viewport()->mapToGlobal(center));
    QCoreApplication::processEvents();
    const auto at = [&](QPoint point) { return view->viewport()->mapTo(&window, point); };
    QSignalSpy drops(view, &PuzzleView::pieceDropped);
    auto sequence = QTest::touchEvent(window.windowHandle(), m_device, false);
    sequence.press(0, at(center), window.windowHandle()).commit();
    sequence.stationary(0).press(1, at(center + QPoint(50, 0)), window.windowHandle()).commit();
    sequence.stationary(0).move(1, at(center + QPoint(35, direction * 35)), window.windowHandle()).commit();
    sequence.stationary(0).move(1, at(center + QPoint(0, direction * 50)), window.windowHandle()).commit();
    QCOMPARE(model->pieces().front().rotation, (initial + direction * 90 + 360) % 360);
    sequence.stationary(0).release(1, at(center + QPoint(0, direction * 50)), window.windowHandle()).commit();
    sequence.move(0, at(center + QPoint(-10, 0)), window.windowHandle()).commit();
    sequence.release(0, at(center + QPoint(-10, 0)), window.windowHandle()).commit();
    QCOMPARE(drops.size(), 0);
    QCOMPARE(model->placedCount(), 0);
    QCOMPARE(item->pos(), model->pieces().front().position);
}

void MainWindowTest::pinchThresholdAndCancellation()
{
    MainWindow window;
    prepare(window);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    auto *item = view->puzzleScene()->pieceItem(0);
    QVERIFY(model->place(0, model->boardRect().center() - item->boundingRect().center(), 0));
    item->setZValue(3);
    const QPoint center = view->mapFromScene(item->mapToScene(item->boundingRect().center()));
    const qreal scale = view->transform().m11();
    const int initial = model->pieces().front().rotation;
    QPinchGesture pinch;
    pinch.setCenterPoint(view->viewport()->mapToGlobal(center));
    // Событие несёт также масштаб: вращение детали не должно менять обзор.
    const auto update = [&](qreal last, qreal angle) {
        pinch.setLastRotationAngle(last);
        pinch.setRotationAngle(angle);
        pinch.setScaleFactor(2);
        pinch.setChangeFlags(QPinchGesture::RotationAngleChanged | QPinchGesture::ScaleFactorChanged);
        QGestureEvent gesture({&pinch});
        QCoreApplication::sendEvent(view->viewport(), &gesture);
    };
    update(0, 15);
    QCOMPARE(model->pieces().front().rotation, initial);
    update(15, 65);
    QCOMPARE(model->pieces().front().rotation, (initial + 90) % 360);
    update(65, 61);
    QCOMPARE(model->pieces().front().rotation, (initial + 90) % 360);
    update(61, 0);
    QCOMPARE(model->pieces().front().rotation, initial);
    QCOMPARE(view->transform().m11(), scale);
    QTouchEvent cancel(QEvent::TouchCancel, m_device);
    QCoreApplication::sendEvent(view->viewport(), &cancel);
    mousePlace(window, 0);
    QVERIFY(model->pieces().front().placed);
}

void MainWindowTest::rotatedDrag_data()
{
    QTest::addColumn<int>("rotation");
    for (int angle : {90, 180, 270})
        QTest::newRow(qPrintable(QString::number(angle))) << angle;
}

void MainWindowTest::rotatedDrag()
{
    QFETCH(int, rotation);
    MainWindow window;
    prepare(window);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    // Квадратное изображение оставляет на поле место для детали во всех ориентациях.
    QImage image(640, 640, QImage::Format_RGB32);
    image.fill(Qt::green);
    QVERIFY(model->start(image, 4, 42));
    model->rotate(0, (rotation - model->pieces().front().rotation) / 90);
    QCOMPARE(model->pieces().front().rotation, rotation);
    auto *item = view->puzzleScene()->pieceItem(0);
    item->finishRotation();
    item->setZValue(3);
    const PuzzlePiece original = model->pieces().front();
    const QPointF local = item->boundingRect().center() + QPointF(3, 2);
    QVERIFY(item->contains(local));
    const QPoint start = view->mapFromScene(item->mapToScene(local));
    const QPointF grabbed = item->mapFromScene(view->mapToScene(start));
    QTest::mousePress(view->viewport(), Qt::LeftButton, {}, start, 0);
    QCOMPARE(item->scale(), 1.0);
    QVERIFY((view->mapFromScene(item->mapToScene(grabbed)) - start).manhattanLength() <= 2);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, start, 0);
    QCOMPARE(model->pieces().front().rotation, rotation);
    QCOMPARE(item->scale(), original.scale);
    QVERIFY(QLineF(item->pos(), original.position).length() < 0.001);
    QCOMPARE(model->placedCount(), 0);
    // На поле повёрнутая деталь остаётся свободной; её настоящий контур следует за точкой захвата.
    const QPoint end = view->mapFromScene(model->boardRect().center()
        + original.rotationTransform().map(grabbed) - item->boundingRect().center());
    item->setZValue(3);
    QTest::mousePress(view->viewport(), Qt::LeftButton, {}, start, 0);
    QTest::mouseMove(view->viewport(), end, 0);
    QVERIFY((view->mapFromScene(item->mapToScene(grabbed)) - end).manhattanLength() <= 2);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, end, 0);
    QCOMPARE(item->scale(), 1.0);
    QVERIFY(!model->pieces().front().placed);
    view->cancelPointer();
    QCOMPARE(item->transform(), original.rotationTransform());
}
void MainWindowTest::touchCancellationAndExtraFinger()
{
    MainWindow window;
    prepare(window);
    QCOMPARE(window.state(), AppController::State::Playing);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    const auto original = model->pieces().front();
    view->puzzleScene()->pieceItem(0)->setZValue(3);
    const QPointF offset = QRectF(QPointF(), original.source.size()).center();
    const QPoint start = view->mapFromScene(view->puzzleScene()->pieceItem(original.id)->mapToScene(offset));
    const QPoint end = view->mapFromScene(QPointF(original.source.topLeft()) + offset);
    const auto windowPoint = [view, &window](QPoint point) { return view->viewport()->mapTo(&window, point); };
    auto sequence = QTest::touchEvent(window.windowHandle(), m_device, false);
    sequence.press(0, windowPoint(start), window.windowHandle()).commit();
    QCOMPARE(view->puzzleScene()->pieceItem(0)->scale(), 1.0);
    sequence.stationary(0).press(1, windowPoint(QPoint(10, 10)), window.windowHandle()).commit();
    sequence.stationary(0).move(1, windowPoint(end), window.windowHandle()).commit();
    QCOMPARE(view->puzzleScene()->pieceItem(0)->pos(), original.position);
    sequence.stationary(0).release(1, windowPoint(end), window.windowHandle()).commit();
    sequence.move(0, windowPoint(end), window.windowHandle()).commit();
    QCOMPARE(view->puzzleScene()->pieceItem(0)->pos(), original.position);
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
    const qreal manualScale = view->transform().m11();
    window.resize(640, 480);
    QCoreApplication::processEvents();
    QCOMPARE(view->transform().m11(), manualScale);
    for (int i = 0; i < original.size(); ++i)
        QCOMPARE(model->pieces().at(i).position, original.at(i).position);
    QTest::mouseClick(window.findChild<QPushButton *>("fitButton"), Qt::LeftButton);
    auto *back = window.findChild<QPushButton *>("backButton");
    QVERIFY(window.rect().contains(QRect(back->mapTo(&window, QPoint()), back->size())));
    QTest::mouseClick(back, Qt::LeftButton);
    QCOMPARE(window.state(), AppController::State::Configuring);
}
void MainWindowTest::windowStateResizesWorkspace_data()
{
    QTest::addColumn<bool>("fullScreen");
    QTest::addColumn<int>("interaction");
    for (bool fullScreen : {false, true}) {
        const QByteArray mode = fullScreen ? "fullscreen-" : "maximized-";
        QTest::newRow((mode + "fit").constData()) << fullScreen << 0;
        QTest::newRow((mode + "zoom").constData()) << fullScreen << 1;
        QTest::newRow((mode + "pan").constData()) << fullScreen << 2;
    }
}
void MainWindowTest::windowStateResizesWorkspace()
{
    QFETCH(bool, fullScreen);
    QFETCH(int, interaction);
    MainWindow window;
    window.resize(700, 500);
    prepare(window, 20);
    auto *view = window.findChild<PuzzleView *>();
    auto *model = window.findChild<GameController *>()->model();
    const QSize normalSize = window.size();
    const QSize normalViewport = view->viewport()->size();
    const QRect normalBoard = view->mapFromScene(model->boardRect()).boundingRect();
    const QRect normalTray = view->mapFromScene(model->trayRect()).boundingRect();
    const auto pieces = model->pieces();
    const QRect screen = fullScreen ? window.screen()->geometry() : window.screen()->availableGeometry();
    // При большом DPR виртуальный экран может быть меньше минимального окна.
    // Увеличение проверяем только при запасе под рамку; вписывание обязательно при любом размере.
    const bool canGrow = screen.width() > normalSize.width() + 64 && screen.height() > normalSize.height() + 64;
    if (interaction == 1) view->zoomIn();
    if (fullScreen) window.showFullScreen();
    else window.showMaximized();
    QTRY_VERIFY(fullScreen ? window.isFullScreen() : window.isMaximized());
    QCoreApplication::processEvents();
    if (canGrow)
        QTRY_VERIFY(view->viewport()->height() > normalViewport.height());
    QTRY_VERIFY(view->viewport()->rect().contains(view->mapFromScene(model->boardRect()).boundingRect()));
    QTRY_VERIFY(view->viewport()->rect().contains(view->mapFromScene(model->trayRect()).boundingRect()));
    if (interaction == 1) {
        view->zoomIn();
    } else if (interaction == 2) {
        // Пустой фон запускает панорамирование и отключает автоматическое вписывание.
        QTest::mousePress(view->viewport(), Qt::LeftButton, {}, QPoint(4, 4));
        QTest::mouseMove(view->viewport(), QPoint(14, 14));
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, QPoint(14, 14));
    }
    window.showNormal();
    QTRY_COMPARE(window.size(), normalSize);
    QTRY_COMPARE(view->viewport()->size(), normalViewport);
    QTRY_COMPARE(view->mapFromScene(model->boardRect()).boundingRect(), normalBoard);
    QTRY_COMPARE(view->mapFromScene(model->trayRect()).boundingRect(), normalTray);
    QVERIFY(view->viewport()->rect().contains(normalBoard));
    QVERIFY(view->viewport()->rect().contains(normalTray));
    QCOMPARE(model->placedCount(), 0);
    for (int id = 0; id < pieces.size(); ++id) {
        QCOMPARE(model->pieces().at(id).position, pieces.at(id).position);
        QCOMPARE(model->pieces().at(id).scale, pieces.at(id).scale);
    }
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
    item->setZValue(3);
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
    // Подготовка теста выправляет случайные углы, поэтому соседние миниатюры могут перекрыться.
    item->setZValue(3);
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
    item->setZValue(3);
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
    for (int id = 0; id < model->pieces().size(); ++id) {
        game->rotatePiece(id, -model->pieces().at(id).rotation / 90);
        game->dropPiece(id, model->pieces().at(id).source.topLeft(), 0);
    }
    QVERIFY(!scene->completionPresented());
    game->restart();
    QTRY_COMPARE(window.state(), AppController::State::Playing);
    QVERIFY(old.isNull());
    QTest::qWait(750);
    QVERIFY(!scene->completionPresented());
    QCOMPARE(model->placedCount(), 0);
    for (int id = 0; id < model->pieces().size(); ++id) {
        game->rotatePiece(id, -model->pieces().at(id).rotation / 90);
        game->dropPiece(id, model->pieces().at(id).source.topLeft(), 0);
    }
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
    for (int id = 0; id < model->pieces().size(); ++id) {
        game->rotatePiece(id, -model->pieces().at(id).rotation / 90);
        game->dropPiece(id, model->pieces().at(id).source.topLeft(), 0);
    }
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
        item->setZValue(3);
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
    for (int id = 0; id < model.pieces().size(); ++id) {
        model.rotate(id, -model.pieces().at(id).rotation / 90);
        QVERIFY(model.place(id, model.pieces().at(id).source.topLeft(), 0));
    }
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
    for (int id = 0; id < count; ++id) {
        model.rotate(id, -model.pieces().at(id).rotation / 90);
        QVERIFY(model.place(id, model.pieces().at(id).source.topLeft(), 0));
    }
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
    QVERIFY2(start->hasFocus(), qPrintable(QStringLiteral("Focused widget: %1 (%2), active window: %3")
        .arg(QApplication::focusWidget() ? QApplication::focusWidget()->objectName() : QStringLiteral("none"))
        .arg(QApplication::focusWidget() ? QString::fromLatin1(QApplication::focusWidget()->metaObject()->className()) : QString())
        .arg(QApplication::activeWindow() ? QApplication::activeWindow()->objectName() : QStringLiteral("none"))));
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
