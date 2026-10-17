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
private:
    QImage m_image; ///< Implicit sharing сохраняет единственный пиксельный буфер.
    QRect m_source; ///< Область исходного изображения.
    QPainterPath m_shape; ///< Тот же контур используется для рисования и hit testing.
    int m_id; ///< Номер детали.
    bool m_locked; ///< Влияет на контур детали.
    QPointF m_target; ///< Позиция, подтверждённая моделью.
    qreal m_glow = 0; ///< Интенсивность краткой подсветки.
    QParallelAnimationGroup *m_placement = nullptr; ///< Дочерняя QObject-анимация удаляется вместе с деталью.
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
