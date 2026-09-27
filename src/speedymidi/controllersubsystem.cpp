/***************************************************************************
 *  controllersubsystem.cpp - Base Class for any Subsystem of Main Controller
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

#include "controllersubsystem.h"
#include "mainwindow.h"
#include "speedymidiapp.h"

ControllerSubsystem::ControllerSubsystem(Controller* controller)
        : QObject(controller)
{
    this->controller=controller;
    this->docRoot=controller->getDocRoot();
    this->view=controller->getView();
    this->mainWindow=controller->getMainWindow();
    this->ui=controller->getMainWindowUI();
    this->app=controller->getApp();
    this->settings=controller->getApp()->getSettings();

    mouseWheelAccu_8thsOfDegrees=0;
}

const EditorState& ControllerSubsystem::getEditorState() const
{
    return controller->getEditorState();
}

const VolatileEditorState& ControllerSubsystem::getVolatileEditorState() const
{
    return controller->getVolatileEditorState();
}

void ControllerSubsystem::cancelInputCapture()
{
    // derived classes must cleanly cancel their input capture states
    //  (e.g. operate like pressing the Escape key)
    //  and call releaseInputCapture().

    releaseInputCapture();
}

// _________________________ helper functions for interaction with controller __________________________

void ControllerSubsystem::beginMacro(const QString& description, const EditorRange& scrollToRangeUndo)
{
    controller->csBeginMacro(description, scrollToRangeUndo);
}

void ControllerSubsystem::addCommand(EditorUndoCommand* cmd)
{
    controller->csAddCommand(cmd);
}

void ControllerSubsystem::endMacro(const EditorState& newState, const EditorRange& scrollToRangeRedo, bool noScrollDuringMacroCreation)
{
    controller->csEndMacro(newState, scrollToRangeRedo, noScrollDuringMacroCreation);
}

bool ControllerSubsystem::isInMacro() const
{
    return controller->isInMacro();
}

void ControllerSubsystem::registerActionHandler(QAction* action, const char* method, Controller::ActionGuards guards)
{
    controller->csRegisterActionHandler(action, this, method, guards);
}

void ControllerSubsystem::unregisterActionHandler(const char* method)
{
    controller->csUnregisterActionHandler(this, method);
}

void ControllerSubsystem::setInputCapture()
{
    controller->csRegisterInputCapture(this);
}

void ControllerSubsystem::releaseInputCapture()
{
    controller->csUnregisterInputCapture(this);
}

bool ControllerSubsystem::capturingInput()
{
    return controller->csIsInputCapturedBy(this);
}

void ControllerSubsystem::setInterruptibleState()
{
    controller->csRegisterInterruptibleState(this);
}

void ControllerSubsystem::releaseInterruptibleState()
{
    controller->csUnregisterInterruptibleState(this);
}

void ControllerSubsystem::applyStateAndUpdate(const EditorState& state)
{
    controller->csApplyStateAndUpdate(state);
}

void ControllerSubsystem::applyVolatileStateAndUpdate(const VolatileEditorState& volatileState)
{
    controller->csApplyVolatileStateAndUpdate(volatileState);
}
