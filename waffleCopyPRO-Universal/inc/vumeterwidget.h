#ifndef VUMETERWIDGET_H
#define VUMETERWIDGET_H

#include <QWidget>
#include <QVector>
#include <QTimer>

class VUMeterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VUMeterWidget(QWidget *parent = nullptr);
    void setLevels(const QVector<float> &levels);

public slots:
    void resetAndDecay();

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
    void allLevelsZero();

private slots:
    void decayLevels();

private:
    QVector<float> m_currentLevels;
    QVector<float> m_targetLevels;
    QTimer *m_decayTimer;
};

#endif // VUMETERWIDGET_H
