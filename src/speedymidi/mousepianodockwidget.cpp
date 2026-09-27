/***************************************************************************
 *  mousepianodockwidget.cpp - Floating Dock Widget for MousePianoWidget
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

#include "mousepianodockwidget.h"
#include "mainwindow.h"
#include "view.h"
#include "speedymidiapp.h"
#include <QAction>

MousePianoDockWidget::MousePianoDockWidget(MainWindow* parent)
            : QDockWidget(parent)
{
    onceShown=false;
    retranslateUi();
}

bool MousePianoDockWidget::event(QEvent *event)
{
    switch(event->type())
    {
    case QEvent::MouseButtonDblClick:
    case QEvent::NonClientAreaMouseButtonDblClick:
        // ignore any double clicks to disable docking completely
        event->ignore();
        return true;
    case QEvent::Close:
        event->ignore();
        ((SpeedyMidiApp*)qApp)->getActionView_MousePiano()->toggle();
        return true;
    case QEvent::LanguageChange:
        retranslateUi();
        return QDockWidget::event(event);
    default:
        return QDockWidget::event(event);
    }
}

void MousePianoDockWidget::keyPressEvent(QKeyEvent* event)
{
    MainWindow* currentMainWindow=qobject_cast<MainWindow*>(parentWidget());
    if(currentMainWindow != NULL)
        qApp->notify(currentMainWindow->getView(), event);

    QDockWidget::keyPressEvent(event);
}

void MousePianoDockWidget::keyReleaseEvent(QKeyEvent* event)
{
    MainWindow* currentMainWindow=qobject_cast<MainWindow*>(parentWidget());
    if(currentMainWindow != NULL)
        qApp->notify(currentMainWindow->getView(), event);

    QDockWidget::keyReleaseEvent(event);
}

void MousePianoDockWidget::retranslateUi()
{
    setWindowTitle(tr("Mouse piano"));
}
