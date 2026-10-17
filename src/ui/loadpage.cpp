#include "loadpage.h"
#include "imagepreview.h"
#include "core/puzzlegenerator.h"
#include "io/sampleimages.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QImageReader>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpinBox>
#include <QVBoxLayout>

LoadPage::LoadPage(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("loadPage"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(24);
    m_preview = new ImagePreview(this);
    m_preview->setObjectName(QStringLiteral("imagePreview"));
    layout->addWidget(m_preview, 5);
    auto *panel = new QWidget(this);
    panel->setMinimumWidth(290);
    auto *controls = new QVBoxLayout(panel);
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(12);
    controls->addStretch();
    auto *eyebrow = new QLabel(tr("ТВОЯ МАЛЕНЬКАЯ ЭКСПЕДИЦИЯ"), panel);
    eyebrow->setProperty("role", "eyebrow");
    QFont small = font(); small.setPointSizeF(font().pointSizeF() / 1.2);
    eyebrow->setFont(small);
    controls->addWidget(eyebrow);
    m_title = new QLabel(tr("Собери\nсвой Север"), panel);
    m_title->setObjectName(QStringLiteral("loadTitle"));
    controls->addWidget(m_title);
    m_intro = new QLabel(tr("Выбери фотографию и собери её по частям. Мышью или пальцем — в своём темпе."), panel);
    m_intro->setWordWrap(true);
    m_intro->setProperty("role", "muted");
    controls->addWidget(m_intro);
    auto *choices = new QHBoxLayout;
    m_samples = new QComboBox(panel);
    m_samples->setObjectName(QStringLiteral("sampleImagesCombo"));
    m_samples->setAccessibleName(tr("Фотографии музея"));
    m_samples->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_samples->setMinimumContentsLength(8);
    m_samples->addItem(tr("Выбери фотографию"), QString());
    for (const QString &path : SampleImages::filePaths()) {
        QString name = QFileInfo(path).fileName();
        if (name == QStringLiteral("kunitsina-estate.jpg")) name = tr("Усадьба М. Т. Куницыной");
        if (name == QStringLiteral("korely-summer.jpg")) name = tr("Малые Корелы · лето");
        if (name == QStringLiteral("korely-winter.jpg")) name = tr("Малые Корелы · зима");
        m_samples->addItem(name, path);
    }
    m_samples->setMinimumHeight(48);
    m_useSample = new QPushButton(tr("Выбрать"), panel);
    m_useSample->setObjectName(QStringLiteral("sampleLoadButton"));
    m_useSample->setMinimumHeight(48);
    choices->addWidget(m_samples, 1);
    choices->addWidget(m_useSample);
    controls->addLayout(choices);
    m_choose = new QPushButton(tr("Своя фотография…"), panel);
    m_choose->setObjectName(QStringLiteral("chooseImageButton"));
    m_choose->setMinimumHeight(48);
    controls->addWidget(m_choose);
    m_presets = new QWidget(panel);
    auto *presetLayout = new QHBoxLayout(m_presets);
    presetLayout->setContentsMargins(0, 0, 0, 0);
    presetLayout->setSpacing(8);
    auto *group = new QButtonGroup(this);
    for (int count : {4, 8, 12, 16, 20}) {
        auto *button = new QPushButton(QString::number(count), m_presets);
        button->setObjectName(QStringLiteral("countPreset%1").arg(count));
        button->setAccessibleName(tr("%1 фрагментов").arg(count));
        button->setCheckable(true);
        button->setMinimumSize(44, 48);
        group->addButton(button, count);
        presetLayout->addWidget(button);
    }
    controls->addWidget(m_presets);
    auto *countRow = new QHBoxLayout;
    auto *countLabel = new QLabel(tr("Фрагментов:"), panel);
    m_count = new QSpinBox(panel);
    m_count->setObjectName(QStringLiteral("pieceCountSpinBox"));
    m_count->setAccessibleName(tr("Количество фрагментов"));
    m_count->setRange(PuzzleGenerator::MinimumCount, PuzzleGenerator::MaximumCount);
    m_count->setValue(12);
    m_count->setMinimumHeight(48);
    m_count->setMinimumWidth(100);
    countLabel->setBuddy(m_count);
    countRow->addWidget(countLabel);
    countRow->addStretch();
    countRow->addWidget(m_count);
    controls->addLayout(countRow);
    connect(group, &QButtonGroup::idClicked, m_count, &QSpinBox::setValue);
    connect(m_count, &QSpinBox::valueChanged, this, [group](int count) {
        // Произвольное целое n не должно оставлять выбранной другую готовую сложность.
        group->setExclusive(false);
        for (QAbstractButton *button : group->buttons()) button->setChecked(group->id(button) == count);
        group->setExclusive(true);
    });
    group->button(12)->setChecked(true);
    m_start = new QPushButton(tr("Начать собирать →"), panel);
    m_start->setObjectName(QStringLiteral("startButton"));
    m_start->setProperty("role", "primary");
    m_start->setMinimumHeight(56);
    controls->addWidget(m_start);
    m_status = new QLabel(tr("Выбери музейный снимок или открой свой файл."), panel);
    m_status->setObjectName(QStringLiteral("loadStatusLabel"));
    m_status->setProperty("role", "muted");
    m_status->setFont(small);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    controls->addWidget(m_status);
    controls->addStretch();
    layout->addWidget(panel, 4);
    setTabOrder(m_choose, m_samples);
    setTabOrder(m_samples, m_useSample);
    setTabOrder(m_useSample, m_count);
    setTabOrder(m_count, m_start);
    setFocusProxy(m_choose);
    connect(m_choose, &QPushButton::clicked, this, &LoadPage::chooseImage);
    connect(m_samples, &QComboBox::currentIndexChanged, this, &LoadPage::updateButtons);
    connect(m_useSample, &QPushButton::clicked, this, [this] { emit imageRequested(m_samples->currentData().toString()); });
    connect(m_start, &QPushButton::clicked, this, [this] { emit startRequested(m_count->value()); });
    updateButtons();
}
void LoadPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const bool compact = width() < 800 || height() < 540 || font().pointSizeF() > 14;
    QFont title = font(); title.setPointSizeF(font().pointSizeF() * (compact ? 1.44 : 2.488)); title.setBold(true);
    m_title->setFont(title);
    m_title->setText(compact ? tr("Собери свой Север") : tr("Собери\nсвой Север"));
    m_title->setWordWrap(true);
    m_intro->setVisible(!compact);
    m_presets->setVisible(!compact);
}
void LoadPage::chooseImage()
{
    QStringList patterns;
    for (const QByteArray &format : QImageReader::supportedImageFormats())
        patterns.append(QStringLiteral("*.") + QString::fromLatin1(format));
    QFileDialog dialog(this, tr("Открыть изображение"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters({tr("Изображения (%1)").arg(patterns.join(QLatin1Char(' '))), tr("Все файлы (*)")});
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty())
        emit imageRequested(dialog.selectedFiles().front());
}
void LoadPage::setImage(const QImage &image, const QString &name)
{
    m_preview->setImage(image);
    m_hasImage = !image.isNull();
    m_capacity = PuzzleGenerator::maximumPieces(image.size());
    // Даже для слишком маленькой картинки поле не должно разрешать значения ниже минимума игры.
    m_count->setMaximum(qMax(PuzzleGenerator::MinimumCount, m_capacity));
    m_status->setText(m_capacity < PuzzleGenerator::MinimumCount
                                  ? tr("Изображение слишком мало для %1 фрагментов со стороной от %2 пикселей.")
                                        .arg(PuzzleGenerator::MinimumCount).arg(PuzzleGenerator::MinimumSide)
                                  : tr("%1 · %2 × %3 · максимум %4 фрагментов")
                                        .arg(name).arg(image.width()).arg(image.height()).arg(m_capacity));
    updateButtons();
}
void LoadPage::setBusy(bool busy)
{
    m_busy = busy;
    if (busy)
        m_status->setText(tr("Загрузка изображения…"));
    updateButtons();
}
void LoadPage::showError(const QString &message) { m_status->setText(message); }
bool LoadPage::hasImage() const { return m_hasImage; }
void LoadPage::updateButtons()
{
    m_choose->setEnabled(!m_busy);
    m_samples->setEnabled(!m_busy);
    m_count->setEnabled(!m_busy);
    for (auto *button : m_presets->findChildren<QPushButton *>())
        button->setEnabled(!m_busy && button->text().toInt() <= m_count->maximum());
    m_useSample->setEnabled(!m_busy && !m_samples->currentData().toString().isEmpty());
    m_start->setEnabled(!m_busy && m_hasImage && m_capacity >= PuzzleGenerator::MinimumCount);
}
