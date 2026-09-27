/***************************************************************************
 *  controller.h - Main Controller
 *                 Owner of all Controller Subsystems
 *                 (cf. Model–View–Controller Architecture)
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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "global.h"
#include "editorstate.h"

class Controller : public QObject
{
    Q_OBJECT
public:
    Controller(MainWindow* mainWindow, DocRoot* docRoot, View* view, EditorState startupState);
    ~Controller();

    void cancelInterruptibleStates();
    void setClean();    // called when file was saved successfully
    void restoreMouseCursor();
    void retranslateUi();

    ControllerSubsystem* getSubsystemByClassName(const char* className) const;

    enum ActionGuard
    {
         AllowAlways = 0x0,
         DisallowWhenInputCaptured = 0x1,
         CancelInterruptibleStates = 0x2,
         MustHaveTracks = 0x4,
         CanUndo = 0x8,
         CanRedo = 0x10
    };
    Q_DECLARE_FLAGS(ActionGuards, ActionGuard)

    // const selectors
    const DocRoot*        getDocRoot()                     const { return docRoot; }
    const View*           getView()                        const { return view; }
    const EditorState&    getEditorState()                 const { return editorState; }
    const VolatileEditorState& getVolatileEditorState()    const { return volatileEditorState; }
    const Ui::MainWindow* getMainWindowUI()                const;

    // non-const selectors
    MainWindow*           getMainWindow()  const { return mainWindow; }
    SpeedyMidiApp*        getApp()         const;
    bool isClean();
    bool isInMacro()                       const { return macroNestingCounter >= 1; }
    bool isKeyboardModifierCommaPressed()  const { return keyboardModifierCommaPressed; }
    QAction* getCurrentSendingAction()     const { return currentSenderAction; }

    // public interface for view class
    bool viewKeyPressEvent(QKeyEvent* event);   // event handlers return true if event was handled
    bool viewKeyReleaseEvent(QKeyEvent* event);
    bool viewMousePressEvent(QMouseEvent* event);
    bool viewMouseMoveEvent(QMouseEvent* event);
    bool viewMouseReleaseEvent(QMouseEvent* event);
    bool viewMouseDoubleClickEvent(QMouseEvent* event);
    bool viewWheelEvent(QWheelEvent* event);
    bool viewCloseEvent();  // return true if event should be accepted
    void viewMenuAboutToShow();
    void viewInitiallyResized();

    // public interface for controller subsystems
    void csRegisterActionHandler(QAction* action, QObject* receiver, const char* method, ActionGuards guards);
    void csUnregisterActionHandler(QObject* receiver, const char* method=NULL);  // set method to NULL for all handlers for this receiver
    void csRegisterInputCapture(ControllerSubsystem* cs);
    void csUnregisterInputCapture(ControllerSubsystem* cs);
    bool csIsInputCaptured();
    bool csIsInputCapturedBy(ControllerSubsystem* cs);
    void csRegisterInterruptibleState(ControllerSubsystem* cs);
    void csUnregisterInterruptibleState(ControllerSubsystem* cs);
    void csApplyStateAndUpdate(const EditorState& newState);
    void csApplyVolatileStateAndUpdate(const VolatileEditorState& newVolatileState);
    void csBeginMacro(const QString& description, const EditorRange& scrollToRangeUndo);
    void csAddCommand(EditorUndoCommand* cmd);
    void csEndMacro(const EditorState& newState, const EditorRange& scrollToRangeRedo, bool noScrollDuringMacroCreation = false);

    // public interface for command classes
    void commandApplyUndoCommandStateAndUpdate(const EditorState& state, const EditorRange& scrollToRange);
    void commandCollectDocumentModificationRange(const EditorRange& modifiedRange);

protected:
    // --------------------------------------------------------------------------------------------------------
    // Subsystems, subsystem states, and actions
    QList<ControllerSubsystem*> subsystemList;
    QList<ControllerSubsystem*> inputCaptureSubsystemList;
    QList<ControllerSubsystem*> interruptibleStateSubsystemList;

    void createSubsystems();
    void removeSubsystem(ControllerSubsystem* cs);

    struct RegisteredAction
    {
        QAction* action;
        QObject* receiver;
        QByteArray method;
        ActionGuards guards;
    };

    QList<RegisteredAction> registeredActionList;
    QAction* currentSenderAction;

    void updateRegisteredActions();
    bool checkActionGuards(ActionGuards guards);
    QList<ControllerSubsystem*> getInputEventTraversalOrder();

    // --------------------------------------------------------------------------------------------------------
    // input
    bool keyboardModifierCommaPressed;

    // --------------------------------------------------------------------------------------------------------
    // The editor states
    EditorState editorState;
    VolatileEditorState volatileEditorState;  // additional volatile/dynamic editor state, not serialized

    // --------------------------------------------------------------------------------------------------------
    // Undo Stack

    QUndoStack* undoStack;
    EditorRange documentModificationsUnionRange;

    int macroNestingCounter;
    bool macroUndoCommandCreated;
    QString macroDescription;
    EditorState macroEditorStateBefore;
    EditorRange macroScrollToRangeUndo;
    bool disableScrollingDuringMacroCreation;

    // --------------------------------------------------------------------------------------------------------
    // Object links
    DocRoot* docRoot;
    View* view;
    MainWindow* mainWindow;

protected slots:
    void editorActionTriggered();

    void undoStack_undoTextChanged(const QString& undoText);
    void undoStack_redoTextChanged(const QString& redoText);
    void undoStackCleanChanged(bool clean);
    void undoStackCanUndoChanged(bool canUndo);
    void undoStackCanRedoChanged(bool canRedo);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Controller::ActionGuards)

#endif // CONTROLLER_H
