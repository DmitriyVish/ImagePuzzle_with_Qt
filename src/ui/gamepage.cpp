#include "gamepage.h"
#include "puzzleview.h"
#include "gamehelpdialog.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

GamePage::GamePage(PuzzleModel *model, QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("gamePage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(tr("Собираем Север"), this);
    title->setWordWrap(true);
    QFont font = title->font(); font.setPointSizeF(font.pointSizeF() * 1.5); font.setBold(true);
    title->setFont(font);
    header->addWidget(title, 1);
    m_helpButton = new QPushButton(tr("Как играть?"), this);
    m_helpButton->setObjectName(QStringLiteral("howToPlayButton"));
    m_helpButton->setMinimumHeight(44);
    header->addWidget(m_helpButton);
    auto *back = new QPushButton(tr("← К выбору"), this);
    back->setObjectName(QStringLiteral("backButton"));
    back->setMinimumHeight(44);
    header->addWidget(back);
    layout->addLayout(header);
    m_summary = new QLabel(tr("Подготовка игрового поля…"), this);
    m_summary->setObjectName(QStringLiteral("pieceCountLabel"));
    layout->addWidget(m_summary);
    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("gameProgress"));
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);
    m_progress->setMaximumHeight(12);
    layout->addWidget(m_progress);
    m_completion = new QLabel(tr("Получилось! Теперь можно рассмотреть фотографию целиком."), this);
    m_completion->setObjectName(QStringLiteral("completionLabel"));
    m_completion->setWordWrap(true);
    layout->addWidget(m_completion);
    m_view = new PuzzleView(model, this);
    m_view->setMinimumSize(160, 80);
    layout->addWidget(m_view, 1);
    auto *zoomRow = new QHBoxLayout;
    auto *minus = new QPushButton(tr("−"), this);
    auto *plus = new QPushButton(tr("+"), this);
    auto *fit = new QPushButton(tr("Всё поле"), this);
    minus->setObjectName(QStringLiteral("zoomOutButton"));
    plus->setObjectName(QStringLiteral("zoomInButton"));
    fit->setObjectName(QStringLiteral("fitButton"));
    minus->setAccessibleName(tr("Уменьшить масштаб"));
    plus->setAccessibleName(tr("Увеличить масштаб"));
    for (QPushButton *button : {minus, plus, fit}) {
        button->setMinimumSize(44, 44);
        zoomRow->addWidget(button);
    }
    zoomRow->addStretch();
    m_restart = new QPushButton(tr("Повторить"), this);
    m_regenerate = new QPushButton(tr("Разбить заново"), this);
    m_restart->setToolTip(tr("Сохранить форму деталей и заново перемешать их"));
    m_regenerate->setToolTip(tr("Создать новое случайное разбиение той же фотографии"));
    m_regenerate->setProperty("role", "primary");
    m_restart->setObjectName(QStringLiteral("restartButton"));
    m_regenerate->setObjectName(QStringLiteral("regenerateButton"));
    for (QPushButton *button : {m_restart, m_regenerate}) {
        button->setMinimumHeight(44);
        zoomRow->addWidget(button);
    }
    // Одна строка инструментов освобождает высоту для фотографии, особенно в окне 640×480.
    layout->addLayout(zoomRow);
    m_help = new GameHelpDialog(this);
    connect(m_helpButton, &QPushButton::clicked, this, &GamePage::showHelp);
    connect(m_help, &QDialog::finished, this, [this] {
        if (isVisible() && m_helpButton->isEnabled()) {
            // После закрытия отдельного модального окна клавиатура возвращается к игровым действиям.
            window()->activateWindow();
            m_helpButton->setFocus(Qt::OtherFocusReason);
        }
    });
    connect(back, &QPushButton::clicked, this, &GamePage::setupRequested);
    connect(m_restart, &QPushButton::clicked, this, &GamePage::restartRequested);
    connect(m_regenerate, &QPushButton::clicked, this, &GamePage::regenerateRequested);
    connect(minus, &QPushButton::clicked, m_view, &PuzzleView::zoomOut);
    connect(plus, &QPushButton::clicked, m_view, &PuzzleView::zoomIn);
    connect(fit, &QPushButton::clicked, m_view, &PuzzleView::fitAll);
    connect(model, &PuzzleModel::progressChanged, this, [this](int placed, int total) {
        m_summary->setText(tr("Собрано: %1 из %2").arg(placed).arg(total));
        m_progress->setRange(0, total);
        m_progress->setValue(placed);
    });
    setFocusProxy(back);
    setState(AppController::State::Configuring);
}
void GamePage::setState(AppController::State state)
{
    const bool active = state == AppController::State::Playing || state == AppController::State::Completed;
    m_view->setEnabled(active);
    m_restart->setEnabled(active);
    m_regenerate->setEnabled(active);
    m_helpButton->setEnabled(active);
    if (!active && m_help->isVisible())
        m_help->reject();
    m_completion->setVisible(state == AppController::State::Completed);
    if (state == AppController::State::Preparing) {
        m_progress->setRange(0, 0);
        m_summary->setText(tr("Подготовка игрового поля…"));
    }
}

void GamePage::setReducedMotion(bool reduced) { m_help->setReducedMotion(reduced); }

void GamePage::showHelp()
{
    if (!m_helpButton->isEnabled() || !isVisible())
        return;
    // Модальная карточка не должна оставлять полноразмерную деталь в состоянии перетаскивания.
    m_view->cancelPointer();
    if (m_help->isVisible()) {
        m_help->raise();
        m_help->activateWindow();
    } else {
        m_help->open();
    }
}
