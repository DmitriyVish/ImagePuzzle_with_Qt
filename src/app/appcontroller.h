#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>

/** @brief Управляет состояниями приложения независимо от виджетов. Все вызовы — в GUI-потоке. */
class AppController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AppController)

public:
    /** @brief Состояния настройки и жизненного цикла игры. */
    enum class State {
        Configuring, ///< Настройка новой игры.
        Preparing,   ///< Подготовка игрового поля.
        Playing,     ///< Открыто игровое поле.
        Completed,   ///< Сборка изображения завершена.
    };
    Q_ENUM(State)

    /** @brief Создаёт контроллер в состоянии настройки. @param parent Владелец QObject. */
    explicit AppController(QObject *parent = nullptr);
    /** @brief Читает состояние без побочных эффектов. @return Состояние приложения. */
    State state() const;

public slots:
    /** @brief Начинает подготовку из настройки. @return true, если переход выполнен. */
    bool beginPreparation();
    /** @brief Запускает повтор из игры или результата. @return true при начале подготовки. */
    bool restartPreparation();
    /** @brief Завершает активную подготовку. @return true, если открыта игра. */
    bool finishPreparation();
    /** @brief Фиксирует завершение активной игры. @return true, если переход выполнен. */
    bool finishGame();
    /** @brief Возвращает к настройке из другого состояния. @return true при переходе. */
    bool returnToSetup();

signals:
    /** @brief Сообщает о выполненном переходе. @param state Новое состояние. */
    void stateChanged(AppController::State state);

private:
    /**
     * @brief Проверяет переход и испускает сигнал после изменения состояния.
     * @param next Запрашиваемое состояние.
     * @return true при переходе; ошибочный или повторный запрос не меняет состояние.
     */
    bool transitionTo(State next);

    State m_state = State::Configuring; ///< Единственный источник состояния приложения.
};

#endif // APPCONTROLLER_H
