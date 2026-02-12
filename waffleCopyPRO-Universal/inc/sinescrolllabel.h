#ifndef SINESCROLLLABEL_H
#define SINESCROLLLABEL_H

#include <QWidget>
#include <QString>
#include <QTimer>
#include <QFont>
#include <QColor>

class SineScrollLabel : public QWidget {
    Q_OBJECT

public:
    enum ScrollMode {
        CoarseMode,    // Entire widget moves (original behavior)
        FineMode       // Each character follows sine wave
    };

    explicit SineScrollLabel(QWidget* parent = nullptr);
    ~SineScrollLabel();

    void setText(const QString& text);
    QString text() const { return m_fullText; }
    
    void setScrollMode(ScrollMode mode);
    ScrollMode scrollMode() const { return m_scrollMode; }
    
    void setTextColor(const QColor& color);
    void setInterval(int msec);
    void startScroll();
    void stopScroll();

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void doScroll();

private:
    QString m_fullText;
    int m_scrollPosition;
    ScrollMode m_scrollMode;
    QTimer* m_scrollTimer;
    QColor m_textColor;
    
    static const int SINE_TABLE[];
    static const int SINE_TABLE_SIZE;
    int m_sineIndex;
    int m_baseY;
    
    void paintCoarseMode(QPainter& painter);
    void paintFineMode(QPainter& painter);
};

#endif // SINESCROLLLABEL_H
