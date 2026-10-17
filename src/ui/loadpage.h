#ifndef LOADPAGE_H
#define LOADPAGE_H
#include <QWidget>
#include <QImage>
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class ImagePreview;
class QBoxLayout;
class QResizeEvent;

/** @brief Экран выбора изображения и количества деталей; файловое чтение выполняет загрузчик. */
class LoadPage : public QWidget
{
    Q_OBJECT
public:
    /** @brief Создаёт элементы управления. @param parent Владелец экрана. */
    explicit LoadPage(QWidget *parent = nullptr);
    /** @brief Показывает успешно загруженное изображение. @param image Рабочее изображение. @param name Имя файла. */
    void setImage(const QImage &image, const QString &name);
    /** @brief Блокирует повторные запросы чтения. @param busy Активна ли загрузка. */
    void setBusy(bool busy);
    /** @brief Выводит ошибку, сохраняя прежнее изображение. @param message Описание ошибки. */
    void showError(const QString &message);
    /** @brief Проверяет наличие изображения. @return true после успешной загрузки. */
    bool hasImage() const;
protected:
    /** @brief Уменьшает декоративную типографику в компактном окне. @param event Новый размер. */
    void resizeEvent(QResizeEvent *event) override;
signals:
    /** @brief Запрашивает чтение файла или ресурса. @param path Путь изображения. */
    void imageRequested(const QString &path);
    /** @brief Запрашивает начало игры. @param pieceCount Количество частей. */
    void startRequested(int pieceCount);
private slots:
    /** @brief Открывает стандартный диалог с фильтрами доступных форматов Qt. */
    void chooseImage();
    /** @brief Обновляет доступность действий из текущих данных и признака загрузки. */
    void updateButtons();
private:
    QSpinBox *m_count; ///< Число частей, ограниченное вместимостью изображения.
    QComboBox *m_samples; ///< Пути встроенных изображений.
    QPushButton *m_choose; ///< Открытие внешнего файла.
    QPushButton *m_useSample; ///< Загрузка выбранного ресурса.
    QPushButton *m_start; ///< Начало игры доступно после загрузки.
    QLabel *m_status; ///< Информация о файле или ошибка.
    ImagePreview *m_preview; ///< Масштабируемый предпросмотр.
    QLabel *m_title; ///< Заголовок с адаптивным размером.
    QLabel *m_intro; ///< Вторичная инструкция, скрываемая в компактном окне.
    QWidget *m_presets; ///< Быстрый выбор стандартных n; точное число доступно в spin box.
    int m_capacity = 0; ///< Максимально допустимое количество частей.
    bool m_busy = false; ///< Защита от параллельных загрузок.
    bool m_hasImage = false; ///< Признак успешного чтения.
};
#endif
