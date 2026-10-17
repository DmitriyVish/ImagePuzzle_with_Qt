#ifndef IMAGEPREVIEW_H
#define IMAGEPREVIEW_H
#include <QImage>
#include <QWidget>

/** @brief Масштабирует предпросмотр при рисовании без изменения пропорций изображения. */
class ImagePreview : public QWidget
{
public:
    /** @brief Создаёт пустой предпросмотр. @param parent Владелец виджета. */
    explicit ImagePreview(QWidget *parent = nullptr);
    /** @brief Заменяет разделяемое изображение. @param image Рабочее изображение. */
    void setImage(const QImage &image);
protected:
    /** @brief Рисует фон и вписанное изображение. @param event Событие перерисовки. */
    void paintEvent(QPaintEvent *event) override;
private:
    QImage m_image; ///< Пиксели разделяются с результатом загрузчика.
    QImage m_cover; ///< Встроенная музейная обложка до выбора игровой фотографии.
};
#endif
