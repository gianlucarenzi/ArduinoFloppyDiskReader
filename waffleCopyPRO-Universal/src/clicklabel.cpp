#include <QLabel>
#include <QDebug>
#include "clicklabel.h"

ClickLabel::ClickLabel(QWidget* parent, Qt::WindowFlags f)
    : QLabel(parent)
{
    Q_UNUSED(f);
}

ClickLabel::~ClickLabel() {}

void ClickLabel::clicked(void)
{
    qDebug() << "CLICKED SLOT";
}

void ClickLabel::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    qDebug() << "MOUSE RELEASE EVENT" << this->objectName();
    emit emitClick();
}
