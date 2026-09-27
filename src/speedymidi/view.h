/***************************************************************************
 *  view.h - View Class (cf. Model-View-Controller Architecture)
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

#ifndef VIEW_H
#define VIEW_H

#include "global.h"
#include <QWidget>
#include <QVector>
#include "editormapper.h"

class QTimer;

class View : public QWidget
{
    Q_OBJECT
public:
    View(QWidget* parent);

public slots:
    void setMidiActivity(int trackIndex, int velocity);

public:

    void setController(Controller* controller, const DocRoot* docRoot);

    void refreshMapper() { mapper.refreshDisplayedItemLists(); }
    void updateForEditorStateModifications(const EditorState& oldState, const EditorState& newState);
    void updateForEditorSelectionModifications(const EditorRange& oldRange, const EditorRange& newRange);
    void updateForModificationsInRange(const EditorRange& range);
    void updateForVolatileEditorStateModificationsInRange(const VolatileEditorState& oldState, const VolatileEditorState& newState);

    // Selectors
    const EditorMapper* getMapper()                     const { return &mapper; }
    const QRect getCellArea()                           const { return cellArea; }
    Controller* getController()                         const { return controller; }
    const DocRoot* getDocRoot()                         const { return docRoot; }
    const EditorState& getEditorState()                 const;
    const VolatileEditorState& getVolatileEditorState() const;
    int getDefaultTrackHeightInPixels() const;

    enum MouseZoneType {
        NoArea,
        WriteLengthArea,
        WritePositionArea,
        MeasureHeader,

        TrackHeader,            // needs trackIndex
        TrackButtonDelete,      // needs trackIndex
        TrackButtonSolo,        // needs trackIndex
        TrackButtonMute,        // needs trackIndex
        TrackButtonRecord,      // needs trackIndex
        TrackNoteRangeSlider,   // needs trackIndex
        TrackResize,            // needs trackIndex
        TrackCells,             // needs trackIndex

        AddTrackButton
    };

    struct MouseZoneResult { MouseZoneType zoneType; int trackIndex; Qt::CursorShape cursor; QRect zoneRect; QPoint relativePos; };
    MouseZoneResult getMouseZone(const QPoint& viewPos) const;

    void restoreMouseCursor();

protected:
    virtual void resizeEvent(QResizeEvent* event);
    virtual void changeEvent(QEvent *e);

    virtual bool event(QEvent* event);
    virtual void toolTipEvent(QHelpEvent* event);
    virtual void keyPressEvent(QKeyEvent* event);
    virtual void keyReleaseEvent(QKeyEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void mouseDoubleClickEvent(QMouseEvent* event);
    virtual void wheelEvent(QWheelEvent* event);

    // ----------------------------------------------------------------------------------------------------
    // Drawing

    void prepareColors();
    void updateMidiActivityForTrack(int trackIndex);

    // Divide screen in certain areas, used for painting and clicking
    QRect cellArea;
    QRect statusArea;
    QRect writeLengthStatusArea;
    QRect writePositionStatusArea;
    QRect allMeasureHeadersArea;
    QRect addNewTrackButtonRect;

    QRect trackNameRect;            // relative to panelRect
    QRect trackMidiSettingsRect;    // relative to panelRect
    int lastToolTipTrackIndex;

    QList<bool> outOfBoundNoteRange[2];     // one entry for every pixel in cell area, top and bottom gradient

    virtual void paintEvent(QPaintEvent* event);
    void paintStatusArea(QPainter& painter, const QRegion& updateRegion);
    void paintMeasureHeaders(QPainter& painter, const QRegion& updateRegion, const SelRectXPos& selPosX);
    void paintTracks(QPainter& painter, const QRegion& updateRegion, const SelRectXPos& selPosX);
    void generateTrackRects(DisplayedTrack* dt, QRect& trackRect, QRect& panelRect, QRect& rangeSliderRect, QRect& trackCellsRect) const;
    void paintTrackHeaderPanel(QPainter& painter, const QRect& panelRect, int trackIndex, bool headerInSelection);
    void paintTrackHeaderRangeSlider(QPainter& painter, const QRect& rangeSliderRect, int trackIndex, bool headerInSelection);
    void paintTrackCells(QPainter& painter, const QRegion& updateRegion, const QRect& trackCellsRect, int trackIndex, const SelRectXPos& selPosX, const QFont& noteNameFont);
    void paintEndGradients(QPainter& painter);

    void getDisplayedMidiNoteRangeForTrack(const QRect& trackRect, int trackIndex, int& midiNoteBottom, int& midiNoteTop) const;
    double getWritePositionStatusBeatCount() const;

    QColor makePastelColor(const QColor& color, qreal whitenessFactor) const;
    QColor shadedPaletteColor(float foreground_factor) const;
    static QColor shadedPaletteColor(float foreground_factor, QColor foregroundColor, QColor backgroundColor);

    // Cached drawing objects
    QMap<int,QImage> noteImageMap;

    enum TrackIconIndexType { IconDelete,IconInactive,IconMute,IconSolo,IconRecord,IconAdd };
    QMap<TrackIconIndexType,QImage> trackIconsMap;
    QVector<int> midiActivityLevels;
    QTimer* midiActivityDecayTimer;

    QFont bigFont;
    QFont tupletBracketFont;
    QFont mediumFont;
    QFont miniFont;
    QFont timeSignatureFont;
    QFont trackNameFont;
    QFont octaveCFont;
    QFont infoFont;

    QColor shadowColor;
    QColor selectionRectFrameColor;
    QColor selectionRectFillColorCells;
    QColor selectionRectFillColorCells_DragModeImmediateListen;
    QColor selectionRectFillColorCells_DragModeTranspose;
    QColor selectionRectFillColorHeaders;
    QColor selectionRectAnchorCellFillColor;
    QColor playbackPositionLineColor;

    double writePositionStatusBeatCount;

    struct TrackIconPosition
    {
        int x,y,sx,sy;
        int icon_x,icon_y;
        TrackIconIndexType iconIndex;
        MouseZoneType zoneType;
    };
    static const TrackIconPosition TRACK_PANEL_BUTTON_POSITIONS[];
    static const int MAX_TRACK_PANEL_BUTTONS;

    bool initialViewResize;

    // Objects & links
    Controller* controller;
    const DocRoot* docRoot;
    EditorMapper mapper;
};

#endif // VIEW_H
