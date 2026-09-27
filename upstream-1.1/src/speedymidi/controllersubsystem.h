/***************************************************************************
 *  controllersubsystem.h - Base Class for any Subsystem of Main Controller
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

#ifndef CONTROLLERSUBSYSTEM_H
#define CONTROLLERSUBSYSTEM_H

#include "global.h"
#include "controller.h"
#include "view.h"

class ControllerSubsystem : public QObject
{
    Q_OBJECT
public:
    ControllerSubsystem(Controller* controller);
    virtual ~ControllerSubsystem() { }

    // Selectors
    const EditorState&         getEditorState()         const;
    const VolatileEditorState& getVolatileEditorState() const;
    const DocRoot*             getDocRoot()             const { return docRoot; }
    Controller*                getController()          const { return controller; }
    MainWindow*                getMainWindow()          const { return mainWindow; }
    Settings*                  getSettings()            const { return settings; }

    // return value indicates whether event was handled
    virtual bool keyPressEvent(QKeyEvent* event) { UNUSED(event); return false; }
    virtual void keyReleaseEvent(QKeyEvent* event) { UNUSED(event); }
    virtual bool mousePressEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone) { UNUSED(event);UNUSED(mouseZone); return false; }
    virtual void mouseMoveEvent(QMouseEvent* event) { UNUSED(event); }
    virtual void mouseReleaseEvent(QMouseEvent* event) { UNUSED(event); }
    virtual bool mouseDoubleClickEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone) { UNUSED(event);UNUSED(mouseZone); return false; }
    virtual bool wheelEvent(QWheelEvent* event) { UNUSED(event); return false; }
    virtual bool closeEvent() { return true; }  // return true if event should be accepted
    virtual void cancelInputCapture();
    virtual void stateChanged() { }
    virtual void cancelInterruptibleState() { }

    virtual void resetWheelAccumulator() { mouseWheelAccu_8thsOfDegrees=0; }

protected:
    // ----------------------------------------------------------------------------------------------------
    // helper functions for interaction with controller

    void beginMacro(const QString& description, const EditorRange& scrollToRangeUndo);
    void addCommand(EditorUndoCommand* cmd);
    void endMacro(const EditorState& newState, const EditorRange& scrollToRangeRedo, bool noScrollDuringMacroCreation = false);
    bool isInMacro() const;

    void registerActionHandler(QAction* action, const char* method, Controller::ActionGuards guards);
    void unregisterActionHandler(const char* method);
    void setInputCapture();
    void releaseInputCapture();
    bool capturingInput();
    void setInterruptibleState();
    void releaseInterruptibleState();
public:
    void applyStateAndUpdate(const EditorState& state);
    void applyVolatileStateAndUpdate(const VolatileEditorState& volatileState);
protected:

    // ----------------------------------------------------------------------------------------------------
    // mouse wheel support

    int mouseWheelAccu_8thsOfDegrees;

    // ----------------------------------------------------------------------------------------------------
    // Object links
    Controller* controller;
    const DocRoot* docRoot;
    const View* view;
    MainWindow* mainWindow;
    const Ui::MainWindow* ui;
    SpeedyMidiApp* app;
    Settings* settings;
};

#endif // CONTROLLERSUBSYSTEM_H
