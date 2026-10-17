#include "imagepreview.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFontMetrics>

ImagePreview::ImagePreview(QWidget *parent) : QWidget(parent)
{
    m_cover.load(QStringLiteral(":/imagepuzzle/assets/museum/kunitsina-estate.jpg"));
    setMinimumSize(160, 72);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(tr("Предпросмотр изображения"));
}
void ImagePreview::setImage(const QImage &image) { m_image = image; update(); }
void ImagePreview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()), 24, 24);
    painter.setClipPath(clip);
    painter.fillRect(rect(), Qt::white);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (m_image.isNull() && !m_cover.isNull()) {
        const QColor card("#24332D");
        painter.fillRect(rect(), card);
        const int inset = width() < 320 ? 16 : 24;
        QFont title = font(); title.setPointSizeF(font().pointSizeF() * 1.2); title.setBold(true);
        QFont caption = font(); caption.setPointSizeF(font().pointSizeF() / 1.2);
        const int titleHeight = QFontMetrics(title).lineSpacing() * 2;
        const int captionHeight = QFontMetrics(caption).height();
        const int textHeight = titleHeight + captionHeight + 8;
        // Фасад показывается целиком: подпись занимает собственную полосу и не перекрывает памятник.
        const QSize size = m_cover.size().scaled(QSize(width(), qMax(1, height() - textHeight - 2 * inset)), Qt::KeepAspectRatio);
        painter.drawImage(QRect(QPoint((width() - size.width()) / 2, 0), size), m_cover);
        QLinearGradient shade(0, qMax(0, size.height() - 40), 0, size.height());
        shade.setColorAt(0, Qt::transparent);
        shade.setColorAt(1, card);
        painter.fillRect(QRect(0, qMax(0, size.height() - 40), width(), 40), shade);
        painter.setPen(Qt::white);
        painter.setFont(title);
        const int textTop = size.height() + qMax(inset, (height() - size.height() - textHeight) / 2);
        painter.drawText(QRect(inset, textTop, width() - 2 * inset, titleHeight), Qt::AlignLeft | Qt::AlignVCenter,
                         tr("Усадьба\nМ. Т. Куницыной"));
        painter.setFont(caption);
        painter.setPen(QColor("#D6E1D8"));
        painter.drawText(QRect(inset, textTop + titleHeight + 8, width() - 2 * inset, captionHeight),
                         Qt::AlignLeft | Qt::AlignVCenter, tr("Архангельск · Малые Корелы"));
    } else if (!m_image.isNull()) {
        const QSize size = m_image.size().scaled(this->size() - QSize(24, 24), Qt::KeepAspectRatio);
        const QRect target(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
        painter.drawImage(target, m_image);
    }
}
