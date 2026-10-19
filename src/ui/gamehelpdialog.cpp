#include "gamehelpdialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QParallelAnimationGroup>
#include <QPauseAnimation>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSequentialAnimationGroup>
#include <QShowEvent>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcHelp, "imagepuzzle.ui.help")

GameHelpDialog::GameHelpDialog(QWidget *parent)
    : QDialog(parent), m_animation(new QParallelAnimationGroup(this))
{
    setObjectName(QStringLiteral("gameHelpDialog"));
    setWindowTitle(tr("Как играть?"));
    setWindowModality(Qt::WindowModal);
    setMaximumWidth(640);
    m_animation->setObjectName(QStringLiteral("helpRevealAnimation"));
    connect(m_animation, &QParallelAnimationGroup::finished, this, &GameHelpDialog::finishReveal);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("helpCard"));
    outer->addWidget(card);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(14);

    auto *heading = new QWidget(card);
    heading->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto *headingLayout = new QVBoxLayout(heading);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    headingLayout->setSpacing(8);
    auto *badge = new QLabel(tr("МАЛЫЕ КОРЕЛЫ · ПУТЕВОДИТЕЛЬ"), heading);
    badge->setObjectName(QStringLiteral("helpBadge"));
    badge->setWordWrap(true);
    headingLayout->addWidget(badge);
    auto *title = new QLabel(tr("Соберите свой Север"), heading);
    title->setObjectName(QStringLiteral("helpTitle"));
    title->setWordWrap(true);
    QFont titleFont = font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.45);
    titleFont.setBold(true);
    title->setFont(titleFont);
    headingLayout->addWidget(title);
    m_intro = new QLabel(tr("Четыре шага — и фотография снова целая."), heading);
    m_intro->setProperty("role", "muted");
    m_intro->setWordWrap(true);
    headingLayout->addWidget(m_intro);
    layout->addWidget(heading);
    addReveal(heading, 0);

    // На маленьком экране прокручиваются только правила: кнопка закрытия всегда остаётся доступной.
    auto *scroll = new QScrollArea(card);
    scroll->setObjectName(QStringLiteral("helpScrollArea"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumHeight(80);
    connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        // Прокрутка копирует пиксели viewport. Прерываем появление до следующей отрисовки,
        // чтобы открывшиеся полосы не зависели от обрезанного снимка QGraphicsEffect.
        if (m_animation->state() != QAbstractAnimation::Stopped) {
            finishReveal();
            qCDebug(lcHelp) << "Help reveal finished by scrolling";
        }
    });
    auto *steps = new QWidget(scroll);
    steps->setObjectName(QStringLiteral("helpSteps"));
    auto *stepsLayout = new QVBoxLayout(steps);
    stepsLayout->setContentsMargins(0, 0, 6, 0);
    stepsLayout->setSpacing(10);
    const QVector<QPair<QString, QString>> instructions{
        {tr("Переносите"), tr("Берите детали из лотка справа и переносите на поле слева мышью или одним пальцем. При захвате деталь увеличится.")},
        {tr("Поворачивайте"), tr("Двойной щелчок левой кнопкой — поворот на 90° вправо. На сенсорном экране вращайте двумя пальцами вправо или влево.")},
        {tr("Совмещайте"), tr("Поверните деталь правильно и отпустите рядом с её местом. Она закрепится и подсветится; соберите так всю фотографию.")},
        {tr("Рассматривайте"), tr("Кнопки + и − или Ctrl+колесо меняют масштаб. Пустое место можно перетаскивать, а «Всё поле» вернёт общий вид.")}
    };
    for (int i = 0; i < instructions.size(); ++i) {
        auto *step = new QFrame(steps);
        step->setObjectName(QStringLiteral("helpStep%1").arg(i + 1));
        step->setProperty("helpStep", true);
        auto *row = new QHBoxLayout(step);
        row->setContentsMargins(12, 12, 12, 12);
        row->setSpacing(12);
        auto *number = new QLabel(QStringLiteral("0%1").arg(i + 1), step);
        number->setProperty("helpNumber", true);
        number->setAlignment(Qt::AlignCenter);
        number->setMinimumSize(38, 38);
        number->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        row->addWidget(number, 0, Qt::AlignTop);
        auto *text = new QVBoxLayout;
        text->setSpacing(4);
        auto *name = new QLabel(instructions.at(i).first, step);
        name->setWordWrap(true);
        QFont bold = font();
        bold.setBold(true);
        name->setFont(bold);
        auto *description = new QLabel(instructions.at(i).second, step);
        description->setWordWrap(true);
        text->addWidget(name);
        text->addWidget(description);
        row->addLayout(text, 1);
        stepsLayout->addWidget(step);
        addReveal(step, 45 * (i + 1));
    }
    stepsLayout->addStretch();
    scroll->setWidget(steps);
    layout->addWidget(scroll, 1);
    m_footer = new QLabel(tr("Памятка всегда рядом — кнопка «Как играть?» над полем."), card);
    m_footer->setWordWrap(true);
    m_footer->setProperty("role", "muted");
    layout->addWidget(m_footer);
    auto *buttons = new QDialogButtonBox(card);
    auto *understood = buttons->addButton(tr("Понятно"), QDialogButtonBox::AcceptRole);
    understood->setObjectName(QStringLiteral("helpUnderstoodButton"));
    understood->setProperty("role", "primary");
    understood->setMinimumHeight(44);
    understood->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
    setStyleSheet(QStringLiteral(R"(
        QDialog#gameHelpDialog { background: #EAE5DA; }
        QFrame#helpCard { background: #F7F4EE; border: 1px solid #D1D3C8; border-radius: 20px; }
        QLabel#helpBadge { color: #27624C; background: #E2EDDF; border-radius: 8px; padding: 7px 10px; }
        QLabel#helpTitle { color: #24332D; }
        QScrollArea#helpScrollArea, QWidget#helpSteps { background: transparent; border: 0; }
        QFrame[helpStep="true"] { background: #FFFFFF; border: 1px solid #DEDCD1; border-radius: 14px; }
        QLabel[helpNumber="true"] { color: #FFFFFF; background: #27624C; border-radius: 10px; padding: 4px; }
        QScrollBar:vertical { background: #EAE5DA; width: 8px; border-radius: 4px; }
        QScrollBar::handle:vertical { background: #A8B3A6; min-height: 24px; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )"));
    connect(this, &QDialog::finished, this, [](int result) {
        qCInfo(lcHelp) << "Help dismissed, result:" << result;
    });
}

void GameHelpDialog::addReveal(QWidget *widget, int delay)
{
    auto *effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    effect->setEnabled(false);
    m_effects.append(effect);
    auto *sequence = new QSequentialAnimationGroup(m_animation);
    sequence->addAnimation(new QPauseAnimation(delay, sequence));
    auto *fade = new QPropertyAnimation(effect, "opacity", sequence);
    fade->setDuration(220);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    sequence->addAnimation(fade);
    m_animation->addAnimation(sequence);
}

void GameHelpDialog::finishReveal()
{
    m_animation->stop();
    // Даже opacity=1 оставляет эффект в цепочке отрисовки QWidget. После появления
    // отключаем его целиком: QScrollArea сможет заново рисовать текст без промежуточных снимков.
    for (auto *effect : m_effects) {
        effect->setOpacity(1);
        effect->setEnabled(false);
    }
}

void GameHelpDialog::setReducedMotion(bool reduced)
{
    m_reducedMotion = reduced;
    if (reduced)
        finishReveal();
}

void GameHelpDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    const QSize available = screen()->availableGeometry().size() - QSize(24, 48);
    m_intro->setVisible(available.height() >= 480);
    m_footer->setVisible(available.height() >= 480);
    resize(qMin(600, available.width()), qMin(620, available.height()));
    findChild<QScrollArea *>(QStringLiteral("helpScrollArea"))->verticalScrollBar()->setValue(0);
    findChild<QPushButton *>(QStringLiteral("helpUnderstoodButton"))->setFocus(Qt::OtherFocusReason);
    if (m_reducedMotion)
        finishReveal();
    else {
        for (auto *effect : m_effects) {
            effect->setEnabled(true);
            effect->setOpacity(0);
        }
        m_animation->start();
    }
    qCInfo(lcHelp) << "Help shown, reduced motion:" << m_reducedMotion;
}

void GameHelpDialog::hideEvent(QHideEvent *event)
{
    finishReveal();
    QDialog::hideEvent(event);
}

void GameHelpDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    // Убираем только второстепенные подписи: сами правила и основное действие остаются доступны.
    if (m_intro) m_intro->setVisible(event->size().height() >= 480);
    if (m_footer) m_footer->setVisible(event->size().height() >= 480);
}
