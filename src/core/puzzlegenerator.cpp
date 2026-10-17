#include "puzzlegenerator.h"

#include <QCoreApplication>
#include <algorithm>
#include <functional>
#include <random>
#include <map>
#include <tuple>

namespace {
/** @brief Создаёт сопряжённые границы ячеек, сохраняя общие узлы Т-образных стыков.
 * @param cells Базовые прямоугольники. @param size Изображение. @param random Генератор формы.
 * @return По одному замкнутому контуру в координатах изображения на ячейку. */
QVector<QPainterPath> makeShapes(const QVector<QRect> &cells, QSize size, std::mt19937 &random)
{
    using Edge = std::tuple<bool, int, int, int>;
    std::map<Edge, QPainterPath> edges;
    const auto edgePath = [&](bool vertical, int fixed, int from, int to) {
        const Edge key(vertical, fixed, from, to);
        const auto found = edges.find(key);
        if (found != edges.end()) return found->second;
        const qreal length = to - from;
        const auto point = [=](qreal along, qreal normal) {
            return vertical ? QPointF(fixed + normal, from + along) : QPointF(from + along, fixed + normal);
        };
        QPainterPath path(point(0, 0));
        if (fixed != 0 && fixed != (vertical ? size.width() : size.height()) && length >= 8) {
            int thickness = qMax(size.width(), size.height());
            for (const QRect &cell : cells) {
                const int low = vertical ? cell.y() : cell.x();
                const int high = low + (vertical ? cell.height() : cell.width());
                const int first = vertical ? cell.x() : cell.y();
                const int second = first + (vertical ? cell.width() : cell.height());
                if ((fixed == first || fixed == second) && low <= from && high >= to)
                    thickness = qMin(thickness, vertical ? cell.width() : cell.height());
            }
            const qreal variation = 0.85 + qreal(random() % 301) / 1000;
            const qreal depth = qMin(length * 0.25, thickness * 0.22) * variation;
            const qreal sign = random() % 2 ? 1 : -1;
            const qreal span = qMin(length, depth * 8);
            const auto p = [&](qreal u, qreal v) { return point(length / 2 + (u - 0.5) * span, sign * v * depth); };
            path.lineTo(p(0.30, 0));
            // Касательные соседних кубических кривых совпадают по направлению:
            // плечо плавно переходит в узкую шейку, затем в округлый выступ и обратно.
            path.cubicTo(p(0.40, 0), p(0.45, 0), p(0.45, 0.12));
            path.cubicTo(p(0.45, 0.24), p(0.35, 0.25), p(0.35, 0.55));
            path.cubicTo(p(0.35, 0.85), p(0.65, 0.85), p(0.65, 0.55));
            path.cubicTo(p(0.65, 0.25), p(0.55, 0.24), p(0.55, 0.12));
            path.cubicTo(p(0.55, 0), p(0.60, 0), p(0.70, 0));
        }
        path.lineTo(point(length, 0));
        edges.emplace(key, path);
        return path;
    };
    QVector<QPainterPath> shapes;
    for (const QRect &cell : cells) {
        QPainterPath shape(QPointF(cell.x(), cell.y()));
        const auto appendSide = [&](bool vertical, int fixed, int from, int to, bool reverse) {
            QVector<int> knots{from, to};
            for (const QRect &other : cells) {
                const int first = vertical ? other.x() : other.y();
                const int second = first + (vertical ? other.width() : other.height());
                if (fixed != first && fixed != second) continue;
                for (int knot : {vertical ? other.y() : other.x(),
                                 vertical ? other.y() + other.height() : other.x() + other.width()})
                    if (knot > from && knot < to) knots.append(knot);
            }
            std::sort(knots.begin(), knots.end());
            knots.erase(std::unique(knots.begin(), knots.end()), knots.end());
            // Длинную сторону делим в точках примыкания коротких, чтобы обе стороны
            // любого стыка использовали тот же контур, включая Т-образные соединения.
            for (int step = 0; step < knots.size() - 1; ++step) {
                const int index = reverse ? int(knots.size()) - 2 - step : step;
                const QPainterPath side = edgePath(vertical, fixed, knots.at(index), knots.at(index + 1));
                shape.connectPath(reverse ? side.toReversed() : side);
            }
        };
        appendSide(false, cell.y(), cell.x(), cell.x() + cell.width(), false);
        appendSide(true, cell.x() + cell.width(), cell.y(), cell.y() + cell.height(), false);
        appendSide(false, cell.y() + cell.height(), cell.x(), cell.x() + cell.width(), true);
        appendSide(true, cell.x(), cell.y(), cell.y() + cell.height(), true);
        shape.closeSubpath();
        shapes.append(shape);
    }
    return shapes;
}
}

int PuzzleGenerator::maximumPieces(QSize size)
{
    if (size.width() <= 0 || size.height() <= 0)
        return 0;
    const qint64 capacity = qint64(size.width() / MinimumSide) * (size.height() / MinimumSide);
    return int(qMin<qint64>(MaximumCount, capacity));
}

PuzzleLayout PuzzleGenerator::generate(QSize size, int count, quint32 seed)
{
    PuzzleLayout result;
    const int maximum = maximumPieces(size);
    if (maximum < MinimumCount) {
        result.error = QCoreApplication::translate("PuzzleGenerator", "Изображение слишком мало для %1 фрагментов со стороной от %2 пикселей.")
                           .arg(MinimumCount).arg(MinimumSide);
        return result;
    }
    if (size.width() > MaximumImageSide || size.height() > MaximumImageSide
        || count < MinimumCount || count > maximum) {
        result.error = QCoreApplication::translate("PuzzleGenerator", "Для этого изображения можно создать от %1 до %2 фрагментов.")
                           .arg(MinimumCount).arg(maximum);
        return result;
    }
    std::mt19937 random(seed);
    const auto capacity = [](int width, int height) {
        return (width / MinimumSide) * (height / MinimumSide);
    };
    std::function<void(QRect, int)> divide = [&](QRect rectangle, int quota) {
        if (quota == 1) {
            result.fragments.append(rectangle);
            return;
        }
        bool vertical = rectangle.width() >= rectangle.height();
        if (random() % 5 == 0 && qMax(rectangle.width(), rectangle.height()) <= 2 * qMin(rectangle.width(), rectangle.height()))
            vertical = !vertical;
        QVector<int> cuts;
        for (int attempt = 0; attempt < 2; ++attempt) {
            const int extent = vertical ? rectangle.width() : rectangle.height();
            const int cross = vertical ? rectangle.height() : rectangle.width();
            for (int cut = MinimumSide; cut <= extent - MinimumSide; ++cut) {
                // Каждая половина должна вместить свою квоту даже у предельного n.
                if (capacity(cut, cross) + capacity(extent - cut, cross) >= quota)
                    cuts.append(cut);
            }
            if (!cuts.isEmpty())
                break;
            vertical = !vertical;
        }
        const int extent = vertical ? rectangle.width() : rectangle.height();
        const int cross = vertical ? rectangle.height() : rectangle.width();
        QVector<int> balanced;
        for (int cut : cuts) {
            if (cut >= extent / 4 && cut <= 3 * extent / 4)
                balanced.append(cut);
        }
        if (!balanced.isEmpty())
            cuts = balanced;
        const int cut = cuts.at(std::uniform_int_distribution<int>(0, int(cuts.size()) - 1)(random));
        const int lower = qMax(1, quota - capacity(extent - cut, cross));
        const int upper = qMin(quota - 1, capacity(cut, cross));
        const int leftQuota = qBound(lower, qRound(double(quota) * cut / extent), upper);
        QRect first = rectangle;
        QRect second = rectangle;
        if (vertical) {
            first.setWidth(cut);
            second.setX(rectangle.x() + cut);
        } else {
            first.setHeight(cut);
            second.setY(rectangle.y() + cut);
        }
        // Обе части используют одну целочисленную границу, поэтому пиксели не теряются.
        divide(first, leftQuota);
        divide(second, quota - leftQuota);
    };
    divide(QRect(QPoint(0, 0), size), count);
    result.shapes = makeShapes(result.fragments, size, random);
    return result;
}
