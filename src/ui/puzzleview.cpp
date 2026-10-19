#include "puzzleview.h"

#include <QMouseEvent>
#include <QLoggingCategory>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTouchEvent>
#include <QWheelEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <cmath>

Q_LOGGING_CATEGORY(lcInput, "imagepuzzle.input", QtInfoMsg)

PuzzleView::PuzzleView(PuzzleModel *model, QWidget *parent)
    : QGraphicsView(parent), m_model(model), m_scene(new PuzzleScene(model, this))
{
    setObjectName(QStringLiteral("puzzleView"));
    setAccessibleName(tr("Поле сборки и фрагменты"));
    setScene(m_scene);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    viewport()->grabGesture(Qt::PinchGesture);
    setMouseTracking(true);
    connect(model, &PuzzleModel::aboutToReset, this, &PuzzleView::cancelPointer);
    connect(model, &PuzzleModel::puzzleReset, this, &PuzzleView::fitAll);
    connect(m_scene, &PuzzleScene::completionPresentationChanged, this, &PuzzleView::fitAll);
}
PuzzleScene *PuzzleView::puzzleScene() { return m_scene; }
void PuzzleView::fitAll()
{
    if (m_dragged || m_pan || m_blockTouchDrag || m_pinchActive)
        cancelPointer();
    m_fit = true;
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (!m_model->pieces().isEmpty()) {
        const bool completed = m_scene->completionPresented();
        fitInView(completed ? m_model->boardRect().adjusted(-24, -24, 24, 24) : m_scene->sceneRect(), Qt::KeepAspectRatio);
        if (transform().m11() > 6.0)
            scale(6.0 / transform().m11(), 6.0 / transform().m11());
    }
}
void PuzzleView::zoom(qreal factor)
{
    if (m_dragged || m_pan || m_blockTouchDrag || m_pinchActive)
        cancelPointer();
    const qreal current = transform().m11();
    const qreal next = qBound(0.03, current * factor, 6.0);
    m_fit = false;
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scale(next / current, next / current);
}
void PuzzleView::zoomIn() { zoom(1.25); }
void PuzzleView::zoomOut() { zoom(0.8); }

void PuzzleView::beginPointer(QPoint position)
{
    m_last = position;
    PieceItem *piece = qgraphicsitem_cast<PieceItem *>(itemAt(position));
    qCDebug(lcInput) << "Pointer begin, viewport:" << position << "scene:" << mapToScene(position)
                    << "piece:" << (piece ? piece->id() : -1) << "DPR:" << devicePixelRatioF();
    if (piece && !m_model->pieces().at(piece->id()).placed) {
        m_dragged = piece;
        // Поворот уже входит в смещение: увеличение сохраняет тот же пиксель под указателем.
        const QPointF local = piece->mapFromScene(mapToScene(position));
        piece->finishRotation();
        piece->setScale(1.0);
        m_offset = piece->transform().map(local);
        piece->setPos(mapToScene(position) - m_offset);
        piece->setZValue(10);
    } else {
        m_pan = true;
        m_fit = false;
    }
    viewport()->setCursor(Qt::ClosedHandCursor);
}
void PuzzleView::movePointer(QPoint position)
{
    if (m_dragged) {
        m_dragged->setPos(mapToScene(position) - m_offset);
    } else if (m_pan) {
        const QPoint delta = position - m_last;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    }
    m_last = position;
}
void PuzzleView::endPointer(QPoint position, bool touch)
{
    movePointer(position);
    PieceItem *piece = m_dragged;
    // Сигнал может завершить игру и отключить виджет: сначала завершаем временный жест.
    m_dragged = nullptr;
    m_pan = false;
    m_mouse = false;
    m_touchId = -1;
    viewport()->unsetCursor();
    if (piece) {
        const int id = piece->id();
        QPointF dropPosition = piece->pos();
        const QPointF pointer = mapToScene(position);
        const QRectF tray = m_model->trayRect();
        if (tray.contains(pointer)) {
            const QPointF center = piece->boundingRect().center();
            QPointF thumbnailCenter = pointer + (center - m_offset) * m_model->thumbnailScale();
            thumbnailCenter.setX(qBound(tray.left(), thumbnailCenter.x(), tray.right()));
            thumbnailCenter.setY(qBound(tray.top(), thumbnailCenter.y(), tray.bottom()));
            // Передаём эквивалентный центр полноразмерной детали, чтобы уменьшение сохраняло точку захвата.
            dropPosition = thumbnailCenter - center;
        }
        const qreal tolerance = (touch ? 32.0 : 24.0) / transform().m11();
        qCDebug(lcInput) << "Pointer end, piece:" << id << "position:" << piece->pos()
                        << "target:" << m_model->pieces().at(id).source.topLeft() << "tolerance:" << tolerance;
        emit pieceDropped(id, dropPosition, tolerance);
        // Отклонённое отпускание также восстанавливает состояние из модели.
        const PuzzlePiece &data = m_model->pieces().at(id);
        if (!data.placed) {
            piece->setPos(data.position);
            piece->setScale(data.scale);
            piece->setZValue(0);
        }
    }
}
void PuzzleView::cancelPointer()
{
    if (m_dragged) {
        const PuzzlePiece &piece = m_model->pieces().at(m_dragged->id());
        m_dragged->setPos(piece.position);
        m_dragged->setScale(piece.scale);
        m_dragged->setZValue(piece.placed ? -1 : 0);
    }
    m_dragged = nullptr;
    m_touchId = -1;
    m_mouse = false;
    m_pan = false;
    m_rotationId = -1;
    m_rotationRemainder = 0;
    m_blockTouchDrag = false;
    m_pinchActive = false;
    viewport()->unsetCursor();
}

void PuzzleView::beginRotation(QPoint position)
{
    PieceItem *piece = m_dragged ? m_dragged : qgraphicsitem_cast<PieceItem *>(itemAt(position));
    const int id = piece && !m_model->pieces().at(piece->id()).placed ? piece->id() : -1;
    // Второй палец меняет режим ввода. Отпускание пальцев после вращения не должно уронить деталь.
    cancelPointer();
    m_rotationId = id;
    m_blockTouchDrag = true;
}

bool PuzzleView::handleGesture(QGestureEvent *event)
{
    auto *pinch = static_cast<QPinchGesture *>(event->gesture(Qt::PinchGesture));
    if (!pinch)
        return false;
    event->accept(pinch);
    if (event->type() == QEvent::GestureOverride)
        return true;
    if (!isEnabled()) {
        cancelPointer();
        return true;
    }
    if (pinch->state() == Qt::GestureCanceled) {
        cancelPointer();
        m_blockTouchDrag = true;
        return true;
    }
    if (!m_pinchActive) {
        if (!m_blockTouchDrag)
            beginRotation(viewport()->mapFromGlobal(pinch->centerPoint().toPoint()));
        m_pinchActive = true;
    }
    if (m_rotationId >= 0 && pinch->changeFlags().testFlag(QPinchGesture::RotationAngleChanged)) {
        // Нормализация исключает скачок на полный оборот при переходе угла через ±180°.
        const qreal delta = std::remainder(pinch->rotationAngle() - pinch->lastRotationAngle(), 360.0);
        if (qIsFinite(delta)) {
            m_rotationRemainder += delta;
            // Порог 60° даёт запас против дрожания и обратного переключения после шага на 90°.
            const int turns = qAbs(m_rotationRemainder) >= 60
                ? int(std::copysign(std::floor((qAbs(m_rotationRemainder) + 30) / 90), m_rotationRemainder)) : 0;
            if (turns != 0) {
                m_rotationRemainder -= turns * 90;
                qCDebug(lcInput) << "Pinch rotation, piece:" << m_rotationId << "quarter turns:" << turns;
                emit pieceRotationRequested(m_rotationId, turns);
            }
        }
    }
    if (pinch->state() == Qt::GestureFinished) {
        m_rotationId = -1;
        m_rotationRemainder = 0;
        m_pinchActive = false;
    }
    return true;
}

bool PuzzleView::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::Gesture || event->type() == QEvent::GestureOverride)
        return handleGesture(static_cast<QGestureEvent *>(event));
    if (event->type() == QEvent::TouchCancel) {
        cancelPointer();
        event->accept();
        return true;
    }
    if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate
        || event->type() == QEvent::TouchEnd) {
        if (!isEnabled()) {
            event->accept();
            return true;
        }
        auto *touch = static_cast<QTouchEvent *>(event);
        // После системной отмены TouchEnd может не прийти; новая последовательность снимает блокировку.
        if (event->type() == QEvent::TouchBegin && m_blockTouchDrag)
            cancelPointer();
        int activePoints = 0;
        QPointF center;
        for (const auto &point : touch->points()) {
            if (point.state() != QEventPoint::State::Released) {
                ++activePoints;
                center += point.position();
            }
        }
        if (activePoints >= 2 && !m_blockTouchDrag && !m_mouse)
            beginRotation((center / activePoints).toPoint());
        if (m_blockTouchDrag) {
            if (event->type() == QEvent::TouchEnd)
                cancelPointer();
            event->accept();
            return true;
        }
        if (event->type() == QEvent::TouchBegin && m_touchId < 0 && !m_mouse && !touch->points().isEmpty()) {
            m_touchId = touch->points().front().id();
            beginPointer(touch->points().front().position().toPoint());
        }
        for (const QEventPoint &point : touch->points()) {
            if (point.id() != m_touchId)
                continue;
            if (point.state() == QEventPoint::State::Released)
                endPointer(point.position().toPoint(), true);
            else
                movePointer(point.position().toPoint());
            break;
        }
        if (event->type() == QEvent::TouchEnd && m_touchId >= 0)
            cancelPointer();
        // Принятые касания не должны повторно исполняться как синтезированные события мыши.
        event->accept();
        return true;
    }
    return QGraphicsView::viewportEvent(event);
}
bool PuzzleView::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture || event->type() == QEvent::GestureOverride)
        return handleGesture(static_cast<QGestureEvent *>(event));
    if (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Hide
        || (event->type() == QEvent::EnabledChange && !isEnabled()))
        cancelPointer();
    return QGraphicsView::event(event);
}
void PuzzleView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_touchId < 0 && !m_blockTouchDrag) {
        m_mouse = true;
        beginPointer(event->position().toPoint());
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}
void PuzzleView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mouse) {
        movePointer(event->position().toPoint());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}
void PuzzleView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_mouse) {
        endPointer(event->position().toPoint(), false);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
void PuzzleView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        zoom(event->angleDelta().y() > 0 ? 1.25 : 0.8);
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}
void PuzzleView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_touchId < 0 && !m_blockTouchDrag) {
        auto *piece = qgraphicsitem_cast<PieceItem *>(itemAt(event->position().toPoint()));
        const int id = piece ? piece->id() : -1;
        cancelPointer();
        if (id >= 0 && !m_model->pieces().at(id).placed)
            emit pieceRotationRequested(id, 1);
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}
void PuzzleView::resizeEvent(QResizeEvent *event)
{
    if (m_dragged || m_pan || m_blockTouchDrag || m_pinchActive)
        cancelPointer();
    QGraphicsView::resizeEvent(event);
    if (m_fit)
        fitAll();
}
