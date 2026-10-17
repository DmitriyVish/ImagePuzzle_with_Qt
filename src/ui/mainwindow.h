#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "app/appcontroller.h"
#include "io/imageloader.h"
#include <QFutureWatcher>
#include <QMainWindow>
#include <memory>
class GameController;
class GamePage;
class LoadPage;
namespace Ui { class MainWindow; }

/** @brief Соединяет фоновую загрузку, экраны и игровой контроллер в GUI-потоке. */
class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)
public:
    /** @brief Создаёт окно настройки. @param parent Родительский виджет. */
    explicit MainWindow(QWidget *parent = nullptr);
    /** @brief Удаляет представление до модели и безопасно отключает фоновые уведомления. */
    ~MainWindow() override;
    /** @brief Читает состояние приложения. @return Текущее состояние. */
    AppController::State state() const;
public slots:
    /** @brief Начинает фоновое чтение, не меняя прежнюю картинку при ошибке. @param path Файл или ресурс. */
    void loadImage(const QString &path);
private slots:
    /** @brief Передаёт прочитанное изображение интерфейсу в GUI-потоке. */
    void imageLoaded();
    /** @brief Запрашивает генерацию пазла. @param pieceCount Число фрагментов. */
    void openGame(int pieceCount);
    /** @brief Отменяет подготовку и возвращает к настройке. */
    void showSetup();
    /** @brief Синхронизирует экран с состоянием. @param state Новое состояние. */
    void updateScreen(AppController::State state);
    /** @brief Показывает заголовок коротким затуханием при разрешённом движении. @param widget Заголовок. */
    void animateArrival(QWidget *widget);
private:
    std::unique_ptr<Ui::MainWindow> m_ui; ///< Невладеющее описание формы.
    AppController *m_controller; ///< Владелец — главное окно.
    GameController *m_game; ///< Владелец — главное окно.
    LoadPage *m_loadPage; ///< Владелец — стек страниц.
    GamePage *m_gamePage; ///< Владелец — стек страниц.
    QFutureWatcher<ImageLoadResult> *m_loader; ///< GUI-наблюдатель результата фонового чтения.
    QImage m_image; ///< Текущее рабочее изображение.
    QString m_imagePath; ///< Путь активного запроса загрузки.
    bool m_reducedMotion = false; ///< Сохранённое предпочтение отключения анимации.
};
#endif
