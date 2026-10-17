#ifndef PUZZLEMODEL_H
#define PUZZLEMODEL_H

#include <QImage>
#include <QObject>
#include <QRectF>
#include <QVector>
#include <QPainterPath>

/** @brief Геометрия и положение одной детали; id совпадает с индексом в модели. */
struct PuzzlePiece {
    int id = -1; ///< Уникальный номер детали.
    QRect source; ///< Ограничивающий прямоугольник контура в изображении; его начало — правильная позиция.
    QPointF position; ///< Текущая позиция в логических координатах сцены.
    bool placed = false; ///< Закреплённая деталь больше не перемещается.
    qreal scale = 1.0; ///< Масштаб миниатюры в лотке; на поле сборки всегда 1.
    QPainterPath shape; ///< Фигурный контур в локальных координатах прямоугольника source.
};

/** @brief Единственный источник позиций и прогресса пазла; используется в GUI-потоке. */
class PuzzleModel : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PuzzleModel)
public:
    /** @brief Создаёт пустую модель. @param parent Владелец модели. */
    explicit PuzzleModel(QObject *parent = nullptr);
    /** @brief Создаёт новый пазл, сохраняя прежний при ошибке. @param image Рабочее изображение.
     * @param count Число частей. @param seed Seed разбиения. @param error Выходное описание ошибки.
     * @return true при успешном создании. */
    bool start(const QImage &image, int count, quint32 seed, QString *error = nullptr);
    /** @brief Перемешивает прежние детали и сбрасывает прогресс. @param seed Seed раскладки. */
    void restart(quint32 seed);
    /** @brief Принимает отпускание детали в полном масштабе, возвращая её в миниатюру при попадании в лоток.
     * @param id Номер детали. @param position Новая позиция верхнего левого угла полноразмерной детали.
     * @param tolerance Допуск закрепления в координатах сцены. @return true, если позиция принята. */
    bool place(int id, QPointF position, qreal tolerance);
    /** @brief Возвращает разделяемое рабочее изображение. @return Изображение модели. */
    const QImage &image() const;
    /** @brief Возвращает детали без копирования. @return Массив деталей. */
    const QVector<PuzzlePiece> &pieces() const;
    /** @brief Возвращает прямоугольник сборки. @return Геометрия поля. */
    QRectF boardRect() const;
    /** @brief Возвращает область раскладки. @return Геометрия лотка. */
    QRectF trayRect() const;
    /** @brief Читает масштаб деталей в лотке. @return Общий масштаб миниатюр текущей раскладки. */
    qreal thumbnailScale() const;
    /** @brief Возвращает доступную область перемещения. @return Поле вместе с лотком. */
    QRectF workspaceRect() const;
    /** @brief Читает прогресс. @return Число закреплённых деталей. */
    int placedCount() const;
    /** @brief Читает seed геометрии. @return Seed последнего разбиения. */
    quint32 seed() const;
signals:
    /** @brief Предупреждает сцену об удалении прежних деталей. */
    void aboutToReset();
    /** @brief Сообщает о новой геометрии или раскладке. */
    void puzzleReset();
    /** @brief Сообщает о принятом перемещении. @param id Номер детали. */
    void pieceChanged(int id);
    /** @brief Сообщает прогресс. @param placed Закреплено. @param total Всего. */
    void progressChanged(int placed, int total);
    /** @brief Сообщает о закреплении последней детали ровно один раз за раунд. */
    void completed();
private:
    /** @brief Упаковывает перемешанные миниатюры в компактный лоток без наложений. @param seed Seed раскладки. */
    void shuffle(quint32 seed);
    QImage m_image; ///< Пиксели хранятся совместно с представлением через implicit sharing.
    QVector<PuzzlePiece> m_pieces; ///< Данные всех деталей.
    QVector<int> m_order; ///< Предыдущий порядок для исключения тождественного перемешивания.
    QRectF m_tray; ///< Лоток занимает около половины ширины изображения и не превышает его высоту.
    qreal m_thumbnailScale = 0.42; ///< Общий масштаб миниатюр текущей раскладки.
    int m_placed = 0; ///< Число закреплений.
    quint32 m_seed = 0; ///< Seed геометрии.
};
#endif
