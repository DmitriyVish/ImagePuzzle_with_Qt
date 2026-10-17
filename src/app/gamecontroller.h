#ifndef GAMECONTROLLER_H
#define GAMECONTROLLER_H

#include "appcontroller.h"
#include "core/puzzlemodel.h"
#include <QTimer>

/** @brief Соединяет состояния приложения с подготовкой, перемещением и завершением пазла. */
class GameController : public QObject
{
    Q_OBJECT
public:
    /** @brief Создаёт игровую модель. @param flow Контроллер состояний, живущий дольше объекта.
     * @param parent Владелец контроллера. */
    explicit GameController(AppController *flow, QObject *parent = nullptr);
    /** @brief Предоставляет модель для сцены в GUI-потоке. @return Принадлежащая контроллеру модель. */
    PuzzleModel *model();
    /** @brief Начинает новый пазл. @param image Рабочее изображение. @param count Число частей.
     * @param seed Seed разбиения. @return true, если подготовка запущена. */
    bool start(const QImage &image, int count, quint32 seed);
    /** @brief Останавливает подготовку и возвращает приложение к настройке. */
    void cancel();
public slots:
    /** @brief Повторяет прежнее разбиение с новым перемешиванием. */
    void restart();
    /** @brief Создаёт другое разбиение того же изображения. */
    void regenerate();
    /** @brief Принимает отпускание только в активной игре. @param id Номер детали.
     * @param position Позиция в сцене. @param tolerance Допуск закрепления. */
    void dropPiece(int id, QPointF position, qreal tolerance);
signals:
    /** @brief Сообщает об ошибке подготовки. @param message Описание для пользователя. */
    void preparationFailed(const QString &message);
private slots:
    /** @brief Завершает отменяемую подготовку в цикле GUI. */
    void prepare();
private:
    AppController *m_flow; ///< Не владеет; жизненный цикл ограничен главным окном.
    PuzzleModel *m_model; ///< Дочерняя модель.
    QTimer m_timer; ///< Таймер продолжения, отменяемый при возврате.
    QImage m_pendingImage; ///< Изображение ожидающего запроса.
    int m_pendingCount = 0; ///< Число деталей запроса.
    quint32 m_pendingSeed = 0; ///< Seed запроса.
    bool m_reuse = false; ///< Признак повторного использования геометрии.
};
#endif
