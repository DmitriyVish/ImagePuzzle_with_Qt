#ifndef PUZZLEVIEW_H
#define PUZZLEVIEW_H

#include "puzzlescene.h"
#include <QGraphicsView>
class QGestureEvent;

/** @brief Перенос мышью/пальцем, двойной клик и двухпальцевое вращение; пустое место перемещает обзор. */
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
    /** @brief Передаёт поворот контроллеру. @param id Номер детали.
     * @param quarterTurns Четверти оборота; положительное направление — по часовой стрелке. */
    void pieceRotationRequested(int id, int quarterTurns);
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
    /** @brief Поворачивает незакреплённую деталь на 90° по часовой стрелке. @param event Событие мыши. */
    void mouseDoubleClickEvent(QMouseEvent *event) override;
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
    /** @brief Выбирает цель вращения и отменяет перенос до конца последовательности касаний.
     * @param position Центр жеста в координатах viewport. */
    void beginRotation(QPoint position);
    /** @brief Принимает вращение QPinchGesture, игнорируя масштаб и сдвиг центра.
     * @param event Событие жеста. @return true, если найден pinch-жест. */
    bool handleGesture(QGestureEvent *event);
    PuzzleModel *m_model; ///< Модель живёт дольше представления.
    PuzzleScene *m_scene; ///< Дочерняя сцена.
    PieceItem *m_dragged = nullptr; ///< Временное положение не записывается в модель до отпускания.
    QPointF m_offset; ///< Смещение захвата с учётом поворота, но без масштаба.
    QPoint m_last; ///< Последняя точка панорамирования.
    int m_touchId = -1; ///< Единственный активный палец.
    bool m_mouse = false; ///< Активен жест мыши.
    bool m_pan = false; ///< Активно панорамирование пустого пространства.
    bool m_fit = true; ///< Сохраняется ли автоматическое вписывание при resize.
    int m_rotationId = -1; ///< Цель текущего вращения; закреплённые детали не выбираются.
    qreal m_rotationRemainder = 0; ///< Накопленный угол после вычета выполненных четвертей оборота.
    bool m_blockTouchDrag = false; ///< После двух пальцев перенос блокируется до отпускания всех пальцев.
    bool m_pinchActive = false; ///< Защищает выбор детали от повторного начала при обновлении жеста.
};
#endif
