#ifndef GAMEHELPDIALOG_H
#define GAMEHELPDIALOG_H

#include <QDialog>
#include <QVector>

class QGraphicsOpacityEffect;
class QParallelAnimationGroup;
class QLabel;

/** @brief Карточка правил с прокручиваемыми шагами; принадлежит игровому экрану и работает в GUI-потоке. */
class GameHelpDialog : public QDialog
{
    Q_OBJECT
public:
    /** @brief Создаёт оконно-модальный диалог без вложенного цикла GUI. @param parent Владелец диалога. */
    explicit GameHelpDialog(QWidget *parent);
    /** @brief Меняет предпочтение анимации и при отключении сразу завершает текущий эффект.
     * @param reduced true для мгновенного появления всех шагов. */
    void setReducedMotion(bool reduced);
protected:
    /** @brief Подбирает размер по экрану, фокусирует кнопку и запускает появление.
     * @param event Событие показа. */
    void showEvent(QShowEvent *event) override;
    /** @brief Останавливает эффекты до скрытия диалога. @param event Событие скрытия. */
    void hideEvent(QHideEvent *event) override;
    /** @brief Освобождает место под правила в компактном окне. @param event Событие изменения размера. */
    void resizeEvent(QResizeEvent *event) override;
private:
    /** @brief Добавляет появление одного блока в общую временную шкалу.
     * @param widget Блок; владеет созданным эффектом. @param delay Задержка в миллисекундах. */
    void addReveal(QWidget *widget, int delay);
    /** @brief Завершает появление и отключает эффекты для обычной отрисовки текста при прокрутке. */
    void finishReveal();
    QParallelAnimationGroup *m_animation; ///< Общая анимация принадлежит диалогу.
    QVector<QGraphicsOpacityEffect *> m_effects; ///< Принадлежат блокам; включены только во время появления.
    QLabel *m_intro = nullptr; ///< Второстепенное вступление скрывается в компактном режиме.
    QLabel *m_footer = nullptr; ///< Напоминание о повторном открытии скрывается в компактном режиме.
    bool m_reducedMotion = false; ///< Предпочтение применяется и к последующим открытиям.
};

#endif
