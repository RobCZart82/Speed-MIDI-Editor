/***************************************************************************
 *  zoomsliderwidget.cpp - MainWindow Subwidget: Zoom Slider
 *
 *  Copyright 2010-2013 Holger Hoffmann
 ***************************************************************************

   This file is part of "Speedy MIDI".

   "Speedy MIDI" is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   "Speedy MIDI" is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with "Speedy MIDI". If not, see <http://www.gnu.org/licenses/>.
*/

#include "zoomsliderwidget.h"
#include <QtGui>
#include <QtWidgets>
#include <QWheelEvent>

ZoomGlassLabel::ZoomGlassLabel(ZoomSliderWidget* parent, ZoomType type)
        : QLabel(parent)
{
    this->type=type;
    holdTriggerTimerId=0;

    const QString resourceName=QStringLiteral(":/images/flat/zoom-%1.svg")
            .arg(type == In ? QStringLiteral("in") : QStringLiteral("out"));
    // Keep the zoom buttons visually balanced with the slider thumb and track.
    setPixmap(QIcon(resourceName).pixmap(QSize(16, 16)));
}

void ZoomGlassLabel::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
    {
        ((ZoomSliderWidget*)parent())->zoomIconTriggered(type);
        holdTriggerTimerId=startTimer(ZOOM_GLASS_LABEL_HOLD_TRIGGER_INTERVAL);
    }
}

void ZoomGlassLabel::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
    {
        if(holdTriggerTimerId)
        {
            killTimer(holdTriggerTimerId);
            holdTriggerTimerId=0;
        }
    }
}

void ZoomGlassLabel::timerEvent(QTimerEvent* event)
{
    if(event->timerId() != holdTriggerTimerId)return;
    ((ZoomSliderWidget*)parent())->zoomIconTriggered(type);
}

void ZoomGlassLabel::wheelEvent(QWheelEvent* e)
{
    // no matter on which subwidget the wheel event occurs, always reach it to the slider
    qApp->sendEvent(((ZoomSliderWidget*)parent())->getSlider(),e);
}

ZoomSliderWidget::ZoomSliderWidget(Qt::Orientation orientation, QWidget* parentWidget)
        : QWidget(parentWidget)
{
    this->orientation=orientation;

    // Subwidgets
    slider=new QSlider(orientation, this);
    slider->setFocusPolicy(Qt::NoFocus);
    slider->setMinimum(0);
    slider->setMaximum(ZOOM_SLIDER_WIDGET_MAX_VALUE);
    slider->setPageStep(10);

    connect(slider, SIGNAL(valueChanged(int)), this, SIGNAL(valueChanged(int)));

    zoomInButton=new ZoomGlassLabel(this,ZoomGlassLabel::In);
    zoomInButton->setFocusPolicy(Qt::NoFocus);
    zoomInButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    zoomInButton->setMinimumSize(ZOOM_GLASS_LABEL_MINIMUM_SIZE, ZOOM_GLASS_LABEL_MINIMUM_SIZE);
    zoomInButton->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);

    zoomOutButton=new ZoomGlassLabel(this,ZoomGlassLabel::Out);
    zoomOutButton->setFocusPolicy(Qt::NoFocus);
    zoomOutButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    zoomOutButton->setMinimumSize(ZOOM_GLASS_LABEL_MINIMUM_SIZE, ZOOM_GLASS_LABEL_MINIMUM_SIZE);
    zoomOutButton->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);

    QBoxLayout* boxLayout =
            new QBoxLayout(orientation == Qt::Horizontal ? QBoxLayout::RightToLeft : QBoxLayout::TopToBottom,
                           this);

    Qt::Alignment subWidgetAlignment=orientation == Qt::Horizontal ? Qt::AlignVCenter : Qt::AlignHCenter;

    boxLayout->addWidget(zoomInButton,0,subWidgetAlignment);
    boxLayout->addWidget(slider,0,subWidgetAlignment);
    boxLayout->addWidget(zoomOutButton,0,subWidgetAlignment);

    boxLayout->setContentsMargins(0,0,0,0);
    boxLayout->setSpacing(0);
}

void ZoomSliderWidget::zoomIconTriggered(ZoomGlassLabel::ZoomType type)
{
    if(!isEnabled())return;

    if(type == ZoomGlassLabel::In)
    {
        slider->setValue(slider->value() + ZOOM_SLIDER_WIDGET_LABEL_CLICK_STEP);
    }
    else
    {
        slider->setValue(slider->value() - ZOOM_SLIDER_WIDGET_LABEL_CLICK_STEP);
    }
}

void ZoomSliderWidget::zoomKeyTriggered(ZoomGlassLabel::ZoomType type)
{
    if(!isEnabled())return;

    if(type == ZoomGlassLabel::In)
    {
        slider->setValue(slider->value() + ZOOM_SLIDER_WIDGET_KEY_PRESSED_STEP);
    }
    else
    {
        slider->setValue(slider->value() - ZOOM_SLIDER_WIDGET_KEY_PRESSED_STEP);
    }
}

void ZoomSliderWidget::setValue(int value)
{
    if(isEnabled())slider->setValue(value);
}
