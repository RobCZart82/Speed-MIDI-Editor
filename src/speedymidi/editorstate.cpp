/***************************************************************************
 *  editorstate.cpp - State of Visual Editor
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

#include "editorstate.h"
#include "writelengthdialog.h"
#include "zoomsliderwidget.h"
#include "doc_root.h"
#include "doc_track.h"

#include <QDomDocument>
#include <cmath>

bool EditorSelectionSupportPoint::isValid(const DocRoot* docRoot, const WriteLength& writeLength) const
{
    if(ticksLeft < 0)return false;
    if(ticksRight <= ticksLeft)return false; // use <= here (no zero-tick selection)
    if(track < 0)return false;

    if(docRoot->hasTracks())
    {
        // If document has at least one track, support point must be on a valid track
        if(track >= docRoot->trackList.size())return false;
    }
    else
    {
        // If document has no tracks, support point must be on (non-existent) track zero
        if(track != 0)return false;
    }

    // Tick positions must be on cell borders
    if(docRoot->roundDownTicksToCellBorder(ticksLeft ,writeLength) != ticksLeft )return false;
    if(docRoot->roundDownTicksToCellBorder(ticksRight,writeLength) != ticksRight)return false;

    return true;
}

bool EditorSelection::isValid(const DocRoot* docRoot, const WriteLength& writeLength) const
{
    // a) Range

    // Checks for all selection modes
    if(ticksLeft < 0)return false;
    if(ticksRight <= ticksLeft)return false;    // use <= here (no zero-tick selection)
    if(trackTop < 0)return false;
    if(trackBottom < trackTop)return false;     // use < here (1 track selection OK)

    // Special checks for current selection mode
    switch(getSelectionMode())
    {
    case S_GlobalMeasure:
        {
            // Tick positions must be on measure borders
            if(docRoot->roundDownTicksToMeasureBorder(ticksLeft ) != ticksLeft )return false;
            if(docRoot->roundDownTicksToMeasureBorder(ticksRight) != ticksRight)return false;
        }
        break;
    case S_GlobalTrack:
        {
            if(!docRoot->hasTracks())return false;   // only valid selection mode w/o tracks is "global measures"

            // Check bottom track position (top was checked above)
            if(trackBottom >= docRoot->trackList.size())return false;
        }
        break;
    case S_LocalCells:
        {
            if(!docRoot->hasTracks())return false;   // only valid selection mode w/o tracks is "global measures"

            // Tick positions must be on cell borders
            if(docRoot->roundDownTicksToCellBorder(ticksLeft ,writeLength) != ticksLeft )return false;
            if(docRoot->roundDownTicksToCellBorder(ticksRight,writeLength) != ticksRight)return false;

            // Check bottom track position (top was checked above)
            if(trackBottom >= docRoot->trackList.size())return false;
        }
        break;
    }

    // b) Anchor

    // Anchor must be exactly 1 cell in valid track within current selection:

    // 1. Check for 1 valid cell
    if(!anchor.isValid(docRoot,writeLength))return false;

    // 2. Check if within selection
    if(anchor.ticksLeft < ticksLeft || anchor.ticksRight > ticksRight)return false;
    if(docRoot->hasTracks())
    {
        if(anchor.track < trackTop || anchor.track > trackBottom)return false;
    }

    return true;
}

bool EditorTrackState::isValid() const
{
    if(!std::isfinite(centerMidiNote) || !std::isfinite(heightInNotes))
        return false;
    if(centerMidiNote < 0 || centerMidiNote > MIDI_MAX_DATA_VALUE)
        return false;
    if(heightInNotes < VIEW_MIN_TRACK_HEIGHT_IN_NOTES || heightInNotes > VIEW_MAX_TRACK_HEIGHT_IN_NOTES)
        return false;

    return true;
}

void EditorTrackState::serialize(QDataStream& dataStream) const
{
    // copy properties to clipboard

    dataStream << solo;
    dataStream << mute;
    dataStream << recordingEnabled;

    dataStream << centerMidiNote;
    dataStream << heightInNotes;
}

void EditorTrackState::deserialize(QDataStream& dataStream)
{
    // paste properties from clipboard

    dataStream >> solo;
    dataStream >> mute;
    dataStream >> recordingEnabled;

    dataStream >> centerMidiNote;
    dataStream >> heightInNotes;
    if(!isValid())dataStream.setStatus(QDataStream::ReadCorruptData);
}

void EditorState::setGlobalMeasureSelection(int fromMeasureIndex, int nMeasures, const DocRoot* docRoot)
{
    selection.trackTop=0;
    selection.ticksLeft=docRoot->measureToTicks(fromMeasureIndex);
    selection.trackBottom=INT_MAX;   // indicates global measure selection
    selection.ticksRight=docRoot->measureToTicks(fromMeasureIndex + nMeasures);

    // Use topmost visible track for anchor cell track
    int anchorCellTrack;
    if(docRoot->hasTracks())
    {
        // If all tracks are completely scrolled off the screen, use last existing track.
        anchorCellTrack=qMin(firstTrack, docRoot->trackList.size() - 1);
    }
    else anchorCellTrack=0;

    selection.anchor.setTo(
            selection.ticksLeft,
            docRoot->roundUpTicksToCellBorder(selection.ticksLeft + 1, writeLength),
            anchorCellTrack);
}

void EditorState::setGlobalTrackSelection(int fromTrackIndex, int nTracks, const DocRoot* docRoot)
{
    Q_ASSERT(docRoot->hasTracks());

    selection.ticksLeft=0;
    selection.trackTop=fromTrackIndex;
    selection.ticksRight=INT_MAX;   // indicates global track selection
    selection.trackBottom=fromTrackIndex + nTracks - 1;

    int firstVisibleMeasureTicks=docRoot->measureToTicks(firstMeasure);

    selection.anchor.setTo(
            firstVisibleMeasureTicks,
            docRoot->roundUpTicksToCellBorder(firstVisibleMeasureTicks + 1, writeLength),
            fromTrackIndex);
}

void EditorState::setStartupDefaultState(const DocRoot* docRoot)
{
    trackStateList.clear();

    // Default editor startup settings for new document or after opening document with no saved editor state

    // scroll to top track, measure 1
    firstMeasure=0;
    firstTrack=0;

    // set default write length (8th note)
    writeLength.denominator=8;
    writeLength.tupletNominator=1;
    writeLength.tupletDenominator=1;

    xZoomSliderValue=ZOOM_SLIDER_WIDGET_MAX_VALUE / 2;
    yZoomSliderValue=ZOOM_SLIDER_WIDGET_MAX_VALUE / 3;

    // Generate inital track state list
    for(int i=0; i < docRoot->trackList.size(); ++i)
    {
        EditorTrackState trackState;
        trackState.centerMidiNote=docRoot->trackList[i]->centerNoteNumber();
        trackStateList.append(trackState);
    }

    setStartupSelection(docRoot);
}

void EditorState::setStartupSelection(const DocRoot* docRoot)
{
    // Set initial selection
    if(docRoot->hasTracks())
    {
        // set selection to local cell selection of first cell in top track
        int firstCellLength=docRoot->roundUpTicksToCellBorder(0 + 1, writeLength);
        selection.reset(firstCellLength);
    }
    else    // document has no tracks
    {
        // set selection to first global measure
        setGlobalMeasureSelection(0, 1, docRoot);
    }
}

bool EditorState::loadFromXML(QDomElement& rootElement, int xmlConfigVersion)
{
    // make backup before trying to load
    EditorState backupState=*this;

    if(tryLoadFromXML(rootElement, xmlConfigVersion))return true;

    // clean up after failed loading from an XML node
    *this=backupState;
    return false;
}

bool EditorState::tryLoadFromXML(QDomElement& rootElement, int xmlConfigVersion)
{
    /* XML format specification:

       <mapping x="[int]" y="[int]" cell_d="[int]" cell_tn="[int]" cell_td="[int]" xzoom="[int]" yzoom="[int]"/>
       <selection>
           <anchor left="[int]" right="[int]" track="[int]"/>
           <range left="[int]" right="[int]" top_track="[int]" bottom_track="[int]"/>
       </selection>
       <track_states>
           <track solo="[bool]" mute="[bool]" record="[bool]" height="[int]" center_note="[int]"/>
           ...
       </track_states>

    */
    UNUSED(xmlConfigVersion); // version is currently unused

    QDomElement mappingElement=rootElement.elementsByTagName(XML_TAG_MAPPING).item(0).toElement();
    if(mappingElement.isNull())return false;
    QDomElement selectionElement=rootElement.elementsByTagName(XML_TAG_SELECTION).item(0).toElement();
    if(selectionElement.isNull())return false;
    QDomElement anchorElement=selectionElement.elementsByTagName(XML_TAG_ANCHOR).item(0).toElement();
    if(anchorElement.isNull())return false;
    QDomElement rangeElement=selectionElement.elementsByTagName(XML_TAG_RANGE).item(0).toElement();
    if(rangeElement.isNull())return false;
    QDomElement trackStatesElement=rootElement.elementsByTagName(XML_TAG_TRACK_STATES).item(0).toElement();
    if(trackStatesElement.isNull())return false;

    QDomNodeList trackElementList=trackStatesElement.elementsByTagName(XML_TAG_TRACK);

    bool ok;
    firstMeasure                = mappingElement.attribute(XML_ATTR_FIRST_MEASURE).toInt(&ok);if(!ok)return false;
    firstTrack                  = mappingElement.attribute(XML_ATTR_FIRST_TRACK).toInt(&ok);if(!ok)return false;
    writeLength.denominator      = mappingElement.attribute(XML_ATTR_CELL_LENGTH_DENOMINATOR).toInt(&ok);if(!ok)return false;
    writeLength.tupletNominator  = mappingElement.attribute(XML_ATTR_CELL_LENGTH_TUPLET_NOMINATOR).toInt(&ok);if(!ok)return false;
    writeLength.tupletDenominator= mappingElement.attribute(XML_ATTR_CELL_LENGTH_TUPLET_DENOMINATOR).toInt(&ok);if(!ok)return false;
    xZoomSliderValue            = mappingElement.attribute(XML_ATTR_XZOOM).toInt(&ok);if(!ok)return false;
    yZoomSliderValue            = mappingElement.attribute(XML_ATTR_YZOOM).toInt(&ok);if(!ok)return false;

    QString selModeString      = selectionElement.attribute(XML_ATTR_MODE);
    if(selModeString == XML_VALUE_MEASURES)
    {
        selection.ticksLeft   = rangeElement.attribute(XML_ATTR_LEFT).toInt(&ok);if(!ok)return false;
        selection.ticksRight  = rangeElement.attribute(XML_ATTR_RIGHT).toInt(&ok);if(!ok)return false;
        selection.trackTop    = 0;
        selection.trackBottom = INT_MAX;
    }
    else if(selModeString == XML_VALUE_TRACKS)
    {
        selection.ticksLeft   = 0;
        selection.ticksRight  = INT_MAX;
        selection.trackTop    = rangeElement.attribute(XML_ATTR_TOP_TRACK).toInt(&ok);if(!ok)return false;
        selection.trackBottom = rangeElement.attribute(XML_ATTR_BOTTOM_TRACK).toInt(&ok);if(!ok)return false;
    }
    else if(selModeString == XML_VALUE_CELLS)
    {
        selection.ticksLeft   = rangeElement.attribute(XML_ATTR_LEFT).toInt(&ok);if(!ok)return false;
        selection.ticksRight  = rangeElement.attribute(XML_ATTR_RIGHT).toInt(&ok);if(!ok)return false;
        selection.trackTop    = rangeElement.attribute(XML_ATTR_TOP_TRACK).toInt(&ok);if(!ok)return false;
        selection.trackBottom = rangeElement.attribute(XML_ATTR_BOTTOM_TRACK).toInt(&ok);if(!ok)return false;
    }
    else return false;  // wrong selection mode

    selection.anchor.ticksLeft = anchorElement.attribute(XML_ATTR_LEFT).toInt(&ok);if(!ok)return false;
    selection.anchor.ticksRight= anchorElement.attribute(XML_ATTR_RIGHT).toInt(&ok);if(!ok)return false;
    selection.anchor.track     = anchorElement.attribute(XML_ATTR_TRACK).toInt(&ok);if(!ok)return false;

    for(int i=0; i < trackElementList.size(); ++i)
    {
        QDomElement trackElement=trackElementList.at(i).toElement();
        if(trackElement.isNull())break;

        EditorTrackState trackState;

        trackState.solo             = trackElement.attribute(XML_ATTR_SOLO).toInt(&ok) != 0;if(!ok)return false;
        trackState.mute             = trackElement.attribute(XML_ATTR_MUTE).toInt(&ok) != 0;if(!ok)return false;
        trackState.recordingEnabled = trackElement.attribute(XML_ATTR_RECORD).toInt(&ok) != 0;if(!ok)return false;

        trackState.heightInNotes = trackElement.attribute(XML_ATTR_HEIGHT_IN_NOTES).toDouble(&ok);if(!ok)return false;
        trackState.centerMidiNote= trackElement.attribute(XML_ATTR_CENTER_NOTE).toDouble(&ok);if(!ok)return false;

        if(!trackState.isValid())return false;

        trackStateList.append(trackState);
    }
    return true;
}

bool EditorState::saveToXML(QDomElement& rootElement, int xmlConfigVersion) const
{
    // Format specification: see tryLoadFromXML(...)
    UNUSED(xmlConfigVersion); // version is currently unused

    QDomDocument domDoc=rootElement.ownerDocument();
    QDomElement mappingElement=domDoc.createElement(XML_TAG_MAPPING);
    rootElement.appendChild(mappingElement);
    QDomElement selectionElement=domDoc.createElement(XML_TAG_SELECTION);
    rootElement.appendChild(selectionElement);
    QDomElement anchorElement=domDoc.createElement(XML_TAG_ANCHOR);
    selectionElement.appendChild(anchorElement);
    QDomElement rangeElement=domDoc.createElement(XML_TAG_RANGE);
    selectionElement.appendChild(rangeElement);
    QDomElement trackStatesElement=domDoc.createElement(XML_TAG_TRACK_STATES);
    rootElement.appendChild(trackStatesElement);

    mappingElement.setAttribute(XML_ATTR_FIRST_MEASURE,firstMeasure);
    mappingElement.setAttribute(XML_ATTR_FIRST_TRACK,firstTrack);
    mappingElement.setAttribute(XML_ATTR_CELL_LENGTH_DENOMINATOR,writeLength.denominator);
    mappingElement.setAttribute(XML_ATTR_CELL_LENGTH_TUPLET_NOMINATOR,writeLength.tupletNominator);
    mappingElement.setAttribute(XML_ATTR_CELL_LENGTH_TUPLET_DENOMINATOR,writeLength.tupletDenominator);
    mappingElement.setAttribute(XML_ATTR_XZOOM,xZoomSliderValue);
    mappingElement.setAttribute(XML_ATTR_YZOOM,yZoomSliderValue);

    // Store selection mode
    SelectionModeType selMode=selection.getSelectionMode();
    QString selModeString;
    switch(selMode)
    {
    case S_GlobalMeasure:selModeString=XML_VALUE_MEASURES;break;
    case S_GlobalTrack:  selModeString=XML_VALUE_TRACKS;break;
    case S_LocalCells:   selModeString=XML_VALUE_CELLS;break;
    }
    selectionElement.setAttribute(XML_ATTR_MODE,selModeString);

    // Store anchor settings
    anchorElement.setAttribute(XML_ATTR_LEFT,selection.anchor.ticksLeft);
    anchorElement.setAttribute(XML_ATTR_RIGHT,selection.anchor.ticksRight);
    anchorElement.setAttribute(XML_ATTR_TRACK,selection.anchor.track);

    // Dependent on selection mode, store required data (Avoid storing INT_MAX).
    if(selMode == S_GlobalMeasure || selMode == S_LocalCells)
    {
        rangeElement.setAttribute(XML_ATTR_LEFT,selection.ticksLeft);
        rangeElement.setAttribute(XML_ATTR_RIGHT,selection.ticksRight);
    }
    if(selMode == S_GlobalTrack || selMode == S_LocalCells)
    {
        rangeElement.setAttribute(XML_ATTR_TOP_TRACK,selection.trackTop);
        rangeElement.setAttribute(XML_ATTR_BOTTOM_TRACK,selection.trackBottom);
    }

    for(int i=0; i < trackStateList.size(); ++i)
    {
        QDomElement trackElement=domDoc.createElement(XML_TAG_TRACK);
        trackStatesElement.appendChild(trackElement);

        trackElement.setAttribute(XML_ATTR_SOLO,   trackStateList[i].solo             ? 1 : 0);
        trackElement.setAttribute(XML_ATTR_MUTE,   trackStateList[i].mute             ? 1 : 0);
        trackElement.setAttribute(XML_ATTR_RECORD, trackStateList[i].recordingEnabled ? 1 : 0);

        // store double values with reduced precision (2 decimals after decimal point suffice)
        trackElement.setAttribute(XML_ATTR_HEIGHT_IN_NOTES, QString("%1").arg(trackStateList[i].heightInNotes, 0, 'f', 2));
        trackElement.setAttribute(XML_ATTR_CENTER_NOTE,     QString("%1").arg(trackStateList[i].centerMidiNote,0, 'f', 2));
    }
    return true;
}

int EditorState::lastSelectedTrack(const DocRoot* docRoot) const
{
    return qMin(selection.trackBottom, docRoot->trackList.size()-1);
}

bool EditorState::isValid(const DocRoot* docRoot) const
{
    // 1. Scroll position
    if(firstMeasure < 0 || firstMeasure > docRoot->getMaxFirstMeasure())return false;
    if(firstTrack   < 0 || firstTrack   > docRoot->trackList.size())return false;

    // 2. Write length settings
    if(writeLength.denominator < 1 || writeLength.denominator > EDITOR_MAX_WRITELENGTH_DENOMINATOR)return false;

    // Check for power of 2 (maximally 1 bit set)
    int d=writeLength.denominator;
    while((d&1) == 0)d >>= 1;
    if(d != 1)return false;

    if(writeLength.tupletNominator < 1   || writeLength.tupletNominator > EDITOR_MAX_TUPLET_FRACTIONAL_COMPONENT)
        return false;
    if(writeLength.tupletDenominator < 1 || writeLength.tupletDenominator > EDITOR_MAX_TUPLET_FRACTIONAL_COMPONENT)
        return false;

    // If tuplets are enabled, nominator and denominator must not be coprime
    if(writeLength.tupletDenominator != 1 || writeLength.tupletNominator != 1)
    {
        int gcd=WriteLengthDialog::greatestCommonDivisor(writeLength.tupletNominator,writeLength.tupletDenominator);
        if(writeLength.tupletNominator   / gcd == 1 ||
           writeLength.tupletDenominator / gcd == 1)
            return false;   // nominator and denominator are coprime
    }

    // 3. Zoom settings
    if(xZoomSliderValue < 0 || xZoomSliderValue > ZOOM_SLIDER_WIDGET_MAX_VALUE)return false;
    if(yZoomSliderValue < 0 || yZoomSliderValue > ZOOM_SLIDER_WIDGET_MAX_VALUE)return false;

    // 4. Selection
    if(!selection.isValid(docRoot, writeLength))return false;

    // 5. Track states

    // a) Number of track states
    if(trackStateList.size() != docRoot->trackList.size())return false;

    // b) Track state data
    for(int i=0; i < trackStateList.size(); ++i)
        if(!trackStateList[i].isValid())return false;

    return true;
}

double EditorState::getTicksPerPixel(const DocRoot* docRoot) const
{
    int ticksPerMixelMin=docRoot->midiTicksPerWholeNote *
                         VIEW_TICKS_PER_PIXEL_MIN_DEFAULT_RESOLUTION /
                         DOCUMENT_DEFAULT_TICKS_PER_WHOLE_NOTE;

    int ticksPerMixelMax=docRoot->midiTicksPerWholeNote *
                         VIEW_TICKS_PER_PIXEL_MAX_DEFAULT_RESOLUTION /
                         DOCUMENT_DEFAULT_TICKS_PER_WHOLE_NOTE;

    return getZoomFactor(ZOOM_SLIDER_WIDGET_MAX_VALUE - xZoomSliderValue, // inverse
                         ticksPerMixelMin,
                         ticksPerMixelMax,
                         VIEW_TICKS_PER_PIXEL_EXP_BASE);
}

int EditorState::getTrackHeightInPixels(int trackIndex) const
{
    // round down track size to nearest integer (round DOWN is required for fit-all zoom functions)
    const double height=trackStateList[trackIndex].heightInNotes * getNoteHeightInPixels();
    if(!std::isfinite(height))return VIEW_MIN_TRACK_HEIGHT_IN_PIXELS;
    int trackHeightInPixels=static_cast<int>(qBound<double>(0,height,INT_MAX));

    // Clamp value to allowed range
    if(trackHeightInPixels < VIEW_MIN_TRACK_HEIGHT_IN_PIXELS)
        trackHeightInPixels=VIEW_MIN_TRACK_HEIGHT_IN_PIXELS;

    return trackHeightInPixels;
}

void EditorState::adjustForChangedWriteLength(const DocRoot* docRoot)
{
    repairSelection(docRoot);   // make sure current selection fits to new cell raster
    setMinimumZoom(docRoot);    // If cell raster is to small, automatically zoom in
}

void EditorState::repairSelection(const DocRoot* docRoot)
{
    bool anchorIsRightAligned=
            selection.anchor.ticksRight == selection.ticksRight;

    // 1. Adjust selection when in local cell selection mode
    if(selection.getSelectionMode() == S_LocalCells)
    {
        if(selection.anchor.ticksLeft  == selection.ticksLeft &&
           selection.anchor.ticksRight == selection.ticksRight)
        {
            // only one column was selected => new selection should also comprise one column
            selection.ticksLeft=docRoot->roundDownTicksToCellBorder(selection.ticksLeft,
                                                                             writeLength);
            selection.ticksRight=docRoot->roundUpTicksToCellBorder(selection.ticksLeft + 1,
                                                                            writeLength);
        }
        else
        {
            // multiple columns were selected => increase selection to next existing cell border
            selection.ticksLeft=docRoot->roundDownTicksToCellBorder(selection.ticksLeft,
                                                                             writeLength);
            selection.ticksRight=docRoot->roundUpTicksToCellBorder(selection.ticksRight,
                                                                            writeLength);
        }
    }

    // 2. Adjust anchor
    if(anchorIsRightAligned)
    {
        // Anchor is aligned with right selection border => let anchor be right-aligned in new selection, too.
        selection.anchor.ticksRight=selection.ticksRight;
        selection.anchor.ticksLeft=docRoot->roundDownTicksToCellBorder(
                selection.ticksRight - 1,writeLength);
    }
    else
    {
        // Anchor is left-aligned or not aligned with selection borders => align anchor cell to new cell raster
        selection.anchor.ticksLeft=docRoot->roundDownTicksToCellBorder(
                selection.anchor.ticksLeft,writeLength);

        selection.anchor.ticksRight=docRoot->roundUpTicksToCellBorder(
                selection.anchor.ticksLeft + 1,writeLength);
    }
}

void EditorState::setMinimumZoom(const DocRoot* docRoot)
{
    // If cell raster gets to small, automatically zoom in
    double standardCellLengthInTicks=
            writeLength.tupletNominator
            * docRoot->midiTicksPerWholeNote
            / writeLength.denominator
            / writeLength.tupletDenominator;

    double standardCellLengthInPixels=standardCellLengthInTicks / getTicksPerPixel(docRoot);

    if(standardCellLengthInPixels < VIEW_MIN_STD_CELL_LENGTH_IN_PIXELS_AUTO_ZOOM)
    {
        // zoom in
        double newTicksPerPixel=
                (double)standardCellLengthInTicks / VIEW_MIN_STD_CELL_LENGTH_IN_PIXELS_AUTO_ZOOM;

        xZoomSliderValue=getXZoomSliderValue(newTicksPerPixel, docRoot);
    }
}

double EditorState::getZoomFactor(int sliderValue, double minZoomFactor, double maxZoomFactor, double expBase)
{
    // sliderValue range = [0;ZOOM_SLIDER_WIDGET_MAX_VALUE]

    // formula: zoomFactor := c*x+d
    //             where x := expBase ^ (sliderValue/ZOOM_SLIDER_WIDGET_MAX_VALUE - 1)
    //                   d := (expBase * min - max) / (expBase - 1)
    //                   c := max - d

    double x=pow(expBase,(double)sliderValue / ZOOM_SLIDER_WIDGET_MAX_VALUE - 1);
    double d=(expBase * minZoomFactor - maxZoomFactor) / (expBase - 1);
    double c=maxZoomFactor - d;

    double zoomFactor=c*x+d;

    // Clamp value because rounding errors may occur.
    if(zoomFactor < minZoomFactor)return minZoomFactor;
    if(zoomFactor > maxZoomFactor)return maxZoomFactor;
    return zoomFactor;
}

int EditorState::getZoomSliderValue(double currentZoomFactor, double minZoomFactor, double maxZoomFactor, double expBase)
{
    // inverse function to getZoomFactor, for formula see getZoomFactor

    double d=(expBase * minZoomFactor - maxZoomFactor) / (expBase - 1);
    double c=maxZoomFactor - d;

    double x=(currentZoomFactor - d) / c;

    int sliderValue=(int)(ZOOM_SLIDER_WIDGET_MAX_VALUE * (1+log(x) / log(expBase)));

    // Clamp value because rounding errors may occur.
    if(sliderValue < 0)return 0;
    if(sliderValue > ZOOM_SLIDER_WIDGET_MAX_VALUE)return ZOOM_SLIDER_WIDGET_MAX_VALUE;
    return sliderValue;
}

int EditorState::getXZoomSliderValue(double TicksPerPixel, const DocRoot* docRoot)
{
    int ticksPerMixelMin=docRoot->midiTicksPerWholeNote *
                         VIEW_TICKS_PER_PIXEL_MIN_DEFAULT_RESOLUTION /
                         DOCUMENT_DEFAULT_TICKS_PER_WHOLE_NOTE;

    int ticksPerMixelMax=docRoot->midiTicksPerWholeNote *
                         VIEW_TICKS_PER_PIXEL_MAX_DEFAULT_RESOLUTION /
                         DOCUMENT_DEFAULT_TICKS_PER_WHOLE_NOTE;

    if(TicksPerPixel < ticksPerMixelMin) TicksPerPixel=ticksPerMixelMin;
    if(TicksPerPixel > ticksPerMixelMax) TicksPerPixel=ticksPerMixelMax;

    return ZOOM_SLIDER_WIDGET_MAX_VALUE - // inverse
            getZoomSliderValue(TicksPerPixel,
                               ticksPerMixelMin,
                               ticksPerMixelMax,
                               VIEW_TICKS_PER_PIXEL_EXP_BASE);
}

double EditorState::getNoteHeightInPixels() const
{
    return getZoomFactor(yZoomSliderValue,
                         VIEW_NOTE_HEIGHT_IN_PIXELS_MIN,
                         VIEW_NOTE_HEIGHT_IN_PIXELS_MAX,
                         VIEW_NOTE_HEIGHT_IN_PIXELS_EXP_BASE);
}
