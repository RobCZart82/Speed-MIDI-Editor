/***************************************************************************
 *  cs_navigation.cpp - Controller Subsystem: Navigation
 *                      (Select, Scroll, Zoom, Cell Length)
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

#include "cs_navigation.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "zoomsliderwidget.h"
#include "speedymidiapp.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "commands.h"
#include "settings.h"
#include "cs_playback.h"
#include "writelengthdialog.h"

// Factor: scroll speed in pixels per timer event / mouse pointer pixel count outside noScrollArea
#define CS_NAVIGATION_MOUSE_AUTOSCROLL_SPEED_FACTOR         2
#define CS_NAVIGATION_MOUSE_AUTOSCROLL_SCROLL_AREA_WIDTH   50
#define CS_NAVIGATION_MOUSE_AUTOSCROLL_MIN_SCROLL_SPEED     5

CS_Navigation::CS_Navigation(Controller* controller)
        : CS_Common(controller)
{
    mouseDragMode=MDM_None;
    mouseDragStartTrackIndex=-1;
    mouseDragPreviousValue=0;
    draggedNote=NULL;
    draggedNoteTrackIndex=-1;
    draggedNoteAnchorX=0;
    draggedNoteAnchorTick=0;
    draggedNoteAnchorMidiNote=0;
    eraseMacroActive=false;
    lastRehearsalMarkerTick=-1;
    lastRehearsalMarkerDocument=NULL;

    scrollBarHorizontalIsPressed=false;

    autoscrollTimerActive=false;
    autoscrollTimerId=-1;
    autoscrollPixelAccuToLeft=0;
    autoscrollPixelAccuToRight=0;
    autoscrollPixelAccuUp=0;
    autoscrollPixelAccuDown=0;

    scrollBarHorizontal = mainWindow->getScrollBarHorizontal();
    scrollBarVertical   = mainWindow->getScrollBarVertical();
    xZoomSliderWidget   = mainWindow->getXZoomSliderWidget();
    yZoomSliderWidget   = mainWindow->getYZoomSliderWidget();

    connect(scrollBarHorizontal, SIGNAL(actionTriggered(int)), SLOT(scrollBarHorizontalActionTriggered(int)));
    connect(scrollBarHorizontal, SIGNAL(sliderPressed()), SLOT(scrollBarHorizontalSliderPressed()));
    connect(scrollBarHorizontal, SIGNAL(sliderReleased()), SLOT(scrollBarHorizontalSliderReleased()));
    connect(scrollBarVertical, SIGNAL(actionTriggered(int)), SLOT(scrollBarVerticalActionTriggered(int)));
    connect(scrollBarVertical, SIGNAL(valueChanged(int)), SLOT(scrollBarVerticalValueChanged(int)));
    connect(xZoomSliderWidget, SIGNAL(valueChanged(int)), SLOT(xZoomSliderValueChanged(int)));
    connect(yZoomSliderWidget, SIGNAL(valueChanged(int)), SLOT(yZoomSliderValueChanged(int)));

    Controller::ActionGuards gDIC = Controller::DisallowWhenInputCaptured;
    Controller::ActionGuards gMHT = Controller::MustHaveTracks;

    registerActionHandler(ui->actionEdit_SelectAll,                   "actionEdit_SelectAll_Triggered", gDIC);
    registerActionHandler(ui->actionEdit_MoveNotes,                   "actionEdit_MoveNotes_Triggered", gDIC);
    registerActionHandler(ui->actionEdit_DrawNotes,                    "actionEdit_DrawNotes_Triggered", gDIC);
    registerActionHandler(ui->actionEdit_EraseNotes,                   "actionEdit_EraseNotes_Triggered", gDIC);
    registerActionHandler(ui->actionView_HorizontalZoom_ZoomIn,       "actionView_HorizontalZoom_ZoomIn_Triggered", gDIC);
    registerActionHandler(ui->actionView_HorizontalZoom_ZoomOut,      "actionView_HorizontalZoom_ZoomOut_Triggered", gDIC);
    registerActionHandler(ui->actionView_VerticalZoom_ZoomIn,         "actionView_VerticalZoom_ZoomIn_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionView_VerticalZoom_ZoomOut,        "actionView_VerticalZoom_ZoomOut_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionView_FitAllTracks,                "actionView_FitAllTracks_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionView_NoteNames,                   "actionView_NoteNames_Triggered", gDIC);
    registerActionHandler(ui->actionWrite_CellLength_Double,          "actionWrite_CellLength_Double_Triggered", gDIC);
    registerActionHandler(ui->actionWrite_CellLength_Halve,           "actionWrite_CellLength_Halve_Triggered", gDIC);
    registerActionHandler(ui->actionWrite_CellLength_Triplets,        "actionWrite_CellLength_Triplets_Triggered", gDIC);
    registerActionHandler(ui->actionWrite_CellLength_Duplets,         "actionWrite_CellLength_Duplets_Triggered", gDIC);
    registerActionHandler(ui->actionWrite_CellLength_ArbitraryTuplets,"actionWrite_CellLength_ArbitraryTuplets_Triggered", gDIC);
}

void CS_Navigation::navigateToUndoCommandLocation(EditorState& state, const EditorRange& scrollToRange, bool disableScrollingDuringMacroCreation)
{
    // Override scroll settings: On demand scrolling only.
    //  Scroll from _current_ editor position to scrollToRange.

    // copy current scroll settings, but clamp to valid values
    state.firstMeasure = getEditorState().firstMeasure;
    state.firstTrack   = getEditorState().firstTrack;

    if(state.firstMeasure > docRoot->getMaxFirstMeasure())
        state.firstMeasure=docRoot->getMaxFirstMeasure();
    if(state.firstTrack > docRoot->trackList.size())
        state.firstTrack=docRoot->trackList.size();

    // Some commands do not scroll during initial creation of macro, only when doing real undo/redo.
    //  This is useful for commands initiated by a mouse-click. We should not scroll in these cases.
    if(!disableScrollingDuringMacroCreation)
    {
        // scroll to specified range
        scrollRangeIntoView(state,scrollToRange);
    }
}

void CS_Navigation::scrollBarHorizontalPageStepAdd()
{
    scrollBarHorizontal->triggerAction(QAbstractSlider::SliderPageStepAdd);
}

void CS_Navigation::initialFitAllTracks()
{
    if(docRoot->hasTracks())    // check guard condition for this action handler
        actionView_FitAllTracks_Triggered();
}

void CS_Navigation::setDefaultTrackHeights()
{
    if(!docRoot->hasTracks())return;

    controller->cancelInterruptibleStates();

    EditorState newState=getEditorState();
    const double noteHeightInPixels=newState.getNoteHeightInPixels();
    const double defaultHeightInNotes=
            (view->getDefaultTrackHeightInPixels() + 0.5) / noteHeightInPixels;

    for(EditorTrackState& trackState : newState.trackStateList)
        trackState.heightInNotes=defaultHeightInNotes;

    applyStateAndUpdate(newState);
}

bool CS_Navigation::inImmediateListenMode() const
{
    CS_Playback* csPlayback=qobject_cast<CS_Playback*>(controller->getSubsystemByClassName("CS_Playback"));
    if(csPlayback != NULL)return csPlayback->getPlaybackMode() == CS_Playback::PBM_ImmediateListen;
    else return false;
}

bool CS_Navigation::noteDrawingEnabled() const
{
    return ui->actionEdit_DrawNotes->isChecked();
}

bool CS_Navigation::noteErasingEnabled() const
{
    return ui->actionEdit_EraseNotes->isChecked();
}

bool CS_Navigation::noteMovingEnabled() const
{
    return ui->actionEdit_MoveNotes->isChecked();
}

void CS_Navigation::eraseNoteAtPosition(const QPoint& position)
{
    const View::MouseZoneResult mouseZone=view->getMouseZone(position);
    if(mouseZone.zoneType != View::TrackCells)return;

    DocEvent* note=NULL;
    if(!view->getNoteAtPosition(position, mouseZone.trackIndex, &note) || !note)return;

    const int tickStart=note->tickPosition;
    const int tickEnd=tickStart + note->tickLength;
    const EditorRange range(tickStart, mouseZone.trackIndex, tickEnd, mouseZone.trackIndex);
    if(!eraseMacroActive)
    {
        beginMacro(tr("Erase Notes"), range);
        eraseMacroActive=true;
        erasedNoteRange.invalidate();
    }

    erasedNoteRange=erasedNoteRange.unionRange(range);
    addCommand(new Command_DeleteEvent(mouseZone.trackIndex, note));
    const_cast<View*>(view)->updateForModificationsInRange(range);
}

void CS_Navigation::finishEraseGesture()
{
    if(!eraseMacroActive)return;
    endMacro(getEditorState(), erasedNoteRange, true);
    eraseMacroActive=false;
    erasedNoteRange.invalidate();
}

int CS_Navigation::noteNumberAtY(int trackIndex, int y) const
{
    TrackToViewYResult trackY=view->getMapper()->trackToViewY(trackIndex);
    const double noteHeight=getEditorState().getNoteHeightInPixels();
    const int trackCenterY=(trackY.TopY + trackY.BottomY) / 2;
    int noteNumber=qRound(getEditorState().trackStateList[trackIndex].centerMidiNote +
                          (trackCenterY - y) / noteHeight);
    return qBound(0, noteNumber, MIDI_MAX_DATA_VALUE);
}

int CS_Navigation::snappedTickAtX(int x) const
{
    const QRect cellArea=view->getCellArea();
    const int clampedX=qBound(cellArea.left(), x, cellArea.right() - 1);
    CellAreaXToTicksResult cell=view->getMapper()->cellAreaXToTicks(clampedX - cellArea.left());
    DisplayedCell* displayedCell=view->getMapper()->getDisplayedCellList()[cell.displayedCellIndex];
    const int leftX=cellArea.left() + displayedCell->leftX;
    const int rightX=leftX + displayedCell->cellWidth;
    return clampedX - leftX < rightX - clampedX ? cell.cellLeftTicks : cell.cellRightTicks;
}

void CS_Navigation::updateDraggedNote(const QPoint& position)
{
    if(!draggedNote)return;

    const int x=position.x();
    const int snappedTick=snappedTickAtX(x);
    const int originalStart=draggedNoteOriginal.tickPosition;
    const qint64 originalEnd=qint64(originalStart) + draggedNoteOriginal.tickLength;
    qint64 changedStart=originalStart;
    qint64 changedLength=draggedNoteOriginal.tickLength;

    if(mouseDragMode == MDM_DrawNote)
    {
        if(x >= draggedNoteAnchorX)
            changedLength=qMax<qint64>(draggedNoteOriginal.tickLength, qint64(snappedTick) - originalStart);
        else
        {
            changedStart=qMin(originalStart, snappedTick);
            changedLength=originalEnd - changedStart;
        }
    }
    else if(mouseDragMode == MDM_ResizeNote)
    {
        if(draggedNoteResizeEdge == NRE_Left)
        {
            changedStart=qMin<qint64>(snappedTick, originalEnd - 1);
            changedLength=originalEnd - changedStart;
        }
        else
        {
            changedLength=qMax<qint64>(1, qint64(snappedTick) - originalStart);
        }
    }
    else if(mouseDragMode == MDM_MoveNote)
    {
        changedStart=qMax<qint64>(0, qint64(originalStart) + snappedTick - draggedNoteAnchorTick);
    }

    // Keep the same maximum-bar headroom as import/insert/scale operations.
    // Validate the entire interval before touching the live note or undo state.
    const qint64 maxTick=INT_MAX - qint64(docRoot->midiTicksPerWholeNote) *
                                   EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR;
    if(changedStart < 0 || changedLength < 1 || changedStart + changedLength > maxTick)
        return;
    DocEvent changed(*draggedNote);
    changed.tickPosition=int(changedStart);
    changed.tickLength=int(changedLength);
    if(mouseDragMode == MDM_MoveNote)
    {
        const int midiNote=draggedNoteOriginal.noteEventData.noteNumber +
                noteNumberAtY(draggedNoteTrackIndex, position.y()) - draggedNoteAnchorMidiNote;
        changed.noteEventData.noteNumber=qBound(0, midiNote, MIDI_MAX_DATA_VALUE);
    }

    if(changed != *draggedNote)
    {
        const int oldStart=draggedNote->tickPosition;
        const int oldEnd=oldStart + draggedNote->tickLength;
        *draggedNote=changed;
        const int newStart=changed.tickPosition;
        const int newEnd=newStart + changed.tickLength;
        const_cast<View*>(view)->updateForModificationsInRange(
                EditorRange(qMin(oldStart,newStart), draggedNoteTrackIndex,
                            qMax(oldEnd,newEnd), draggedNoteTrackIndex));
    }
}

void CS_Navigation::finishNoteGesture(bool commit)
{
    if(mouseDragMode == MDM_DrawNote)
    {
        if(draggedNote)
        {
            EditorRange range(draggedNote->tickPosition, draggedNoteTrackIndex,
                              draggedNote->tickPosition + draggedNote->tickLength, draggedNoteTrackIndex);
            docRoot->trackList[draggedNoteTrackIndex]->removeEvent(draggedNote);
            if(commit)
            {
                beginMacro(tr("Draw Note"), range);
                addCommand(new Command_InsertEvent(draggedNoteTrackIndex, draggedNote));
                endMacro(getEditorState(), range, true);
            }
            else
            {
                delete draggedNote;
                const_cast<View*>(view)->updateForModificationsInRange(range);
            }
        }
    }
    else if((mouseDragMode == MDM_ResizeNote || mouseDragMode == MDM_MoveNote) && draggedNote)
    {
        if(!commit)
        {
            const int currentStart=draggedNote->tickPosition;
            const int currentEnd=currentStart + draggedNote->tickLength;
            *draggedNote=draggedNoteOriginal;
            const_cast<View*>(view)->updateForModificationsInRange(
                    EditorRange(qMin(currentStart,draggedNoteOriginal.tickPosition), draggedNoteTrackIndex,
                                qMax(currentEnd,draggedNoteOriginal.tickPosition + draggedNoteOriginal.tickLength),
                                draggedNoteTrackIndex));
            draggedNote=NULL;
            draggedNoteTrackIndex=-1;
            return;
        }

        DocEvent finalProperties(*draggedNote);
        const int finalStart=finalProperties.tickPosition;
        const int finalEnd=finalStart + finalProperties.tickLength;
        *draggedNote=draggedNoteOriginal;
        if(finalProperties != draggedNoteOriginal)
        {
            const int oldStart=draggedNoteOriginal.tickPosition;
            const int oldEnd=oldStart + draggedNoteOriginal.tickLength;
            const QString description=mouseDragMode == MDM_MoveNote ? tr("Move Note") : tr("Resize Note");
            beginMacro(description, EditorRange(oldStart, draggedNoteTrackIndex, oldEnd, draggedNoteTrackIndex));
            addCommand(new Command_EventProperties(draggedNoteTrackIndex, draggedNote, finalProperties));
            endMacro(getEditorState(),
                     EditorRange(qMin(oldStart,finalStart), draggedNoteTrackIndex,
                                 qMax(oldEnd,finalEnd), draggedNoteTrackIndex), true);
        }
        else
        {
            const_cast<View*>(view)->updateForModificationsInRange(
                    EditorRange(draggedNoteOriginal.tickPosition, draggedNoteTrackIndex,
                                draggedNoteOriginal.tickPosition + draggedNoteOriginal.tickLength,
                                draggedNoteTrackIndex));
        }
    }
    draggedNote=NULL;
    draggedNoteTrackIndex=-1;
}

bool CS_Navigation::keyPressEvent(QKeyEvent* event)
{
    // Modifiers
    bool modShift=(event->modifiers() & Qt::ShiftModifier  ) != 0;
    bool modCtrl =(event->modifiers() & Qt::ControlModifier) != 0;
    bool modAlt  =(event->modifiers() & Qt::AltModifier    ) != 0;

    // Group 1: Keys that are permanently enabled
    switch(event->key())
    {
    case Qt::Key_Shift:     // show selection anchor
        {
            applyVolatileStateAndUpdate(getVolatileEditorState());
            return false;
        }
    case Qt::Key_Escape:    // cancel mouse drag operation
        {
            if(mouseDragMode == MDM_None)
                break;      // no drag mode active

            cancelInputCapture();
            return true;
        }
    }

    // Group 2: Keys disabled when mouse drag operation is in progress or ALT key is pressed
    if(mouseDragMode != MDM_None || modAlt)
        return false;

    bool immediateListenMode=inImmediateListenMode();

    // Group 2a: Keys enabled even if document has no tracks
    switch(event->key())
    {
    case Qt::Key_Left:
        if(modCtrl)
            scrollBarHorizontal->triggerAction(QAbstractSlider::SliderSingleStepSub);
        else
        {
            if(modShift && !immediateListenMode) modifySelectionByKeyboard(KSA_Left);
            else initiateSelectionByKeyboard(KSA_Left);
        }
        return true;
    case Qt::Key_Right:
        if(modCtrl)
            scrollBarHorizontal->triggerAction(QAbstractSlider::SliderSingleStepAdd);
        else
        {
            if(modShift && !immediateListenMode) modifySelectionByKeyboard(KSA_Right);
            else initiateSelectionByKeyboard(KSA_Right);
        }
        return true;
    case Qt::Key_PageUp:
        if(modCtrl)
            scrollToRehearsalMarker(KSA_PageLeft);
        else
        {
            if(modShift && !immediateListenMode) modifySelectionByKeyboard(KSA_PageLeft);
            else initiateSelectionByKeyboard(KSA_PageLeft);
        }
        return true;
    case Qt::Key_PageDown:
        if(modCtrl)
            scrollToRehearsalMarker(KSA_PageRight);
        else
        {
            if(modShift && !immediateListenMode) modifySelectionByKeyboard(KSA_PageRight);
            else initiateSelectionByKeyboard(KSA_PageRight);
        }
        return true;
    case Qt::Key_Home:
        if(modShift && !immediateListenMode) modifySelectionByKeyboard(KSA_Home);
        else initiateSelectionByKeyboard(KSA_Home);
        return true;
    case Qt::Key_End:
        if(modShift && !immediateListenMode) modifySelectionByKeyboard(KSA_End);
        else initiateSelectionByKeyboard(KSA_End);
        return true;
    }

    // Group 2b: Keys disabled if document has no tracks
    if(!docRoot->hasTracks())
        return false;

    switch(event->key())
    {
    case Qt::Key_Up:
        if(modCtrl)
            scrollBarVertical->triggerAction(QAbstractSlider::SliderSingleStepSub);
        else
        {
            if(modShift) modifySelectionByKeyboard(KSA_Up);
            else initiateSelectionByKeyboard(KSA_Up);
        }
        return true;
    case Qt::Key_Down:
        if(modCtrl)
            scrollBarVertical->triggerAction(QAbstractSlider::SliderSingleStepAdd);
        else
        {
            if(modShift) modifySelectionByKeyboard(KSA_Down);
            else initiateSelectionByKeyboard(KSA_Down);
        }
        return true;
    }

    return false;
}

void CS_Navigation::keyReleaseEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_Shift: // Remove selection anchor
        {
            applyVolatileStateAndUpdate(getVolatileEditorState());
            break;
        }
    }
}

bool CS_Navigation::mousePressEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone)
{
    bool modShift=(event->modifiers() & Qt::ShiftModifier  ) != 0;

    // A right-click on the piano-roll grid exits the persistent editing tool
    // mode. Ignore it during a captured drag so it cannot interrupt a gesture.
    if(event->button() == Qt::RightButton && mouseDragMode == MDM_None &&
       mouseZone.zoneType == View::TrackCells &&
       (noteDrawingEnabled() || noteErasingEnabled() || noteMovingEnabled()))
    {
        ui->actionEdit_DrawNotes->setChecked(false);
        ui->actionEdit_EraseNotes->setChecked(false);
        ui->actionEdit_MoveNotes->setChecked(false);
        const_cast<View*>(view)->restoreMouseCursor();
        return true;
    }

    if(event->button() == Qt::LeftButton)
    {
        mouseDragStartReferencePoint=event->pos();
        mouseDragStartTrackIndex=mouseZone.trackIndex;

        // Modified editor state
        EditorState newState=getEditorState();

        CellAreaXToTicksResult ticksResult;

        switch(mouseZone.zoneType)
        {
        case View::WriteLengthArea:
            // calling controller->cancelInterruptibleStates() is not necessary here
            execWriteLengthDialog(false);
            return true;
            
        case View::MeasureHeader:
            // global measure selection (selection rectangle also on measure headers)
            mouseDragMode=MDM_Select;
            setInputCapture();

            ticksResult=view->getMapper()->cellAreaXToTicks(event->position().toPoint().x() - view->getCellArea().left());

            if(modShift)
            {
                // Shift was pressed: continue selection and use anchor point for opposite cell
                newState.selection=extendSelectionFromAnchorByMouse(
                        newState.selection,true,
                        EditorRange(docRoot->measureToTicks(ticksResult.measureIndex),   0,
                                    docRoot->measureToTicks(ticksResult.measureIndex+1), INT_MAX));
            }
            else
            {
                // Begin new selection
                newState.setGlobalMeasureSelection(ticksResult.measureIndex, 1, docRoot);
            }
            applyStateAndUpdate(newState);
            startAutoScrollTimer();
            return true;
            
        case View::TrackHeader:
            // global track selection (selection rectangle also on track headers)
            mouseDragMode=MDM_Select;
            setInputCapture();

            if(modShift)
            {
                // Shift was pressed: continue selection and use anchor point for opposite cell
                newState.selection=extendSelectionFromAnchorByMouse(
                        newState.selection,true,
                        EditorRange(0,       mouseZone.trackIndex,
                                    INT_MAX, mouseZone.trackIndex));
            }
            else
            {
                // Begin new selection
                newState.setGlobalTrackSelection(mouseZone.trackIndex, 1, docRoot);
            }
            applyStateAndUpdate(newState);
            startAutoScrollTimer();
            return true;
            
        case View::TrackNoteRangeSlider:
            // adjust displayed note range for clicked track
            mouseDragMode=MDM_AdjustDisplayedNoteRange;
            setInputCapture();

            mouseDragPreviousValue=
                    getEditorState().trackStateList[mouseDragStartTrackIndex].centerMidiNote;
            return true;
            
        case View::TrackResize:
            {
                // adjust track height
                mouseDragMode=MDM_AdjustTrackHeight;
                setInputCapture();

                // When clicking the resize area, adjust height in notes to displayed value that
                //  was clamped by EditorMapper::refreshDisplayedItemLists in the vertical part
                double noteHeightInPixels=getEditorState().getNoteHeightInPixels();
                int currentHeightInPixels=
                        (int)(getEditorState().trackStateList[mouseDragStartTrackIndex].heightInNotes *
                              noteHeightInPixels);

                // Clamp to allowed range (exactly like in EditorMapper::refreshDisplayedItemLists)
                if(currentHeightInPixels < VIEW_MIN_TRACK_HEIGHT_IN_PIXELS)
                    currentHeightInPixels=VIEW_MIN_TRACK_HEIGHT_IN_PIXELS;

                double adjustedHeightInNotes=((double)currentHeightInPixels) / noteHeightInPixels;
                mouseDragPreviousValue=adjustedHeightInNotes;
            }
            return true;
            
        case View::TrackCells:
            {
                if(noteErasingEnabled())
                {
                    controller->cancelInterruptibleStates();
                    mouseDragMode=MDM_EraseNotes;
                    lastErasePosition=event->pos();
                    setInputCapture();
                    eraseNoteAtPosition(lastErasePosition);
                    return true;
                }

                DocEvent* note=NULL;
                bool leftEdge=false;
                if(noteMovingEnabled() &&
                   view->getNoteResizeHit(event->pos(), mouseZone.trackIndex, &note, &leftEdge))
                {
                    controller->cancelInterruptibleStates();
                    mouseDragMode=MDM_ResizeNote;
                    setInputCapture();
                    draggedNote=note;
                    draggedNoteOriginal=*note;
                    draggedNoteResizeEdge=leftEdge ? NRE_Left : NRE_Right;
                    draggedNoteTrackIndex=mouseZone.trackIndex;
                    startAutoScrollTimer();
                    return true;
                }

                if(noteMovingEnabled() &&
                   view->getNoteAtPosition(event->pos(), mouseZone.trackIndex, &note) && note)
                {
                    controller->cancelInterruptibleStates();
                    mouseDragMode=MDM_MoveNote;
                    setInputCapture();
                    draggedNote=note;
                    draggedNoteOriginal=*note;
                    draggedNoteTrackIndex=mouseZone.trackIndex;
                    draggedNoteAnchorX=event->pos().x();
                    draggedNoteAnchorTick=snappedTickAtX(draggedNoteAnchorX);
                    draggedNoteAnchorMidiNote=noteNumberAtY(mouseZone.trackIndex,event->pos().y());
                    const_cast<View*>(view)->setCursor(Qt::ClosedHandCursor);
                    startAutoScrollTimer();
                    return true;
                }

                if(noteDrawingEnabled())
                {
                    controller->cancelInterruptibleStates();
                    ticksResult=view->getMapper()->cellAreaXToTicks(
                            event->position().toPoint().x() - view->getCellArea().left());

                    const qint64 maxTick=INT_MAX - qint64(docRoot->midiTicksPerWholeNote) *
                                                   EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR;
                    if(ticksResult.cellRightTicks > maxTick)return true;
                    DocEvent* newNote=new DocEvent;
                    newNote->type=DocEvent::E_Note;
                    newNote->tickPosition=ticksResult.cellLeftTicks;
                    newNote->tickLength=ticksResult.cellRightTicks - ticksResult.cellLeftTicks;
                    newNote->noteEventData.noteNumber=noteNumberAtY(mouseZone.trackIndex,
                            event->position().toPoint().y());
                    newNote->noteEventData.velocity=settings->newNoteMidiVelocity;
                    newNote->noteEventData.midiKeypressSerialNo=0;

                    draggedNoteTrackIndex=mouseZone.trackIndex;
                    draggedNoteAnchorX=event->position().toPoint().x();
                    docRoot->trackList[mouseZone.trackIndex]->insertEvent(newNote);
                    const_cast<View*>(view)->updateForModificationsInRange(
                            EditorRange(newNote->tickPosition, mouseZone.trackIndex,
                                        newNote->tickPosition + newNote->tickLength, mouseZone.trackIndex));
                    draggedNote=newNote;
                    draggedNoteOriginal=*newNote;
                    mouseDragMode=MDM_DrawNote;
                    setInputCapture();
                    startAutoScrollTimer();
                    return true;
                }
            }

            // local cell selection (selection rectangle on cells only, not on headers)
            mouseDragMode=MDM_Select;
            setInputCapture();

            ticksResult=view->getMapper()->cellAreaXToTicks(event->position().toPoint().x() - view->getCellArea().left());

            if(modShift && !inImmediateListenMode())
            {
                // Shift was pressed: continue selection and use anchor point for opposite cell
                newState.selection=extendSelectionFromAnchorByMouse(
                        newState.selection,true,
                        EditorRange(
                                ticksResult.cellLeftTicks, mouseZone.trackIndex,
                                ticksResult.cellRightTicks,mouseZone.trackIndex));
            }
            else
            {
                // Begin new local cell selection
                newState.selection.ticksLeft=ticksResult.cellLeftTicks;
                newState.selection.trackTop=mouseZone.trackIndex;
                newState.selection.ticksRight=ticksResult.cellRightTicks;
                newState.selection.trackBottom=mouseZone.trackIndex;

                // Reset anchor cell
                newState.selection.anchor.setTo(
                        ticksResult.cellLeftTicks,
                        ticksResult.cellRightTicks,
                        mouseZone.trackIndex);
            }

            applyStateAndUpdate(newState);
            startAutoScrollTimer();
            return true;
            
        default:
            // do nothing
            break;
        }
    }

    return false;   // event not handled
}

void CS_Navigation::mouseMoveEvent(QMouseEvent* event)
{
    switch(mouseDragMode)
    {
    case MDM_None:
        break;
    case MDM_DrawNote:
    case MDM_ResizeNote:
    case MDM_MoveNote:
        updateDraggedNote(event->position().toPoint());
        if(mouseDragMode == MDM_MoveNote)
            const_cast<View*>(view)->setCursor(Qt::ClosedHandCursor);
        break;
    case MDM_EraseNotes:
        {
            const QPoint currentPosition=event->pos();
            const QPoint delta=currentPosition-lastErasePosition;
            const int distance=qMax(qAbs(delta.x()),qAbs(delta.y()));
            const int steps=qMax(1, (distance + 2) / 3);
            for(int step=1; step <= steps; ++step)
            {
                const QPoint sample=lastErasePosition + QPoint(
                            delta.x()*step/steps, delta.y()*step/steps);
                eraseNoteAtPosition(sample);
            }
            lastErasePosition=currentPosition;
        }
        break;
    case MDM_Select:
        {
            EditorState newState=getEditorState();

            updateSelectionByDragging(newState, event->pos());
            applyStateAndUpdate(newState);
        }
        break;
    case MDM_AdjustDisplayedNoteRange:
        {
            EditorState newState=getEditorState();

            int relativeY=event->pos().y() - mouseDragStartReferencePoint.y();
            double noteHeightInPixels=newState.getNoteHeightInPixels();

            double newCenterMidiNote=(double)relativeY / noteHeightInPixels + mouseDragPreviousValue;

            // Clamp value to allowed range
            if(newCenterMidiNote < 0)
                newCenterMidiNote=0;
            else if(newCenterMidiNote > MIDI_MAX_DATA_VALUE)
                newCenterMidiNote=MIDI_MAX_DATA_VALUE;

            newState.trackStateList[mouseDragStartTrackIndex].centerMidiNote = newCenterMidiNote;
            applyStateAndUpdate(newState);
        }
        break;
    case MDM_AdjustTrackHeight:
        {
            EditorState newState=getEditorState();

            int relativeY=event->pos().y() - mouseDragStartReferencePoint.y();
            double noteHeightInPixels=newState.getNoteHeightInPixels();

            int oldHeightInPixels=(int)(mouseDragPreviousValue * noteHeightInPixels);
            int newHeightInPixels=oldHeightInPixels + relativeY;

            // Clamp to allowed range (exactly like in EditorMapper::refreshDisplayedItemLists)
            if(newHeightInPixels < VIEW_MIN_TRACK_HEIGHT_IN_PIXELS)
                newHeightInPixels=VIEW_MIN_TRACK_HEIGHT_IN_PIXELS;

            double newHeightInNotes=((double)newHeightInPixels) / noteHeightInPixels;

            if(newHeightInNotes < VIEW_MIN_TRACK_HEIGHT_IN_NOTES)
                newHeightInNotes=VIEW_MIN_TRACK_HEIGHT_IN_NOTES;

            if(newHeightInNotes > VIEW_MAX_TRACK_HEIGHT_IN_NOTES)
                newHeightInNotes=VIEW_MAX_TRACK_HEIGHT_IN_NOTES;

            newState.trackStateList[mouseDragStartTrackIndex].heightInNotes = newHeightInNotes;
            applyStateAndUpdate(newState);
        }
        break;
    }
}

void CS_Navigation::mouseReleaseEvent(QMouseEvent* event)
{
    UNUSED(event);

    if(mouseDragMode == MDM_None)return;

    // If enabled, kill autoscroll timer
    if(autoscrollTimerActive)
    {
        killTimer(autoscrollTimerId);
        autoscrollTimerActive=false;
    }

    const bool noteGesture=mouseDragMode == MDM_DrawNote || mouseDragMode == MDM_ResizeNote ||
            mouseDragMode == MDM_MoveNote;
    if(noteGesture)
        finishNoteGesture(true);
    else if(mouseDragMode == MDM_EraseNotes)
        finishEraseGesture();

    mouseDragMode=MDM_None;
    releaseInputCapture();
}

bool CS_Navigation::wheelEvent(QWheelEvent* event)
{
    if(scrollBarHorizontalIsPressed)return false;

    mouseWheelAccu_8thsOfDegrees+=event->angleDelta().y();
    const int MOUSE_WHEEL_DELTA=120;    // 15 degrees * 8, see documentation for QWheelEvent::delta()

    // Check for wheel operations on note range slider
    View::MouseZoneResult mouseZone=view->getMouseZone(event->position().toPoint());
    if(mouseDragMode == MDM_None && mouseZone.zoneType == View::TrackNoteRangeSlider)
    {
        // get working copy of editor track state
        EditorTrackState ts=getEditorState().trackStateList[mouseZone.trackIndex];

        while(mouseWheelAccu_8thsOfDegrees >= MOUSE_WHEEL_DELTA)  // wheel up
        {
            mouseWheelAccu_8thsOfDegrees-=MOUSE_WHEEL_DELTA;
            ts.centerMidiNote+=3;
        }
        while(mouseWheelAccu_8thsOfDegrees <= -MOUSE_WHEEL_DELTA) // wheel down
        {
            mouseWheelAccu_8thsOfDegrees+=MOUSE_WHEEL_DELTA;
            ts.centerMidiNote-=3;
        }

        // Clamp value to allowed range
        if(ts.centerMidiNote < 0)
            ts.centerMidiNote=0;
        else if(ts.centerMidiNote > MIDI_MAX_DATA_VALUE)
            ts.centerMidiNote=MIDI_MAX_DATA_VALUE;

        // Prepare and apply modified editor state
        EditorState newState=getEditorState();
        newState.trackStateList[mouseZone.trackIndex]=ts;

        applyStateAndUpdate(newState);
        return true;
    }

    if(mouseDragMode == MDM_None || mouseDragMode == MDM_Select)
    {
        // Ctrl Shift Function
        //            scroll horizontally
        //        x   scroll vertically
        //   x        horizontal zoom
        //   x    x   vertical zoom

        bool modShift=(app->keyboardModifiers() & Qt::ShiftModifier)   != 0;
        bool modCtrl =(app->keyboardModifiers() & Qt::ControlModifier) != 0;

        bool zoomMode=modCtrl && mouseDragMode == MDM_None;
        if(zoomMode)
        {
            while(mouseWheelAccu_8thsOfDegrees >= MOUSE_WHEEL_DELTA)  // wheel up
            {
                mouseWheelAccu_8thsOfDegrees-=MOUSE_WHEEL_DELTA;

                if(modShift)
                    yZoomSliderWidget->zoomKeyTriggered(ZoomGlassLabel::In);
                else
                    xZoomSliderWidget->zoomKeyTriggered(ZoomGlassLabel::In);
            }
            while(mouseWheelAccu_8thsOfDegrees <= -MOUSE_WHEEL_DELTA) // wheel down
            {
                mouseWheelAccu_8thsOfDegrees+=MOUSE_WHEEL_DELTA;

                if(modShift)
                    yZoomSliderWidget->zoomKeyTriggered(ZoomGlassLabel::Out);
                else
                    xZoomSliderWidget->zoomKeyTriggered(ZoomGlassLabel::Out);
            }
        }
        else    // scroll mode
        {
            if(modShift)    // vertical scroll
            {
                while(mouseWheelAccu_8thsOfDegrees >= MOUSE_WHEEL_DELTA)  // wheel up
                {
                    mouseWheelAccu_8thsOfDegrees-=MOUSE_WHEEL_DELTA;
                    scrollBarVertical->triggerAction(QAbstractSlider::SliderSingleStepSub);
                }
                while(mouseWheelAccu_8thsOfDegrees <= -MOUSE_WHEEL_DELTA) // wheel down
                {
                    mouseWheelAccu_8thsOfDegrees+=MOUSE_WHEEL_DELTA;
                    scrollBarVertical->triggerAction(QAbstractSlider::SliderSingleStepAdd);
                }
            }
            else    // horizontal scroll
            {
                // Modified editor state
                EditorState newState=getEditorState();

                while(mouseWheelAccu_8thsOfDegrees >= MOUSE_WHEEL_DELTA)  // wheel up
                {
                    mouseWheelAccu_8thsOfDegrees-=MOUSE_WHEEL_DELTA;

                    --newState.firstMeasure;
                    if(newState.firstMeasure < 0)
                    {
                        newState.firstMeasure=0;
                        break;
                    }
                }
                while(mouseWheelAccu_8thsOfDegrees <= -MOUSE_WHEEL_DELTA) // wheel down
                {
                    mouseWheelAccu_8thsOfDegrees+=MOUSE_WHEEL_DELTA;
                    ++newState.firstMeasure;
                    if(newState.firstMeasure > docRoot->getMaxFirstMeasure())
                    {
                        newState.firstMeasure=docRoot->getMaxFirstMeasure();
                        break;
                    }
                }

                applyStateAndUpdate(newState);
            }
        }

        return true;
    }

    return false;   // event not handled
}

void CS_Navigation::stateChanged()
{
    if(lastRehearsalMarkerDocument != docRoot ||
       getEditorState().selection != lastRehearsalMarkerSelection)
        lastRehearsalMarkerTick=-1;
    updateScrollAndZoomBars();
    yZoomSliderWidget->setEnabled(docRoot->hasTracks());

    setWriteLengthChecks(getEditorState().writeLength);
}

void CS_Navigation::cancelInputCapture()
{
    Q_ASSERT(mouseDragMode != MDM_None);

    if(autoscrollTimerActive)
    {
        killTimer(autoscrollTimerId);
        autoscrollTimerActive=false;
    }

    EditorState newState=getEditorState();

    if(mouseDragMode == MDM_DrawNote)
    {
        // Discard the temporary note preview when the gesture is cancelled.
        finishNoteGesture(false);
        mouseDragMode=MDM_None;
        releaseInputCapture();
        return;
    }
    if(mouseDragMode == MDM_ResizeNote || mouseDragMode == MDM_MoveNote)
    {
        finishNoteGesture(false);
        mouseDragMode=MDM_None;
        releaseInputCapture();
        return;
    }
    if(mouseDragMode == MDM_EraseNotes)
    {
        // Keep any notes already erased in this stroke as one undoable action.
        finishEraseGesture();
        mouseDragMode=MDM_None;
        releaseInputCapture();
        return;
    }

    switch(mouseDragMode)
    {
    case MDM_None:
    case MDM_Select:
    case MDM_DrawNote:
    case MDM_ResizeNote:
    case MDM_EraseNotes:
    case MDM_MoveNote:
        break;
    case MDM_AdjustDisplayedNoteRange:
        // Cancel operation: Remove preview
        {
            newState.trackStateList[mouseDragStartTrackIndex].centerMidiNote = mouseDragPreviousValue;
            applyStateAndUpdate(newState);
        }
        break;
    case MDM_AdjustTrackHeight:
        // Cancel operation: Remove preview
        {
            newState.trackStateList[mouseDragStartTrackIndex].heightInNotes = mouseDragPreviousValue;
            applyStateAndUpdate(newState);
        }
        break;
    }

    // If enabled, kill autoscroll timer
    if(autoscrollTimerActive)
    {
        killTimer(autoscrollTimerId);
        autoscrollTimerActive=false;
    }

    mouseDragMode=MDM_None;
    releaseInputCapture();
}

void CS_Navigation::setWriteLengthChecks(const WriteLength& writeLength)
{
    bool noneChecked     = writeLength.tupletNominator == 1 && writeLength.tupletDenominator == 1;
    bool dupletsChecked  = writeLength.tupletNominator == 3 && writeLength.tupletDenominator == 2;
    bool tripletsChecked = writeLength.tupletNominator == 2 && writeLength.tupletDenominator == 3;

    ui->actionWrite_CellLength_Duplets         ->setChecked(dupletsChecked);
    ui->actionWrite_CellLength_Triplets        ->setChecked(tripletsChecked);
    ui->actionWrite_CellLength_ArbitraryTuplets->setChecked(!noneChecked &&
                                                            !dupletsChecked &&
                                                            !tripletsChecked);
}

void CS_Navigation::startAutoScrollTimer()
{
    autoscrollPixelAccuToLeft=0;
    autoscrollPixelAccuToRight=0;
    autoscrollPixelAccuUp=0;
    autoscrollPixelAccuDown=0;

    autoscrollTimerId=startTimer(CS_NAVIGATION_MOUSE_AUTOSCROLL_TIMER_INTERVAL);
    autoscrollTimerActive=true;
}

void CS_Navigation::timerEvent(QTimerEvent* event)
{
    if(autoscrollTimerActive && event->timerId() == autoscrollTimerId)
    {
        // get mouse cursor position relative to editor view
        QPoint mousePos=view->mapFromGlobal(QCursor::pos());

        // calculate scroll speed factor relative to count of pixels outside the no-scroll area
        QRect noScrollArea(
                QPoint(view->getCellArea().left(), 0),
                QPoint(view->getCellArea().right() - CS_NAVIGATION_MOUSE_AUTOSCROLL_SCROLL_AREA_WIDTH,
                       view->getCellArea().bottom()));

        int speedToLeft  = CS_NAVIGATION_MOUSE_AUTOSCROLL_SPEED_FACTOR * (noScrollArea.left() - mousePos.x());
        int speedToRight = CS_NAVIGATION_MOUSE_AUTOSCROLL_SPEED_FACTOR * (mousePos.x() - noScrollArea.right());
        int speedUp      = CS_NAVIGATION_MOUSE_AUTOSCROLL_SPEED_FACTOR * (noScrollArea.top() - mousePos.y());
        int speedDown    = CS_NAVIGATION_MOUSE_AUTOSCROLL_SPEED_FACTOR * (mousePos.y() - noScrollArea.bottom());

        // If speed drops below minimum value, do not scroll at all. Too low speed gives bad user feedback.
        if(speedToLeft  < CS_NAVIGATION_MOUSE_AUTOSCROLL_MIN_SCROLL_SPEED) speedToLeft=0;
        if(speedToRight < CS_NAVIGATION_MOUSE_AUTOSCROLL_MIN_SCROLL_SPEED) speedToRight=0;
        if(speedUp      < CS_NAVIGATION_MOUSE_AUTOSCROLL_MIN_SCROLL_SPEED) speedUp=0;
        if(speedDown    < CS_NAVIGATION_MOUSE_AUTOSCROLL_MIN_SCROLL_SPEED) speedDown=0;

        // In global track selection mode, auto-scroll is only allowed in vertical direction.
        if(getEditorState().selection.getSelectionMode() == S_GlobalTrack)
        {
            speedToLeft=0;
            speedToRight=0;
        }

        double ticksPerPixel=getEditorState().getTicksPerPixel(docRoot);

        //  The scroll amount in pixels is accumulated in autoscrollPixelAccu[Left,Right,Up,Down].
        //  If the pixel width of the next/previous measure is reached, scroll one measure to the right/left.

        EditorState newState=getEditorState();

        if(speedToLeft > 0)
        {
            autoscrollPixelAccuToLeft += speedToLeft;
            autoscrollPixelAccuToRight=0;   // reset opposite accumulator

            // While accumulator contains more pixels than the pixel width of the previous measure,
            //  scroll to the left
            while(newState.firstMeasure > 0)
            {
                int previousMeasureTicks = ticksPerMeasure( docRoot->ticksToMeasure(
                        docRoot->measureToTicks( newState.firstMeasure - 1 )).measureProperties);
                int previousMeasurePixelSize = (int)(previousMeasureTicks / ticksPerPixel);

                if(autoscrollPixelAccuToLeft < previousMeasurePixelSize)break;

                --newState.firstMeasure;
                autoscrollPixelAccuToLeft-=previousMeasurePixelSize;
            }
        }
        if(speedToRight > 0)
        {
            autoscrollPixelAccuToRight += speedToRight;
            autoscrollPixelAccuToLeft=0;   // reset opposite accumulator

            // While accumulator contains more pixels than the pixel width of the current first measure,
            //  scroll to the right
            while(newState.firstMeasure < docRoot->getMaxFirstMeasure())
            {
                int firstMeasureTicks = ticksPerMeasure( docRoot->ticksToMeasure(
                        docRoot->measureToTicks( newState.firstMeasure )).measureProperties);
                int firstMeasurePixelSize = (int)(firstMeasureTicks / ticksPerPixel);

                if(autoscrollPixelAccuToRight < firstMeasurePixelSize)break;

                ++newState.firstMeasure;
                autoscrollPixelAccuToRight-=firstMeasurePixelSize;
            }
        }

        if(speedUp > 0)
        {
            autoscrollPixelAccuUp += speedUp;
            autoscrollPixelAccuDown=0;   // reset opposite accumulator

            // While accumulator contains more pixels than the pixel height of the previous track,
            //  scroll up
            while(newState.firstTrack > 0)
            {
                int previousTrackHeightInPixels=newState.getTrackHeightInPixels(newState.firstTrack - 1);

                if(autoscrollPixelAccuUp < previousTrackHeightInPixels)break;

                --newState.firstTrack;
                autoscrollPixelAccuUp-=previousTrackHeightInPixels;
            }
        }

        if(speedDown > 0)
        {
            autoscrollPixelAccuDown += speedDown;
            autoscrollPixelAccuUp=0;   // reset opposite accumulator

            // While accumulator contains more pixels than the pixel height of the current first track,
            //  scroll down

            EditorMapper tempMapper(view, &newState);

            // Stop when last track is the only visible track
            while(newState.firstTrack < docRoot->trackList.size() - 1)
            {
                tempMapper.refreshDisplayedItemLists();

                // Stop when last track is completely visible
                DisplayedTrack* dtLast=tempMapper.getDisplayedTrackList().last();

                if(dtLast->trackIndex == docRoot->trackList.size()-1 &&
                   dtLast->cellBottomY <= view->getCellArea().height() - VIEW_END_OF_SCREEN_GRADIENT_SIZE)
                    break;

                int firstTrackHeightInPixels=newState.getTrackHeightInPixels(newState.firstTrack);

                if(autoscrollPixelAccuDown < firstTrackHeightInPixels)break;

                ++newState.firstTrack;
                autoscrollPixelAccuDown-=firstTrackHeightInPixels;
            }
        }

        // when user is selecting with left mouse button down, also update selection
        if(mouseDragMode == MDM_Select)
        {
            updateSelectionByDragging(newState, mousePos);
        }

        applyStateAndUpdate(newState);

        if(mouseDragMode == MDM_DrawNote || mouseDragMode == MDM_ResizeNote ||
           mouseDragMode == MDM_MoveNote)
            updateDraggedNote(mousePos);
    }
}

void CS_Navigation::actionEdit_SelectAll_Triggered()
{
    // set global measure selection from start to last filled measure
    EditorState newState=getEditorState();
    newState.setGlobalMeasureSelection(0, docRoot->getMaxMeasure() + 1, docRoot);
    applyStateAndUpdate(newState);
}

void CS_Navigation::actionEdit_DrawNotes_Triggered()
{
    // The controller reverses Qt's automatic toggle for checkable actions so
    // each action handler can explicitly commit its own checked state.
    ui->actionEdit_DrawNotes->setChecked(!ui->actionEdit_DrawNotes->isChecked());
    if(ui->actionEdit_DrawNotes->isChecked())
    {
        ui->actionEdit_MoveNotes->setChecked(false);
        ui->actionEdit_EraseNotes->setChecked(false);
    }
    const_cast<View*>(view)->restoreMouseCursor();
}

void CS_Navigation::actionEdit_MoveNotes_Triggered()
{
    ui->actionEdit_MoveNotes->setChecked(!ui->actionEdit_MoveNotes->isChecked());
    if(ui->actionEdit_MoveNotes->isChecked())
    {
        ui->actionEdit_DrawNotes->setChecked(false);
        ui->actionEdit_EraseNotes->setChecked(false);
    }
    const_cast<View*>(view)->restoreMouseCursor();
}

void CS_Navigation::actionEdit_EraseNotes_Triggered()
{
    ui->actionEdit_EraseNotes->setChecked(!ui->actionEdit_EraseNotes->isChecked());
    if(ui->actionEdit_EraseNotes->isChecked())
    {
        ui->actionEdit_MoveNotes->setChecked(false);
        ui->actionEdit_DrawNotes->setChecked(false);
    }
    const_cast<View*>(view)->restoreMouseCursor();
}

void CS_Navigation::actionView_HorizontalZoom_ZoomIn_Triggered()
{
    xZoomSliderWidget->zoomKeyTriggered(ZoomGlassLabel::In);
}

void CS_Navigation::actionView_HorizontalZoom_ZoomOut_Triggered()
{
    xZoomSliderWidget->zoomKeyTriggered(ZoomGlassLabel::Out);
}

void CS_Navigation::actionView_VerticalZoom_ZoomIn_Triggered()
{
    yZoomSliderWidget->zoomKeyTriggered(ZoomGlassLabel::In);
}

void CS_Navigation::actionView_VerticalZoom_ZoomOut_Triggered()
{
    yZoomSliderWidget->zoomKeyTriggered(ZoomGlassLabel::Out);
}

void CS_Navigation::actionView_FitAllTracks_Triggered()
{
    // Fit the complete song horizontally and all tracks vertically.
    EditorState newState=getEditorState();
    newState.firstMeasure=0;
    newState.firstTrack=0;

    // Use whole measures so the overview starts at measure 1 and ends at the
    // bar line after the last event. Leave a small margin at the right edge.
    const int availableWidth=view->getCellArea().width();
    const int endTicks=docRoot->measureToTicks(docRoot->getMaxMeasure() + 1);
    if(availableWidth > 0 && endTicks > 0)
    {
        const double ticksPerPixel=(double)endTicks / (availableWidth * 0.98);
        newState.xZoomSliderValue=EditorState::getXZoomSliderValue(ticksPerPixel, docRoot);
    }

    // Calculate y-zoom value that allows to fit in all tracks
    newState.yZoomSliderValue=ZOOM_SLIDER_WIDGET_MAX_VALUE; // start with maximum zoom

    // Subtract height of all track separators from available cell area height
    int availableHeight=view->getCellArea().height() - VIEW_END_OF_SCREEN_GRADIENT_SIZE;
    availableHeight-=(docRoot->trackList.size() - 1) * VIEW_TRACK_SEPARATOR_Y_INBETWEEN;

    // Iteratively try to zoom every track into available height.
    //  Some track heights may get clamped to the minimum track height.
    //  In this case, subtract this constant height from availableHeight
    //  and make a new linear approximation of the required y-zoom value.

    // Define a maximum retry count to avoid any (possible existing?) effects by rounding errors.
    //  Retry should never be exceeded.
    int retry=0;
    while(retry < docRoot->trackList.size() * 2)
    {
        // sum up all track heights of not size-clamped and size-clamped tracks
        int totalUnclampedTrackHeights=0;
        int totalClampedTrackHeights=0;

        for(int trackIndex=0; trackIndex < docRoot->trackList.size(); ++trackIndex)
        {
            int trackHeightInPixels=newState.getTrackHeightInPixels(trackIndex);
            if(trackHeightInPixels == VIEW_MIN_TRACK_HEIGHT_IN_PIXELS)
                totalClampedTrackHeights += trackHeightInPixels;
            else
                totalUnclampedTrackHeights += trackHeightInPixels;
        }

        // stop if all tracks fit into cell area or all tracks are size-clamped
        if(totalUnclampedTrackHeights + totalClampedTrackHeights <= availableHeight ||
           totalUnclampedTrackHeights == 0)
            break;

        // tracks still don't fit

        // Evenly separate available space to all currently not size-clamped tracks
        int availableHeightForLinearZoom = availableHeight - totalClampedTrackHeights;
        if(availableHeightForLinearZoom <= 0)
        {
            newState.yZoomSliderValue=0;
            break;
        }
        double heightFactor=(double)availableHeightForLinearZoom / totalUnclampedTrackHeights;

        double newNoteHeightInPixels = newState.getNoteHeightInPixels() * heightFactor;

        // Calculate new y-zoom factor
        newState.yZoomSliderValue = EditorState::getZoomSliderValue(newNoteHeightInPixels,
                                                                    VIEW_NOTE_HEIGHT_IN_PIXELS_MIN,
                                                                    VIEW_NOTE_HEIGHT_IN_PIXELS_MAX,
                                                                    VIEW_NOTE_HEIGHT_IN_PIXELS_EXP_BASE);

        if(newState.yZoomSliderValue == 0)
            break;    // reached minimum possible zoom

        ++retry;
    }

    applyStateAndUpdate(newState);
}

void CS_Navigation::actionView_NoteNames_Triggered()
{
    VolatileEditorState newVolatileState=getVolatileEditorState();
    newVolatileState.showNoteNames^=1;
    applyVolatileStateAndUpdate(newVolatileState);

    ui->actionView_NoteNames->setChecked(newVolatileState.showNoteNames);
}

void CS_Navigation::actionWrite_CellLength_Double_Triggered()
{
    // increase write length
    if(getEditorState().writeLength.denominator >= 2)
    {
        EditorState newState=getEditorState();
        newState.writeLength.denominator/=2;
        newState.adjustForChangedWriteLength(docRoot);
        applyStateAndUpdate(newState);
    }
}

void CS_Navigation::actionWrite_CellLength_Halve_Triggered()
{
    // decrease write length
    if(getEditorState().writeLength.denominator < EDITOR_MAX_WRITELENGTH_DENOMINATOR)
    {
        EditorState newState=getEditorState();
        newState.writeLength.denominator*=2;
        newState.adjustForChangedWriteLength(docRoot);
        applyStateAndUpdate(newState);
    }
}

void CS_Navigation::actionWrite_CellLength_Triplets_Triggered()
{
    EditorState newState=getEditorState();
    if(getEditorState().writeLength.tupletDenominator == 3 && getEditorState().writeLength.tupletNominator == 2)
    {
        // toggle off
        newState.writeLength.tupletDenominator=1;
        newState.writeLength.tupletNominator=1;
    }
    else
    {
        // toggle on
        newState.writeLength.tupletNominator=2;
        newState.writeLength.tupletDenominator=3;
    }
    newState.adjustForChangedWriteLength(docRoot);
    applyStateAndUpdate(newState);
}

void CS_Navigation::actionWrite_CellLength_Duplets_Triggered()
{
    EditorState newState=getEditorState();
    if(getEditorState().writeLength.tupletDenominator == 2 && getEditorState().writeLength.tupletNominator == 3)
    {
        // toggle off
        newState.writeLength.tupletDenominator=1;
        newState.writeLength.tupletNominator=1;
    }
    else
    {
        // toggle on
        newState.writeLength.tupletNominator=3;
        newState.writeLength.tupletDenominator=2;
    }

    newState.adjustForChangedWriteLength(docRoot);
    applyStateAndUpdate(newState);
}

void CS_Navigation::actionWrite_CellLength_ArbitraryTuplets_Triggered()
{
    execWriteLengthDialog(true);
}

int CS_Navigation::getHorizontalScrollBarRange() const
{
    // The range is the maximum of the maximum measure in the document
    // and the current leftmost displayed measure.
    int maxRange=qMax(docRoot->getMaxMeasure(), getEditorState().firstMeasure);

    // Minimum range is 1 so scrollbar never gets disabled.
    return qBound(1,maxRange,docRoot->getMaxFirstMeasure());
}

void CS_Navigation::updateScrollAndZoomBars()
{
    scrollBarHorizontal->blockSignals(true);
    scrollBarVertical->blockSignals(true);
    xZoomSliderWidget->blockSignals(true);
    yZoomSliderWidget->blockSignals(true);

    // Set fixed page step.
    // Page left, right, up, down events will be handled manually in scrollBarHorizontalActionTriggered(...)
    scrollBarHorizontal->setPageStep(1);
    scrollBarVertical->setPageStep(1);

    // Do not set range of scrollBarHorizontal as long as drag operation is in progress
    if(!scrollBarHorizontalIsPressed)
        scrollBarHorizontal->setRange(0, getHorizontalScrollBarRange());

    scrollBarVertical->setRange(0, docRoot->trackList.size());

    scrollBarHorizontal->setSliderPosition(getEditorState().firstMeasure);
    scrollBarVertical->setSliderPosition(getEditorState().firstTrack);

    xZoomSliderWidget->setValue(getEditorState().xZoomSliderValue);
    yZoomSliderWidget->setValue(getEditorState().yZoomSliderValue);

    scrollBarHorizontal->blockSignals(false);
    scrollBarVertical->blockSignals(false);
    xZoomSliderWidget->blockSignals(false);
    yZoomSliderWidget->blockSignals(false);
}

void CS_Navigation::scrollBarHorizontalActionTriggered(int action)
{
    EditorState newState=getEditorState();

    EditorMapper tempMapper(view,&newState);
    tempMapper.refreshDisplayedItemLists();

    switch(action)
    {
    case QAbstractSlider::SliderSingleStepAdd:
        if(newState.firstMeasure < docRoot->getMaxFirstMeasure())
            ++newState.firstMeasure;
        break;
    case QAbstractSlider::SliderSingleStepSub:
        if(newState.firstMeasure > 0)
            --newState.firstMeasure;
        break;
    case QAbstractSlider::SliderPageStepAdd:
        scrollPageRight(newState, tempMapper);
        break;
    case QAbstractSlider::SliderPageStepSub:
        scrollPageLeft(newState, tempMapper);
        break;
    case QAbstractSlider::SliderToMinimum:
        newState.firstMeasure=0;
        break;
    case QAbstractSlider::SliderToMaximum:
        newState.firstMeasure=scrollBarHorizontal->maximum();
        break;
    case QAbstractSlider::SliderMove:
        newState.firstMeasure=scrollBarHorizontal->sliderPosition();

        // slider position may be out of bounds (but never the value)
        if(newState.firstMeasure < 0)newState.firstMeasure=0;
        break;
    }

    newState.firstMeasure=qBound(0,newState.firstMeasure,docRoot->getMaxFirstMeasure());

    applyStateAndUpdate(newState);
}

void CS_Navigation::scrollPageRight(EditorState& newState, EditorMapper& tempMapper) const
{
    // Determine current number of measures on page and scroll this number of steps
    int lastVisibleMeasure=tempMapper.getLastVisibleMeasure();

    if(lastVisibleMeasure == newState.firstMeasure)
        newState.firstMeasure=lastVisibleMeasure+1;    // scroll at least one measure
    else
        newState.firstMeasure=lastVisibleMeasure;      // scroll to last measure that is partly visible on the right

    if(newState.firstMeasure > docRoot->getMaxFirstMeasure())
        newState.firstMeasure=docRoot->getMaxFirstMeasure();
}

void CS_Navigation::scrollPageLeft(EditorState& newState, EditorMapper& tempMapper) const
{
    // Goal: Scroll back until current first measure is out of screen, then scroll back 1 measure.
    // As the coordinate tables do not apply for measures left from current firstMeasure,
    // we approximate the measure and then adjust stepwise as necessary (in worst case = time signature
    // changes drastically and many cells on screen) this might be computationally expensive, but it is
    // seldom the case.

    if(newState.firstMeasure == 0)return;    // at leftmost measure

    // Approximation
    int prevFirstMeasure=newState.firstMeasure;
    int rightmostMeasure=
            tempMapper.getDisplayedMeasureList()[tempMapper.getDisplayedMeasureList().size()-1]->measureIndex;

    newState.firstMeasure-=(rightmostMeasure - newState.firstMeasure);
    if(newState.firstMeasure < 0)newState.firstMeasure=0;

    tempMapper.refreshDisplayedItemLists();

    // Check whether rightmost visible cell is within previous first measure.
    // If not, adjust the new first measure and repeat.

    int lastVisibleMeasure=tempMapper.getLastVisibleMeasure();

    while(lastVisibleMeasure > prevFirstMeasure) // Too less measures scrolled => scroll more
    {
        if(newState.firstMeasure == 0)break;

        --newState.firstMeasure;

        tempMapper.refreshDisplayedItemLists();
        lastVisibleMeasure=tempMapper.getLastVisibleMeasure();
    }
    while(lastVisibleMeasure < prevFirstMeasure) // Too many measures scrolled => scroll less
    {
        ++newState.firstMeasure;

        tempMapper.refreshDisplayedItemLists();
        lastVisibleMeasure=tempMapper.getLastVisibleMeasure();
    }
    // Scroll at least 1 measure to the left.
    if(newState.firstMeasure >= prevFirstMeasure)newState.firstMeasure = prevFirstMeasure-1;
}

void CS_Navigation::scrollBarHorizontalSliderPressed()
{
    // Set flag preventing the adjustment of horizontal scroll bar range during dragging the slider
    scrollBarHorizontalIsPressed=true;
    setInputCapture();
}

void CS_Navigation::scrollBarHorizontalSliderReleased()
{
    // Reset flag preventing the adjustment of horizontal scroll bar range during dragging the slider
    scrollBarHorizontalIsPressed=false;
    releaseInputCapture();

    // Change scroll bar range now
    scrollBarHorizontal->setRange(0, getHorizontalScrollBarRange());
}

void CS_Navigation::scrollBarVerticalActionTriggered(int action)
{
    switch(action)
    {
    case QAbstractSlider::SliderPageStepAdd:    // page down
        {
            if(view->getMapper()->getDisplayedTrackList().size() == 0)
                break;

            DisplayedTrack* dtLast=view->getMapper()->getDisplayedTrackList().last();

            // If last document track is completely visible, scroll to behind last track
            if(dtLast->trackIndex == docRoot->trackList.size()-1 &&
               dtLast->cellBottomY <= view->getCellArea().height() - VIEW_END_OF_SCREEN_GRADIENT_SIZE)
            {
                scrollBarVertical->setSliderPosition(docRoot->trackList.size());
            }
            else
            {
                // scroll to last track partly on page, but scroll at least one track
                scrollBarVertical->setSliderPosition(qMax(dtLast->trackIndex,
                                                          getEditorState().firstTrack + 1));
            }
        }
        break;
    case QAbstractSlider::SliderPageStepSub:    // page up
        {
            EditorState newState=getEditorState();
            EditorMapper tempMapper(view,&newState);

            // Scroll up until current first track is not visible any more or or only partly visible.
            while(newState.firstTrack > 0)
            {
                --newState.firstTrack;
                tempMapper.refreshDisplayedItemLists();

                DisplayedTrack* dtLast=tempMapper.getDisplayedTrackList().last();

                if(dtLast->trackIndex <= getEditorState().firstTrack &&
                   dtLast->cellBottomY > view->getCellArea().height() - VIEW_END_OF_SCREEN_GRADIENT_SIZE)
                    break;
            }

            // Scroll down again until current first track is at least partly visible,
            // but scroll at least one track
            while(newState.firstTrack < getEditorState().firstTrack - 1)
            {
                DisplayedTrack* dtLast=tempMapper.getDisplayedTrackList().last();

                if(getEditorState().firstTrack == docRoot->trackList.size())
                {
                    // Scrolling up from maximum scroll position (no tracks on screen):
                    //  Last track must be completely visible
                    if(dtLast->trackIndex == docRoot->trackList.size() - 1 &&
                       dtLast->cellBottomY <= view->getCellArea().height() - VIEW_END_OF_SCREEN_GRADIENT_SIZE)
                        break;
                }
                else
                {
                    // Scrolling up from higher scroll position (at least one track on screen):
                    if(dtLast->trackIndex >= getEditorState().firstTrack)
                        break;
                }

                ++newState.firstTrack;
                tempMapper.refreshDisplayedItemLists();
            }

            scrollBarVertical->setSliderPosition(newState.firstTrack);
        }
        break;
    default:
        // other actions: stick to default behaviour
        break;
    }
}

void CS_Navigation::scrollBarVerticalValueChanged(int value)
{
    UNUSED(value);

    // Modified editor state
    EditorState newState=getEditorState();

    if(newState.firstTrack != scrollBarVertical->value())
    {
        newState.firstTrack=scrollBarVertical->value();
        applyStateAndUpdate(newState);
    }
}

void CS_Navigation::updateSelectionByDragging(EditorState& newState, const QPoint& mousePos)
{
    Q_ASSERT(mouseDragMode == MDM_Select);

    SelectionModeType selMode=newState.selection.getSelectionMode();

    // Find cell/measure/track nearest to mouse pointer

    int nearestTrack;
    if(selMode == S_GlobalMeasure)
    {
        nearestTrack=0;
    }
    else
    {
        // find track nearest to mouse pointer in y-direction
        int cellAreaY=mousePos.y() - view->getCellArea().top();

        nearestTrack=-1;
        for(int i=0; i < view->getMapper()->getDisplayedTrackList().size(); ++i)
        {
            DisplayedTrack* dt=view->getMapper()->getDisplayedTrackList()[i];
            if(cellAreaY < dt->cellBottomY + VIEW_TRACK_SEPARATOR_Y_INBETWEEN/2)
            {
                nearestTrack=dt->trackIndex;
                break;
            }
        }
        if(nearestTrack == -1)nearestTrack=docRoot->trackList.size()-1;
    }

    int nearestCellLeftTicks,nearestCellRightTicks;
    if(selMode == S_GlobalTrack)
    {
        nearestCellLeftTicks=0;
        nearestCellRightTicks=0;
    }
    else
    {
        // find ticks of cell nearest to mouse pointer in x-direction
        int cellAreaX=mousePos.x() - view->getCellArea().left();
        if(cellAreaX < 0)cellAreaX=0;
        if(cellAreaX >= view->getCellArea().width())cellAreaX=view->getCellArea().width()-1;

        CellAreaXToTicksResult r=view->getMapper()->cellAreaXToTicks(cellAreaX);

        nearestCellLeftTicks=r.cellLeftTicks;
        nearestCellRightTicks=r.cellRightTicks;
    }

    if(inImmediateListenMode())
    {
        // During immediate listen mode, clicks on measure or track headers are disabled,
        //  so we must be in local cell selection mode
        Q_ASSERT(newState.selection.getSelectionMode() == S_LocalCells);

        // Begin new local cell selection
        newState.selection.ticksLeft=nearestCellLeftTicks;
        newState.selection.trackTop=nearestTrack;
        newState.selection.ticksRight=nearestCellRightTicks;
        newState.selection.trackBottom=nearestTrack;

        // Reset anchor cell
        newState.selection.anchor.setTo(
                nearestCellLeftTicks,
                nearestCellRightTicks,
                nearestTrack);
    }
    else
    {
        newState.selection=extendSelectionFromAnchorByMouse(
                newState.selection,false,
                EditorRange(nearestCellLeftTicks,nearestTrack,
                            nearestCellRightTicks,nearestTrack));
    }
}

EditorSelection CS_Navigation::extendSelectionFromAnchorByMouse(const EditorSelection& sel, bool allowModeChange, const EditorRange& requestedRange)
{
    // Extending the selection from the current anchor point can imply changing the selection type
    //   (global measure / global track / local cells)

    EditorSelection newSelection(sel);

    SelectionModeType newSelMode;
    if(allowModeChange)
    {
        // for shift + mouse click
        newSelMode=requestedRange.getSelectionMode();
    }
    else
    {
        // for mouse left button dragging: remain in current selection mode
        newSelMode=sel.getSelectionMode();
    }

    if(newSelMode == S_GlobalMeasure)
    {
        newSelection.trackTop=0;
        newSelection.trackBottom=INT_MAX;
    }
    else
    {
        if(sel.anchor.track <= requestedRange.trackTop)
        {
            newSelection.trackTop=sel.anchor.track;
            newSelection.trackBottom=requestedRange.trackBottom;
        }
        else
        {
            newSelection.trackTop=requestedRange.trackTop;
            newSelection.trackBottom=sel.anchor.track;
        }
    }

    if(newSelMode == S_GlobalTrack)
    {
        newSelection.ticksLeft=0;
        newSelection.ticksRight=INT_MAX;
    }
    else
    {
        if(sel.anchor.ticksLeft <= requestedRange.ticksLeft)
        {
            newSelection.ticksLeft=sel.anchor.ticksLeft;
            newSelection.ticksRight=requestedRange.ticksRight;
        }
        else
        {
            newSelection.ticksLeft=requestedRange.ticksLeft;
            newSelection.ticksRight=sel.anchor.ticksRight;
        }

        if(newSelMode == S_GlobalMeasure)
        {
            // global measure selection: always extend selection to full measures

            newSelection.ticksLeft  = docRoot->roundDownTicksToMeasureBorder(newSelection.ticksLeft);
            newSelection.ticksRight = docRoot->roundUpTicksToMeasureBorder(newSelection.ticksRight);
        }
    }

    return newSelection;
}

void CS_Navigation::initiateSelectionByKeyboard(KeyboardSelectionActionType action)
{
    // Determine current selection mode
    SelectionModeType selMode=getEditorState().selection.getSelectionMode();

    // Modified editor state
    EditorState newState=getEditorState();

    // Modify selection as appropriate
    switch(selMode)
    {
    case S_GlobalMeasure:
        if(action == KSA_Up || action == KSA_Down)  // vertical move
        {
            // Revert to local cell selection mode.
            selMode=S_LocalCells;

            // Redefine selection to anchor cell only.
            newState.selection.ticksLeft   = newState.selection.anchor.ticksLeft;
            newState.selection.ticksRight  = newState.selection.anchor.ticksRight;
            newState.selection.trackTop    = newState.selection.anchor.track;
            newState.selection.trackBottom = newState.selection.anchor.track;
        }
        else    // horizontal move
        {
            // Stay in global measure selection mode. Reset selection to measure where anchor is in.
            EditorSelectionSupportPoint sp;
            sp.ticksLeft   = docRoot->roundDownTicksToMeasureBorder(newState.selection.anchor.ticksLeft);
            sp.ticksRight  = docRoot->roundUpTicksToMeasureBorder(newState.selection.anchor.ticksLeft + 1);

            sp=modifySelectionSupportPointAndScrollPosByKeyboard(newState,S_GlobalMeasure,sp,action);

            // Reset anchor
            newState.selection.anchor.ticksLeft  = docRoot->roundDownTicksToMeasureBorder(sp.ticksLeft);
            newState.selection.anchor.ticksRight = docRoot->roundUpTicksToCellBorder(sp.ticksLeft + 1,
                                                                                     newState.writeLength);

            // Redefine selection
            newState.selection.ticksLeft=sp.ticksLeft;
            newState.selection.ticksRight=sp.ticksRight;
        }
        break;
    case S_GlobalTrack:
        if(action != KSA_Up && action != KSA_Down)  // horizontal move
        {
            // Revert to local cell selection mode.
            selMode=S_LocalCells;

            // Redefine selection to anchor cell only.
            newState.selection.ticksLeft   = newState.selection.anchor.ticksLeft;
            newState.selection.ticksRight  = newState.selection.anchor.ticksRight;
            newState.selection.trackTop    = newState.selection.anchor.track;
            newState.selection.trackBottom = newState.selection.anchor.track;
        }
        else    // vertical move
        {
            // Stay in global track selection mode. Reset selection to track where anchor is in.
            EditorSelectionSupportPoint sp;
            sp.track=getEditorState().selection.anchor.track;

            sp=modifySelectionSupportPointAndScrollPosByKeyboard(newState,S_GlobalTrack,sp,action);

            // Reset anchor
            newState.selection.anchor.track=sp.track;

            // Redefine selection
            newState.selection.trackTop=sp.track;
            newState.selection.trackBottom=sp.track;
        }
        break;
    case S_LocalCells:
        // Move anchor
        EditorSelectionSupportPoint sp=
                modifySelectionSupportPointAndScrollPosByKeyboard(newState, S_LocalCells,
                                                      getEditorState().selection.anchor, action);

        // Reset anchor
        newState.selection.anchor=sp;

        // Redefine selection: copy tick settings from anchor cell, track settings see below
        newState.selection.ticksLeft   = newState.selection.anchor.ticksLeft;
        newState.selection.ticksRight  = newState.selection.anchor.ticksRight;

        if(newState.selection.trackTop == newState.selection.trackBottom)
        {
            // only one track selected, copy track index from anchor cell
            newState.selection.trackTop    = newState.selection.anchor.track;
            newState.selection.trackBottom = newState.selection.anchor.track;
        }
        else
        {
            // Multiple tracks selected, proceed depending on action type
            switch(action)
            {
            case KSA_Up:
                // Set track to currently topmost selected track
                newState.selection.trackBottom  = newState.selection.trackTop;
                newState.selection.anchor.track = newState.selection.trackTop;
                break;
            case KSA_Down:
                // Set track to currently bottommost selected track
                newState.selection.trackTop     = newState.selection.trackBottom;
                newState.selection.anchor.track = newState.selection.trackBottom;
                break;
            default:
                // horizontal movement: do not modify the track indices
                break;
            }
        }
        break;
    }

    if(selMode == S_GlobalMeasure)
    {
        // Scroll measure with anchor cell into view
        scrollRangeIntoView(newState,
                EditorRange(docRoot->roundDownTicksToMeasureBorder(newState.selection.anchor.ticksLeft),
                            newState.selection.anchor.track,
                            docRoot->roundUpTicksToMeasureBorder(newState.selection.anchor.ticksLeft + 1),
                            newState.selection.anchor.track));
    }
    else
    {
        // Scroll anchor cell into view
        scrollRangeIntoView(newState,
                EditorRange(newState.selection.anchor.ticksLeft,  newState.selection.anchor.track,
                            newState.selection.anchor.ticksRight, newState.selection.anchor.track));
    }

    applyStateAndUpdate(newState);
}

void CS_Navigation::modifySelectionByKeyboard(KeyboardSelectionActionType action)
{
    // Determine current selection mode
    SelectionModeType selMode=getEditorState().selection.getSelectionMode();

    // 1st support point is sel.anchor, now get 2nd support point
    EditorSelectionSupportPoint sp2=getSelectionSecondSupportPoint(getEditorState().selection,
                                                                   getEditorState().writeLength);

    // Modified editor state
    EditorState newState=getEditorState();

    // Modify it by keyboard action
    sp2=modifySelectionSupportPointAndScrollPosByKeyboard(newState,selMode,sp2,action);

    // Construct new selection from both support points (anchor and 2nd-support-point).
    // Selection by keyboard never changes the selection mode.

    switch(selMode)
    {
    case S_GlobalMeasure:
        {
            int anchorMeasureTicksLeft  = docRoot->roundDownTicksToMeasureBorder(getEditorState().selection.anchor.ticksLeft);
            int anchorMeasureTicksRight = docRoot->roundUpTicksToMeasureBorder(getEditorState().selection.anchor.ticksLeft + 1);

            newState.selection.ticksLeft=qMin(sp2.ticksLeft,anchorMeasureTicksLeft);
            newState.selection.ticksRight=qMax(sp2.ticksRight,anchorMeasureTicksRight);

            // only scroll in horizontal direction
            scrollRangeIntoView(newState,
                    EditorRange(sp2.ticksLeft,getEditorState().firstTrack,sp2.ticksRight,getEditorState().firstTrack));
            break;
        }
    case S_GlobalTrack:
        {
            newState.selection.trackTop=qMin(sp2.track,getEditorState().selection.anchor.track);
            newState.selection.trackBottom=qMax(sp2.track,getEditorState().selection.anchor.track);

            int leftmostMeasureTicks=docRoot->measureToTicks(getEditorState().firstMeasure);

            // only scroll in vertical direction
            scrollRangeIntoView(newState,
                    EditorRange(leftmostMeasureTicks,sp2.track,leftmostMeasureTicks,sp2.track));
            break;
        }
    case S_LocalCells:
        {
            newState.selection.ticksLeft=qMin(sp2.ticksLeft,getEditorState().selection.anchor.ticksLeft);
            newState.selection.ticksRight=qMax(sp2.ticksRight,getEditorState().selection.anchor.ticksRight);

            newState.selection.trackTop=qMin(sp2.track,getEditorState().selection.anchor.track);
            newState.selection.trackBottom=qMax(sp2.track,getEditorState().selection.anchor.track);

            // scroll in horizontal + vertical direction allowed
            scrollRangeIntoView(newState,
                    EditorRange(sp2.ticksLeft,sp2.track,sp2.ticksRight,sp2.track));
            break;
        }
    }

    applyStateAndUpdate(newState);
}

EditorSelectionSupportPoint CS_Navigation::modifySelectionSupportPointAndScrollPosByKeyboard(EditorState& newState, SelectionModeType selectionMode, const EditorSelectionSupportPoint& sp, KeyboardSelectionActionType action) const
{
    EditorSelectionSupportPoint p=sp;

    switch(selectionMode)
    {
    case S_GlobalMeasure:
        switch(action)
        {
        case KSA_Left:
            if(p.ticksLeft > 0)
            {
                p.ticksRight=p.ticksLeft;
                p.ticksLeft=docRoot->roundDownTicksToMeasureBorder(p.ticksLeft - 1);
            }
            break;
        case KSA_Right:
            {
                TicksToMeasureResult r=docRoot->ticksToMeasure(p.ticksRight);
                p.ticksLeft=p.ticksRight;
                p.ticksRight=docRoot->measureToTicks(r.measureIndex + 1);
            }
            break;
        case KSA_Up:
        case KSA_Down:
            // do nothing
            break;
        case KSA_PageLeft:
        case KSA_PageRight:
            {
                // Use cell position as reference point (first scroll it into view if not currently visible)
                EditorMapper tempMapper(view,&newState);
                tempMapper.refreshDisplayedItemLists();

                scrollRangeIntoView(newState,
                        EditorRange(p.ticksLeft,-1,p.ticksRight,-1));
                TicksToViewXResult rViewX=tempMapper.ticksToViewX(p.ticksLeft);

                if(action == KSA_PageLeft)
                {
                    if(newState.firstMeasure == 0)
                    {
                        // No scrolling possible, set support point to first measure
                        p.ticksLeft=0;
                        p.ticksRight=docRoot->measureToTicks(1);
                        break;
                    }

                    scrollPageLeft(newState, tempMapper);
                }
                else scrollPageRight(newState, tempMapper);
                tempMapper.refreshDisplayedItemLists();

                // Find the cell under reference point after scrolling
                CellAreaXToTicksResult rCell=
                        tempMapper.cellAreaXToTicks((rViewX.cellLeftX + rViewX.cellRightX) / 2 -
                                                    view->getCellArea().left());

                p.ticksLeft  = docRoot->roundDownTicksToMeasureBorder(rCell.cellLeftTicks);
                p.ticksRight = docRoot->roundUpTicksToMeasureBorder(p.ticksLeft + 1);
            }
            break;
        case KSA_Home:
            p.ticksLeft=0;
            p.ticksRight=docRoot->measureToTicks(1);
            break;
        case KSA_End:
            {
                int maxDocMeasure=docRoot->getMaxMeasure();
                int endTargetTicks=docRoot->measureToTicks(maxDocMeasure + 1);
                p.ticksLeft=endTargetTicks;
                p.ticksRight=docRoot->measureToTicks(maxDocMeasure + 2);
            }
            break;
        }
        break;
    case S_GlobalTrack:
        switch(action)
        {
        case KSA_Up:
            if(p.track > 0) --p.track;
            break;
        case KSA_Down:
            if(p.track < docRoot->trackList.size()-1) ++p.track;
            break;
        case KSA_Left:
        case KSA_Right:
        case KSA_PageLeft:
        case KSA_PageRight:
        case KSA_Home:
        case KSA_End:
            // do nothing
            break;
        }
        break;
    case S_LocalCells:
        switch(action)
        {
        case KSA_Left:
            if(p.ticksLeft > 0)
            {
                p.ticksRight=p.ticksLeft;
                p.ticksLeft=docRoot->roundDownTicksToCellBorder(p.ticksLeft - 1,newState.writeLength);
            }
            break;
        case KSA_Right:
            p.ticksLeft=p.ticksRight;
            p.ticksRight=docRoot->roundUpTicksToCellBorder(p.ticksRight + 1,newState.writeLength);
            break;
        case KSA_Up:
            if(p.track > 0) --p.track;
            break;
        case KSA_Down:
            if(p.track < docRoot->trackList.size()-1) ++p.track;
            break;
        case KSA_PageLeft:
        case KSA_PageRight:
            {
                // Use cell position as reference point (first scroll it into view if not currently visible)
                scrollRangeIntoView(newState,
                        EditorRange(p.ticksLeft,-1,p.ticksRight,-1));

                EditorMapper tempMapper(view,&newState);
                tempMapper.refreshDisplayedItemLists();

                TicksToViewXResult rViewX=tempMapper.ticksToViewX(p.ticksLeft);

                if(action == KSA_PageLeft)
                {
                    if(newState.firstMeasure == 0)
                    {
                        // No scrolling possible, set support point to first cell
                        p.ticksLeft=0;
                        p.ticksRight=docRoot->roundUpTicksToCellBorder(0 + 1, newState.writeLength);
                        break;
                    }

                    scrollPageLeft(newState, tempMapper);
                }
                else scrollPageRight(newState, tempMapper);
                tempMapper.refreshDisplayedItemLists();

                // Find the cell under reference point after scrolling
                CellAreaXToTicksResult rCell=
                        tempMapper.cellAreaXToTicks((rViewX.cellLeftX + rViewX.cellRightX) / 2 -
                                                    view->getCellArea().left());

                p.ticksLeft=rCell.cellLeftTicks;
                p.ticksRight=rCell.cellRightTicks;
            }
            break;
        case KSA_Home:
            p.ticksLeft=0;
            p.ticksRight=docRoot->roundUpTicksToCellBorder(0 + 1, newState.writeLength);
            break;
        case KSA_End:
            {
                int endTargetTicks=docRoot->measureToTicks(docRoot->getMaxMeasure() + 1);
                p.ticksLeft=endTargetTicks;
                p.ticksRight=docRoot->roundUpTicksToCellBorder(endTargetTicks + 1,newState.writeLength);
            }
            break;
        }
        break;
    }
    return p;
}

EditorSelectionSupportPoint CS_Navigation::getSelectionSecondSupportPoint(const EditorSelection& sel, const WriteLength& writeLength) const
{
    switch(sel.getSelectionMode())
    {
    case S_GlobalMeasure:
        {
            TicksToMeasureResult rAnchor = docRoot->ticksToMeasure(sel.anchor.ticksLeft);
            TicksToMeasureResult rLeft   = docRoot->ticksToMeasure(sel.ticksLeft);
            TicksToMeasureResult rRight  = docRoot->ticksToMeasure(sel.ticksRight);

            if(rLeft.measureIndex == rAnchor.measureIndex)
            {
                if(rRight.measureIndex == rAnchor.measureIndex + 1) // one measure only
                {
                    return EditorSelectionSupportPoint(
                            sel.anchor.ticksLeft - rAnchor.measureInternalTicks,
                            sel.anchor.ticksLeft - rAnchor.measureInternalTicks +
                            ticksPerMeasure(rAnchor.measureProperties),
                            -1);
                }
                else
                {
                    // return rightmost full measure
                    return EditorSelectionSupportPoint(
                            docRoot->roundDownTicksToMeasureBorder(sel.ticksRight - 1), sel.ticksRight, -1);
                }
            }
            else
            {
                // return leftmost full measure
                return EditorSelectionSupportPoint(sel.ticksLeft,
                                                   sel.ticksLeft +
                                                   ticksPerMeasure(rLeft.measureProperties),-1);
            }
        }
        break;
    case S_GlobalTrack:
        if(sel.trackTop == sel.anchor.track)
        {
            return EditorSelectionSupportPoint(-1,-1,sel.trackBottom);
        }
        else
        {
            return EditorSelectionSupportPoint(-1,-1,sel.trackTop);
        }
        break;
    case S_LocalCells:
        {
            EditorSelectionSupportPoint p;
            if(sel.ticksLeft == sel.anchor.ticksLeft)
            {
                if(sel.ticksRight == sel.anchor.ticksRight) // only one cell selected
                {
                    p.ticksLeft=sel.ticksLeft;
                    p.ticksRight=sel.ticksRight;
                }
                else
                {
                    // return rightmost selected cell
                    p.ticksLeft=docRoot->roundDownTicksToCellBorder(sel.ticksRight - 1,writeLength);
                    p.ticksRight=sel.ticksRight;
                }
            }
            else
            {
                // return leftmost selected cell
                p.ticksLeft=sel.ticksLeft;
                p.ticksRight=docRoot->roundUpTicksToCellBorder(sel.ticksLeft + 1,writeLength);
            }

            if(sel.trackTop == sel.anchor.track)
            {
                p.track=sel.trackBottom;
            }
            else
            {
                p.track=sel.trackTop;
            }

            return p;
        }
        break;
    }
    Q_ASSERT(false);
    return EditorSelectionSupportPoint();
}

void CS_Navigation::scrollToRehearsalMarker(KeyboardSelectionActionType action)
{
    Q_ASSERT(action == KSA_PageLeft || action == KSA_PageRight);

    // Selection must stay on the cell grid, but consecutive marker navigation
    // needs the exact event tick (several markers may share a single cell).
    int ticks=getEditorState().selection.ticksLeft;
    if(lastRehearsalMarkerDocument == docRoot &&
       getEditorState().selection == lastRehearsalMarkerSelection)
    {
        const DocMeasureItem* previous=docRoot->getMeasureItemAtExact(lastRehearsalMarkerTick);
        if(previous && previous->setRehearsalMarker)ticks=lastRehearsalMarkerTick;
    }

    if(action == KSA_PageLeft)
    {
        // Find last measure item with a tick position < ticks
        //  that has a rehearsal marker or is the first measure item at tick position 0
        DocMeasureItem* rehearsalMarkerMeasureItem=docRoot->measureItemList[0];
        for(int i=1; i < docRoot->measureItemList.size(); ++i)
        {
            DocMeasureItem* measureItem=docRoot->measureItemList[i];
            if(measureItem->tickPosition >= ticks)break;
            else if(measureItem->setRehearsalMarker)rehearsalMarkerMeasureItem=measureItem;
        }
        ticks=rehearsalMarkerMeasureItem->tickPosition;
    }
    else    // KSA_PageRight
    {
        // Find first measure item with a tick position > ticks
        //  that has a rehearsal marker. This item may not exist, in this case do not scroll.
        DocMeasureItem* rehearsalMarkerMeasureItem=NULL;
        for(int i=0; i < docRoot->measureItemList.size(); ++i)
        {
            DocMeasureItem* measureItem=docRoot->measureItemList[i];
            if(measureItem->tickPosition > ticks && measureItem->setRehearsalMarker)
            {
                rehearsalMarkerMeasureItem=measureItem;
                break;
            }
        }
        if(!rehearsalMarkerMeasureItem)return;
        else ticks=rehearsalMarkerMeasureItem->tickPosition;
    }

    // Determine current selection mode
    SelectionModeType selMode=getEditorState().selection.getSelectionMode();

    // Modified editor state
    EditorState newState=getEditorState();

    // Modify selection as appropriate
    switch(selMode)
    {
    case S_GlobalMeasure:
        // Stay in global measure selection mode
        newState.setGlobalMeasureSelection(docRoot->ticksToMeasure(ticks).measureIndex, 1, docRoot);
        break;
    case S_GlobalTrack: // Revert to local cell selection mode
    case S_LocalCells:
        newState.selection.ticksLeft=docRoot->roundDownTicksToCellBorder(ticks, newState.writeLength);
        newState.selection.ticksRight=docRoot->roundUpTicksToCellBorder(ticks + 1, newState.writeLength);
        newState.selection.trackTop=newState.firstSelectedTrack();
        newState.selection.trackBottom=newState.lastSelectedTrack(docRoot);

        // Reset anchor
        newState.selection.anchor.setTo(newState.selection.ticksLeft,
                                        newState.selection.ticksRight,
                                        newState.selection.trackTop);
        break;
    }

    // Scroll selection into view (horizontal scroll only)
    scrollRangeIntoView(newState,
            EditorRange(newState.selection.ticksLeft,  -1,
                        newState.selection.ticksRight, -1));

    applyStateAndUpdate(newState);
    lastRehearsalMarkerTick=ticks;
    lastRehearsalMarkerSelection=newState.selection;
    lastRehearsalMarkerDocument=docRoot;
}

void CS_Navigation::xZoomSliderValueChanged(int value)
{
    EditorState newState=getEditorState();
    newState.xZoomSliderValue=value;
    applyStateAndUpdate(newState);
}

void CS_Navigation::yZoomSliderValueChanged(int value)
{
    EditorState newState=getEditorState();
    newState.yZoomSliderValue=value;
    applyStateAndUpdate(newState);
}

void CS_Navigation::execWriteLengthDialog(bool setFocusToOtherTuplet)
{
    WriteLengthDialog dlg(mainWindow);
    dlg.writeLength           = getEditorState().writeLength;
    dlg.setFocusToOtherTuplet = setFocusToOtherTuplet;

    if(dlg.exec() == QDialog::Accepted)
    {
        // Apply new write length settings
        EditorState newState=getEditorState();
        newState.writeLength = dlg.writeLength;

        newState.adjustForChangedWriteLength(docRoot);
        applyStateAndUpdate(newState);
    }
}

