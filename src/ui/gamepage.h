#ifndef GAMEPAGE_H
#define GAMEPAGE_H
#include "app/appcontroller.h"
#include <QWidget>
class PuzzleModel;
class PuzzleView;
class QLabel;
class QPushButton;
class QProgressBar;
class GameHelpDialog;

/** @brief Игровой экран с общей сценой, прогрессом и действиями нового раунда. */
class GamePage : public QWidget
{
    Q_OBJECT
public:
    /** @brief Создаёт сцену модели. @param model Модель пазла. @param parent Владелец экрана. */
    explicit GamePage(PuzzleModel *model, QWidget *parent = nullptr);
    /** @brief Отображает состояние приложения. @param state Новое состояние. */
    void setState(AppController::State state);
    /** @brief Применяет предпочтение анимации к открытой и будущим карточкам правил.
     * @param reduced true для мгновенного появления подсказки. */
    void setReducedMotion(bool reduced);
public slots:
    /** @brief Открывает правила в активной игре, отменяя незавершённый перенос; повторный вызов поднимает карточку. */
    void showHelp();
signals:
    /** @brief Запрашивает возврат к выбору изображения. */
    void setupRequested();
    /** @brief Запрашивает повтор прежнего разбиения. */
    void restartRequested();
    /** @brief Запрашивает новую геометрию того же изображения. */
    void regenerateRequested();
private:
    PuzzleView *m_view; ///< Представление сцены.
    QLabel *m_summary; ///< Точное число закреплений.
    QLabel *m_completion; ///< Результат сборки.
    QProgressBar *m_progress; ///< Количество собранных деталей либо ожидание подготовки.
    QPushButton *m_restart; ///< Повтор раунда.
    QPushButton *m_regenerate; ///< Новое разбиение.
    QPushButton *m_helpButton; ///< Повторное открытие правил.
    GameHelpDialog *m_help; ///< Единственная карточка правил принадлежит игровому экрану.
};
#endif
