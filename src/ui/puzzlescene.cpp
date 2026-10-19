#include "puzzlescene.h"
#include "expeditiontheme.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QGraphicsPixmapItem>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QTimer>
#include <QSet>
#include <QLoggingCategory>
#include <QLinearGradient>
#include <cmath>

Q_LOGGING_CATEGORY(lcEffects, "imagepuzzle.ui.effects")

namespace {
/** @brief Рисует три светящиеся спиральные дуги вокруг детали без участия в выборе указателем. */
class RotationVortexItem : public QGraphicsItem
{
public:
    /** @brief Создаёт ореол в локальных координатах владельца. @param piece Деталь, владеющая ореолом. */
    explicit RotationVortexItem(PieceItem *piece) : QGraphicsItem(piece), m_piece(piece)
    {
        const QRectF pieceRect = piece->boundingRect();
        m_margin = qBound(14.0, qMin(pieceRect.width(), pieceRect.height()) * 0.16, 36.0);
        m_bounds = pieceRect.adjusted(-m_margin, -m_margin, m_margin, m_margin);
        setAcceptedMouseButtons(Qt::NoButton);
        setData(0, QStringLiteral("rotationVortex"));
        setZValue(1);
        hide();
    }
    /** @brief Ограничивает все слои свечения для корректного обновления сцены. @return Локальные границы ореола. */
    QRectF boundingRect() const override { return m_bounds; }
    /** @brief Исключает декоративные дуги из hit testing. @return Пустой контур. */
    QPainterPath shape() const override { return {}; }
    /** @brief Рисует мягкие хвосты и яркие концы дуг. @param painter Рисователь.
     * @param option Параметры Qt. @param widget Виджет отображения. */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        constexpr qreal pi = 3.141592653589793;
        const qreal progress = m_piece->rotationProgress();
        const qreal intensity = std::sin(pi * progress);
        if (intensity <= 0) return;
        const QRectF pieceRect = m_piece->boundingRect();
        const QPointF center = pieceRect.center();
        const qreal rx = pieceRect.width() / 2 + m_margin * 0.35;
        const qreal ry = pieceRect.height() / 2 + m_margin * 0.35;
        const qreal width = qBound(5.0, qMin(pieceRect.width(), pieceRect.height()) * 0.075, 18.0);
        const int direction = data(1).toInt();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setBrush(Qt::NoBrush);
        // Многослойные векторные штрихи дают мягкое свечение без размытия всей текстуры детали.
        for (int arm = 0; arm < 3; ++arm) {
            QPainterPath path;
            QPointF first;
            QPointF tip;
            for (int step = 0; step <= 24; ++step) {
                const qreal t = step / 24.0;
                const qreal angle = arm * 2 * pi / 3 + direction * (progress * 3.8 + t * 1.7);
                const qreal radius = 1 - 0.12 * t;
                const QPointF point = center + QPointF(rx * radius * std::cos(angle), ry * radius * std::sin(angle));
                if (step == 0) { first = point; path.moveTo(point); }
                else path.lineTo(point);
                tip = point;
            }
            for (const auto &layer : {QPair<qreal, int>{1.8, 38}, {1.0, 110}, {0.26, 235}}) {
                QLinearGradient glow(first, tip);
                const int alpha = qRound(layer.second * intensity);
                glow.setColorAt(0, QColor(37, 170, 106, 0));
                glow.setColorAt(0.5, QColor(42, 202, 120, alpha / 2));
                glow.setColorAt(1, QColor(136, 255, 179, alpha));
                painter->setPen(QPen(QBrush(glow), width * layer.first, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter->drawPath(path);
            }
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(160, 255, 205, qRound(220 * intensity)));
            painter->drawEllipse(tip, width * 0.18, width * 0.18);
            painter->setBrush(Qt::NoBrush);
        }
    }
private:
    PieceItem *m_piece; ///< Владелец живёт дольше дочернего элемента.
    QRectF m_bounds; ///< Постоянные границы учитывают ширину внешнего слоя свечения.
    qreal m_margin = 0; ///< Запас в логических координатах вокруг детали.
};

/** @brief Рисует мягкое многослойное свечение только вдоль внутренних швов. */
class SeamGlowItem : public QGraphicsItem
{
public:
    /** @brief Сохраняет геометрию эффекта. @param seams Внутренние швы. @param board Граница фотографии. */
    SeamGlowItem(QPainterPath seams, QRectF board) : m_seams(std::move(seams)), m_board(board)
    {
        setAcceptedMouseButtons(Qt::NoButton);
        setData(0, QStringLiteral("completionSeamGlow"));
        setZValue(4);
    }
    /** @brief Ограничивает эффект фотографией. @return Граница изображения. */
    QRectF boundingRect() const override { return m_board; }
    /** @brief Исключает декоративный слой из выбора указателем. @return Пустой контур. */
    QPainterPath shape() const override { return {}; }
    /** @brief Рисует ореол и тонкую светлую сердцевину. @param painter Рисователь.
     * @param option Параметры Qt. @param widget Виджет отображения. */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setClipRect(m_board);
        painter->setBrush(Qt::NoBrush);
        for (const auto &layer : {QPair<qreal, int>{14, 18}, {8, 40}, {3, 120}, {1, 205}}) {
            QPen pen(QColor(255, 218, 153, layer.second), layer.first, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawPath(m_seams);
        }
    }
private:
    QPainterPath m_seams; ///< Общие границы рисуются один раз.
    QRectF m_board; ///< Ореол не выходит за края снимка.
};
}

PieceItem::PieceItem(const QImage &image, const PuzzlePiece &piece)
    : m_image(image), m_source(piece.source), m_shape(piece.shape), m_id(piece.id), m_locked(piece.placed),
      m_target(piece.position), m_turnAnimation(new QVariantAnimation(this))
{
    m_vortex = new RotationVortexItem(this);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setPos(piece.position);
    setScale(piece.scale);
    setOrientation(piece.rotation);
    setAcceptedMouseButtons(Qt::NoButton);
    m_turnAnimation->setObjectName(QStringLiteral("rotationAnimation"));
    m_turnAnimation->setStartValue(0.0);
    m_turnAnimation->setEndValue(1.0);
    m_turnAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_turnAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_turnProgress = value.toReal();
        applyOrientation(m_turnFrom + (m_turnTarget - m_turnFrom) * m_turnProgress);
        m_vortex->update();
    });
    connect(m_turnAnimation, &QVariantAnimation::finished, this, &PieceItem::finishRotation);
}
int PieceItem::type() const { return Type; }
int PieceItem::id() const { return m_id; }
void PieceItem::setOrientation(int degrees)
{
    m_rotation = degrees;
    finishRotation();
}
int PieceItem::orientation() const { return m_rotation; }
qreal PieceItem::displayedAngle() const { return m_displayAngle; }
qreal PieceItem::rotationProgress() const { return m_turnProgress; }
void PieceItem::applyOrientation(qreal degrees)
{
    m_displayAngle = degrees;
    // Qt применяет scale() к локальным точкам до базового transform(): его перенос
    // должен быть рассчитан вокруг уже масштабированного центра, иначе миниатюра смещается.
    const QPointF center = boundingRect().center() * scale();
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(degrees);
    transform.translate(-center.x(), -center.y());
    setTransform(transform);
}
void PieceItem::rotateTo(int degrees, int quarterTurns, bool animate)
{
    // Цель накапливается, пока идёт анимация: 270° → 0° сохраняет направление,
    // а новый щелчок продолжает движение из текущего кадра вместо скачка к прошлой цели.
    m_turnAnimation->stop();
    m_turnFrom = m_displayAngle;
    m_turnTarget += quarterTurns * 90.0;
    m_rotation = degrees;
    if (!animate) {
        finishRotation();
        return;
    }
    m_turnProgress = 0;
    m_vortex->setData(1, quarterTurns < 0 ? -1 : 1);
    m_vortex->show();
    setZValue(2);
    m_turnAnimation->setDuration(qBound(200, qRound(qAbs(m_turnTarget - m_turnFrom) * 2.7), 420));
    m_turnAnimation->start();
    qCDebug(lcEffects) << "Rotation animation started, piece:" << m_id
                      << "from:" << m_turnFrom << "to:" << m_turnTarget;
}
void PieceItem::finishRotation()
{
    const bool active = m_vortex && m_vortex->isVisible();
    m_turnAnimation->stop();
    m_turnTarget = m_rotation;
    m_turnProgress = 0;
    applyOrientation(m_rotation);
    if (m_vortex) m_vortex->hide();
    // Закрепление управляет собственным слоем; окончание поворота не должно гасить его свечение.
    if (active) {
        setZValue(m_locked ? -1 : 0);
        qCDebug(lcEffects) << "Rotation animation finished, piece:" << m_id << "angle:" << m_rotation;
    }
}
QVariant PieceItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemScaleHasChanged)
        applyOrientation(m_displayAngle);
    return QGraphicsObject::itemChange(change, value);
}
QRectF PieceItem::boundingRect() const { return QRectF(QPointF(0, 0), m_source.size()); }
QPainterPath PieceItem::shape() const { return m_shape; }
void PieceItem::setLocked(bool locked) { m_locked = locked; update(); }
bool PieceItem::locked() const { return m_locked; }
qreal PieceItem::placementGlow() const { return m_glow; }
void PieceItem::setPlacementGlow(qreal value) { m_glow = qBound(0.0, value, 1.0); update(); }
void PieceItem::settleAt(QPointF target, bool animate)
{
    m_target = target;
    if (!animate) {
        if (m_placement) m_placement->stop();
        setPos(target);
        setPlacementGlow(0);
        setZValue(m_locked ? -1 : 0);
        return;
    }
    m_placement = new QParallelAnimationGroup(this);
    m_placement->setObjectName(QStringLiteral("placementAnimation"));
    auto *position = new QPropertyAnimation(this, "pos", m_placement);
    position->setDuration(160);
    position->setStartValue(pos());
    position->setEndValue(target);
    position->setEasingCurve(QEasingCurve::OutCubic);
    m_placement->addAnimation(position);
    auto *glow = new QPropertyAnimation(this, "placementGlow", m_placement);
    glow->setDuration(220);
    glow->setStartValue(0.0);
    glow->setKeyValueAt(0.3, 1.0);
    glow->setEndValue(0.0);
    m_placement->addAnimation(glow);
    setZValue(2);
    connect(m_placement, &QParallelAnimationGroup::finished, this, &PieceItem::finishPlacement);
    m_placement->start();
}
void PieceItem::finishPlacement()
{
    if (!m_locked) return;
    if (m_placement) m_placement->stop();
    setPos(m_target);
    setPlacementGlow(0);
    setZValue(-1);
}
void PieceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    // Текстурная кисть рисует сразу по контуру со сглаживанием: прямоугольная
    // обрезка не оставляет жёстких ступенек, пиксели изображения не копируются на каждую деталь.
    QBrush texture(m_image);
    texture.setTransform(QTransform::fromTranslate(-m_source.x(), -m_source.y()));
    painter->setBrush(texture);
    painter->setPen(Qt::NoPen);
    painter->drawPath(m_shape);
    if (!m_locked) {
        // Косметический контур остаётся тонким при любом масштабе изображения.
        QPen pen(QApplication::palette().color(QPalette::Text));
        pen.setCosmetic(true);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(m_shape);
    }
    if (m_glow > 0) {
        // Внутреннее свечение не расширяет boundingRect и не перекрывает соседнюю деталь.
        painter->setClipPath(m_shape);
        painter->setBrush(Qt::NoBrush);
        for (const auto &layer : {QPair<qreal, int>{10, 35}, {5, 90}, {2, 190}}) {
            QPen pen(QColor(255, 218, 153, qRound(layer.second * m_glow)), layer.first);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawPath(m_shape);
        }
    }
}

PuzzleScene::PuzzleScene(PuzzleModel *model, QObject *parent)
    : QGraphicsScene(parent), m_model(model), m_completionDelay(new QTimer(this)),
      m_completionAnimation(new QVariantAnimation(this))
{
    m_completionDelay->setSingleShot(true);
    connect(m_completionDelay, &QTimer::timeout, this, &PuzzleScene::presentCompleted);
    m_completionAnimation->setObjectName(QStringLiteral("completionAnimation"));
    m_completionAnimation->setDuration(480);
    m_completionAnimation->setStartValue(0.0);
    m_completionAnimation->setEndValue(1.0);
    connect(m_completionAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        const qreal t = value.toReal();
        if (m_photo) m_photo->setOpacity(qMin(1.0, t / 0.45));
        if (m_seamGlow) m_seamGlow->setOpacity(std::sin(t * 3.141592653589793) * 0.8);
    });
    connect(m_completionAnimation, &QVariantAnimation::finished, this, [this] {
        if (m_photo) m_photo->setOpacity(1);
        if (m_seamGlow) { m_seamGlow->setOpacity(0); m_seamGlow->hide(); }
        qCDebug(lcEffects) << "Completion glow finished";
        emit completionEffectsFinished();
    });
    connect(model, &PuzzleModel::puzzleReset, this, &PuzzleScene::rebuild);
    connect(model, &PuzzleModel::pieceChanged, this, &PuzzleScene::updatePiece);
    connect(model, &PuzzleModel::pieceRotated, this, [this](int id, int turns) {
        if (auto *item = pieceItem(id))
            item->rotateTo(m_model->pieces().at(id).rotation, turns, !m_reducedMotion);
    });
    connect(model, &PuzzleModel::completed, this, &PuzzleScene::showCompleted);
}
PuzzleScene::~PuzzleScene()
{
    m_completionDelay->stop();
    m_completionAnimation->stop();
}
bool PuzzleScene::completionPresented() const { return m_photo != nullptr; }
void PuzzleScene::setReducedMotion(bool reduced)
{
    m_reducedMotion = reduced;
    if (!reduced) return;
    for (auto *item : m_items) {
        item->finishRotation();
        item->finishPlacement();
    }
    m_completionDelay->stop();
    if (m_pendingCompletion) presentCompleted();
    const bool running = m_completionAnimation->state() == QAbstractAnimation::Running;
    m_completionAnimation->stop();
    if (m_photo) m_photo->setOpacity(1);
    if (m_seamGlow) { m_seamGlow->setOpacity(0); m_seamGlow->hide(); }
    if (running) emit completionEffectsFinished();
}
PieceItem *PuzzleScene::pieceItem(int id) const
{
    return id >= 0 && id < m_items.size() ? m_items.at(id) : nullptr;
}
void PuzzleScene::rebuild()
{
    // Сначала останавливаем callbacks, затем удаляем их графические цели.
    m_completionDelay->stop();
    m_completionAnimation->stop();
    m_pendingCompletion = false;
    m_photo = nullptr;
    m_seamGlow = nullptr;
    clear();
    m_items.clear();
    setBackgroundBrush(ExpeditionTheme::ground());
    QPen outline(ExpeditionTheme::outline());
    outline.setCosmetic(true);
    auto *board = addRect(m_model->boardRect(), outline, QBrush(Qt::white));
    board->setZValue(-2);
    m_tray = addRect(m_model->trayRect(), outline, QBrush(QColor("#F7F4EE")));
    m_tray->setZValue(-2);
    for (const PuzzlePiece &piece : m_model->pieces()) {
        auto *item = new PieceItem(m_model->image(), piece);
        addItem(item);
        m_items.append(item);
    }
    setSceneRect(m_model->workspaceRect().adjusted(-24, -24, 24, 24));
}
void PuzzleScene::updatePiece(int id)
{
    PieceItem *item = pieceItem(id);
    if (!item)
        return;
    const PuzzlePiece &piece = m_model->pieces().at(id);
    const bool newlyPlaced = piece.placed && !item->locked();
    // Новый угол придёт следом через pieceRotated с направлением поворота. При обычном
    // перемещении/закреплении доводим прошлую анимацию до дискретной ориентации модели.
    if (item->orientation() == piece.rotation)
        item->finishRotation();
    item->setScale(piece.scale);
    item->setLocked(piece.placed);
    item->settleAt(piece.position, newlyPlaced && !m_reducedMotion);
}
void PuzzleScene::showCompleted()
{
    m_pendingCompletion = true;
    if (m_reducedMotion) presentCompleted();
    else m_completionDelay->start(220);
}
void PuzzleScene::presentCompleted()
{
    if (!m_pendingCompletion || m_photo || m_model->pieces().isEmpty()
        || m_model->placedCount() != m_model->pieces().size()) return;
    m_pendingCompletion = false;
    for (auto *item : m_items) item->finishPlacement();
    if (m_tray)
        m_tray->hide();
    // fitInView центрирует фотографию относительно границ сцены, из которых нужно исключить пустой лоток.
    setSceneRect(m_model->boardRect().adjusted(-24, -24, 24, 24));
    // После победы целый снимок убирает даже субпиксельные швы сглаживания между отдельными масками.
    m_photo = addPixmap(QPixmap::fromImage(m_model->image()));
    m_photo->setData(0, QStringLiteral("completedPhoto"));
    m_photo->setTransformationMode(Qt::SmoothTransformation);
    m_photo->setZValue(3);
    emit completionPresentationChanged();
    if (m_reducedMotion) {
        emit completionEffectsFinished();
        return;
    }
    m_photo->setOpacity(0);
    m_seamGlow = new SeamGlowItem(internalSeams(), m_model->boardRect());
    addItem(m_seamGlow);
    m_seamGlow->setOpacity(0);
    m_completionAnimation->start();
    qCDebug(lcEffects) << "Completion glow started, seams for pieces:" << m_items.size();
}
QPainterPath PuzzleScene::internalSeams() const
{
    QPainterPath seams;
    QSet<QString> seen;
    const QRectF board = m_model->boardRect();
    const auto append = [&](const QVector<QPointF> &points) {
        if (points.size() == 2) {
            if (QLineF(points[0], points[1]).length() < 0.00001) return;
            const auto on = [&](bool x, qreal edge) {
                return qAbs((x ? points[0].x() : points[0].y()) - edge) < 0.00001
                    && qAbs((x ? points[1].x() : points[1].y()) - edge) < 0.00001;
            };
            if (on(true, board.left()) || on(true, board.right())
                || on(false, board.top()) || on(false, board.bottom())) return;
        }
        const auto key = [&](bool reverse) {
            QString value;
            for (int step = 0; step < points.size(); ++step) {
                const auto &point = points.at(reverse ? points.size() - 1 - step : step);
                value += QString::number(qRound64(point.x() * 1000000)) + QLatin1Char(',')
                    + QString::number(qRound64(point.y() * 1000000)) + QLatin1Char(';');
            }
            return value;
        };
        // Контуры соседей обходятся в обратных направлениях; один и тот же шов не удваивает яркость.
        const QString canonical = qMin(key(false), key(true));
        if (seen.contains(canonical)) return;
        seen.insert(canonical);
        seams.moveTo(points.front());
        if (points.size() == 2) seams.lineTo(points.back());
        else seams.cubicTo(points[1], points[2], points[3]);
    };
    for (const auto &piece : m_model->pieces()) {
        const auto path = QTransform::fromTranslate(piece.source.x(), piece.source.y()).map(piece.shape);
        QPointF previous;
        for (int i = 0; i < path.elementCount(); ++i) {
            const auto element = path.elementAt(i);
            if (element.isMoveTo()) previous = QPointF(element);
            else if (element.isLineTo()) {
                append({previous, QPointF(element)});
                previous = QPointF(element);
            } else if (element.isCurveTo()) {
                append({previous, QPointF(element), QPointF(path.elementAt(i + 1)), QPointF(path.elementAt(i + 2))});
                previous = QPointF(path.elementAt(i + 2));
                i += 2;
            }
        }
    }
    return seams;
}
