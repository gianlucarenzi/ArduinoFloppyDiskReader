#include "clicklabel.h"

ClickLabel::ClickLabel(QWidget* parent, Qt::WindowFlags f)
    : QLabel(parent) {
    // Avoid gcc warning
    f = f;
}

ClickLabel::~ClickLabel() {}

void ClickLabel::mousePressEvent(QMouseEvent* event) {
    // Avoid gcc warning
    event = event;
    emit clicked();
}
