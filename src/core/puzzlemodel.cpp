#include "puzzlemodel.h"
#include "puzzlegenerator.h"

#include <QLineF>
#include <QLoggingCategory>
#include <QTransform>
#include <algorithm>
#include <numeric>
#include <limits>
#include <random>
#include <utility>

Q_LOGGING_CATEGORY(lcPuzzle, "imagepuzzle.puzzle")

QTransform PuzzlePiece::rotationTransform() const
{
    const QPointF center = QRectF(QPointF(), source.size()).center();
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(rotation);
    transform.translate(-center.x(), -center.y());
    return transform;
}

QRectF PuzzlePiece::boundsAtScale(qreal atScale) const
{
    QSizeF size(source.size());
    if (rotation % 180 != 0)
        size.transpose();
    // Центр исходного прямоугольника неизменен при повороте; начало контура может стать отрицательным.
    const QPointF center = QRectF(QPointF(), source.size()).center() * atScale;
    size *= atScale;
    return QRectF(center - QPointF(size.width() / 2, size.height() / 2), size);
}

PuzzleModel::PuzzleModel(QObject *parent) : QObject(parent) {}
const QImage &PuzzleModel::image() const { return m_image; }
const QVector<PuzzlePiece> &PuzzleModel::pieces() const { return m_pieces; }
QRectF PuzzleModel::boardRect() const { return QRectF(QPointF(0, 0), m_image.size()); }
QRectF PuzzleModel::trayRect() const { return m_tray; }
qreal PuzzleModel::thumbnailScale() const { return m_thumbnailScale; }
QRectF PuzzleModel::workspaceRect() const { return boardRect().united(m_tray); }
int PuzzleModel::placedCount() const { return m_placed; }
quint32 PuzzleModel::seed() const { return m_seed; }

bool PuzzleModel::start(const QImage &image, int count, quint32 seed, QString *error)
{
    const PuzzleLayout layout = PuzzleGenerator::generate(image.size(), count, seed);
    if (image.isNull() || !layout.error.isEmpty()) {
        const QString message = image.isNull() ? tr("Сначала выберите изображение.") : layout.error;
        if (error)
            *error = message;
        qCWarning(lcPuzzle) << "Puzzle generation failed:" << message << "count:" << count;
        return false;
    }
    emit aboutToReset();
    m_image = image;
    m_seed = seed;
    m_pieces.clear();
    m_order.clear();
    for (const QPainterPath &shape : layout.shapes) {
        const QRect bounds = shape.boundingRect().toAlignedRect().intersected(m_image.rect());
        const QPainterPath local = QTransform::fromTranslate(-bounds.x(), -bounds.y()).map(shape);
        m_pieces.append({int(m_pieces.size()), bounds, {}, false, 1.0, local});
    }
    shuffle(seed ^ 0x9e3779b9u);
    qCInfo(lcPuzzle) << "Curved puzzle created, seed:" << seed << "count:" << count << "size:" << image.size();
    emit puzzleReset();
    emit progressChanged(0, int(m_pieces.size()));
    return true;
}

void PuzzleModel::shuffle(quint32 seed)
{
    QVector<int> order(m_pieces.size());
    std::iota(order.begin(), order.end(), 0);
    const QVector<int> previous = m_order.isEmpty() ? order : m_order;
    std::mt19937 random(seed);
    std::shuffle(order.begin(), order.end(), random);
    if (order == previous && order.size() > 1)
        std::rotate(order.begin(), order.begin() + 1, order.end());
    m_order = order;
    std::uniform_int_distribution<int> angle(0, 3);
    for (PuzzlePiece &piece : m_pieces)
        piece.rotation = angle(random) * 90;
    const qreal spacing = qMax(4.0, qMin(m_image.width(), m_image.height()) * 0.02);
    constexpr qreal initialScale = 0.42;
    qreal widest = 0;
    for (const PuzzlePiece &piece : std::as_const(m_pieces))
        widest = qMax(widest, qreal(piece.source.width()));
    const qreal width = qMax(m_image.width() * 0.48, (widest + 2 * spacing) * initialScale);
    const QPointF origin(m_image.width() + qMax(8.0, m_image.width() * 0.025), 0);
    m_tray = QRectF(origin, QSizeF(width, m_image.height()));
    // Свободные прямоугольники позволяют заполнить промежутки между деталями разной высоты.
    // При неудачной упаковке уменьшаем все миниатюры одинаково, сохраняя их относительные размеры.
    for (qreal scale = initialScale; ; scale *= 0.85) {
        const qreal padding = spacing * scale;
        QVector<QRectF> free{QRectF(padding, padding, width - padding, m_image.height() - padding)};
        QVector<QPointF> positions(m_pieces.size());
        bool fits = true;
        for (int id : order) {
            const QRectF bounds = m_pieces.at(id).boundsAtScale(scale);
            const QSizeF size = bounds.size() + QSizeF(padding, padding);
            int best = -1;
            qreal score = std::numeric_limits<qreal>::max();
            for (int i = 0; i < free.size(); ++i) {
                const QRectF &space = free.at(i);
                if (size.width() <= space.width() && size.height() <= space.height()) {
                    const qreal candidate = qMin(space.width() - size.width(), space.height() - size.height());
                    if (candidate < score) { best = i; score = candidate; }
                }
            }
            if (best < 0) { fits = false; break; }
            const QRectF used(free.at(best).topLeft(), size);
            positions[id] = origin + used.topLeft() - bounds.topLeft();
            QVector<QRectF> remaining;
            for (const QRectF &space : std::as_const(free)) {
                if (!space.intersects(used)) { remaining.append(space); continue; }
                if (used.left() > space.left()) remaining.append(QRectF(space.left(), space.top(), used.left() - space.left(), space.height()));
                if (used.right() < space.right()) remaining.append(QRectF(used.right(), space.top(), space.right() - used.right(), space.height()));
                if (used.top() > space.top()) remaining.append(QRectF(space.left(), space.top(), space.width(), used.top() - space.top()));
                if (used.bottom() < space.bottom()) remaining.append(QRectF(space.left(), used.bottom(), space.width(), space.bottom() - used.bottom()));
            }
            // Вложенные свободные области не дают новых мест и раздувают последующий поиск.
            free.clear();
            for (int i = 0; i < remaining.size(); ++i) {
                bool contained = false;
                for (int j = 0; j < remaining.size(); ++j) {
                    if (i != j && remaining.at(j).contains(remaining.at(i))
                        && (remaining.at(j) != remaining.at(i) || j < i)) { contained = true; break; }
                }
                if (!contained) free.append(remaining.at(i));
            }
        }
        if (!fits) continue;
        m_thumbnailScale = scale;
        qreal bottom = 0;
        for (int id = 0; id < m_pieces.size(); ++id) {
            m_pieces[id].position = positions.at(id);
            m_pieces[id].placed = false;
            m_pieces[id].scale = scale;
            bottom = qMax(bottom, positions.at(id).y() - origin.y() + m_pieces.at(id).boundsAtScale(scale).bottom());
        }
        // Рамка лотка охватывает только занятую высоту, а не пустую область до нижнего края поля.
        m_tray.setHeight(qMin(qreal(m_image.height()), bottom + padding));
        break;
    }
    qCDebug(lcPuzzle) << "Rotated compact tray, seed:" << seed << "size:" << m_tray.size()
                      << "thumbnail scale:" << m_thumbnailScale;
    m_placed = 0;
}

void PuzzleModel::restart(quint32 seed)
{
    if (m_pieces.isEmpty())
        return;
    emit aboutToReset();
    shuffle(seed);
    qCInfo(lcPuzzle) << "Puzzle restarted, layout seed:" << seed;
    emit puzzleReset();
    emit progressChanged(0, int(m_pieces.size()));
}

bool PuzzleModel::place(int id, QPointF position, qreal tolerance)
{
    if (id < 0 || id >= m_pieces.size() || m_pieces.at(id).placed
        || !qIsFinite(position.x()) || !qIsFinite(position.y()) || !qIsFinite(tolerance) || tolerance < 0)
        return false;
    PuzzlePiece &piece = m_pieces[id];
    const QPointF target = piece.source.topLeft();
    const QPointF center = position + QRectF(QPointF(), piece.source.size()).center();
    // Отпускание в лотке не должно закреплять соседнюю с ним угловую деталь при большом допуске.
    const bool snaps = piece.rotation == 0 && !m_tray.contains(center) && QLineF(position, target).length() <= tolerance;
    qreal scale = 1.0;
    if (snaps)
        position = target;
    else {
        if (m_tray.contains(center)) {
            const QSizeF fullSize = piece.boundsAtScale(1).size();
            scale = qMin(m_thumbnailScale, qMin(m_tray.width() / fullSize.width(), m_tray.height() / fullSize.height()));
            const QRectF bounds = piece.boundsAtScale(scale);
            position = center - QRectF(QPointF(), piece.source.size()).center() * scale;
            // При точном заполнении лотка округление может поменять границы местами на долю пикселя.
            const qreal left = m_tray.left() - bounds.left();
            const qreal top = m_tray.top() - bounds.top();
            position.setX(qBound(left, position.x(), qMax(left, m_tray.right() - bounds.right())));
            position.setY(qBound(top, position.y(), qMax(top, m_tray.bottom() - bounds.bottom())));
        }
    }
    if (!workspaceRect().adjusted(-1e-7, -1e-7, 1e-7, 1e-7).contains(piece.boundsAtScale(scale).translated(position)))
        return false;
    if (snaps) {
        piece.placed = true;
        ++m_placed;
    }
    piece.position = position;
    piece.scale = scale;
    emit pieceChanged(id);
    if (piece.placed) {
        emit progressChanged(m_placed, int(m_pieces.size()));
        if (m_placed == m_pieces.size()) {
            qCInfo(lcPuzzle) << "Puzzle completed, seed:" << m_seed << "count:" << m_placed;
            emit completed();
        }
    }
    return true;
}

bool PuzzleModel::rotate(int id, int quarterTurns)
{
    if (id < 0 || id >= m_pieces.size() || m_pieces.at(id).placed || quarterTurns % 4 == 0)
        return false;
    PuzzlePiece candidate = m_pieces.at(id);
    const QRectF previous = candidate.boundsAtScale(candidate.scale).translated(candidate.position);
    candidate.rotation = (candidate.rotation + (quarterTurns % 4) * 90 + 360) % 360;
    const QRectF area = m_tray.contains(previous.center()) ? m_tray : workspaceRect();
    // У края лотка поворот может потребовать уменьшения: длинная сторона не должна исчезнуть за рамкой.
    const QSizeF fullSize = candidate.boundsAtScale(1).size();
    const qreal nextScale = qMin(candidate.scale, qMin(area.width() / fullSize.width(), area.height() / fullSize.height()));
    const QPointF center = QRectF(QPointF(), candidate.source.size()).center();
    candidate.position += center * (candidate.scale - nextScale);
    candidate.scale = nextScale;
    const QRectF bounds = candidate.boundsAtScale(candidate.scale);
    const qreal left = area.left() - bounds.left();
    const qreal top = area.top() - bounds.top();
    candidate.position.setX(qBound(left, candidate.position.x(), qMax(left, area.right() - bounds.right())));
    candidate.position.setY(qBound(top, candidate.position.y(), qMax(top, area.bottom() - bounds.bottom())));
    m_pieces[id] = candidate;
    qCDebug(lcPuzzle) << "Piece rotated, id:" << id << "angle:" << candidate.rotation;
    emit pieceChanged(id);
    emit pieceRotated(id, quarterTurns % 4);
    return true;
}
