#include "mainwindow.h"
#include "gamepage.h"
#include "loadpage.h"
#include "puzzleview.h"
#include "expeditiontheme.h"
#include "app/gamecontroller.h"
#include "ui_mainwindow.h"
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QtConcurrentRun>
#include <QCheckBox>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QSettings>
#include <QStatusBar>

Q_LOGGING_CATEGORY(lcWindow, "imagepuzzle.ui")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_ui(std::make_unique<Ui::MainWindow>()),
      m_controller(new AppController(this)), m_game(new GameController(m_controller, this)),
      m_loader(new QFutureWatcher<ImageLoadResult>(this))
{
    m_ui->setupUi(this);
    ExpeditionTheme::apply(this);
    setWindowTitle(tr("Собери свой Север · Малые Корелы"));
    auto *brand = new QFrame(m_ui->centralwidget);
    brand->setObjectName(QStringLiteral("brandHeader"));
    auto *brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(0, 0, 0, 12);
    auto *brandName = new QLabel(tr("МАЛЫЕ КОРЕЛЫ"), brand);
    QFont brandFont = font(); brandFont.setBold(true); brandName->setFont(brandFont);
    brandLayout->addWidget(brandName);
    brandLayout->addStretch();
    auto *motion = new QCheckBox(tr("Меньше анимации"), brand);
    motion->setObjectName(QStringLiteral("reducedMotionCheckBox"));
    motion->setMinimumHeight(44);
    QFont small = font(); small.setPointSizeF(font().pointSizeF() / 1.2); motion->setFont(small);
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("ImagePuzzle"), QStringLiteral("NorthernExpedition"));
    m_reducedMotion = settings.value(QStringLiteral("reducedMotion"), false).toBool();
    motion->setChecked(m_reducedMotion);
    connect(motion, &QCheckBox::toggled, this, [this](bool reduced) {
        m_reducedMotion = reduced;
        if (auto *view = findChild<PuzzleView *>()) view->puzzleScene()->setReducedMotion(reduced);
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("ImagePuzzle"), QStringLiteral("NorthernExpedition"));
        settings.setValue(QStringLiteral("reducedMotion"), reduced);
        settings.sync();
        if (settings.status() != QSettings::NoError)
            qCWarning(lcWindow) << "Cannot persist reduced motion setting:" << settings.fileName() << settings.status();
        // Отключение действует и на уже начатый эффект, а не только на следующий экран.
        if (reduced) {
            for (auto *animation : findChildren<QPropertyAnimation *>()) animation->stop();
            for (auto *effect : findChildren<QGraphicsOpacityEffect *>()) effect->setOpacity(1);
        }
        qCInfo(lcWindow) << "Reduced motion:" << reduced;
    });
    brandLayout->addWidget(motion);
    m_ui->windowLayout->insertWidget(0, brand);
    m_loadPage = new LoadPage(m_ui->pages);
    m_gamePage = new GamePage(m_game->model(), m_ui->pages);
    m_gamePage->findChild<PuzzleView *>()->puzzleScene()->setReducedMotion(m_reducedMotion);
    m_ui->pages->addWidget(m_loadPage);
    m_ui->pages->addWidget(m_gamePage);
    m_controller->setObjectName(QStringLiteral("appController"));
    connect(m_loadPage, &LoadPage::imageRequested, this, &MainWindow::loadImage);
    connect(m_loadPage, &LoadPage::startRequested, this, &MainWindow::openGame);
    connect(m_gamePage, &GamePage::setupRequested, this, &MainWindow::showSetup);
    connect(m_gamePage, &GamePage::restartRequested, m_game, &GameController::restart);
    connect(m_gamePage, &GamePage::regenerateRequested, m_game, &GameController::regenerate);
    connect(m_gamePage->findChild<PuzzleView *>(), &PuzzleView::pieceDropped, m_game, &GameController::dropPiece);
    connect(m_controller, &AppController::stateChanged, this, &MainWindow::updateScreen);
    connect(m_loader, &QFutureWatcher<ImageLoadResult>::finished, this, &MainWindow::imageLoaded);
    connect(m_game, &GameController::preparationFailed, m_loadPage, &LoadPage::showError);
    updateScreen(m_controller->state());
}
MainWindow::~MainWindow()
{
    // Виджеты, использующие модель, удаляются до дочернего игрового контроллера.
    if (auto *view = findChild<PuzzleView *>())
        view->cancelPointer();
    delete takeCentralWidget();
}
AppController::State MainWindow::state() const { return m_controller->state(); }
void MainWindow::loadImage(const QString &path)
{
    if (m_loader->isRunning() || m_controller->state() != AppController::State::Configuring || path.isEmpty())
        return;
    m_imagePath = path;
    m_loadPage->setBusy(true);
    // Фоновый код захватывает только путь, а не окно или виджеты.
    m_loader->setFuture(QtConcurrent::run([path] { return ImageLoader::load(path); }));
}
void MainWindow::imageLoaded()
{
    const ImageLoadResult result = m_loader->result();
    m_loadPage->setBusy(false);
    if (!result.error.isEmpty()) {
        m_loadPage->showError(result.error);
        return;
    }
    m_image = result.image;
    m_loadPage->setImage(m_image, QFileInfo(m_imagePath).fileName());
}
void MainWindow::openGame(int pieceCount)
{
    if (m_image.isNull() || m_loader->isRunning())
        return;
    m_game->start(m_image, pieceCount, QRandomGenerator::global()->generate());
}
void MainWindow::showSetup() { m_game->cancel(); }
void MainWindow::updateScreen(AppController::State state)
{
    m_gamePage->setState(state);
    const bool configuring = state == AppController::State::Configuring;
    QWidget *page = configuring ? static_cast<QWidget *>(m_loadPage) : static_cast<QWidget *>(m_gamePage);
    m_ui->pages->setCurrentWidget(page);
    page->setFocus(Qt::OtherFocusReason);
    if (state == AppController::State::Configuring)
        animateArrival(m_loadPage->findChild<QLabel *>(QStringLiteral("loadTitle")));
    else if (state == AppController::State::Completed)
        animateArrival(m_gamePage->findChild<QLabel *>(QStringLiteral("completionLabel")));
    switch (state) {
    case AppController::State::Configuring: statusBar()->showMessage(tr("Настройка новой игры")); break;
    case AppController::State::Preparing: statusBar()->showMessage(tr("Подготовка игрового поля…")); break;
    case AppController::State::Playing: statusBar()->showMessage(tr("Перетаскивайте фрагменты на поле слева")); break;
    case AppController::State::Completed: statusBar()->showMessage(tr("Изображение собрано")); break;
    }
    qCDebug(lcWindow) << "Screen state:" << state;
}
void MainWindow::animateArrival(QWidget *widget)
{
    if (!widget || m_reducedMotion)
        return;
    // Анимируется только небольшой заголовок: поле и координаты пальца остаются стабильными.
    auto *effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    auto *animation = new QPropertyAnimation(effect, "opacity", effect);
    animation->setDuration(220);
    animation->setStartValue(0.35);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
