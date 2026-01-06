#ifndef VUMETERWIDGET_H
#define VUMETERWIDGET_H

#include <QWidget>
#include <QVector>
#include <QTimer>
#include <QColor>

class VuMeterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VuMeterWidget(QWidget *parent = nullptr);

public slots:
    void setAudioLevels(const QVector<float>& levels);
    void resetLevels();
    void startDecay();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QVector<float> m_currentLevels;
    QVector<float> m_peakLevels; // For peak hold, if desired
    QTimer m_decayTimer;
    int m_decayRate; // Milliseconds for decay to zero
    QColor m_red;
    QColor m_orange;
    QColor m_yellow;
    QColor m_green;
    float m_peakDecayFactor; // Separate decay factor for peak levels

    void decayLevels();
};

#endif // VUMETERWIDGET_H
