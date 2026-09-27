/***************************************************************************
 *  mousepianodockwidget.h - Floating Dock Widget for MousePianoWidget
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

#ifndef MOUSEPIANODOCKWIDGET_H
#define MOUSEPIANODOCKWIDGET_H

#include "global.h"
#include <QDockWidget>

class MousePianoDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    MousePianoDockWidget(MainWindow* parent);

    bool onceShown;

protected:
    virtual bool event(QEvent *event);
    virtual void keyPressEvent(QKeyEvent* event);
    virtual void keyReleaseEvent(QKeyEvent* event);

    void retranslateUi();
};

#endif // MOUSEPIANODOCKWIDGET_H
