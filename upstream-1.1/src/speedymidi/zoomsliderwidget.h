/***************************************************************************
 *  zoomsliderwidget.h - MainWindow Subwidget: Zoom Slider
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

#ifndef ZOOMSLIDERWIDGET_H
#define ZOOMSLIDERWIDGET_H

#include "global.h"
#include <QWidget>
#include <QLabel>

class ZoomGlassLabel : public QLabel
{
    Q_OBJECT
public:
    enum ZoomType { In, Out };

    ZoomGlassLabel(ZoomSliderWidget* parent, ZoomType type);

protected:
    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void timerEvent(QTimerEvent* event);
    virtual void wheelEvent(QWheelEvent* e);

    ZoomType type;
    int holdTriggerTimerId;
};

class ZoomSliderWidget : public QWidget
{
    Q_OBJECT
public:
    ZoomSliderWidget(Qt::Orientation orientation, QWidget* parentWidget);
    void zoomIconTriggered(ZoomGlassLabel::ZoomType type);
    void zoomKeyTriggered(ZoomGlassLabel::ZoomType type);

    void setValue(int value);
    QSlider* getSlider() const { return slider; }

signals:
    void valueChanged(int);

protected:
    Qt::Orientation orientation;

    // Subwidgets
    QSlider* slider;
    ZoomGlassLabel* zoomInButton;
    ZoomGlassLabel* zoomOutButton;
};

#endif // ZOOMSLIDERWIDGET_H
