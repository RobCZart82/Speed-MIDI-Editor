/***************************************************************************
 *  controller.cpp - Main Controller
 *                   Owner of all Controller Subsystems
 *                   (cf. Model–View–Controller Architecture)
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

#include "controller.h"
#include "controllersubsystem.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "doc_root.h"
#include "commands.h"

#include "cs_file.h"
#include "cs_navigation.h"
#include "cs_clipboard.h"
#include "cs_localmassedit.h"
#include "cs_playback.h"
#include "cs_write.h"
#include "cs_utilities.h"

#include <QUndoStack>

Controller::Controller(MainWindow* mainWindow, DocRoot* docRoot, View* view, EditorState startupState)
        : QObject(mainWindow)
{
    this->mainWindow=mainWindow;
    this->docRoot=docRoot;
    this->view=view;

    currentSenderAction=NULL;

    keyboardModifierCommaPressed=false;

    macroNestingCounter=0;
    macroUndoCommandCreated=false;
    disableScrollingDuringMacroCreation=false;

    mainWindow->setWindowModified(false);
    undoStack=new QUndoStack(this);
    connect(undoStack,SIGNAL(undoTextChanged(QString)), SLOT(undoStack_undoTextChanged(QString)));
    connect(undoStack,SIGNAL(redoTextChanged(QString)), SLOT(undoStack_redoTextChanged(QString)));
    connect(undoStack,SIGNAL(cleanChanged(bool)),       SLOT(undoStackCleanChanged(bool)));
    connect(undoStack,SIGNAL(canUndoChanged(bool)),     SLOT(undoStackCanUndoChanged(bool)));
    connect(undoStack,SIGNAL(canRedoChanged(bool)),     SLOT(undoStackCanRedoChanged(bool)));

    // register undo/redo action handlers
    csRegisterActionHandler(getMainWindowUI()->actionEdit_Undo, undoStack, "undo", DisallowWhenInputCaptured | CancelInterruptibleStates | CanUndo);
    csRegisterActionHandler(getMainWindowUI()->actionEdit_Redo, undoStack, "redo", DisallowWhenInputCaptured | CancelInterruptibleStates | CanRedo);

    createSubsystems();

    // Validate startup state, never trust any data loaded from SMF
    if(!startupState.isValid(docRoot))
        startupState.setStartupDefaultState(docRoot);

    editorState=startupState;

    // connect to view
    view->setController(this, docRoot);
    csApplyStateAndUpdate(startupState);
}

Controller::~Controller()
{
    cancelInterruptibleStates();
    inputCaptureSubsystemList.clear();

    while(!subsystemList.isEmpty())
        removeSubsystem(subsystemList.first());

    csUnregisterActionHandler(undoStack, "undo");
    csUnregisterActionHandler(undoStack, "redo");
    Q_ASSERT(registeredActionList.isEmpty());
}

const Ui::MainWindow* Controller::getMainWindowUI() const
{
    return mainWindow->getUI();
}

SpeedyMidiApp* Controller::getApp() const
{
    return mainWindow->getApp();
}

ControllerSubsystem* Controller::getSubsystemByClassName(const char* className) const
{
    for(int i=0; i < subsystemList.size(); ++i)
    {
        ControllerSubsystem* cs=subsystemList[i];
        if(!strcmp(cs->metaObject()->className(),className))
            return cs;
    }

    return NULL;    // class not found
}

// _____________________________________ Public interface for view _____________________________________

bool Controller::viewKeyPressEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Control:
        return false;   // unhandled event

    case Qt::Key_Comma:
        keyboardModifierCommaPressed=true;
        return true;    // event was handled
    default:
        break;
    }

    // notify capturing subsystems or - if no subsystem currently captures - notify all subsystems
    QList<ControllerSubsystem*> csList=getInputEventTraversalOrder();
    for(int i=0; i < csList.size(); ++i)
    {
        if(csList[i]->keyPressEvent(event))
            return true;  // event was handled
    }
    return false;   // unhandled event
}

bool Controller::viewKeyReleaseEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_Comma:
        keyboardModifierCommaPressed=false;
        return true;    // event was handled
    }

    // notify all subsystems
    for(int i=0; i < subsystemList.size(); ++i)
        subsystemList[i]->keyReleaseEvent(event);

    return true;   // event was handled
}

bool Controller::viewMousePressEvent(QMouseEvent* event)
{
    event->accept();

    // update view cursor shape
    View::MouseZoneResult mouseZone=view->getMouseZone(event->pos());
    view->setCursor(mouseZone.cursor);

//EXTENSION CONTEXT-MENU:show right mouse button context menu -OR-
//EXTENSION NOTE-DRAG:enable direct dragging of notes
//  if(event->button() == Qt::RightButton) ...
//EXTENSION DRAG&DROP: check if clicked on selection border => start drag & drop operation

    // notify capturing subsystems or - if no subsystem currently captures - all subsystems
    QList<ControllerSubsystem*> csList=getInputEventTraversalOrder();
    for(int i=0; i < csList.size(); ++i)
    {
        if(csList[i]->mousePressEvent(event, mouseZone))
            return true;  // event was handled
    }
    return false;   // unhandled event
}

bool Controller::viewMouseMoveEvent(QMouseEvent* event)
{
    event->accept();

    if(event->buttons() == Qt::NoButton)
    {
        // update view cursor shape when no button is pressed
        View::MouseZoneResult mouseZone=view->getMouseZone(event->pos());
        view->setCursor(mouseZone.cursor);
    }

    // notify all subsystems
    for(int i=0; i < subsystemList.size(); ++i)
        subsystemList[i]->mouseMoveEvent(event);

    return true;   // event was handled
}

bool Controller::viewMouseReleaseEvent(QMouseEvent* event)
{
    event->accept();

    // Restore correct view cursor shape
    View::MouseZoneResult mouseZone=view->getMouseZone(event->pos());
    view->setCursor(mouseZone.cursor);

    // notify all subsystems
    for(int i=0; i < subsystemList.size(); ++i)
        subsystemList[i]->mouseReleaseEvent(event);

    return true;   // event was handled
}

bool Controller::viewMouseDoubleClickEvent(QMouseEvent* event)
{
    event->accept();

    // update view cursor shape
    View::MouseZoneResult mouseZone=view->getMouseZone(event->pos());
    view->setCursor(mouseZone.cursor);

    // notify capturing subsystems or - if no subsystem currently captures - all subsystems
    QList<ControllerSubsystem*> csList=getInputEventTraversalOrder();
    for(int i=0; i < csList.size(); ++i)
    {
        if(csList[i]->mouseDoubleClickEvent(event, mouseZone))
            return true;  // event was handled
    }
    return false;   // unhandled event
}

bool Controller::viewWheelEvent(QWheelEvent* event)
{
    event->accept();

    // notify capturing subsystems or - if no subsystem currently captures - all subsystems
    QList<ControllerSubsystem*> csList=getInputEventTraversalOrder();
    for(int i=0; i < csList.size(); ++i)
    {
        if(csList[i]->wheelEvent(event))
        {
            // Event was handled. Reset all other cs's wheel accumulators.
            for(int j=0; j < subsystemList.size(); ++j)
            {
                if(subsystemList[j] != csList[i])
                    subsystemList[j]->resetWheelAccumulator();
            }

            return true;  // event was handled
        }
    }
    return false;   // unhandled event
}

bool Controller::viewCloseEvent()  // return true if event should be accepted
{
    // If any subsystem is capturing input, closing is disabled
    if(!inputCaptureSubsystemList.isEmpty())return false;

    // cancel interruptible states before subsystem may show any dialogs etc. in closeEvent()
    cancelInterruptibleStates();

    // test all subsystems
    for(int i=0; i < subsystemList.size(); ++i)
    {
        if(!subsystemList[i]->closeEvent())
            return false;   // editor must not be closed
    }

    return true;    // editor may be closed
}

void Controller::viewMenuAboutToShow()
{
    while(!inputCaptureSubsystemList.isEmpty())
    {
        ControllerSubsystem* cs=inputCaptureSubsystemList.first();
        cs->cancelInputCapture();

        // cs->cancelInputCapture() must have called csUnregisterInputCapture()
        Q_ASSERT(inputCaptureSubsystemList.isEmpty() || cs != inputCaptureSubsystemList.first());
    }
}

void Controller::viewInitiallyResized()
{
    CS_Navigation* csNavigation=
            qobject_cast<CS_Navigation*>(getSubsystemByClassName("CS_Navigation"));
    if(csNavigation != NULL)
    {
        if(mainWindow->isCreatedByWizard())
            csNavigation->initialFitAllTracks();
    }
}

// _____________________________ Public interface for controller subsystems ____________________________

void Controller::csRegisterActionHandler(QAction* action, QObject* receiver, const char* method, ActionGuards guards)
{
    // check if action signal triggered() is already registered
    for(int i=0; i < registeredActionList.size(); ++i)
    {
        if(registeredActionList[i].action == action)
        {
            qDebug() << "csRegisterActionHandler: action handler " << method << " is already registered";
            return;
        }
    }

    RegisteredAction ra;
    ra.action    = action;
    ra.receiver  = receiver;
    ra.method    = method;
    ra.guards    = guards;

    // Method must exist
    if(receiver->metaObject()->indexOfMethod((ra.method + "()").data()) == -1)
    {
        qDebug() << "csRegisterActionHandler: action handler " << method << " does not exist";
        return;
    }

    // register action handler
    registeredActionList.append(ra);

    // connect action to the single slot that will check guard conditions and pass the call to <method>
    connect(action, SIGNAL(triggered()), SLOT(editorActionTriggered()));
}

void Controller::csUnregisterActionHandler(QObject* receiver, const char* method)
{
    bool unregisteredAnyHandler=false;

    int i=0;
    while(i < registeredActionList.size())
    {
        RegisteredAction ra=registeredActionList[i];

        if(ra.receiver == receiver)
        {
            if(method == NULL || ra.method == method)
            {
                // disconnect and remove registered action from list
                disconnect(ra.action, SIGNAL(triggered()), this, SLOT(editorActionTriggered()));
                registeredActionList.removeAt(i);
                unregisteredAnyHandler=true;
            }
            else ++i;
        }
        else ++i;
    }

    if(!unregisteredAnyHandler && method != NULL)
        qDebug() << "csUnregisterActionHandler: action handler " << method << " was not registered";
}

void Controller::csRegisterInputCapture(ControllerSubsystem* cs)
{
    // check if already on list of subsystems capturing input
    for(int i=0; i < inputCaptureSubsystemList.size(); ++i)
    {
        if(inputCaptureSubsystemList[i] == cs)
        {
            qDebug() << "csRegisterInputCapture: subsystem " <<
                    cs->metaObject()->className() << " already on list";
            return;
        }
    }

    // Nested captures:
    //  Put into list behind end position, so any subsystem already capturing input is still in control
    //  what to do, and we will get no double events by passing through the event.
    //  When any of the capturing subsystems is releasing capture, the next lower-prioritized subsystem
    //  can decide what to do.
    inputCaptureSubsystemList.append(cs);
    updateRegisteredActions();
}

void Controller::csUnregisterInputCapture(ControllerSubsystem* cs)
{
    for(int i=0; i < inputCaptureSubsystemList.size(); ++i)
    {
        if(inputCaptureSubsystemList[i] == cs)
        {
            inputCaptureSubsystemList.removeAt(i);

            view->restoreMouseCursor();
            updateRegisteredActions();
            return;
        }
    }

    qDebug() << "csUnregisterInputCapture: subsystem " << cs->metaObject()->className() << " not in list";
//TODO: Remove assert
    Q_ASSERT(false);
}

bool Controller::csIsInputCaptured()
{
    return !inputCaptureSubsystemList.isEmpty();
}

bool Controller::csIsInputCapturedBy(ControllerSubsystem* cs)
{
    for(int i=0; i < inputCaptureSubsystemList.size(); ++i)
        if(inputCaptureSubsystemList[i] == cs)return true;

    return false;
}

void Controller::csRegisterInterruptibleState(ControllerSubsystem* cs)
{
    // check if already on list of subsystems with an interruptible state
    for(int i=0; i < interruptibleStateSubsystemList.size(); ++i)
    {
        if(interruptibleStateSubsystemList[i] == cs)
        {
            qDebug() << "csRegisterInterruptibleState: subsystem " <<
                    cs->metaObject()->className() << " already on list";
            return;
        }
    }

    // insert into list, order does not matter
    interruptibleStateSubsystemList.append(cs);
}

void Controller::csUnregisterInterruptibleState(ControllerSubsystem* cs)
{
    for(int i=0; i < interruptibleStateSubsystemList.size(); ++i)
    {
        if(interruptibleStateSubsystemList[i] == cs)
        {
            interruptibleStateSubsystemList.removeAt(i);
            view->restoreMouseCursor();
            return;
        }
    }

    qDebug() << "csUnregisterInterruptibleState: subsystem " <<
            cs->metaObject()->className() << " not in list";
//TODO: Remove assert
    Q_ASSERT(false);
}

void Controller::csApplyStateAndUpdate(const EditorState& newState)
{
    Q_ASSERT(newState.isValid(docRoot));

    EditorState oldState=getEditorState();   // Remember previous state
    editorState = newState;

    view->refreshMapper();
    view->updateForEditorStateModifications(oldState,newState);
    view->updateForModificationsInRange(documentModificationsUnionRange);
    documentModificationsUnionRange.invalidate();   // Invalidate the union range of document modifications

    // Notify all subsystems that editor state has changed -> UI update etc.
    for(int i=0; i < subsystemList.size(); ++i)
        subsystemList[i]->stateChanged();

    updateRegisteredActions();
}

void Controller::csApplyVolatileStateAndUpdate(const VolatileEditorState& newVolatileState)
{
    VolatileEditorState oldVolatileState=getVolatileEditorState();
    volatileEditorState = newVolatileState;

    view->updateForVolatileEditorStateModificationsInRange(oldVolatileState, newVolatileState);

    updateRegisteredActions();
}

void Controller::csBeginMacro(const QString& description, const EditorRange& scrollToRangeUndo)
{
    // Macros may be nested. Create a QUndoCommand macro only for the outer macro.
    if(macroNestingCounter == 0)
    {
        macroDescription=description;
        macroEditorStateBefore=getEditorState();
        macroScrollToRangeUndo=scrollToRangeUndo;
        macroUndoCommandCreated=false;
    }

    ++macroNestingCounter;
}

void Controller::csAddCommand(EditorUndoCommand* cmd)
{
    // Macros may be nested. Create a QUndoCommand macro only for the outer macro.
    Q_ASSERT(macroNestingCounter >= 1);

    if(!macroUndoCommandCreated)
    {
        // Create QUndoStack-macro on demand now,
        //  because a real undoable command is being added
        undoStack->beginMacro(macroDescription);

        // Remember editor state before the operation
        Command_SetEditorState* cmdSetEditorState=
                new Command_SetEditorState(macroEditorStateBefore,macroScrollToRangeUndo,false);
        cmdSetEditorState->connectToController(this,docRoot);

        undoStack->push(cmdSetEditorState);

        docRoot->unmodifiedDefaultDocument=false;   // document was modified at least once
        macroUndoCommandCreated=true;
    }

    cmd->connectToController(this,docRoot);
    undoStack->push(cmd);   // add real undoable command
}

void Controller::csEndMacro(const EditorState& newState, const EditorRange& scrollToRangeRedo, bool noScrollDuringMacroCreation)
{
    Q_ASSERT(macroNestingCounter >= 1);
    --macroNestingCounter;

    if(macroNestingCounter == 0)
    {
        // Macro command finished

        if(macroUndoCommandCreated)
        {
            // editorState must not be modified during macro recording
            Q_ASSERT(editorState == macroEditorStateBefore);

            // Remember editor state after the operation
            if(noScrollDuringMacroCreation)
                disableScrollingDuringMacroCreation=true;

            Command_SetEditorState* cmdSetEditorState=
                    new Command_SetEditorState(newState,scrollToRangeRedo,true);
            cmdSetEditorState->connectToController(this, docRoot);

            undoStack->push(cmdSetEditorState);

            if(noScrollDuringMacroCreation)
                disableScrollingDuringMacroCreation=false;

            undoStack->endMacro();
        }
        else
        {
            // no commands were issued, but change the state
            csApplyStateAndUpdate(newState);
        }
    }
}

// _______________________________ Public interface for command classes ________________________________

void Controller::commandApplyUndoCommandStateAndUpdate(const EditorState& state, const EditorRange& scrollToRange)
{
    EditorState newState=state;

    CS_Navigation* csNavigation=
            qobject_cast<CS_Navigation*>(getSubsystemByClassName("CS_Navigation"));
    if(csNavigation != NULL)
    {
        csNavigation->navigateToUndoCommandLocation(
                newState, scrollToRange, disableScrollingDuringMacroCreation);
    }

    // apply these settings
    csApplyStateAndUpdate(newState);
}

void Controller::commandCollectDocumentModificationRange(const EditorRange& modifiedRange)
{
    // called directly after individual undo/redo operations
    documentModificationsUnionRange = documentModificationsUnionRange.unionRange(modifiedRange);
}

// ________________________________________ Internal functions _________________________________________

void Controller::createSubsystems()
{
    subsystemList.append(new CS_File(this));
    subsystemList.append(new CS_Navigation(this));
    subsystemList.append(new CS_Clipboard(this));
    subsystemList.append(new CS_LocalMassEdit(this));
    subsystemList.append(new CS_Write(this));
    subsystemList.append(new CS_Utilities(this));
    subsystemList.append(new CS_Playback(this));
}

void Controller::removeSubsystem(ControllerSubsystem* cs)
{
    // remove all registered action handlers
    csUnregisterActionHandler(cs, NULL);

    // remove from subsystemList and delete subsystem
    subsystemList.removeOne(cs);
    delete cs;
}

void Controller::editorActionTriggered()
{
    // all editor actions have their triggered() signal connected to this single slot
    QAction* sendingAction = qobject_cast<QAction *>(sender());
    if(sendingAction == NULL)
    {
        qDebug() << "editorActionTriggered: unknown sender action";
        return;
    }

    if(sendingAction->isCheckable())
    {
        sendingAction->toggle();    // undo automatic toggle, so we stay in control
    }

    // find action in registered list
    for(int i=0; i < registeredActionList.size(); ++i)
    {
        const RegisteredAction& ra=registeredActionList[i];

        if(ra.action == sendingAction)
        {
            // action found, check guards
            if(!checkActionGuards(ra.guards))
                return;   // action is currently disabled

            // if requested stop any interruptible states
            if(ra.guards & CancelInterruptibleStates)
            {
                cancelInterruptibleStates();
            }

            // call specified method slot in receiver class
            currentSenderAction=sendingAction;
            if(!metaObject()->invokeMethod(ra.receiver, ra.method.data()))
            {
                qDebug() << "editorActionTriggered: could not invoke method" << ra.method;
            }
            currentSenderAction=NULL;
            return;
        }
    }

    qDebug() << "editorActionTriggered: sending action was not registered";
}

void Controller::updateRegisteredActions()
{
    // enable/disable all registered actions according to their guards
    for(int i=0; i < registeredActionList.size(); ++i)
    {
        const RegisteredAction& ra=registeredActionList[i];
        ra.action->setEnabled(checkActionGuards(ra.guards & ~DisallowWhenInputCaptured));
    }
}

bool Controller::checkActionGuards(ActionGuards guards)
{
    if(guards & DisallowWhenInputCaptured)
    {
        // Action is only enabled if no subsystem is capturing input
        if(!inputCaptureSubsystemList.isEmpty())return false;
    }
    if(guards & MustHaveTracks)
    {
        if(!docRoot->hasTracks())return false;
    }
    if(guards & CanUndo)
    {
        if(!undoStack->canUndo())return false;
    }
    if(guards & CanRedo)
    {
        if(!undoStack->canRedo())return false;
    }

    // passed all guards
    return true;
}

void Controller::cancelInterruptibleStates()
{
    while(!interruptibleStateSubsystemList.isEmpty())
    {
        // subsystem must call csUnregisterInterruptibleState, so list will get empty
        interruptibleStateSubsystemList.first()->cancelInterruptibleState();
    }

    updateRegisteredActions();
}

QList<ControllerSubsystem*> Controller::getInputEventTraversalOrder()
{
    if(!inputCaptureSubsystemList.isEmpty())
    {
        // while input is captured, use only first subsystem on capture list for
        //  keyPress-, mousePress-, mouseDoubleClick-, and wheel-Events
        QList<ControllerSubsystem*> firstCapturedCSList;
        firstCapturedCSList.append(inputCaptureSubsystemList[0]);
        return firstCapturedCSList;
    }
    else return subsystemList;
}

void Controller::undoStack_undoTextChanged(const QString& undoText)
{
    QString actionText(tr("Undo"));
    if(!undoText.isEmpty())
        actionText+=": " + undoText;

    getMainWindowUI()->actionEdit_Undo->setText(actionText);
}

void Controller::undoStack_redoTextChanged(const QString& redoText)
{
    QString actionText(tr("Redo"));
    if(!redoText.isEmpty())
        actionText+=": " + redoText;

    getMainWindowUI()->actionEdit_Redo->setText(actionText);
}

void Controller::setClean()
{
    undoStack->setClean();
}

bool Controller::isClean()
{
    return undoStack->isClean();
}

void Controller::undoStackCleanChanged(bool clean)
{
    mainWindow->setWindowModified(!clean);
}

void Controller::undoStackCanUndoChanged(bool canUndo)
{
    UNUSED(canUndo);
    updateRegisteredActions();
}

void Controller::undoStackCanRedoChanged(bool canRedo)
{
    UNUSED(canRedo);
    updateRegisteredActions();
}

void Controller::restoreMouseCursor()
{
    view->restoreMouseCursor();
}

void Controller::retranslateUi()
{
    undoStack_undoTextChanged(undoStack->undoText());
    undoStack_redoTextChanged(undoStack->redoText());
}
