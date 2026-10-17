#ifndef PUZZLEGENERATOR_H
#define PUZZLEGENERATOR_H

#include <QRect>
#include <QString>
#include <QVector>
#include <QPainterPath>

/** @brief Результат разбиения: при ошибке fragments пуст, error содержит описание. */
struct PuzzleLayout {
    QVector<QRect> fragments; ///< Базовые ячейки: полностью покрывают изображение до формирования фигурных краёв.
    QVector<QPainterPath> shapes; ///< Совпадающие фигурные контуры в координатах изображения; индексы соответствуют ячейкам.
    QString error; ///< Причина невозможности разбиения.
};

/** @brief Детерминированное разбиение изображения на заданное число деталей с плавными сопряжёнными краями. */
class PuzzleGenerator
{
public:
    static constexpr int MinimumSide = 16; ///< Минимальная сторона детали в пикселях изображения.
    static constexpr int MinimumCount = 4; ///< Нижний предел количества деталей.
    static constexpr int MaximumCount = 20; ///< Верхний предел количества деталей.
    static constexpr int MaximumImageSide = 1600; ///< Максимальная сторона рабочего изображения.

    /** @brief Вычисляет предел n для размера. @param size Размер изображения. @return Допустимое количество частей. */
    static int maximumPieces(QSize size);
    /**
     * @brief Разбивает изображение без пробелов и перекрытий.
     * @param size Размер рабочего изображения, не больше MaximumImageSide.
     * @param count Точное количество частей, от MinimumCount до maximumPieces(size).
     * @param seed Начальное состояние генератора случайных чисел.
     * @return Геометрия деталей либо описание ошибки; одинаковый seed воспроизводим в одной сборке.
     */
    static PuzzleLayout generate(QSize size, int count, quint32 seed);
};

#endif
