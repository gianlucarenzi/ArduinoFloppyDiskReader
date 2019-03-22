#ifndef CLICKLABEL_H
#define CLICKLABEL_H

#include <QLabel>
#include <QWidget>
#include <Qt>

#ifndef Q_NULLPTR
#define Q_NULLPTR	0
#endif

class ClickLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickLabel(QWidget* parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~ClickLabel();

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event);

};

#endif // CLICKLABEL_H

