#ifndef CLICKLABEL_H
#define CLICKLABEL_H

#include <QLabel>
#include <QWidget>
#include <Qt>

class ClickLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickLabel(QWidget* parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~ClickLabel();

signals:
    void emitClick();

private slots:
    void clicked(void);

protected:
    void mouseReleaseEvent(QMouseEvent* event);

};

#endif // CLICKLABEL_H
