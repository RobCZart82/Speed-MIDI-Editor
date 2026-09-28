/***************************************************************************
 *  cs_navigation.h - Controller Subsystem: Navigation
 *                    (Select, Scroll, Zoom, Cell Length)
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

#ifndef CS_NAVIGATION_H
#define CS_NAVIGATION_H

#include "cs_common.h"
#include "doc_event.h"

class CS_Navigation : public CS_Common
{
    Q_OBJECT
public:
    CS_Navigation(Controller* controller);

    void navigateToUndoCommandLocation(EditorState& state, const EditorRange& scrollToRange, bool disableScrollingDuringMacroCreation);
    void scrollBarHorizontalPageStepAdd();
    void initialFitAllTracks();
    void setDefaultTrackHeights();

    virtual bool keyPressEvent(QKeyEvent* event);
    virtual void keyReleaseEvent(QKeyEvent* event);
    virtual bool mousePressEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual bool wheelEvent(QWheelEvent* event);
    virtual void timerEvent(QTimerEvent* timerEvent);
    virtual void stateChanged();
    virtual void cancelInputCapture();

protected slots:
    void actionEdit_SelectAll_Triggered();
    void actionEdit_MoveNotes_Triggered();
    void actionEdit_DrawNotes_Triggered();
    void actionEdit_EraseNotes_Triggered();

    void actionView_HorizontalZoom_ZoomIn_Triggered();
    void actionView_HorizontalZoom_ZoomOut_Triggered();
    void actionView_VerticalZoom_ZoomIn_Triggered();
    void actionView_VerticalZoom_ZoomOut_Triggered();
    void actionView_FitAllTracks_Triggered();
    void actionView_NoteNames_Triggered();

    void actionWrite_CellLength_Double_Triggered();
    void actionWrite_CellLength_Halve_Triggered();
    void actionWrite_CellLength_Triplets_Triggered();
    void actionWrite_CellLength_Duplets_Triggered();
    void actionWrite_CellLength_ArbitraryTuplets_Triggered();

    void scrollBarHorizontalActionTriggered(int action);
    void scrollBarHorizontalSliderPressed();
    void scrollBarHorizontalSliderReleased();
    void scrollBarVerticalActionTriggered(int action);
    void scrollBarVerticalValueChanged(int value);
    void xZoomSliderValueChanged(int value);
    void yZoomSliderValueChanged(int value);

protected:
    bool inImmediateListenMode() const;

    void setWriteLengthChecks(const WriteLength& writeLength);
    void startAutoScrollTimer();

    int getHorizontalScrollBarRange() const;
    void updateScrollAndZoomBars();
    void scrollPageRight(EditorState& newState, EditorMapper& tempMapper) const;
    void scrollPageLeft(EditorState& newState, EditorMapper& tempMapper) const;

    enum KeyboardSelectionActionType {
        KSA_Left,
        KSA_Right,
        KSA_Up,
        KSA_Down,
        KSA_PageLeft,
        KSA_PageRight,
        KSA_Home,
        KSA_End
    };

    void updateSelectionByDragging(EditorState& newState, const QPoint& mousePos);
    EditorSelection extendSelectionFromAnchorByMouse(const EditorSelection& sel, bool allowModeChange, const EditorRange& newRange);
    void initiateSelectionByKeyboard(KeyboardSelectionActionType action);
    void modifySelectionByKeyboard(KeyboardSelectionActionType action);
    EditorSelectionSupportPoint modifySelectionSupportPointAndScrollPosByKeyboard(EditorState& newState, SelectionModeType selectionMode, const EditorSelectionSupportPoint& sp, KeyboardSelectionActionType action) const;
    EditorSelectionSupportPoint getSelectionSecondSupportPoint(const EditorSelection& sel, const WriteLength& writeLength) const;
    void scrollToRehearsalMarker(KeyboardSelectionActionType action);
    void execWriteLengthDialog(bool setFocusToOtherTuplet);

    // Modal mouse drag operations
    enum MouseDragModeType { MDM_None, MDM_Select, MDM_AdjustTrackHeight, MDM_AdjustDisplayedNoteRange,
                             MDM_DrawNote, MDM_ResizeNote, MDM_EraseNotes, MDM_MoveNote } mouseDragMode;
    enum NoteResizeEdgeType { NRE_Left, NRE_Right };

    bool noteDrawingEnabled() const;
    bool noteErasingEnabled() const;
    bool noteMovingEnabled() const;
    int noteNumberAtY(int trackIndex, int y) const;
    int snappedTickAtX(int x) const;
    void updateDraggedNote(const QPoint& position);
    void finishNoteGesture(bool commit);
    void eraseNoteAtPosition(const QPoint& position);
    void finishEraseGesture();

    QPoint mouseDragStartReferencePoint;
    int mouseDragStartTrackIndex;
    double mouseDragPreviousValue;    // used for drag modes AdjustDisplayedNoteRange, AdjustTrackHeight
    DocEvent* draggedNote;
    DocEvent draggedNoteOriginal;
    NoteResizeEdgeType draggedNoteResizeEdge;
    int draggedNoteTrackIndex;
    int draggedNoteAnchorX;
    int draggedNoteAnchorTick;
    int draggedNoteAnchorMidiNote;
    QPoint lastErasePosition;
    bool eraseMacroActive;
    EditorRange erasedNoteRange;

    bool scrollBarHorizontalIsPressed;

    bool autoscrollTimerActive;
    int autoscrollTimerId;
    int autoscrollPixelAccuToLeft;
    int autoscrollPixelAccuToRight;
    int autoscrollPixelAccuUp;
    int autoscrollPixelAccuDown;

    // Object links
    QScrollBar* scrollBarHorizontal;
    QScrollBar* scrollBarVertical;
    ZoomSliderWidget* xZoomSliderWidget;
    ZoomSliderWidget* yZoomSliderWidget;
};

#endif // CS_NAVIGATION_H
