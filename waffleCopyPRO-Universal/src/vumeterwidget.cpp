#include "vumeterwidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <QDebug>
#include <algorithm> // For std::max

VUMeterWidget::VUMeterWidget(QWidget *parent) : QWidget(parent)
{
    setAutoFillBackground(false);
    m_decayTimer = new QTimer(this);
    connect(m_decayTimer, &QTimer::timeout, this, &VUMeterWidget::decayLevels);
    m_decayTimer->setInterval(30); // Update every 30ms for smooth animation
}

void VUMeterWidget::setLevels(const QVector<float> &levels)
{
    m_targetLevels = levels;

    if (m_currentLevels.size() != m_targetLevels.size()) {
        m_currentLevels.resize(m_targetLevels.size());
        std::fill(m_currentLevels.begin(), m_currentLevels.end(), 0.0f);
    }

    if (!m_decayTimer->isActive()) {
        m_decayTimer->start();
    }
}

#define DURATION_OF_DECAY_IN_MSEC   500.0f

void VUMeterWidget::decayLevels()
{
    bool all_levels_are_effectively_zero = true;
    float decay_step = 1.0f / (DURATION_OF_DECAY_IN_MSEC / m_decayTimer->interval()); // Decay to zero in fixed time

    for (int i = 0; i < m_currentLevels.size(); ++i) {
        if (m_currentLevels[i] < m_targetLevels[i]) {
            // If new target is higher, jump up to it
            m_currentLevels[i] = m_targetLevels[i];
        } else if (m_currentLevels[i] > m_targetLevels[i]) {
            // If current is higher than target, decay towards target
            m_currentLevels[i] -= decay_step;
            if (m_currentLevels[i] < m_targetLevels[i]) { // Don't decay below target
                m_currentLevels[i] = m_targetLevels[i];
            }
        }
        // Ensure it doesn't go below zero due to floating point inaccuracies
        if (m_currentLevels[i] < 0.001f) { // Consider anything below 0.001 as zero
            m_currentLevels[i] = 0.0f;
        }

        if (m_currentLevels[i] > 0.0f) {
            all_levels_are_effectively_zero = false;
        }
    }

    update();

    if (all_levels_are_effectively_zero) { // If all current levels are zero
        m_decayTimer->stop();
        emit allLevelsZero();
    }
}

void VUMeterWidget::resetAndDecay()
{
    if (m_targetLevels.size() != m_currentLevels.size()) {
        m_targetLevels.resize(m_currentLevels.size());
    }
    std::fill(m_targetLevels.begin(), m_targetLevels.end(), 0.0f);
    if (!m_decayTimer->isActive()) {
        m_decayTimer->start();
    }
}

void VUMeterWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_currentLevels.isEmpty()) {
        return;
    }

    int num_bars = m_currentLevels.size();
    if (num_bars == 0) return;

    int spacing = 2;
    int bar_width = (width() - (num_bars - 1) * spacing) / num_bars;
    if (bar_width < 1) bar_width = 1; // Ensure bar has at least 1 pixel width
    int total_width = num_bars * bar_width + (num_bars - 1) * spacing;
    int x_offset = (width() - total_width) / 2;

    for (int i = 0; i < num_bars; ++i) {
        int bar_height = static_cast<int>(m_currentLevels[i] * height());
        if (bar_height > height()) {
            bar_height = height();
        }
        if (bar_height < 0) { // Ensure height is not negative
            bar_height = 0;
        }

        QRect bar_rect(x_offset + i * (bar_width + spacing), height() - bar_height, bar_width, bar_height);

        QLinearGradient gradient(bar_rect.topLeft(), bar_rect.bottomLeft());
        gradient.setColorAt(0.0, Qt::red);
        gradient.setColorAt(0.2, Qt::yellow);
        gradient.setColorAt(0.4, Qt::green);

        painter.fillRect(bar_rect, gradient);
    }
}
