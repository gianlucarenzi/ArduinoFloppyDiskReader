#include "vumeterwidget.h"
#include <QPainter>
#include <QLinearGradient>
#include "inc/debugmsg.h" // For debugging

VuMeterWidget::VuMeterWidget(QWidget *parent)
    : QWidget{parent},
      m_decayRate(500), // 500ms for decay to zero
      m_red(255, 0, 0),
      m_orange(255, 165, 0),
      m_yellow(255, 255, 0),
      m_green(0, 255, 0),
      m_peakDecayFactor(0.1f) // Initialize peak decay factor to 0.1f (500ms decay from 1.0)
{
    // Set a fixed height for the widget (each bar 24px wide, total height 48px)
    setFixedHeight(48);
    // Initial minimum width, will be adjusted dynamically based on actual channels
    setMinimumWidth(100);

    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);

    // Setup decay timer
    m_decayTimer.setTimerType(Qt::PreciseTimer);
    m_decayTimer.setInterval(16); // ~60Hz for smooth animation
    connect(&m_decayTimer, &QTimer::timeout, this, &VuMeterWidget::decayLevels);
    m_decayTimer.start();
}

void VuMeterWidget::setAudioLevels(const QVector<float>& levels)
{
    if (levels.isEmpty()) {
        // If no levels are received, start decay process
        startDecay();
        return;
    }

    // Keep decay timer running for smooth continuous updates

    // DebugMsg::print(__func__, "VuMeterWidget::setAudioLevels received:" + levels);

    // Resize internal level vectors if channel count changes
    if (m_currentLevels.size() != levels.size()) {
        m_currentLevels.resize(levels.size());
        m_peakLevels.resize(levels.size());
        m_peakLevels.fill(0.0f); // Reset peaks for new channel count
        // Adjust minimum width based on new channel count
        int barSpacing = 2;
        int barWidth = 24;
        setMinimumWidth(levels.size() * barWidth + (levels.size() + 1) * barSpacing);
    }

    for (int i = 0; i < levels.size(); ++i) {
        float level = qBound(0.0f, levels[i], 1.0f);
        m_currentLevels[i] = level;
        // Update peak level if current level is higher
        if (level > m_peakLevels[i]) {
            m_peakLevels[i] = level;
        }
    }
    update(); // Request a repaint
}

void VuMeterWidget::resetLevels()
{
    m_currentLevels.fill(0.0f);
    m_peakLevels.fill(0.0f);
    update();
}

void VuMeterWidget::startDecay()
{
    if (!m_decayTimer.isActive()) {
        m_decayTimer.start();
    }
}

void VuMeterWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_currentLevels.isEmpty()) {
        return;
    }

    int numChannels = m_currentLevels.size();
    int barSpacing = 2; // Space between bars
    int barWidth = 24; // Fixed width for each bar as per user request

    // Adjust barWidth if widget is too narrow, but maintain fixed total width if possible
    int totalWidthForBars = numChannels * barWidth + (numChannels + 1) * barSpacing;
    if (width() < totalWidthForBars) {
        barWidth = (width() - (numChannels + 1) * barSpacing) / numChannels;
        if (barWidth <= 0) return;
    }

    for (int i = 0; i < numChannels; ++i) {
        float level = m_currentLevels[i];
        float peakLevel = m_peakLevels[i]; // Use individual peak level

        int x = barSpacing + i * (barWidth + barSpacing);
        int currentBarHeight = static_cast<int>(level * height());

        // Draw the full-height gradient as the background for the bar
        QLinearGradient gradient(x, height(), x, 0); // Vertical gradient from bottom to top
        gradient.setColorAt(0.0, m_green);
        gradient.setColorAt(0.25, m_yellow);
        gradient.setColorAt(0.5, m_orange);
        gradient.setColorAt(0.75, m_red);

        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        painter.drawRect(x, height() - currentBarHeight, barWidth, currentBarHeight);
    }
}

void VuMeterWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    // The width of the widget should be at least enough for the current number of bars
    if (m_currentLevels.size() > 0) {
        int barSpacing = 2;
        int barWidth = 24;
        int requiredWidth = m_currentLevels.size() * barWidth + (m_currentLevels.size() + 1) * barSpacing;
        if (width() < requiredWidth) {
            setMinimumWidth(requiredWidth);
        }
    }
}

void VuMeterWidget::decayLevels()
{
    bool changed = false;
    float decayFactor = 0.05f; // Adjust this for faster/slower decay per tick

    for (int i = 0; i < m_currentLevels.size(); ++i) { // Decay current levels
        if (m_currentLevels[i] > 0.0f) {
            m_currentLevels[i] -= decayFactor;
            if (m_currentLevels[i] < 0.0f) {
                m_currentLevels[i] = 0.0f;
            }
            changed = true;
        }
    }

    for (int i = 0; i < m_peakLevels.size(); ++i) { // Decay peak levels
        if (m_peakLevels[i] > 0.0f) {
            m_peakLevels[i] -= m_peakDecayFactor; // Use the new peak decay factor
            if (m_peakLevels[i] < 0.0f) {
                m_peakLevels[i] = 0.0f;
            }
            changed = true;
        }
    }

    if (changed) {
        update(); // Request repaint if levels have changed
    }
}
