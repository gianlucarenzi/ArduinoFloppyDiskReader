#ifndef VUMETERWIDGET_H
#define VUMETERWIDGET_H

#include <QWidget>
#include <QVector>

class VUMeterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VUMeterWidget(QWidget *parent = nullptr);
    void setLevels(const QVector<float> &levels);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<float> m_levels;
};

#endif // VUMETERWIDGET_H
