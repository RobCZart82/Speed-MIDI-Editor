/***************************************************************************
 *  view.cpp - View Class (cf. Model-View-Controller Architecture)
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

#include "view.h"
#include "controller.h"
#include "doc_root.h"
#include "doc_measureitem.h"
#include "doc_track.h"
#include "doc_event.h"

#include <QPainter>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolTip>

namespace
{
QImage loadFlatIcon(const QString& resourcePath, const QSize& size = QSize(16, 16))
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(resourcePath);
    if(renderer.isValid())
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
    }
    return image;
}

void drawPlaybackPositionLine(QPainter& painter, int x, int top, int bottom,
                              const QColor& color)
{
    QColor glowColor(color);
    glowColor.setAlpha(72);
    painter.setPen(QPen(glowColor, 7.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(x, top, x, bottom);

    painter.setPen(QPen(color, 2.0));
    painter.drawLine(x, top, x, bottom);
}
}

const View::TrackIconPosition View::TRACK_PANEL_BUTTON_POSITIONS[]={
    {  2, 2,16,16, 0, 0, View::IconDelete,   View::TrackButtonDelete },
    { 14,22,30,19,13, 4, View::IconSolo,     View::TrackButtonSolo },
    { 44,22,30,19,13, 4, View::IconMute,     View::TrackButtonMute },
    { 74,22,30,19,13, 4, View::IconRecord,   View::TrackButtonRecord },
};
const int View::MAX_TRACK_PANEL_BUTTONS=
        sizeof(View::TRACK_PANEL_BUTTON_POSITIONS) / sizeof(View::TRACK_PANEL_BUTTON_POSITIONS[0]);

View::View(QWidget* parent)
        : QWidget(parent), midiActivityDecayTimer(new QTimer(this)), mapper(this)
{
    controller=NULL;
    docRoot=NULL;

    setMouseTracking(true); // needed for cursor updates

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setMinimumSize(VIEW_CELL_AREA_LEFT + VIEW_MIN_VIEW_SIZE_XY,
                   VIEW_CELL_AREA_TOP  + VIEW_MIN_VIEW_SIZE_XY);
    setBackgroundRole(QPalette::Base);
    setAttribute(Qt::WA_OpaquePaintEvent,true);
    setFocusPolicy(Qt::StrongFocus);

    // Preload note images
    int denominator=1;
    while(denominator <= EDITOR_MAX_WRITELENGTH_DENOMINATOR)
    {
        noteImageMap.insert(denominator, QImage(QString(":/images/Note%1.png").arg(denominator)));
        denominator*=2;
    }

    trackIconsMap.insert(IconDelete,   loadFlatIcon(":/images/flat/track-delete.svg"));
    trackIconsMap.insert(IconInactive, loadFlatIcon(":/images/flat/track-inactive.svg"));
    trackIconsMap.insert(IconMute,     loadFlatIcon(":/images/flat/track-mute.svg"));
    trackIconsMap.insert(IconSolo,     loadFlatIcon(":/images/flat/track-solo.svg"));
    trackIconsMap.insert(IconRecord,   loadFlatIcon(":/images/flat/track-record.svg"));
    trackIconsMap.insert(IconAdd,      loadFlatIcon(":/images/flat/track-add.svg", QSize(20, 20)));

    // create fonts
    bigFont=QFont(VIEW_FONT_NAME, 14, QFont::Bold);
    tupletBracketFont=QFont(VIEW_FONT_NAME, 10, QFont::Normal, true);
    mediumFont=QFont(VIEW_FONT_NAME, 10, QFont::Bold);
    miniFont=QFont(VIEW_FONT_NAME, 9);
    timeSignatureFont=QFont(VIEW_FONT_NAME, 12, QFont::Bold);
    timeSignatureFont.setStretch(130);
    trackNameFont=QFont(VIEW_FONT_NAME, 11, QFont::Bold);
    octaveCFont=trackNameFont;
    infoFont=QFont(VIEW_FONT_NAME, 8);

    lastToolTipTrackIndex = -1;
    trackNameRect         = QRect(QPoint(20, 2), QSize(VIEW_TRACK_HEADER_PANEL_WIDTH-34, 20));
    trackMidiSettingsRect = QRect(QPoint(4, 50), QSize(VIEW_TRACK_HEADER_PANEL_WIDTH-18, 0xffff));   // reserve the meter strip

    midiActivityDecayTimer->setInterval(30);
    connect(midiActivityDecayTimer, &QTimer::timeout, this, [this]() {
        bool activityChanged=false;
        for(int& level : midiActivityLevels)
        {
            if(level > 0)
            {
                level=qMax(0,level-6);
                activityChanged=true;
            }
        }
        if(activityChanged)
        {
            for(int trackIndex=0; trackIndex < midiActivityLevels.size(); ++trackIndex)
                updateMidiActivityForTrack(trackIndex);
        }
    });
    midiActivityDecayTimer->start();

    prepareColors();

    writePositionStatusBeatCount=-1;

    initialViewResize=false;
}

void View::setMidiActivity(int trackIndex, int velocity)
{
    if(trackIndex < 0 || !docRoot || trackIndex >= docRoot->trackList.size())
        return;

    if(midiActivityLevels.size() < docRoot->trackList.size())
        midiActivityLevels.resize(docRoot->trackList.size());

    midiActivityLevels[trackIndex]=qMax(midiActivityLevels[trackIndex],qBound(0,velocity,127));
    updateMidiActivityForTrack(trackIndex);
}

int View::getDefaultTrackHeightInPixels() const
{
    // The header information consists of the patch name plus four MIDI
    // properties. Reserve their rendered line height and a small bottom gap.
    const QFontMetrics metrics(infoFont);
    const int contentHeight = trackMidiSettingsRect.top() + metrics.lineSpacing() * 5 + 6;
    return qMax(VIEW_MIN_TRACK_HEIGHT_IN_PIXELS, contentHeight);
}

void View::updateMidiActivityForTrack(int trackIndex)
{
    for(DisplayedTrack* displayedTrack : mapper.getDisplayedTrackList())
    {
        if(displayedTrack->trackIndex != trackIndex)continue;

        QRect trackRect,panelRect,rangeSliderRect,trackCellsRect;
        generateTrackRects(displayedTrack,trackRect,panelRect,rangeSliderRect,trackCellsRect);
        update(panelRect);
        return;
    }
}

void View::setController(Controller* controller, const DocRoot* docRoot)
{
    this->controller=controller;
    this->docRoot=docRoot;
    midiActivityLevels.fill(0, docRoot ? docRoot->trackList.size() : 0);

    // force update of complete view because showing new document
    update();

    // activate initial resize messages to controller
    initialViewResize=true;
}

const EditorState& View::getEditorState() const
{
    return controller->getEditorState();
}

const VolatileEditorState& View::getVolatileEditorState() const
{
    return controller->getVolatileEditorState();
}

void View::updateForEditorStateModifications(const EditorState& oldState, const EditorState& newState)
{
    // Intelligent update function when editor state is about to change

    // Simplify the complicated update logic:
    //  - If scroll, zoom, write length or selection MODE changes, do a complete update.
    //  - otherwise, update only areas disselected or newly selected

    if(newState.firstMeasure                 != oldState.firstMeasure                 ||
       newState.firstTrack                   != oldState.firstTrack                   ||
       newState.xZoomSliderValue             != oldState.xZoomSliderValue             ||
       newState.yZoomSliderValue             != oldState.yZoomSliderValue             ||
       newState.writeLength.denominator       != oldState.writeLength.denominator       ||
       newState.writeLength.tupletNominator   != oldState.writeLength.tupletNominator   ||
       newState.writeLength.tupletDenominator != oldState.writeLength.tupletDenominator ||
       newState.selection.getSelectionMode() != oldState.selection.getSelectionMode())
    {
        update();
    }
    else
    {
        // Check for track state changes. If track count has changed, only update the track indices
        //  available before AND after the change. Subsequent tracks in new resp. old state will be
        //  updated in applyStateAndUpdate when considering documentModificationsUnionRange.
        int minListSize=qMin(newState.trackStateList.size(),oldState.trackStateList.size());
        for(int i=0; i < minListSize; ++i)
        {
            EditorTrackState oldTrackState=oldState.trackStateList[i];
            EditorTrackState newTrackState=newState.trackStateList[i];

            if(newTrackState.heightInNotes != oldTrackState.heightInNotes)
            {
                // Update current track, also tracks below have shifted.
                updateForModificationsInRange(EditorRange(0,i,INT_MAX,INT_MAX));
            }
            else if(newTrackState.centerMidiNote   != oldTrackState.centerMidiNote ||
                    newTrackState.solo             != oldTrackState.solo ||
                    newTrackState.mute             != oldTrackState.mute ||
                    newTrackState.recordingEnabled != oldTrackState.recordingEnabled)
            {
                // Update current track only
                updateForModificationsInRange(EditorRange(0,i,INT_MAX,i));
            }
        }

        // Selection settings
        updateForEditorSelectionModifications(oldState.selection,newState.selection);

        // Write position status area
        if(getWritePositionStatusBeatCount() != writePositionStatusBeatCount)
        {
            update(writePositionStatusArea);
        }
    }
}

void View::updateForEditorSelectionModifications(const EditorRange& oldRange, const EditorRange& newRange)
{
    // Intelligent update function when editor selection is about to change

    if(newRange.getSelectionMode() != oldRange.getSelectionMode())
    {
        // Selection mode changed => complete update
        update();
        return;
    }

    // Remaining in same selection mode
    SelectionModeType SelMode=newRange.getSelectionMode();

    // Check if new selection is disjoint from old selection
    if(oldRange.isDisjoint(newRange))
    {
        // simply update areas of old and new selection
        updateForModificationsInRange(oldRange);
        updateForModificationsInRange(newRange);
        return;
    }

    // Old selection and new selection are not disjoint

    EditorRange UnionRange=oldRange.unionRange(newRange);

    int t=VIEW_SELECTION_FRAME_THICKNESS;
    QRect UpdateRect;

    if(SelMode != S_GlobalTrack)
    {
        // Check right and left borders

        // Therefore first get maximum y-extension of UnionRange
        if(SelMode == S_GlobalMeasure)
        {
            UpdateRect.setTop(0);
            UpdateRect.setBottom(rect().bottom());
        }
        else    // local cell selection
        {
            TrackToViewYResult resTop=mapper.trackToViewY(UnionRange.trackTop);
            TrackToViewYResult resBottom=mapper.trackToViewY(UnionRange.trackBottom);

            // set update rect coordinates (clamped to cell area boundaries)
            UpdateRect.setTop(qMax(resTop.TopY, cellArea.top()));
            UpdateRect.setBottom(qMin(resBottom.BottomY, cellArea.bottom()));
        }

        // Changes at left border?
        if(newRange.ticksLeft != oldRange.ticksLeft)
        {
            int ticksMin=qMin(newRange.ticksLeft,oldRange.ticksLeft);
            int ticksMax=qMax(newRange.ticksLeft,oldRange.ticksLeft);

            TicksToViewXResult rMin=mapper.ticksToViewX(ticksMin);
            TicksToViewXResult rMax=mapper.ticksToViewX(ticksMax);

            // set update rect coordinates (clamped to cell area boundaries)
            UpdateRect.setLeft(qMax(rMin.cellLeftX, cellArea.left()));
            UpdateRect.setRight(qMin(rMax.cellRightX, cellArea.right()));

            update(UpdateRect.adjusted(-t,-t,t,t));
        }

        // Changes at right border?
        if(newRange.ticksRight != oldRange.ticksRight)
        {
            int ticksMin=qMin(newRange.ticksRight,oldRange.ticksRight);
            int ticksMax=qMax(newRange.ticksRight,oldRange.ticksRight);

            TicksToViewXResult rMin=mapper.ticksToViewX(ticksMin);
            TicksToViewXResult rMax=mapper.ticksToViewX(ticksMax);

            // set update rect coordinates (clamped to cell area boundaries)
            UpdateRect.setLeft(qMax(rMin.cellLeftX, cellArea.left()));
            UpdateRect.setRight(qMin(rMax.cellRightX, cellArea.right()));

            update(UpdateRect.adjusted(-t,-t,t,t));
        }
    }

    if(SelMode != S_GlobalMeasure)
    {
        // Check top and bottom borders

        // Therefore first get maximum x-extension of UnionRange
        if(SelMode == S_GlobalTrack)
        {
            UpdateRect.setLeft(0);
            UpdateRect.setRight(rect().right());
        }
        else    // local cell selection
        {
            TicksToViewXResult resLeft=mapper.ticksToViewX(UnionRange.ticksLeft);
            TicksToViewXResult resRight=mapper.ticksToViewX(UnionRange.ticksRight);

            // set update rect coordinates (clamped to cell area boundaries)
            UpdateRect.setLeft(qMax(resLeft.cellLeftX, cellArea.left()));
            UpdateRect.setRight(qMin(resRight.cellRightX, cellArea.right()));
        }

        // Changes at top border?
        if(newRange.trackTop != oldRange.trackTop)
        {
            int trackMin=qMin(newRange.trackTop,oldRange.trackTop);
            int trackMax=qMax(newRange.trackTop,oldRange.trackTop);

            // Update rect will include old and new trackTop completely,
            //  so also the border line will be updated correctly.

            TrackToViewYResult resMin=mapper.trackToViewY(trackMin);
            TrackToViewYResult resMax=mapper.trackToViewY(trackMax);

            // set update rect coordinates (clamped to cell area boundaries)
            UpdateRect.setTop(qMax(resMin.TopY, cellArea.top()));
            UpdateRect.setBottom(qMin(resMax.BottomY, cellArea.bottom()));

            update(UpdateRect.adjusted(-t,-t,t,t));
        }

        // Changes at bottom border?
        if(newRange.trackBottom != oldRange.trackBottom)
        {
            int trackMin=qMin(newRange.trackBottom,oldRange.trackBottom);
            int trackMax=qMax(newRange.trackBottom,oldRange.trackBottom);

            // Update rect will include old and new trackBottom completely,
            //  so also the border line will be updated correctly.

            TrackToViewYResult resMin=mapper.trackToViewY(trackMin);
            TrackToViewYResult resMax=mapper.trackToViewY(trackMax);

            // set update rect coordinates (clamped to cell area boundaries)
            UpdateRect.setTop(qMax(resMin.TopY, cellArea.top()));
            UpdateRect.setBottom(qMin(resMax.BottomY, cellArea.bottom()));

            update(UpdateRect.adjusted(-t,-t,t,t));
        }
    }
}

void View::updateForModificationsInRange(const EditorRange& range)
{
    // Intelligent update function when document was modified

    // [0;INT_MAX] in a range-coordinate means also the respective headers must be updated
    // BEWARE: Don't use range.getSelectionMode() here because range is not a selection,
    //         it's an arbitrary range

    QRect UpdateRect;

    // a) Right and left borders
    if(range.ticksLeft == 0 && range.ticksRight == INT_MAX)
    {
        UpdateRect.setLeft(0);
        UpdateRect.setRight(rect().right());
    }
    else
    {
        // local cell selection or global measure selection
        TicksToViewXResult resLeft=mapper.ticksToViewX(range.ticksLeft);
        TicksToViewXResult resRight=mapper.ticksToViewX(range.ticksRight);

        // set update rect coordinates (clamped to cell area boundaries)
        UpdateRect.setLeft(qMax(resLeft.cellLeftX, cellArea.left()));
        UpdateRect.setRight(qMin(resRight.cellRightX, cellArea.right()));
    }

    // b) top and bottom borders
    if(range.trackTop == 0 && range.trackBottom == INT_MAX)
    {
        // global measure selection
        UpdateRect.setTop(0);
        UpdateRect.setBottom(rect().bottom());
    }
    else
    {
        // local cell selection or global track selection

        if(range.trackTop == docRoot->trackList.size())
        {
            // If trackTop references first non-existing track index, take top of empty area.
            //  This is required to do an update of the empty area below the bottommost visible track.
            int emptyAreaStartY=mapper.trackToViewY(range.trackTop - 1).BottomY;

            // set update rect coordinates (clamped to cell area boundaries)
            UpdateRect.setTop(qMax(emptyAreaStartY, cellArea.top()));
            UpdateRect.setBottom(cellArea.bottom());
        }
        else
        {
            TrackToViewYResult resTop=mapper.trackToViewY(range.trackTop);
            TrackToViewYResult resBottom=mapper.trackToViewY(range.trackBottom);

            if(resTop.TopY > cellArea.bottom() || resBottom.BottomY < 0)
                return;     // area out of screen, no update required

            // set update rect coordinates (clamped to cell area boundaries)
            UpdateRect.setTop(qMax(resTop.TopY, cellArea.top()));
            UpdateRect.setBottom(qMin(resBottom.BottomY, cellArea.bottom()));
        }
    }

    UpdateRect.adjust(-1,-1,1,1);   // note events overlap for 1 pixel
    update(UpdateRect);
}

void View::updateForVolatileEditorStateModificationsInRange(const VolatileEditorState& oldState, const VolatileEditorState& newState)
{
    if(newState.showNoteNames != oldState.showNoteNames)
        update(cellArea);

    // Always update anchor state: Shift-key state might change without notice of keyPressEvent-handler
    updateForModificationsInRange(getEditorState().selection.anchor.toRange());

    if(newState.playbackLineCellX != oldState.playbackLineCellX)
    {
        // update to remove old line
        if(oldState.playbackLineCellX != -1)
        {
            // Redraw the complete glow footprint so the old cursor halo cannot
            // remain behind as a translucent trail.
            QRect updateRect(oldState.playbackLineCellX - 4, 0, 9, rect().height());
            update(updateRect);
        }

        // update to draw new line
        QRect updateRect(newState.playbackLineCellX - 4, 0, 9, rect().height());
        update(updateRect);
    }

    if(newState.keyboardDragMode != oldState.keyboardDragMode)
        updateForModificationsInRange(getEditorState().selection);
}

View::MouseZoneResult View::getMouseZone(const QPoint& viewPos) const
{
    MouseZoneResult result;
    result.zoneType=NoArea;
    result.trackIndex=-1;               // invalidate index
    result.cursor=Qt::ArrowCursor;      // default cursor
    result.zoneRect=QRect();
    result.relativePos=QPoint(0,0);

//EXTENSION DRAG&DROP: check if clicked on selection border => start drag & drop operation

    bool keyboardDragActive = getVolatileEditorState().keyboardDragMode != VolatileEditorState::KDM_None;
    bool iconsSMR_enabled   =
            getVolatileEditorState().keyboardDragMode == VolatileEditorState::KDM_None ||
            getVolatileEditorState().keyboardDragMode == VolatileEditorState::KDM_ImmediateListen;

    if(!keyboardDragActive && writeLengthStatusArea.contains(viewPos))
    {
        result.zoneType=WriteLengthArea;
        result.cursor=Qt::PointingHandCursor;
        result.zoneRect=writeLengthStatusArea;
        result.relativePos=viewPos - writeLengthStatusArea.topLeft();
        return result;
    }
    else if(!keyboardDragActive && writePositionStatusArea.contains(viewPos))
    {
        result.zoneType=WritePositionArea;
        result.zoneRect=writePositionStatusArea;
        result.relativePos=viewPos - writePositionStatusArea.topLeft();
        return result;
    }
    else if(!keyboardDragActive && allMeasureHeadersArea.contains(viewPos))
    {
        // no global measure selections during any keyboard drag mode
        result.zoneType=MeasureHeader;
        result.cursor=Qt::CrossCursor;
        result.zoneRect=allMeasureHeadersArea;
        result.relativePos=viewPos - allMeasureHeadersArea.topLeft();
        return result;
    }
    else if(!keyboardDragActive && addNewTrackButtonRect.contains(viewPos))
    {
        result.zoneType=AddTrackButton;
        result.cursor=Qt::PointingHandCursor;
        result.zoneRect=addNewTrackButtonRect;
        result.relativePos=viewPos - addNewTrackButtonRect.topLeft();
        return result;
    }
    else
    {
        // Track loop
        for(int i=0; i < mapper.getDisplayedTrackList().size(); ++i)
        {
            DisplayedTrack* dt=mapper.getDisplayedTrackList()[i];

            QRect trackRect,panelRect,rangeSliderRect,trackCellsRect;
            generateTrackRects(dt,trackRect,panelRect,rangeSliderRect,trackCellsRect);

            // no track button clicks or global track selections during any keyboard drag mode
            if(panelRect.contains(viewPos))
            {
                // Check for special track panel buttons
                for(int iButton=0; iButton < MAX_TRACK_PANEL_BUTTONS; ++iButton)
                {
                    QRect buttonRect(
                            QPoint(panelRect.x() + TRACK_PANEL_BUTTON_POSITIONS[iButton].x,
                                   panelRect.y() + TRACK_PANEL_BUTTON_POSITIONS[iButton].y),
                            QSize(TRACK_PANEL_BUTTON_POSITIONS[iButton].sx,
                                  TRACK_PANEL_BUTTON_POSITIONS[iButton].sy));

                    if(buttonRect.contains(viewPos))
                    {
                        switch(TRACK_PANEL_BUTTON_POSITIONS[iButton].zoneType)
                        {
                        case TrackButtonSolo:
                        case TrackButtonMute:
                        case TrackButtonRecord:
                            if(iconsSMR_enabled)
                            {
                                result.zoneType=TRACK_PANEL_BUTTON_POSITIONS[iButton].zoneType;
                                result.trackIndex=dt->trackIndex;
                                result.cursor=Qt::PointingHandCursor;
                                result.zoneRect=buttonRect;
                                result.relativePos=viewPos - buttonRect.topLeft();
                                return result;
                            }
                            break;
                        default:    // other icons (delete)
                            if(!keyboardDragActive)
                            {
                                result.zoneType=TRACK_PANEL_BUTTON_POSITIONS[iButton].zoneType;
                                result.trackIndex=dt->trackIndex;
                                result.cursor=Qt::PointingHandCursor;
                                result.zoneRect=buttonRect;
                                result.relativePos=viewPos - buttonRect.topLeft();
                                return result;
                            }
                            break;
                        }
                    }
                }

                if(!keyboardDragActive)
                {
                    // clicked on other area on track header panel
                    result.zoneType=TrackHeader;
                    result.trackIndex=dt->trackIndex;
                    result.cursor=Qt::CrossCursor;
                    result.zoneRect=panelRect;
                    result.relativePos=viewPos - panelRect.topLeft();
                    return result;
                }
            }
            if(rangeSliderRect.contains(viewPos))
            {
                result.zoneType=TrackNoteRangeSlider;
                result.trackIndex=dt->trackIndex;
                result.cursor=Qt::SizeVerCursor;
                result.zoneRect=rangeSliderRect;
                result.relativePos=viewPos - rangeSliderRect.topLeft();
                return result;
            }
            if(trackCellsRect.contains(viewPos))
            {
                result.zoneType=TrackCells;
                result.trackIndex=dt->trackIndex;
                result.cursor=Qt::CrossCursor;
                result.zoneRect=trackCellsRect;
                result.relativePos=viewPos - trackCellsRect.topLeft();
                return result;
            }
            QRect inbetweenRect(0,trackRect.bottom() + 1,rect().width(),VIEW_TRACK_SEPARATOR_Y_INBETWEEN+1);
            if(inbetweenRect.contains(viewPos))
            {
                result.zoneType=TrackResize;
                result.trackIndex=dt->trackIndex;
                result.cursor=Qt::SplitVCursor;
                result.zoneRect=inbetweenRect;
                result.relativePos=viewPos - inbetweenRect.topLeft();
                return result;
            }
        }
    }

    return result;
}

void View::restoreMouseCursor()
{
    MouseZoneResult mouseZone=getMouseZone(mapFromGlobal(QCursor::pos()));
    setCursor(mouseZone.cursor);
}

bool View::event(QEvent* event)
{
    bool b=QWidget::event(event);

    switch(event->type())
    {
    case QEvent::ToolTip:
        toolTipEvent((QHelpEvent*)event);
        break;
    default:
        break;
    }
    return b;
}

void View::toolTipEvent(QHelpEvent* event)
{
    MouseZoneResult r=getMouseZone(event->pos());
    QRect smallRectAroundPos(event->pos().x() - 2, event->pos().y() - 2, 2*2, 2*2);

    switch(r.zoneType)
    {
    case NoArea:
    case WritePositionArea:
    case TrackResize:
    case TrackCells:
        QToolTip::hideText();
        event->ignore();
        break;

    case WriteLengthArea:
        QToolTip::showText(event->globalPos(), tr("Set cell length"),
                           this, r.zoneRect);
        break;

    case MeasureHeader:
        QToolTip::showText(event->globalPos(), tr("Measure attributes (Double-click to edit)"),
                           this, smallRectAroundPos);
        break;

    case TrackHeader:
        {
            // Force the tooltip to move to new track
            if(lastToolTipTrackIndex != r.trackIndex)QToolTip::hideText();

            QString msg=tr("Track attributes (Double-click to edit)");

            if(r.relativePos.x() >= VIEW_TRACK_HEADER_PANEL_WIDTH-10)
                QToolTip::showText(event->globalPos(),
                                   tr("MIDI activity meter — based on note velocity, not audio level"),
                                   this, r.zoneRect);
            else if(trackNameRect.contains(r.relativePos))
                QToolTip::showText(event->globalPos(), msg, this, trackNameRect);
            else if(trackMidiSettingsRect.contains(r.relativePos))
                QToolTip::showText(event->globalPos(), msg, this, trackMidiSettingsRect);
            else
            {
                QToolTip::hideText();
                event->ignore();
            }
        }
        break;

    case TrackButtonDelete:
        // Force the tooltip to move to new track
        if(lastToolTipTrackIndex != r.trackIndex)QToolTip::hideText();

        QToolTip::showText(event->globalPos(), tr("Delete track"), this, r.zoneRect);
        break;
    case TrackButtonSolo:
        // Force the tooltip to move to new track
        if(lastToolTipTrackIndex != r.trackIndex)QToolTip::hideText();

        QToolTip::showText(event->globalPos(), tr("Solo"), this, r.zoneRect);
        break;
    case TrackButtonMute:
        // Force the tooltip to move to new track
        if(lastToolTipTrackIndex != r.trackIndex)QToolTip::hideText();

        QToolTip::showText(event->globalPos(), tr("Mute"), this, r.zoneRect);
        break;
    case TrackButtonRecord:
        // Force the tooltip to move to new track
        if(lastToolTipTrackIndex != r.trackIndex)QToolTip::hideText();

        QToolTip::showText(event->globalPos(), tr("Recording (enable to enter notes)"), this, r.zoneRect);
        break;

    case TrackNoteRangeSlider:
        // Force the tooltip to move to new track
        if(lastToolTipTrackIndex != r.trackIndex)QToolTip::hideText();

        QToolTip::showText(event->globalPos(), tr("Range of displayed note heights"), this, r.zoneRect);
        break;

    case AddTrackButton:
        QToolTip::showText(event->globalPos(), tr("Append new track"), this, r.zoneRect);
        break;
    }

    lastToolTipTrackIndex=r.trackIndex;
}

void View::keyPressEvent(QKeyEvent* event)
{
    if(!controller->viewKeyPressEvent(event))
        QWidget::keyPressEvent(event);
}

void View::keyReleaseEvent(QKeyEvent* event)
{
    if(!controller->viewKeyReleaseEvent(event))
        QWidget::keyReleaseEvent(event);
}

void View::mousePressEvent(QMouseEvent* event)
{
    if(!controller->viewMousePressEvent(event))
        QWidget::mousePressEvent(event);
}

void View::mouseMoveEvent(QMouseEvent* event)
{
    if(!controller->viewMouseMoveEvent(event))
        QWidget::mouseMoveEvent(event);
}

void View::mouseReleaseEvent(QMouseEvent* event)
{
    if(!controller->viewMouseReleaseEvent(event))
        QWidget::mouseReleaseEvent(event);
}

void View::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(!controller->viewMouseDoubleClickEvent(event))
        QWidget::mouseDoubleClickEvent(event);
}

void View::wheelEvent(QWheelEvent* event)
{
    if(!controller->viewWheelEvent(event))
        QWidget::wheelEvent(event);
}

// ___________________________________________ Drawing ____________________________________________

void View::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::PaletteChange:
        prepareColors();
        break;
    default:
        break;
    }
}

void View::prepareColors()
{
    QPalette activePalette(palette());
    activePalette.setCurrentColorGroup(QPalette::Active);

    shadowColor=shadedPaletteColor(.5,activePalette.color(QPalette::WindowText),QColor(Qt::darkGray));

    selectionRectFrameColor=shadedPaletteColor(1,activePalette.color(QPalette::Highlight),activePalette.color(QPalette::WindowText));

    selectionRectFillColorCells=shadedPaletteColor(.5,activePalette.color(QPalette::Highlight),activePalette.color(QPalette::Base));
    selectionRectFillColorCells.setAlpha(128);

    selectionRectFillColorCells_DragModeImmediateListen=shadedPaletteColor(.5,QColor(Qt::green),activePalette.color(QPalette::Base));
    selectionRectFillColorCells_DragModeImmediateListen.setAlpha(128);

    selectionRectFillColorCells_DragModeTranspose=shadedPaletteColor(.5,QColor(Qt::yellow),activePalette.color(QPalette::Base));
    selectionRectFillColorCells_DragModeTranspose.setAlpha(128);

    selectionRectFillColorHeaders=selectionRectFillColorCells;
    selectionRectFillColorHeaders.setAlpha(96);     // Header selection color is more transparent

    selectionRectAnchorCellFillColor=shadedPaletteColor(.75,activePalette.color(QPalette::Highlight),activePalette.color(QPalette::Base));
    selectionRectAnchorCellFillColor.setAlpha(128);

    playbackPositionLineColor.setRgb(0x00,0x9f,0xff,0xff);
}

void View::resizeEvent(QResizeEvent* event)
{
    // Divide screen in certain areas, used for painting and clicking
    cellArea.setCoords(VIEW_CELL_AREA_LEFT,
                       VIEW_CELL_AREA_TOP,
                       event->size().width()-1,
                       event->size().height()-1);

    statusArea.setRect( 0,0, cellArea.left(), VIEW_MEASURE_HEADER_HEIGHT);

    writeLengthStatusArea=statusArea;
    writeLengthStatusArea.setWidth(6 * statusArea.width() / 10);
    writeLengthStatusArea.adjust(1,1,-1,-1);

    writePositionStatusArea=statusArea;
    writePositionStatusArea.setLeft(writeLengthStatusArea.left() + writeLengthStatusArea.width());
    writePositionStatusArea.adjust(1,1,0,-1);

    allMeasureHeadersArea.setRect(cellArea.left(), 0,
                                  cellArea.width(), VIEW_MEASURE_HEADER_HEIGHT);

    // create 2 boolean entries for each pixel in cell area to remember out-of-bound notes
    for(int direction=0; direction < 2; ++direction)
    {
        outOfBoundNoteRange[direction].clear();
        for(int cellAreaX=0; cellAreaX < cellArea.width(); ++cellAreaX)
            outOfBoundNoteRange[direction].append(false);
    }

    // --------------------------------------------------------------------------------------------

    mapper.refreshDisplayedItemLists();
    update();

    if(initialViewResize)controller->viewInitiallyResized();
}

void View::paintEvent ( QPaintEvent* event )
{
    initialViewResize=false;    // no more initial resize events after first paint event

    QPainter painter(this);

    // Background
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Window));
    painter.drawRect(rect());

    SelRectXPos selPosX=mapper.getSelectionRectXPosition();

    // Paint content
    painter.save();
    paintStatusArea(painter,event->region());
    painter.restore();

    painter.save();
    paintMeasureHeaders(painter,event->region(),selPosX);
    painter.restore();

    painter.save();
    paintTracks(painter,event->region(),selPosX);
    painter.restore();

    paintEndGradients(painter);

    // Keep the track-pane boundary aligned with the first measure-header edge.
    painter.setPen(QPen(QColor(18,31,52),1));
    painter.drawLine(cellArea.left(), 0, cellArea.left(), cellArea.bottom());
}

void View::paintStatusArea(QPainter& painter, const QRegion& updateRegion)
{
    if(!updateRegion.intersects(statusArea))return;   // no update required

    // draw background, separation lines and shadow lines
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadedPaletteColor(.5,
                                        palette().color(QPalette::Window),
                                        palette().color(QPalette::Base)));
    painter.drawRect(statusArea);

    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawLine(statusArea.left(),     // top
                     statusArea.top(),
                     statusArea.right(),
                     statusArea.top());
    painter.drawLine(statusArea.left(),     // bottom
                     statusArea.bottom(),
                     statusArea.right(),
                     statusArea.bottom());

    painter.setPen(shadowColor);
    painter.drawLine(statusArea.left(),     // shadow below
                     statusArea.bottom() + 1,
                     statusArea.right() + 1,
                     statusArea.bottom() + 1);

    painter.drawLine(writePositionStatusArea.left(),     // box separator
                     writePositionStatusArea.top(),
                     writePositionStatusArea.left(),
                     writePositionStatusArea.bottom());

    // paint contents
    painter.setClipRect(statusArea, Qt::IntersectClip);

    // Align the note symbol's bottom with the values in the adjacent status box.
    QImage noteImage=noteImageMap[getEditorState().writeLength.denominator];
    QSize noteImageTargetSize(18,36);
    const int valueBaseline=statusArea.bottom() - statusArea.height() / 3;

    QRect noteImageTargetRect(7 * writeLengthStatusArea.width() / 10 - noteImageTargetSize.width() / 2,
                              valueBaseline - noteImageTargetSize.height(),
                              noteImageTargetSize.width(),
                              noteImageTargetSize.height());
    painter.drawImage(noteImageTargetRect, noteImage, QRect(QPoint(0,0),noteImage.size()));

    // tupletNominator and tupletDenominator settings
    if(getEditorState().writeLength.tupletDenominator != 1)
    {
        QString tupletStr;
        if(!(   // if not using tuplet ratios (2:3),(3:2), or (4:5) print additional message showing denominator
           (getEditorState().writeLength.tupletNominator == 2 && getEditorState().writeLength.tupletDenominator == 3) ||
           (getEditorState().writeLength.tupletNominator == 3 && getEditorState().writeLength.tupletDenominator == 2) ||
           (getEditorState().writeLength.tupletNominator == 4 && getEditorState().writeLength.tupletDenominator == 5)))
        {
            tupletStr=tr("%1/%2").
                      arg(getEditorState().writeLength.tupletDenominator).
                      arg(getEditorState().writeLength.tupletNominator);
        }
        else
        {
            tupletStr=tr("%1").arg(getEditorState().writeLength.tupletDenominator);
        }

        QRect boundingRect;
        painter.setFont(tupletBracketFont);

        painter.drawText(QRect(noteImageTargetRect.center().x() - writeLengthStatusArea.width() / 2, 0,
                               writeLengthStatusArea.width(), 16),
                         Qt::AlignCenter | Qt::AlignVCenter,
                         tupletStr,
                         &boundingRect);

        // draw tuplet bracket
        painter.setPen(QPen(palette().windowText(),2,Qt::SolidLine,Qt::SquareCap,Qt::MiterJoin));
        static const QSize bracketSize(10,4);
        static const int BRACKET_SPACE_X=3;

        for(int side=-1; side <= 1; side+=2)
        {
            QPoint p[3];
            p[0]=boundingRect.center();
            p[0].setX(p[0].x() + side * (boundingRect.width() / 2 + BRACKET_SPACE_X));
            p[1].setX(p[0].x() + side * bracketSize.width());
            p[2].setX(p[1].x());
            p[1].setY(p[0].y());
            p[2].setY(p[1].y() + 4);

            painter.drawPolyline(p,3);
        }
    }
    else
    {
        // If tuplet denominator is 1, nominator must also be 1
        Q_ASSERT(getEditorState().writeLength.tupletNominator == 1);
    }

    // draw base write length text

    QRect DescriptionTextRect=writeLengthStatusArea;
    DescriptionTextRect.adjust(3,0,0,0);
    DescriptionTextRect.setBottom(22);

    QFont sectionLabelFont(infoFont);
    sectionLabelFont.setPointSize(infoFont.pointSize() + 1);
    painter.setFont(sectionLabelFont);
    painter.setPen(shadedPaletteColor(.5,
                                      palette().color(QPalette::WindowText),
                                      palette().color(QPalette::Window)));
    painter.drawText(DescriptionTextRect,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Cell length"));

    QString baseWriteLengthStr=tr("1/%1").arg(getEditorState().writeLength.denominator);

    painter.setFont(mediumFont);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(QPoint(writeLengthStatusArea.left() + 3, valueBaseline),
                     baseWriteLengthStr);


    // draw write position text
    DescriptionTextRect=writePositionStatusArea;
    DescriptionTextRect.setBottom(22);

    painter.setFont(sectionLabelFont);
    painter.setPen(shadedPaletteColor(.5,
                                      palette().color(QPalette::WindowText),
                                      palette().color(QPalette::Window)));
    painter.drawText(DescriptionTextRect,
                     Qt::AlignCenter | Qt::AlignVCenter,
                     tr("Beat"));

    writePositionStatusBeatCount=getWritePositionStatusBeatCount();
    QString writePositionString=tr("%1").arg(writePositionStatusBeatCount + 1.,0,'g',3);

    painter.setFont(bigFont);
    painter.setPen(palette().color(QPalette::WindowText));
    const QFontMetrics beatCountMetrics(bigFont);
    const int beatTextX=writePositionStatusArea.center().x() -
            beatCountMetrics.horizontalAdvance(writePositionString) / 2;
    painter.drawText(QPoint(beatTextX, valueBaseline), writePositionString);
}

void View::paintMeasureHeaders(QPainter& painter, const QRegion& updateRegion, const SelRectXPos& selPosX)
{
    // Draw bottom shadow line
    painter.setPen(palette().color(QPalette::WindowText));
    painter.setPen(shadowColor);
    painter.drawLine(allMeasureHeadersArea.left(),
                     allMeasureHeadersArea.bottom() + 1,
                     allMeasureHeadersArea.right() + 1,
                     allMeasureHeadersArea.bottom() + 1);

    QColor BeatLineColor=shadedPaletteColor(.5);

    // Measure loop, pass 1: draw background items
    for(int i=0; i < mapper.getDisplayedMeasureList().size(); ++i)
    {
        DisplayedMeasure* dm=mapper.getDisplayedMeasureList()[i];

        QRect measureHeaderRect(allMeasureHeadersArea.left() + dm->leftX, allMeasureHeadersArea.top(),
                                dm->rightX - dm->leftX, allMeasureHeadersArea.height());
        if(!updateRegion.intersects(measureHeaderRect))continue;   // no update required

        painter.save();
        painter.setClipRect(measureHeaderRect, Qt::IntersectClip);

        // -----------------------------------------------------------------------------
        // draw header box and background
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawLine(measureHeaderRect.left(),measureHeaderRect.top(),
                         measureHeaderRect.left(),measureHeaderRect.bottom());
        painter.drawLine(measureHeaderRect.left(),measureHeaderRect.top(),
                         measureHeaderRect.right(),measureHeaderRect.top());
        painter.drawLine(measureHeaderRect.left(),measureHeaderRect.bottom(),
                         measureHeaderRect.right(),measureHeaderRect.bottom());

        measureHeaderRect.adjust(1,1,0,-1);

        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().window());
        painter.drawRect(measureHeaderRect);

        // If a any measure item (but not a rehearsal marker) is on that measure
        //  draw lighter background to enhance visibility
        if(dm->measureProperties.setTimeSignature ||
           dm->measureProperties.setKeySignature ||
           dm->measureProperties.setTempo ||
           dm->measureProperties.setPlaybackOptions)
        {
            QRect measureItemsCell=measureHeaderRect;
            measureItemsCell.setHeight(VIEW_MEASURE_HEADER_ITEMS_CELL_HEIGHT);

            QLinearGradient gradient(
                    measureItemsCell.topLeft(), measureItemsCell.topRight());
            gradient.setColorAt(0,palette().color(QPalette::Base));
            gradient.setColorAt(1,palette().color(QPalette::Window));

            painter.setBrush(QBrush(gradient));
            painter.drawRect(measureItemsCell);
        }

        // advance to next measure
        painter.restore();
    }

    // Selection rectangle if in global measure selection
    if(getEditorState().selection.getSelectionMode() == S_GlobalMeasure)
    {
        int leftX  = qMax(selPosX.left , 0)               ;
        int rightX = qMin(selPosX.right, cellArea.width());

        if(rightX > leftX)  // Area
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(selectionRectFillColorHeaders));
            painter.drawRect(QRect(QPoint(cellArea.left() + leftX , 1),
                                   QPoint(cellArea.left() + rightX, allMeasureHeadersArea.bottom()-1)));
        }

        painter.setPen(QPen(selectionRectFrameColor,VIEW_SELECTION_FRAME_THICKNESS));
        if(rightX > leftX)  // Top border line
        {
            painter.drawLine(QPoint(cellArea.left() + leftX , 0),
                             QPoint(cellArea.left() + rightX, 0));
        }
        if(selPosX.left >= 0 && selPosX.left < INT_MAX)     // Left border line
        {
            painter.drawLine(QPoint(cellArea.left() + selPosX.left, 0),
                             QPoint(cellArea.left() + selPosX.left, allMeasureHeadersArea.bottom()-1));
        }
        if(selPosX.right >= 0 && selPosX.right < INT_MAX)   // Right border line
        {
            painter.drawLine(QPoint(cellArea.left() + selPosX.right, 0),
                             QPoint(cellArea.left() + selPosX.right, allMeasureHeadersArea.bottom()-1));
        }
    }

    // Measure loop, pass 2: draw foreground items
    for(int i=0; i < mapper.getDisplayedMeasureList().size(); ++i)
    {
        DisplayedMeasure* dm=mapper.getDisplayedMeasureList()[i];

        QRect measureHeaderRect(allMeasureHeadersArea.left() + dm->leftX, allMeasureHeadersArea.top(),
                                dm->rightX - dm->leftX, allMeasureHeadersArea.height() - 1);
        if(!updateRegion.intersects(measureHeaderRect))continue;   // no update required

        painter.save();
        painter.setClipRect(measureHeaderRect, Qt::IntersectClip);

        // distribute available space in header
        QRect measureItemsCell=measureHeaderRect;
        measureItemsCell.setHeight(VIEW_MEASURE_HEADER_ITEMS_CELL_HEIGHT);

        QRect measureHeaderMarkerCell=measureHeaderRect;
        measureHeaderMarkerCell.setTop(measureItemsCell.y() + measureItemsCell.height());
        measureHeaderMarkerCell.setHeight(VIEW_MEASURE_HEADER_MARKER_CELL_HEIGHT - 1);
        measureHeaderMarkerCell.adjust(1,0,0,0);

        QRect measureIndexCell=measureHeaderRect;
        measureIndexCell.setTop(measureHeaderMarkerCell.y() + measureHeaderMarkerCell.height());

        // -----------------------------------------------------------------------------
        // Draw measure items

        // draw item settings: time signature, playback options, key signature, tempo
        if(dm->measureProperties.setTimeSignature)
        {
            QRect tsCell(measureItemsCell.left(), measureItemsCell.top(),
                         measureItemsCell.left(), measureItemsCell.height());

            QRect nominatorCell=tsCell;
            nominatorCell.setBottom(tsCell.center().y()+1);
            nominatorCell.setWidth(20);
            QRect denominatorCell=tsCell;
            denominatorCell.setTop(tsCell.center().y()-1);
            denominatorCell.setWidth(20);

            painter.setFont(timeSignatureFont);
            painter.setPen(palette().color(QPalette::WindowText));
            painter.drawText(nominatorCell,
                             Qt::AlignCenter | Qt::AlignVCenter,
                             tr("%1").arg(dm->measureProperties.timeSignatureNominator));
            painter.drawText(denominatorCell,
                             Qt::AlignCenter | Qt::AlignVCenter,
                             tr("%1").arg(dm->measureProperties.timeSignatureDenominator));

            measureItemsCell.adjust(20+6,0,0,0);
        }
        else measureItemsCell.adjust(4,0,0,0);

        if(dm->measureProperties.setPlaybackOptions)
        {
            QRect poCell(measureItemsCell.left(), measureItemsCell.top(),
                         measureItemsCell.left(), measureItemsCell.height());

            QString playbackOptionsText;
            if(dm->measureProperties.swingHardness == 0) playbackOptionsText+=tr("No swing");
            else playbackOptionsText+=tr("Swing");

            painter.setFont(miniFont);
            QFontMetrics miniFontMetrics(miniFont);

            painter.setPen(palette().color(QPalette::WindowText));
            painter.drawText(poCell, Qt::AlignLeft | Qt::AlignVCenter, playbackOptionsText);

            measureItemsCell.adjust(miniFontMetrics.boundingRect(playbackOptionsText).width()+6,0,0,0);
        }
        else measureItemsCell.adjust(4,0,0,0);

        if(dm->measureProperties.setKeySignature)
        {
            QRect ksCell(measureItemsCell.left(), measureItemsCell.top(),
                         measureItemsCell.left() + 30, measureItemsCell.height());

            QString keySignatureName=dm->measureProperties.keySignatureName();

            painter.setFont(miniFont);
            QFontMetrics miniFontMetrics(miniFont);

            painter.setPen(palette().color(QPalette::WindowText));
            painter.drawText(ksCell, Qt::AlignLeft | Qt::AlignVCenter, keySignatureName);

            measureItemsCell.adjust(miniFontMetrics.boundingRect(keySignatureName).width()+6,0,0,0);
        }

        if(dm->measureProperties.setTempo)
        {
            QImage noteImage=noteImageMap[dm->measureProperties.timeSignatureDenominator];

            // increase target rectangle for maybe large note images, but clip

            QRect noteImageTargetRect=measureItemsCell.adjusted(0,-10,0,-4);
            if(dm->measureProperties.timeSignatureDenominator >= 64) // shift down 64th and 128th note (very rare)
                noteImageTargetRect.translate(0,2);

            noteImageTargetRect.setWidth(noteImageTargetRect.height());

            QSize targetSize=noteImage.size();
            targetSize.scale(noteImageTargetRect.height(),noteImageTargetRect.height(), Qt::KeepAspectRatio);

            QRect imageTargetRect(noteImageTargetRect.topLeft(), targetSize);

            painter.save();
            painter.setClipRect(measureItemsCell);
            painter.drawImage(imageTargetRect,
                              noteImage,
                              QRect(QPoint(0,0),noteImage.size()));
            painter.restore();

            measureItemsCell.setLeft(imageTargetRect.right() + 2);

            QRect bpmTextCell(measureItemsCell.left(), measureItemsCell.top(),
                              measureItemsCell.left() + 30, measureItemsCell.height());

            QString s=tr("= %1").arg(dm->measureProperties.BPM);
            painter.setFont(miniFont);
            painter.setPen(palette().color(QPalette::WindowText));
            painter.drawText(bpmTextCell, Qt::AlignLeft | Qt::AlignVCenter, s);
        }

        // -----------------------------------------------------------------------------
        // Draw beat lines and measure index

        painter.setPen(BeatLineColor);
        for(int iBeat=1; iBeat < dm->measureProperties.timeSignatureNominator; ++iBeat)
        {
            // draw on rightmost position of previous cell
            TicksToViewXResult r=mapper.ticksToViewX(
                    dm->tickPosition + iBeat * docRoot->ticksPerBeat(dm->measureProperties));

            int x=r.cellLeftX + r.cellInternalOffsetX;
            painter.drawLine(x,measureIndexCell.top(),
                             x,measureIndexCell.bottom());
        }

        painter.setPen(palette().color(QPalette::WindowText));
        painter.setFont(miniFont);
        painter.drawText(measureIndexCell.adjusted(4,1,0,0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         tr("%1").arg(dm->measureIndex + 1));

        // -----------------------------------------------------------------------------
        // Draw rehearsal markers

        // background
        if(dm->measureOffsetToLastRehearsalMarker == -1)
        {
            // No marker available. Draw dark gray block for better layout style.
            painter.setPen(Qt::NoPen);
            painter.setBrush(shadedPaletteColor(.2f,
                                                palette().color(QPalette::WindowText),
                                                palette().color(QPalette::Window)));
            painter.drawRect(measureHeaderMarkerCell);
        }
        else
        {
            // prepare a pastel color version of last marker color for cell background
            QColor backgroundMarkerColor=makePastelColor(dm->measureProperties.rehearsalMarkerColor,2);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(backgroundMarkerColor));
            painter.drawRect(measureHeaderMarkerCell);

            // draw marker cell contents
            if(dm->measureProperties.setRehearsalMarker)
            {
                QLinearGradient gradient(
                        measureHeaderMarkerCell.topLeft(), measureHeaderMarkerCell.topRight());
                gradient.setColorAt(0,dm->measureProperties.rehearsalMarkerColor);
                gradient.setColorAt(1,backgroundMarkerColor);

                painter.setBrush(QBrush(gradient));
                painter.drawRect(measureHeaderMarkerCell);

                // Select black or white as text color depending on brightness of marker color
                //  to always get a good contrast
                painter.setPen(QColor(dm->measureProperties.rehearsalMarkerColor.value() > 128 ?
                                      Qt::black : Qt::white));
                painter.setFont(mediumFont);
                painter.drawText(measureHeaderMarkerCell.adjusted(2,2,-2,-2),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 dm->measureProperties.rehearsalMarkerText);
            }
            else
            {
                // Draw measure offset to last marker (one-based index).
                // Repeat marker name in first displayed measure.
                QString s;
                if(dm->measureIndex == getEditorState().firstMeasure)
                {
                    s=tr("%1 (%2)").
                      arg(dm->measureOffsetToLastRehearsalMarker + 1).
                      arg(dm->measureProperties.rehearsalMarkerText);
                }
                else
                {
                    s=tr("%1").arg(dm->measureOffsetToLastRehearsalMarker + 1);
                }

                painter.setFont(miniFont);
                painter.setPen(palette().color(QPalette::WindowText));
                painter.drawText(measureHeaderMarkerCell.adjusted(4,0,0,0),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 s);
            }
        }

        // advance to next measure
        painter.restore();
    }

    // -----------------------------------------------------------------------------
    // playback position line

    if(getVolatileEditorState().playbackLineCellX != -1)
    {
        drawPlaybackPositionLine(painter,
                                 getVolatileEditorState().playbackLineCellX,
                                 allMeasureHeadersArea.top(),
                                 allMeasureHeadersArea.bottom(),
                                 playbackPositionLineColor);
    }
}

void View::paintTracks(QPainter& painter, const QRegion& updateRegion, const SelRectXPos& selPosX)
{
    // Prepare variable sized note name font, depending on y-zoom
    int noteNameFontPointSize=(int)(.7 * getEditorState().getNoteHeightInPixels());
    if(noteNameFontPointSize < 8)noteNameFontPointSize=8;
    if(noteNameFontPointSize > 12)noteNameFontPointSize=12;

    QFont noteNameFont(VIEW_FONT_NAME, noteNameFontPointSize, QFont::Bold);

    // Track loop
    for(int i=0; i < mapper.getDisplayedTrackList().size(); ++i)
    {
        DisplayedTrack* dt=mapper.getDisplayedTrackList()[i];

        QRect trackRect,panelRect,rangeSliderRect,trackCellsRect;
        generateTrackRects(dt,trackRect,panelRect,rangeSliderRect,trackCellsRect);

        // draw top/bottom track borders and bottom shadow
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawLine(panelRect.left(),
                         trackCellsRect.top() - 1,
                         trackCellsRect.right(),
                         trackCellsRect.top() - 1);
        painter.drawLine(panelRect.left(),
                         trackCellsRect.bottom() + 1,
                         trackCellsRect.right(),
                         trackCellsRect.bottom() + 1);

        painter.setPen(shadowColor);
        painter.drawLine(panelRect.left() + 1,
                         trackCellsRect.bottom() + 2,
                         trackCellsRect.right(),
                         trackCellsRect.bottom() + 2);

        // Selection rectangle if in global track selection
        bool headerInSelection=
                getEditorState().selection.getSelectionMode() == S_GlobalTrack &&
                dt->trackIndex >= getEditorState().selection.trackTop &&
                dt->trackIndex <= getEditorState().selection.trackBottom;

        // Draw track contents

        if(updateRegion.intersects(panelRect))
        {
            painter.save();
            paintTrackHeaderPanel(painter, panelRect, dt->trackIndex, headerInSelection);
            painter.restore();
        }

        if(updateRegion.intersects(rangeSliderRect))
        {
            painter.save();
            paintTrackHeaderRangeSlider(painter,rangeSliderRect, dt->trackIndex, headerInSelection);
            painter.restore();
        }

        if(updateRegion.intersects(trackCellsRect))
        {
            painter.save();
            paintTrackCells(painter, updateRegion, trackCellsRect, dt->trackIndex, selPosX, noteNameFont);
            painter.restore();
        }

        // Selection rectangle (frame only) if in global track selection
        if(getEditorState().selection.getSelectionMode() == S_GlobalTrack &&
           dt->trackIndex >= getEditorState().selection.trackTop &&
           dt->trackIndex <= getEditorState().selection.trackBottom)
        {
            // Left border line
            painter.setPen(QPen(selectionRectFrameColor,VIEW_SELECTION_FRAME_THICKNESS));
            painter.drawLine(QPoint(trackRect.left(), trackRect.top()    - 1),
                             QPoint(trackRect.left(), trackRect.bottom() + 1));

            if(dt->trackIndex == getEditorState().selection.trackTop)    // Top border line
            {
                painter.drawLine(QPoint(trackRect.left(),  trackRect.top() - 1),
                                 QPoint(cellArea.left()-1, trackRect.top() - 1));
            }
            if(dt->trackIndex == getEditorState().selection.trackBottom) // Bottom border line
            {
                painter.drawLine(QPoint(trackRect.left(),  trackRect.bottom() + 1),
                                 QPoint(cellArea.left()-1, trackRect.bottom() + 1));
            }
        }
    }

    // Append "add new track" button below last track panel
    int addTrackButtonY;
    if(mapper.getDisplayedTrackList().isEmpty())
        addTrackButtonY = 0;
    else
        addTrackButtonY = mapper.getDisplayedTrackList().last()->cellBottomY + VIEW_TRACK_SEPARATOR_Y_INBETWEEN;

    if(addTrackButtonY < cellArea.bottom())
    {
        addNewTrackButtonRect.setRect(
                VIEW_TRACK_SEPARATOR_X_LEFT, cellArea.top() + addTrackButtonY - 1,
                VIEW_TRACK_HEADER_WIDTH, 24);

        painter.setPen(QPen(QColor("#315b82"), 1));
        painter.setBrush(QColor("#dce9f5"));
        painter.drawRoundedRect(addNewTrackButtonRect.adjusted(0, 0, -1, -1), 3, 3);

        const QRect iconRect(addNewTrackButtonRect.left() + 3,
                             addNewTrackButtonRect.center().y() - 10, 20, 20);
        const QImage icon=trackIconsMap[IconAdd];
        painter.drawImage(iconRect, icon, QRect(QPoint(0, 0), icon.size()));

        painter.setPen(QColor("#203f5e"));
        painter.setFont(mediumFont);
        QRect labelRect=addNewTrackButtonRect.adjusted(28, 0, -4, 0);
        painter.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, tr("Add New Track"));
    }
    else addNewTrackButtonRect=QRect();     // null rectangle
}

void View::generateTrackRects(DisplayedTrack* dt, QRect& trackRect, QRect& panelRect, QRect& rangeSliderRect, QRect& trackCellsRect) const
{
    trackRect.setRect(VIEW_TRACK_SEPARATOR_X_LEFT,
                      cellArea.top() + dt->cellTopY,
                      cellArea.width() - VIEW_TRACK_SEPARATOR_X_LEFT,
                      dt->cellBottomY - dt->cellTopY - 1);

    panelRect.setRect(trackRect.left(),
                      trackRect.top(),
                      VIEW_TRACK_HEADER_PANEL_WIDTH,
                      trackRect.height());

    rangeSliderRect.setRect(panelRect.x() + panelRect.width(),
                            trackRect.top(),
                            VIEW_TRACK_HEADER_NOTE_RANGE_SLIDER_WIDTH,
                            trackRect.height());

    trackCellsRect.setRect(cellArea.left(), trackRect.top(),
                           cellArea.width(),trackRect.height());
}

void View::paintTrackHeaderPanel(QPainter& painter, const QRect& panelRect, int trackIndex, bool headerInSelection)
{
    EditorTrackState ts=getEditorState().trackStateList[trackIndex];
    DocTrack* track=docRoot->trackList[trackIndex];

    painter.setClipRect(panelRect, Qt::IntersectClip);

    // draw separation lines
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawLine(panelRect.left(),panelRect.top(),
                     panelRect.left(),panelRect.bottom());
    painter.drawLine(panelRect.right(),panelRect.top(),
                     panelRect.right(),panelRect.bottom());

    // draw background
    QLinearGradient panelBackgroundGradient(panelRect.topLeft(), panelRect.bottomLeft());
    panelBackgroundGradient.setColorAt(0, shadedPaletteColor(0.8,
                                                             palette().color(QPalette::Window),
                                                             palette().color(QPalette::Base)));
    panelBackgroundGradient.setColorAt(1, shadedPaletteColor(0.9,
                                                             palette().color(QPalette::Window),
                                                             palette().color(QPalette::WindowText)));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(panelBackgroundGradient));
    painter.drawRect(panelRect.adjusted(1,0,-1,0));

    if(headerInSelection)
    {
        // draw selection area before foreground elements are painted
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(selectionRectFillColorHeaders));
        painter.drawRect(panelRect.adjusted(1,0,-1,0));
    }

    // draw track name
    painter.setPen(palette().color(QPalette::WindowText));
    painter.setFont(trackNameFont);
    painter.drawText(trackNameRect.translated(panelRect.topLeft()),
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
                     track->name);

    // draw track buttons
    bool buttonState[MAX_TRACK_PANEL_BUTTONS];
    buttonState[0]=false;   // delete button
    buttonState[1]=ts.solo;
    buttonState[2]=ts.mute;
    buttonState[3]=ts.recordingEnabled;
    Q_ASSERT(MAX_TRACK_PANEL_BUTTONS == 4);

    for(int i=0; i < MAX_TRACK_PANEL_BUTTONS; ++i)
    {
        QPoint buttonTopLeft(panelRect.left() + TRACK_PANEL_BUTTON_POSITIONS[i].x,
                             panelRect.top()  + TRACK_PANEL_BUTTON_POSITIONS[i].y);

        QRect buttonRect(QRect(buttonTopLeft,
                               QSize(TRACK_PANEL_BUTTON_POSITIONS[i].sx,
                                     TRACK_PANEL_BUTTON_POSITIONS[i].sy)));

        // Keep Solo, Mute, and Record self-explanatory inside their own
        // compact button. The previous separate letters and symbols crowded
        // the track header at this size.
        QString letter;
        QColor activeColor;
        switch(TRACK_PANEL_BUTTON_POSITIONS[i].zoneType)
        {
        case TrackButtonSolo:  letter=QStringLiteral("S"); activeColor=QColor("#3974ad"); break;
        case TrackButtonMute:  letter=QStringLiteral("M"); activeColor=QColor("#3974ad"); break;
        case TrackButtonRecord:letter=QStringLiteral("R"); activeColor=QColor("#c84c4c"); break;
        default:;
        }
        if(!letter.isEmpty())
        {
            const QRect letterButtonRect=buttonRect.adjusted(6,1,-6,-1);
            const bool active=buttonState[i];
            const QColor borderColor=active ? activeColor : QColor("#72879b");
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(borderColor, 1));
            painter.setBrush(active ? activeColor : QColor("#edf3f8"));
            painter.drawRoundedRect(letterButtonRect, 3, 3);

            painter.setPen(active ? QColor(Qt::white) : QColor("#52687d"));
            painter.setFont(QFont(VIEW_FONT_NAME, 9, QFont::Bold));
            painter.drawText(letterButtonRect.adjusted(1, 1, -1, -1),
                             Qt::AlignCenter, letter);
        }
        else
        {
            const QPoint iconOffset(TRACK_PANEL_BUTTON_POSITIONS[i].icon_x,
                                    TRACK_PANEL_BUTTON_POSITIONS[i].icon_y);
            painter.drawImage(buttonTopLeft + iconOffset,
                              trackIconsMap[TRACK_PANEL_BUTTON_POSITIONS[i].iconIndex]);
        }
    }

    // draw MIDI settings
    QString patchName;
    if(track->midiPatch >= 1 && track->midiPatch <= MIDI_N_PATCH_NAMES)
        patchName=MIDI_PATCH_NAME[track->midiPatch - 1];

    painter.setPen(palette().color(QPalette::WindowText));
    painter.setFont(infoFont);

    QRect r=trackMidiSettingsRect.translated(panelRect.topLeft());

    QRect boundingRect;
    painter.drawText(r, Qt::AlignLeft | Qt::TextSingleLine,patchName,&boundingRect);

    r.adjust(0,boundingRect.height(),0,0);
    painter.drawText(r, Qt::AlignLeft, tr("Patch\nChannel\nVolume\nPan"),&boundingRect);

    r.adjust(boundingRect.width() + 8,0,0,0);
    painter.drawText(r, Qt::AlignLeft, tr(":\n:\n:\n:"));

    r.adjust(5,0,0,0);
    r.setWidth(20);
    painter.drawText(r, Qt::AlignRight,
                     tr("%1\n%2\n%3\n%4").arg(track->midiPatch).arg(track->midiChannel).
                     arg(track->midiVolume).arg(track->midiPanorama));

    // A compact MIDI activity indicator; its level follows note velocity, not rendered audio.
    const QRect meterRect(panelRect.right()-8, panelRect.top()+4, 6, qMax(1,panelRect.height()-8));
    painter.setPen(QColor("#405c76"));
    painter.setBrush(QColor("#26394c"));
    painter.drawRect(meterRect);

    const QRect meterInner=meterRect.adjusted(1,1,-1,-1);
    const int level=trackIndex < midiActivityLevels.size() ? midiActivityLevels[trackIndex] : 0;
    const int segmentCount=qMax(1,(meterInner.height()+1)/4);
    const int litSegments=(level*segmentCount+126)/127;
    for(int segment=0; segment < litSegments; ++segment)
    {
        const int top=meterInner.bottom()-segment*4-2;
        if(top < meterInner.top())break;

        const qreal position=static_cast<qreal>(segment)/segmentCount;
        const QColor color=position >= 0.88 ? QColor("#cf4b4b")
                         : position >= 0.72 ? QColor("#dbb245")
                                            : QColor("#36a86b");
        painter.fillRect(QRect(meterInner.left(),top,meterInner.width(),3),color);
    }
}

void View::paintTrackHeaderRangeSlider(QPainter& painter, const QRect& rangeSliderRect, int trackIndex, bool headerInSelection)
{
    EditorTrackState ts=getEditorState().trackStateList[trackIndex];

    double noteHeightInPixels=getEditorState().getNoteHeightInPixels();

    QColor bigOctaveNumberColor=shadedPaletteColor(.4,
                                                   palette().color(QPalette::WindowText),
                                                   palette().color(QPalette::Window));

    // draw (usually dark-gray) background
    painter.setPen(Qt::NoPen);
    QColor rangeSliderBackgroundColor=shadedPaletteColor(.15,
                                                         palette().color(QPalette::WindowText),
                                                         palette().color(QPalette::Window));
    painter.setBrush(rangeSliderBackgroundColor);
    painter.drawRect(rangeSliderRect);

    if(headerInSelection)
    {
        // draw selection area before foreground elements are painted
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(selectionRectFillColorHeaders));
        painter.drawRect(rangeSliderRect);
    }

    // draw MIDI octaves
    int midiNoteBottom,midiNoteTop;
    getDisplayedMidiNoteRangeForTrack(rangeSliderRect, trackIndex, midiNoteBottom, midiNoteTop);

    // Round limits up/down to next octave
    midiNoteBottom=12*(midiNoteBottom/12);
    midiNoteTop   =12*(midiNoteTop   /12+1);

    painter.setClipRect(rangeSliderRect, Qt::IntersectClip);

    int trackCenterY= rangeSliderRect.center().y();
    for(int note=midiNoteBottom; note < midiNoteTop; note+=12)
    {
        int midiOctave = note / 12;

        double bottomC_CenterY=trackCenterY - (note - ts.centerMidiNote) * noteHeightInPixels;
        double bottomC_BottomY=bottomC_CenterY + noteHeightInPixels / 2;

        double topC_CenterY=trackCenterY - (note + 12 - ts.centerMidiNote) * noteHeightInPixels;
        double topC_BottomY=topC_CenterY + noteHeightInPixels / 2;

        double octave_CenterY=(topC_BottomY + bottomC_BottomY) / 2;

        // draw rectangle showing octave color corresponding to mouse piano octave color
        painter.setBrush(QBrush(MOUSEPIANO_OCTAVE_COLORS[midiOctave]));
        painter.setPen(Qt::NoPen);
        painter.drawRect(rangeSliderRect.left(), (int)bottomC_BottomY - 1,
                         rangeSliderRect.width(),2);

        // draw big MIDI octave number
        painter.setPen(bigOctaveNumberColor);
        painter.setFont(bigFont);
        int height=painter.fontMetrics().height();
        QRect octaveNumberTextRect(rangeSliderRect.left(), (int)(octave_CenterY - height/2),
                                   rangeSliderRect.width(),height);
        painter.drawText(octaveNumberTextRect,
                         Qt::AlignCenter | Qt::AlignVCenter,
                         QString("%1").arg(midiOctave));

        // draw bottom C octave in musical notation standard
        QString CText;
        if(midiOctave >= 4)
        {
            CText=tr("c");
            for(int i=4; i < midiOctave; ++i)
                CText+='\'';
        }
        else
        {
            for(int i=3; i > midiOctave; --i)
                CText+=',';
            CText+=tr("C");
        }

		// horizontally shrink the text in case of octaves with very many ' or , characters
        QFont variableStretchFont=octaveCFont;
        if(midiOctave == 0 || midiOctave >= 8)
			variableStretchFont.setStretch(80);
			
        painter.setFont(variableStretchFont);

        QRect CTextRect(rangeSliderRect.left(), (int)(bottomC_BottomY - 20),
                        rangeSliderRect.width(),20);
        painter.drawText(CTextRect,
                         Qt::AlignCenter | Qt::AlignBottom,
                         CText);
    }
}

void View::paintTrackCells(QPainter& painter, const QRegion& updateRegion, const QRect& trackCellsRect, int trackIndex, const SelRectXPos& selPosX, const QFont& noteNameFont)
{
    painter.save();
    painter.setClipRect(trackCellsRect, Qt::IntersectClip);

    // Color shading definitions for grid structure. Draw lighter lines first, darker lines last.
    QColor GridColorMeasureBorder=shadedPaletteColor(.6);
    QColor GridColorBeatLine=shadedPaletteColor(.3);
    QColor GridColorOctaveSeparatorLine=shadedPaletteColor(.4);
    QColor GridColorCellBorder=shadedPaletteColor(.13);

    // -----------------------------------------------------------------------------
    // draw background
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadedPaletteColor(0));
    painter.drawRect(trackCellsRect);

    // -----------------------------------------------------------------------------
    // mark shrinked cells

    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(GridColorCellBorder,Qt::Dense5Pattern));
    for(int i=0; i < mapper.getDisplayedCellList().size() - 1; ++i)
    {
        DisplayedCell* dc=mapper.getDisplayedCellList()[i];
        DisplayedCell* dcNext=mapper.getDisplayedCellList()[i+1];
        if(!dc->shrinked)continue;

        QRect ShrinkedCellBackgroundRect(cellArea.left() + dc->leftX, trackCellsRect.top(),
                                         dcNext->leftX - dc->leftX, trackCellsRect.height());

        if(updateRegion.intersects(ShrinkedCellBackgroundRect)) // Optimize for update region
            painter.drawRect(ShrinkedCellBackgroundRect);
    }

    // -----------------------------------------------------------------------------
    // draw track note separator BARS (horizontal black & white piano keys)

    int midiNoteBottom,midiNoteTop;
    getDisplayedMidiNoteRangeForTrack(trackCellsRect, trackIndex, midiNoteBottom, midiNoteTop);
    int trackCenterY= trackCellsRect.center().y();

    EditorTrackState ts=getEditorState().trackStateList[trackIndex];
    double noteHeightInPixels=getEditorState().getNoteHeightInPixels();

    painter.setBrush(GridColorCellBorder);
    for(int note=midiNoteBottom; note <= midiNoteTop; ++note)
    {
        int noteCenterY=(int)(trackCenterY - (note - ts.centerMidiNote) * noteHeightInPixels);
        int noteBottomY=(int)(noteCenterY + noteHeightInPixels / 2);

        switch(note % 12)
        {
            // black key on piano?
        case 1:
        case 3:
        case 6:
        case 8:
        case 10:
            {
                int noteTopY=(int)(noteBottomY - noteHeightInPixels);
                QRect blackKeyRect(trackCellsRect.left(),noteTopY,
                                   trackCellsRect.width(),noteBottomY - noteTopY);
                if(blackKeyRect.height() > 2)blackKeyRect.adjust(0,0,0,-1);
                if(blackKeyRect.height() > 2)blackKeyRect.adjust(0,1,0,0);

                painter.setPen(Qt::NoPen);
                painter.drawRect(blackKeyRect);
                break;
            }
            // C key
        case 0:
            painter.setPen(GridColorCellBorder);
            painter.drawLine(trackCellsRect.left(),noteBottomY,trackCellsRect.right(),noteBottomY);
            break;
        }
    }

    // -----------------------------------------------------------------------------
    // paint cell borders

    // If cell raster is too small, omit CELL border lines or even BEAT border lines
    double standardCellLengthInTicks=
            getEditorState().writeLength.tupletNominator
            * docRoot->midiTicksPerWholeNote
            / getEditorState().writeLength.denominator
            / getEditorState().writeLength.tupletDenominator;
    double standardCellLengthInPixels=standardCellLengthInTicks / getEditorState().getTicksPerPixel(docRoot);

    for(int i=0; i < mapper.getDisplayedCellList().size(); ++i)
    {
        DisplayedCell* dc=mapper.getDisplayedCellList()[i];
        if(dc->measureInternalCellIndex != 0)   // skip first line in measure (measure separator line)
        {
            // draw on leftmost position of current cell
            int x=cellArea.left() + dc->leftX;

            // Optimize for update region
            if(updateRegion.intersects(QRect(x,trackCellsRect.top(),x,trackCellsRect.bottom())))
            {
                DisplayedMeasure* dm=mapper.getDisplayedMeasureList()[dc->measureIndex - getEditorState().firstMeasure];
                int ticksWithinBeat=(dc->tickPosition - dm->tickPosition) %
                                    docRoot->ticksPerBeat(dm->measureProperties);

                if(ticksWithinBeat == 0)
                {
                    // BEAT border line
                    double beatLengthInPixels=docRoot->ticksPerBeat(dm->measureProperties) /
                                              getEditorState().getTicksPerPixel(docRoot);
                    if(beatLengthInPixels < VIEW_MIN_STD_CELL_LENGTH_IN_PIXELS_MANUAL_ZOOM)continue;

                    painter.setPen(GridColorBeatLine);
                }
                else
                {
                    // CELL border line
                    if(standardCellLengthInPixels < VIEW_MIN_STD_CELL_LENGTH_IN_PIXELS_MANUAL_ZOOM)continue;

                    painter.setPen(GridColorCellBorder);
                }

                painter.drawLine(x,trackCellsRect.top(),x,trackCellsRect.bottom());
            }
        }
    }

    // -----------------------------------------------------------------------------
    // paint measure borders

    for(int i=0; i < mapper.getDisplayedCellList().size(); ++i)
    {
        DisplayedCell* dc=mapper.getDisplayedCellList()[i];
        if(dc->measureInternalCellIndex == 0)
        {
            // draw on leftmost position of current cell
            int x=cellArea.left() + dc->leftX;

            // Optimize for update region
            if(updateRegion.intersects(QRect(x,trackCellsRect.top(),x,trackCellsRect.bottom())))
            {
                if(i == 0)
                {
                    // first cell delimits border of cell area to track header
                    painter.setPen(palette().color(QPalette::WindowText));
                }
                else
                    painter.setPen(GridColorMeasureBorder);    // start of measure

                painter.drawLine(x,trackCellsRect.top(),x,trackCellsRect.bottom());
            }
        }
    }

    // -----------------------------------------------------------------------------
    // draw selection rectangle if visible

    painter.restore();

    if(trackIndex >= getEditorState().selection.trackTop &&
       trackIndex <= getEditorState().selection.trackBottom)
    {
        int leftX  = qMax(selPosX.left , 0)               ;
        int rightX = qMin(selPosX.right, cellArea.width());

        if(rightX > leftX)
        {
            // Area
            painter.setPen(Qt::NoPen);
            painter.save();

            // If shift is pressed, draw anchor cell separately. Always use the live state
            //  of the modifier key because showAnchor is not reset when modal dialogs pop up
            if(getEditorState().selection.anchor.track == trackIndex &&
               (QApplication::keyboardModifiers() & Qt::ShiftModifier))
            {
                // Anchor cell always is a single cell
                TicksToViewXResult rx=mapper.ticksToViewX(getEditorState().selection.anchor.ticksLeft);
                TrackToViewYResult ry=mapper.trackToViewY(getEditorState().selection.anchor.track);

                QRect AnchorCellRect(QPoint(rx.cellLeftX,ry.TopY),QPoint(rx.cellRightX,ry.BottomY));
                if(AnchorCellRect.intersects(cellArea))
                {
                    painter.setBrush(QColor(selectionRectAnchorCellFillColor));
                    painter.drawRect(AnchorCellRect);

                    // Set clip region to whole window, but subtract area of anchor cell
                    QRegion ClipRegion(rect());
                    ClipRegion = ClipRegion.subtracted(QRegion(AnchorCellRect));
                    painter.setClipRegion(ClipRegion,Qt::ReplaceClip);
                }
            }

            switch(getVolatileEditorState().keyboardDragMode)
            {
            case VolatileEditorState::KDM_None:
                painter.setBrush(selectionRectFillColorCells);
                break;
            case VolatileEditorState::KDM_ImmediateListen:
                painter.setBrush(selectionRectFillColorCells_DragModeImmediateListen);
                break;
            case VolatileEditorState::KDM_Transpose:
                painter.setBrush(selectionRectFillColorCells_DragModeTranspose);
                break;
            }

            painter.drawRect(QRect(QPoint(cellArea.left() + leftX , trackCellsRect.top()   -1),
                                   QPoint(cellArea.left() + rightX, trackCellsRect.bottom()+1)));
            painter.restore();
        }

        painter.setPen(QPen(selectionRectFrameColor,VIEW_SELECTION_FRAME_THICKNESS));
        if(rightX > leftX)
        {
            // Top border if no global measure selection active and in topmost selected track
            if(getEditorState().selection.getSelectionMode() != S_GlobalMeasure &&
               trackIndex == getEditorState().selection.trackTop)
            {
                painter.drawLine(QPoint(cellArea.left() + leftX , trackCellsRect.top()-1),
                                 QPoint(cellArea.left() + rightX, trackCellsRect.top()-1));
            }
            // Bottom border if no global measure selection active and in bottommost selected track
            //  - OR - global measure selection active and this is the bottommost existing track
            if((getEditorState().selection.getSelectionMode() != S_GlobalMeasure &&
                trackIndex == getEditorState().selection.trackBottom)
                || (getEditorState().selection.getSelectionMode() == S_GlobalMeasure &&
                    trackIndex == docRoot->trackList.size()-1))
            {
                painter.drawLine(QPoint(cellArea.left() + leftX , trackCellsRect.bottom()+1),
                                 QPoint(cellArea.left() + rightX, trackCellsRect.bottom()+1));
            }
        }

        // Left and right borders
        if(selPosX.left >= 0 && selPosX.left < INT_MAX)
        {
            painter.drawLine(QPoint(cellArea.left() + selPosX.left, trackCellsRect.top()   -1),
                             QPoint(cellArea.left() + selPosX.left, trackCellsRect.bottom()+1));
        }
        if(selPosX.right >= 0 && selPosX.right < INT_MAX)
        {
            painter.drawLine(QPoint(cellArea.left() + selPosX.right, trackCellsRect.top()   -1),
                             QPoint(cellArea.left() + selPosX.right, trackCellsRect.bottom()+1));
        }
    }

    painter.setClipRect(trackCellsRect, Qt::IntersectClip);

    // -----------------------------------------------------------------------------
//EXTENSION: draw marker events

    // -----------------------------------------------------------------------------
    // draw note events

    QColor noteEventFrameColor(palette().color(QPalette::WindowText));
    QColor noteEventTopLeftContrastColor(palette().color(QPalette::Base));

    if(!getVolatileEditorState().showNoteNames)painter.setPen(noteEventFrameColor);

    QFontMetrics noteNameFontMetrics(noteNameFont);
    painter.setFont(noteNameFont);
    QRect longestPossibleNoteNameBoundingRect=
            noteNameFontMetrics.boundingRect(DocEvent::NoteEvent::getLongestPossibleName());

    // reset out-of-bound gradient flags
    for(int cellAreaX=0; cellAreaX < cellArea.width(); ++cellAreaX)
        for(int direction=0; direction < 2; ++direction)
            outOfBoundNoteRange[direction][cellAreaX]=false;

    DocTrack* track=docRoot->trackList[trackIndex];
    DocEvent* event=track->firstEvent;
    int rangeLeft =mapper.getDisplayedCellList().first()->tickPosition;
    int rangeRight=mapper.getDisplayedCellList().last ()->tickPosition;
    while(event)
    {
        // Note event within displayed range?
        if(event->type == DocEvent::E_Note && event->touchesRange(rangeLeft, rangeRight))
        {
            // draw note rectangle
            int noteCenterY=(int)(trackCenterY -
                                  (event->noteEventData.noteNumber - ts.centerMidiNote) * noteHeightInPixels);
            int noteBottomY=(int)(noteCenterY + noteHeightInPixels / 2);
            int noteTopY=(int)(noteBottomY - noteHeightInPixels);

            int l,r;
            if(event->tickPosition < rangeLeft)
            {
                l=cellArea.left() - 1;
            }
            else
            {
                TicksToViewXResult r1=mapper.ticksToViewX(event->tickPosition);
                l=r1.cellLeftX + r1.cellInternalOffsetX;
            }

            if(event->tickPosition + event->tickLength >= rangeRight)
            {
                r=cellArea.right() + 1;
            }
            else
            {
                TicksToViewXResult r2=mapper.ticksToViewX(event->tickPosition + event->tickLength);
                r=r2.cellLeftX + r2.cellInternalOffsetX;
            }

            QRect noteRect(l, noteTopY, r-l, noteBottomY - noteTopY - 1);

            // if note is out of displayed note number bounds, remember this for out-of-bound gradient drawing
            QRect visibilityTestRect=noteRect.adjusted(0,1,0,-2);
            if(visibilityTestRect.bottom() < trackCellsRect.top())
            {
                // note too high, enable top gradient at corresponding pixels
                for(int x=noteRect.left(); x < noteRect.right(); ++x)
                {
                    int cellAreaX=x - cellArea.left();
                    if(cellAreaX >= outOfBoundNoteRange[0].size())break;

                    if(cellAreaX >= 0)
                        outOfBoundNoteRange[0][cellAreaX]=true;
                }
            }
            if(visibilityTestRect.top() > trackCellsRect.bottom())
            {
                // note too low, enable bottom gradient at corresponding pixels
                for(int x=noteRect.left(); x < noteRect.right(); ++x)
                {
                    int cellAreaX=x - cellArea.left();
                    if(cellAreaX >= outOfBoundNoteRange[1].size())break;

                    if(cellAreaX >= 0)
                        outOfBoundNoteRange[1][cellAreaX]=true;
                }
            }

            // optimize for update region, but take border line of width 1 into account
            QRect updateIntersectionRect=noteRect;
            if(getVolatileEditorState().showNoteNames)
            {
                // when showing note names, also account for longest possible note name.
                if(updateIntersectionRect.width() < longestPossibleNoteNameBoundingRect.width())
                    updateIntersectionRect.setWidth(longestPossibleNoteNameBoundingRect.width());
            }

            // take border line of width 1 into account
            updateIntersectionRect.adjust(-1,-1,1,1);

            if(updateRegion  .intersects(updateIntersectionRect) &&
               trackCellsRect.intersects(updateIntersectionRect))
            {
                QLinearGradient gradient(noteRect.topLeft(), noteRect.topRight());

                gradient.setColorAt(  0, shadedPaletteColor(0.7, track->eventColor, noteEventTopLeftContrastColor));
                gradient.setColorAt(0.5, track->eventColor);
                gradient.setColorAt(1.0, shadedPaletteColor(0.7, track->eventColor, noteEventFrameColor));

                painter.setBrush(QBrush(gradient));

                if(getVolatileEditorState().showNoteNames)
                {
                    // If displaying note names, first fill by gradient (no frame)
                    painter.setPen(Qt::NoPen);

                    // if frame is drawn only partly, the gradient must be drawn where frame is missing
                    painter.drawRect(noteRect.adjusted(0,0,1,1));

                    // Determine note name. This is costly because we have to determine the key signature
                    //  of the measure where the note starts.
                    int keySignature=docRoot->ticksToMeasure(event->tickPosition).measureProperties.keySignature;
                    QString noteName=event->noteEventData.getName(keySignature);

                    QRect boundingRect=noteNameFontMetrics.boundingRect(noteName);

                    painter.setPen(noteEventFrameColor);
                    painter.drawText(QRect(noteRect.left() + 2, noteRect.center().y() - boundingRect.height()/2,
                                           boundingRect.width(), boundingRect.height()),
                                     Qt::AlignLeft | Qt::AlignVCenter,
                                     noteName);

                    // draw frame
                    painter.setBrush(Qt::NoBrush);

                    // does the name fit into the note rectangle?
                    if(boundingRect.height() - 2 <= noteRect.height())
                    {
                        // yes, draw frame in one part
                        painter.drawRect(noteRect);
                    }
                    else
                    {
                        // no, draw frame in parts:

                        // left "["
                        painter.drawLine(QPoint(noteRect.left()    , noteRect.top()),
                                         QPoint(noteRect.left()    , noteRect.bottom() + 1));
                        painter.drawLine(QPoint(noteRect.left()    , noteRect.top()),
                                         QPoint(noteRect.left() + 1, noteRect.top()));
                        painter.drawLine(QPoint(noteRect.left()    , noteRect.bottom() + 1),
                                         QPoint(noteRect.left() + 1, noteRect.bottom() + 1));

                        int rightPartLeftX=noteRect.left() + 2 + boundingRect.width() + 2;

                        // right border
                        if(rightPartLeftX <= noteRect.right() + 1)
                            painter.drawLine(QPoint(noteRect.right() + 1, noteRect.top()),
                                             QPoint(noteRect.right() + 1, noteRect.bottom() + 1));

                        // top and down border line right from area occupied by note name
                        if(rightPartLeftX < noteRect.right())
                        {
                            painter.drawLine(QPoint(rightPartLeftX, noteRect.top()), noteRect.topRight());
                            painter.drawLine(QPoint(rightPartLeftX, noteRect.bottom() + 1),
                                             QPoint(noteRect.right(), noteRect.bottom() + 1));
                        }
                    }
                }
                else
                {
                    // draw frame line and fill by gradient
                    painter.drawRect(noteRect);
                }
            }
        }
        event=event->nextEvent;
    }

    // -----------------------------------------------------------------------------
    // draw note-out-of-bounds gradients

    painter.setPen(Qt::NoPen);

    for(int direction=0; direction < 2; ++direction)    // top and bottom gradient
    {
        int y0= direction == 0 ? trackCellsRect.top() : trackCellsRect.bottom();
        int y1= direction == 0 ? y0 + (int)noteHeightInPixels : y0 - (int)noteHeightInPixels;

        QLinearGradient gradient(QPoint(trackCellsRect.left(),y0),
                                 QPoint(trackCellsRect.left(),y1));

        QColor gradientBaseColor=shadedPaletteColor(0.9, track->eventColor, noteEventFrameColor);
        QColor transparentEventColor(gradientBaseColor);
        transparentEventColor.setAlpha(0);

        gradient.setColorAt(0, gradientBaseColor);
        gradient.setColorAt(1, transparentEventColor);

        painter.setBrush(QBrush(gradient));

        // optimization: issue drawing commands only for concatenated ranges, not for each x column

        // find start of next gradient range
        int cellAreaXStart=0;
        while(cellAreaXStart < outOfBoundNoteRange[direction].size())
        {
            if(!outOfBoundNoteRange[direction][cellAreaXStart])
            {
                ++cellAreaXStart;
                continue;
            }

            // find end of this gradient range
            int cellAreaXEnd=cellAreaXStart+1;
            while(cellAreaXEnd < outOfBoundNoteRange[direction].size())
            {
                if(!outOfBoundNoteRange[direction][cellAreaXEnd])break;
                else ++cellAreaXEnd;
            }

            // draw the gradient
            QRect gradientRect(QPoint(cellAreaXStart + cellArea.left(), qMin(y0,y1)),
                               QPoint(cellAreaXEnd   + cellArea.left(), qMax(y0,y1)));
            painter.drawRect(gradientRect);

            cellAreaXStart=cellAreaXEnd + 1;
        }
    }

    // -----------------------------------------------------------------------------
    // playback position line

    if(getVolatileEditorState().playbackLineCellX != -1)
    {
        drawPlaybackPositionLine(painter,
                                 getVolatileEditorState().playbackLineCellX,
                                 trackCellsRect.top(),
                                 trackCellsRect.bottom(),
                                 playbackPositionLineColor);
    }
}

void View::paintEndGradients(QPainter& painter)
{
    // Use alpha gradient
    QColor cTransparent=palette().color(QPalette::Window);cTransparent.setAlpha(0);
    QColor cOpaque=palette().color(QPalette::Window);

    // -----------------------------------------------------------------------------
    // 1. Right end of view
    QLinearGradient rightGradient(rect().width() - VIEW_END_OF_SCREEN_GRADIENT_SIZE, 0,
                                  rect().width() - 1,                                0);
    rightGradient.setColorAt(0,cTransparent);
    rightGradient.setColorAt(1,cOpaque);

    painter.setBrush(QBrush(rightGradient));
    painter.drawRect(QRect(rect().width() - VIEW_END_OF_SCREEN_GRADIENT_SIZE,
                           0,
                           VIEW_END_OF_SCREEN_GRADIENT_SIZE,
                           rect().height()));

    // -----------------------------------------------------------------------------
    // 2. Bottom end of view
    QLinearGradient bottomGradient(0, rect().height() - VIEW_END_OF_SCREEN_GRADIENT_SIZE,
                                   0, rect().height() - 1);
    bottomGradient.setColorAt(0,cTransparent);
    bottomGradient.setColorAt(1,cOpaque);

    painter.setBrush(QBrush(bottomGradient));
    painter.drawRect(QRect(0,
                           rect().height() - VIEW_END_OF_SCREEN_GRADIENT_SIZE,
                           rect().width(),
                           VIEW_END_OF_SCREEN_GRADIENT_SIZE));
}

void View::getDisplayedMidiNoteRangeForTrack(const QRect& trackRect, int trackIndex, int& midiNoteBottom, int& midiNoteTop) const
{
    // determine range of midi note heights to draw

    EditorTrackState ts=getEditorState().trackStateList[trackIndex];
    double noteHeightInPixels=getEditorState().getNoteHeightInPixels();

    int trackCenterY= trackRect.center().y();
    midiNoteBottom= (int)(ts.centerMidiNote -
                        (trackRect.bottom() - trackCenterY) / noteHeightInPixels - 1);
    midiNoteTop   = (int)(ts.centerMidiNote +
                        (trackCenterY - trackRect.top()) / noteHeightInPixels + 1);
    if(midiNoteBottom < 0)midiNoteBottom=0;
    if(midiNoteTop > MIDI_MAX_DATA_VALUE)midiNoteTop=MIDI_MAX_DATA_VALUE;
}

double View::getWritePositionStatusBeatCount() const
{
    TicksToMeasureResult measureResult=docRoot->ticksToMeasure(getEditorState().selection.ticksLeft);
    return (double)measureResult.measureInternalTicks / docRoot->ticksPerBeat(measureResult.measureProperties);
}

QColor View::makePastelColor(const QColor& color, qreal whitenessFactor) const
{
    return QColor(
            static_cast<int>(255 - ((255 - color.red  ()) / whitenessFactor)),
            static_cast<int>(255 - ((255 - color.green()) / whitenessFactor)),
            static_cast<int>(255 - ((255 - color.blue ()) / whitenessFactor)));
}

QColor View::shadedPaletteColor(float foreground_factor) const
{
    QColor foregroundColor=palette().color(QPalette::WindowText);
    QColor backgroundColor=palette().color(QPalette::Base);

    return QColor(
            static_cast<int>((foreground_factor*foregroundColor.red()   + (1-foreground_factor)*backgroundColor.red())),
            static_cast<int>((foreground_factor*foregroundColor.green() + (1-foreground_factor)*backgroundColor.green())),
            static_cast<int>((foreground_factor*foregroundColor.blue()  + (1-foreground_factor)*backgroundColor.blue())));
}

QColor View::shadedPaletteColor(float foreground_factor, QColor foregroundColor, QColor backgroundColor)
{
    return QColor(
            static_cast<int>((foreground_factor*foregroundColor.red()   + (1-foreground_factor)*backgroundColor.red())),
            static_cast<int>((foreground_factor*foregroundColor.green() + (1-foreground_factor)*backgroundColor.green())),
            static_cast<int>((foreground_factor*foregroundColor.blue()  + (1-foreground_factor)*backgroundColor.blue())));
}
