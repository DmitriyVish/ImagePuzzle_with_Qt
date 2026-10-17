#ifndef PUZZLEVIEW_H
#define PUZZLEVIEW_H

#include "puzzlescene.h"
#include <QGraphicsView>

/** @brief Общий механизм перетаскивания мышью и одним пальцем; пустое место перемещает обзор. */
class PuzzleView : public QGraphicsView
{
    Q_OBJECT
public:
    /** @brief Создаёт сцену и приёмник ввода. @param model Модель. @param parent Владелец виджета. */
    explicit PuzzleView(PuzzleModel *model, QWidget *parent = nullptr);
    /** @brief Предоставляет сцену для визуализации и тестов. @return Сцена представления. */
    PuzzleScene *puzzleScene();
public slots:
    /** @brief Показывает всё поле и лоток без изменения координат деталей. */
    void fitAll();
    /** @brief Увеличивает масштаб. */
    void zoomIn();
    /** @brief Уменьшает масштаб. */
    void zoomOut();
    /** @brief Отменяет текущий жест и восстанавливает позицию из модели. */
    void cancelPointer();
signals:
    /** @brief Передаёт отпускание контроллеру. @param id Номер детали. @param position Позиция сцены.
     * @param tolerance Допуск в координатах сцены. */
    void pieceDropped(int id, QPointF position, qreal tolerance);
protected:
    /** @brief Обрабатывает касания viewport. @param event Событие. @return true при обработке. */
    bool viewportEvent(QEvent *event) override;
    /** @brief Отменяет жест при потере окна или отключении. @param event Событие. @return Результат Qt. */
    bool event(QEvent *event) override;
    /** @brief Начинает перенос мышью. @param event Событие мыши. */
    void mousePressEvent(QMouseEvent *event) override;
    /** @brief Обновляет перенос мышью. @param event Событие мыши. */
    void mouseMoveEvent(QMouseEvent *event) override;
    /** @brief Завершает перенос мышью. @param event Событие мыши. */
    void mouseReleaseEvent(QMouseEvent *event) override;
    /** @brief Масштабирует Ctrl+колесом. @param event Событие колеса. */
    void wheelEvent(QWheelEvent *event) override;
    /** @brief Поддерживает режим показа всего поля. @param event Событие размера. */
    void resizeEvent(QResizeEvent *event) override;
private:
    /** @brief Выбирает деталь или начинает панорамирование. @param position Координаты viewport. */
    void beginPointer(QPoint position);
    /** @brief Обновляет жест. @param position Координаты viewport. */
    void movePointer(QPoint position);
    /** @brief Завершает жест. @param position Координаты viewport. @param touch Источник ввода. */
    void endPointer(QPoint position, bool touch);
    /** @brief Ограничивает ручной масштаб. @param factor Множитель масштаба. */
    void zoom(qreal factor);
    PuzzleModel *m_model; ///< Модель живёт дольше представления.
    PuzzleScene *m_scene; ///< Дочерняя сцена.
    PieceItem *m_dragged = nullptr; ///< Временное положение не записывается в модель до отпускания.
    QPointF m_offset; ///< Смещение точки захвата внутри детали.
    QPoint m_last; ///< Последняя точка панорамирования.
    int m_touchId = -1; ///< Единственный активный палец.
    bool m_mouse = false; ///< Активен жест мыши.
    bool m_pan = false; ///< Активно панорамирование пустого пространства.
    bool m_fit = true; ///< Сохраняется ли автоматическое вписывание при resize.
};
#endif
