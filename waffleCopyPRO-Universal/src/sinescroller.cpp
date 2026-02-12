#include "sinescroller.h"
#include <QPainter>
#include <QPaintEvent>
#include <QFontMetrics>
#include <cmath>

const int SineScroller::SINE_TABLE[] = { 0, -1, -2, -3, -4, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };
const int SineScroller::SINE_TABLE_SIZE = sizeof(SINE_TABLE) / sizeof(SINE_TABLE[0]);

SineScroller::SineScroller(QWidget* parent)
    : QWidget(parent)
    , m_scrollPosition(0)
    , m_scrollMode(CoarseMode)
    , m_scrollTimer(new QTimer(this))
    , m_textColor(Qt::white)
    , m_sineIndex(0)
    , m_baseY(0)
{
    m_scrollTimer->setInterval(60);
    m_scrollTimer->setSingleShot(false);
    connect(m_scrollTimer, &QTimer::timeout, this, &SineScroller::doScroll);
    
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::transparent);
    setPalette(pal);
}

SineScroller::~SineScroller()
{
    if (m_scrollTimer) {
        m_scrollTimer->stop();
        delete m_scrollTimer;
    }
}

void SineScroller::setText(const QString& text)
{
    m_fullText = text;
    m_scrollPosition = 0;
    update();
}

void SineScroller::setScrollMode(ScrollMode mode)
{
    m_scrollMode = mode;
    update();
}

void SineScroller::setTextColor(const QColor& color)
{
    m_textColor = color;
    update();
}

void SineScroller::setInterval(int msec)
{
    m_scrollTimer->setInterval(msec);
}

void SineScroller::startScroll()
{
    m_scrollTimer->start();
}

void SineScroller::stopScroll()
{
    m_scrollTimer->stop();
}

void SineScroller::doScroll()
{
    m_scrollPosition++;
    if (m_scrollPosition >= m_fullText.length()) {
        m_scrollPosition = 0;
    }
    
    m_sineIndex++;
    if (m_sineIndex >= SINE_TABLE_SIZE) {
        m_sineIndex = 0;
    }
    
    update();
}

void SineScroller::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(font());
    
    if (m_scrollMode == CoarseMode) {
        paintCoarseMode(painter);
    } else {
        paintFineMode(painter);
    }
}

void SineScroller::paintCoarseMode(QPainter& painter)
{
    if (m_fullText.isEmpty()) return;
    
    QString visibleText = m_fullText.mid(m_scrollPosition);
    QFontMetrics fm(font());
    
    painter.setPen(m_textColor);
    painter.setClipping(false);
    
    int yOffset = SINE_TABLE[m_sineIndex];
    // Position text baseline to account for ascent and sine wave movement
    int baseY = height() / 2 + fm.ascent() / 2;
    
    painter.drawText(0, baseY + yOffset, visibleText);
}

void SineScroller::paintFineMode(QPainter& painter)
{
    if (m_fullText.isEmpty()) return;
    
    QString visibleText = m_fullText.mid(m_scrollPosition);
    QFontMetrics fm(font());
    
    painter.setPen(m_textColor);
    painter.setClipping(false);
    
    // Position text baseline to account for ascent and sine wave movement
    int baseY = height() / 2 + fm.ascent() / 2;
    int x = 0;
    
    // Calculate sine wave parameters for smooth character animation
    const double sineAmplitude = 5;   // Further reduced amplitude to fit in 32px area
    const double sineFrequency = 0.15;  // How many cycles per character (wavelength)
    const double waveSpeed = 0.2;       // Speed at which the wave travels
    
    for (int i = 0; i < visibleText.length() && x < width(); ++i) {
        QChar ch = visibleText[i];
        
        // Calculate Y offset based on character position
        // The wave travels by using m_scrollPosition to offset the phase
        // This creates a traveling sine wave effect where characters move along the wave
        double phase = i * sineFrequency - m_scrollPosition * waveSpeed;
        int yOffset = static_cast<int>(sineAmplitude * std::sin(phase));
        
        painter.drawText(x, baseY + yOffset, QString(ch));
        
        x += fm.horizontalAdvance(ch);
    }
}
