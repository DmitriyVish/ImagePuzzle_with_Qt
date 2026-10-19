#ifndef PUZZLESCENE_H
#define PUZZLESCENE_H

#include "core/puzzlemodel.h"
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsObject>
class QParallelAnimationGroup;
class QTimer;
class QVariantAnimation;
class QGraphicsPixmapItem;

/** @brief Отрисовывает область общего QImage без отдельной копии пикселей детали. */
class PieceItem : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(qreal placementGlow READ placementGlow WRITE setPlacementGlow)
public:
    enum { Type = QGraphicsItem::UserType + 1 }; ///< Тип для безопасного поиска детали в сцене.
    /** @brief Создаёт элемент. @param image Разделяемое изображение. @param piece Данные детали. */
    PieceItem(const QImage &image, const PuzzlePiece &piece);
    /** @brief Возвращает тип элемента. @return Type. */
    int type() const override;
    /** @brief Возвращает границы в локальных координатах. @return Прямоугольник детали. */
    QRectF boundingRect() const override;
    /** @brief Возвращает настоящий фигурный контур для выбора детали мышью и пальцем. @return Локальный контур. */
    QPainterPath shape() const override;
    /** @brief Рисует деталь. @param painter Рисователь. @param option Параметры Qt. @param widget Виджет Qt. */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    /** @brief Читает номер детали. @return id модели. */
    int id() const;
    /** @brief Мгновенно задаёт ориентацию, отменяя поворот и вихрь.
     * @param degrees Угол по часовой стрелке: 0, 90, 180 или 270. */
    void setOrientation(int degrees);
    /** @brief Читает подтверждённую ориентацию независимо от промежуточного кадра. @return Угол модели. */
    int orientation() const;
    /** @brief Читает промежуточный угол изображения. @return Угол в градусах, включая переход через полный оборот. */
    qreal displayedAngle() const;
    /** @brief Читает фазу появления и затухания вихря. @return Значение от 0 до 1; вне поворота — 0. */
    qreal rotationProgress() const;
    /** @brief Плавно поворачивает изображение с зелёным вихрем, продолжая быстрые повторные команды.
     * @param degrees Новая ориентация модели. @param quarterTurns Подписанное число четвертей оборота.
     * @param animate Разрешены ли движение и свечение; false задаёт ориентацию мгновенно. */
    void rotateTo(int degrees, int quarterTurns, bool animate);
    /** @brief Завершает визуальный поворот в ориентации модели и скрывает вихрь; модель не меняется. */
    void finishRotation();
    /** @brief Меняет вид закреплённой детали. @param locked Признак закрепления. */
    void setLocked(bool locked);
    /** @brief Читает закрепление. @return true для детали, принятой моделью. */
    bool locked() const;
    /** @brief Читает интенсивность подсветки. @return Значение от 0 до 1. */
    qreal placementGlow() const;
    /** @brief Меняет подсветку контура при закреплении. @param value Значение от 0 до 1. */
    void setPlacementGlow(qreal value);
    /** @brief Синхронизирует позицию с моделью, анимируя только новое закрепление.
     * @param target Итоговая позиция. @param animate Разрешено ли движение и свечение. */
    void settleAt(QPointF target, bool animate);
    /** @brief Завершает текущую анимацию закреплённой детали без изменения состояния модели. */
    void finishPlacement();
protected:
    /** @brief Сохраняет центр поворота при изменении масштаба миниатюры.
     * @param change Вид изменения Qt. @param value Значение Qt. @return Результат базового обработчика. */
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
private:
    /** @brief Перестраивает матрицу вокруг масштабированного центра. @param degrees Промежуточный угол кадра. */
    void applyOrientation(qreal degrees);
    QImage m_image; ///< Implicit sharing сохраняет единственный пиксельный буфер.
    QRect m_source; ///< Область исходного изображения.
    QPainterPath m_shape; ///< Тот же контур используется для рисования и hit testing.
    int m_id; ///< Номер детали.
    int m_rotation = 0; ///< Ориентация; перенос центра в базовой матрице учитывает текущий scale().
    qreal m_displayAngle = 0; ///< Угол текущего кадра, отделённый от дискретной ориентации модели.
    qreal m_turnFrom = 0; ///< Угол начала текущего отрезка анимации.
    qreal m_turnTarget = 0; ///< Непрерывная цель; сохраняет направление при переходе 270° → 0°.
    qreal m_turnProgress = 0; ///< Фаза зелёного вихря.
    bool m_locked; ///< Влияет на контур детали.
    QPointF m_target; ///< Позиция, подтверждённая моделью.
    qreal m_glow = 0; ///< Интенсивность краткой подсветки.
    QParallelAnimationGroup *m_placement = nullptr; ///< Дочерняя QObject-анимация удаляется вместе с деталью.
    QVariantAnimation *m_turnAnimation; ///< Единственная переиспользуемая анимация поворота принадлежит детали.
    QGraphicsItem *m_vortex = nullptr; ///< Неинтерактивный дочерний ореол, удаляемый вместе с деталью.
};

/** @brief Отображает модель; не принимает игровых решений. */
class PuzzleScene : public QGraphicsScene
{
    Q_OBJECT
public:
    /** @brief Привязывает сцену к модели. @param model Модель, живущая дольше сцены. @param parent Владелец. */
    explicit PuzzleScene(PuzzleModel *model, QObject *parent = nullptr);
    /** @brief Останавливает таймеры до удаления графических элементов. */
    ~PuzzleScene() override;
    /** @brief Находит элемент по номеру. @param id Номер модели. @return Элемент или nullptr. */
    PieceItem *pieceItem(int id) const;
    /** @brief Включает мгновенное отображение и завершает активные эффекты. @param reduced Отключить анимацию. */
    void setReducedMotion(bool reduced);
    /** @brief Проверяет переход к итоговому снимку. @return true после окончания последнего закрепления. */
    bool completionPresented() const;
signals:
    /** @brief Сообщает, что фотографию можно вписать вместо игрового поля с лотком. */
    void completionPresentationChanged();
    /** @brief Сообщает об окончании перехода и полного затухания швов. */
    void completionEffectsFinished();
private slots:
    /** @brief Перестраивает элементы после сброса модели. */
    void rebuild();
    /** @brief Обновляет одну деталь. @param id Номер модели. */
    void updatePiece(int id);
    /** @brief Запускает переход к фотографии после окончания последнего закрепления. */
    void showCompleted();
    /** @brief Показывает целый снимок после окончания закрепления последней детали. */
    void presentCompleted();
private:
    /** @brief Строит уникальные внутренние швы, исключая внешнюю рамку изображения. @return Контур свечения. */
    QPainterPath internalSeams() const;
    PuzzleModel *m_model; ///< Не владеет моделью.
    QVector<PieceItem *> m_items; ///< Указатели принадлежащих сцене элементов.
    QGraphicsRectItem *m_tray = nullptr; ///< Лоток принадлежит сцене и скрывается после победы.
    QGraphicsPixmapItem *m_photo = nullptr; ///< Целый снимок поверх собранных деталей.
    QGraphicsItem *m_seamGlow = nullptr; ///< Неинтерактивное свечение внутренних швов.
    QTimer *m_completionDelay; ///< Отложенный переход отменяется при новой игре.
    QVariantAnimation *m_completionAnimation; ///< Общая временная шкала затухания швов и появления снимка.
    bool m_reducedMotion = false; ///< Эффекты разрешены до явного отключения пользователем.
    bool m_pendingCompletion = false; ///< Последняя деталь ещё завершает закрепление.
};
#endif
