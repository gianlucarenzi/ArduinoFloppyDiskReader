#include "vumeterwidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>

VUMeterWidget::VUMeterWidget(QWidget *parent) : QWidget(parent)
{
    setAutoFillBackground(false);
}

void VUMeterWidget::setLevels(const QVector<float> &levels)
{
    m_levels = levels;
    update();
}

void VUMeterWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_levels.isEmpty()) {
        return;
    }

    int num_bars = m_levels.size();
    if (num_bars == 0) return;

    int spacing = 2;
    int bar_width = (width() - (num_bars - 1) * spacing) / num_bars;
    int total_width = num_bars * bar_width + (num_bars - 1) * spacing;
    int x_offset = (width() - total_width) / 2;

    for (int i = 0; i < num_bars; ++i) {
        int bar_height = static_cast<int>(m_levels[i] * height());
        if (bar_height > height()) {
            bar_height = height();
        }

        QRect bar_rect(x_offset + i * (bar_width + spacing), height() - bar_height, bar_width, bar_height);

        QLinearGradient gradient(bar_rect.topLeft(), bar_rect.bottomLeft());
        gradient.setColorAt(0.0, Qt::red);
        gradient.setColorAt(0.2, Qt::yellow);
        gradient.setColorAt(0.6, Qt::green);

        painter.fillRect(bar_rect, gradient);
    }
}
