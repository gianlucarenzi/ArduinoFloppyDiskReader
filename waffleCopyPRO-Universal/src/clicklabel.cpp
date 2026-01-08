#include <QLabel>
#include "inc/debugmsg.h"
#include "clicklabel.h"

ClickLabel::ClickLabel(QWidget* parent, Qt::WindowFlags f)
    : QLabel(parent)
{
    Q_UNUSED(f);
}

ClickLabel::~ClickLabel() {}

void ClickLabel::clicked(void)
{
    DebugMsg::print(__func__, "CLICKED SLOT");
}

void ClickLabel::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    DebugMsg::print(__func__, "MOUSE RELEASE EVENT" + this->objectName());
    emit emitClick();
}
