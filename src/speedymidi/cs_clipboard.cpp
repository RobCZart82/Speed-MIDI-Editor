/***************************************************************************
 *  cs_clipboard.cpp - Controller Subsystem: Clipboard
 *                     (Cut, Copy, Paste, Scale)
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

#include "cs_clipboard.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "doc_root.h"
#include "doc_measureitem.h"
#include "doc_track.h"
#include "doc_event.h"
#include "smfdocument.h"
#include "commands.h"

#include <QClipboard>
#include <QMimeData>
#include <limits>
#include <QMessageBox>

#define CS_CLIPBOARD_MIME_TYPE "application/speedymidi"
#define CS_CLIPBOARD_V2_MIME_TYPE "application/speedymidi-v2"
#define CS_CLIPBOARD_V3_MIME_TYPE "application/speedymidi-v3"
#define CS_CLIPBOARD_V4_MIME_TYPE "application/speedymidi-v4"

CS_Clipboard::CS_Clipboard(Controller* controller)
        : CS_Common(controller)
{
    Controller::ActionGuards gDIC = Controller::DisallowWhenInputCaptured;
    Controller::ActionGuards gCIS = Controller::CancelInterruptibleStates;

    registerActionHandler(ui->actionEdit_Cut, "actionEdit_Cut_Triggered", gDIC | gCIS);
    registerActionHandler(ui->actionEdit_Copy, "actionEdit_Copy_Triggered", gDIC);
    registerActionHandler(ui->actionEdit_Paste, "actionEdit_Paste_Triggered", gDIC | gCIS);
    registerActionHandler(ui->actionEdit_PasteScaleToSelection, "actionEdit_PasteScaleToSelection_Triggered", gDIC | gCIS);
}

void CS_Clipboard::actionEdit_Cut_Triggered()
{
    // Cut is a combination of copy and clear cells
    beginMacro(tr("Cut"), getEditorState().selection);

    actionEdit_Copy_Triggered();

    // clear cells
    for(int trackIndex=getEditorState().firstSelectedTrack(); trackIndex <= getEditorState().lastSelectedTrack(docRoot); ++trackIndex)
        clearTrackRange_(trackIndex, getEditorState().selection.ticksLeft, getEditorState().selection.ticksRight,false);

    endMacro(getEditorState(), getEditorState().selection);  // no state change
}

void CS_Clipboard::actionEdit_Copy_Triggered()
{
    // Prepare clipboard data
    QByteArray clipboardData;
    QDataStream dataStream(&clipboardData, QIODevice::WriteOnly);

    serializeSelection(dataStream);

    // Put data to clipboard with a specific MIME type
    QMimeData* mimeData=new QMimeData;
    mimeData->setData(CS_CLIPBOARD_V4_MIME_TYPE, clipboardData);
    QByteArray v3Data;
    QDataStream v3Stream(&v3Data,QIODevice::WriteOnly);
    serializeSelection(v3Stream,true,true,false);
    mimeData->setData(CS_CLIPBOARD_V3_MIME_TYPE,v3Data);
    QByteArray v2Data;
    QDataStream v2Stream(&v2Data,QIODevice::WriteOnly);
    serializeSelection(v2Stream,true,false);
    mimeData->setData(CS_CLIPBOARD_V2_MIME_TYPE,v2Data);
    // Keep a basic payload for older versions, whose event enums cannot read
    // the extension tags or opaque event types.
    QByteArray legacyData;
    QDataStream legacyStream(&legacyData,QIODevice::WriteOnly);
    serializeSelection(legacyStream,false,false);
    mimeData->setData(CS_CLIPBOARD_MIME_TYPE,legacyData);

    QClipboard* clipboard=QApplication::clipboard();
    clipboard->setMimeData(mimeData);
}

void CS_Clipboard::actionEdit_Paste_Triggered()
{
    pasteFromClipboard(false);
}

void CS_Clipboard::actionEdit_PasteScaleToSelection_Triggered()
{
    pasteFromClipboard(true);
}

//EXTENSION edit/merge

void CS_Clipboard::serializeSelection(QDataStream& dataStream, bool extended, bool exactTempo, bool meterMetadata)
{
    const EditorSelection& sel=getEditorState().selection;

    // selection mode
    SelectionModeType selMode=sel.getSelectionMode();
    if(exactTempo)dataStream << qint32(meterMetadata ? -4:-3); // Versioned measure records.
    dataStream << (int)selMode;

    // measure count
    if(selMode == S_GlobalMeasure)dataStream << getNumberOfSelectedMeasures();

    // track count
    int numberOfSelectedTracks=
            getEditorState().lastSelectedTrack(docRoot) -
            getEditorState().firstSelectedTrack() + 1;

    dataStream << numberOfSelectedTracks;

    // selected tick range
    int selectedTickRange=sel.ticksRight - sel.ticksLeft;
    dataStream << selectedTickRange;

    // tick resolution
    dataStream << docRoot->midiTicksPerWholeNote;

    // selection mode S_GlobalMeasure only:
    if(selMode == S_GlobalMeasure)
    {
        // Prepare a list of measure items for later serialization
        QList<DocMeasureItem*> measureItemsToSerializeList;

        // Properties of first selected measure
        DocMeasureItem* firstProperties=new DocMeasureItem(
                docRoot->ticksToMeasure(sel.ticksLeft).measureProperties);
        firstProperties->tickPosition=sel.ticksLeft;
        firstProperties->setRequiredFlagsFirstMeasure();
        measureItemsToSerializeList.append(firstProperties);

        // Further measureItems within selected range
        for(int i=0; i < docRoot->measureItemList.size(); ++i)
        {
            DocMeasureItem* measureItem=docRoot->measureItemList[i];
            if(measureItem->tickPosition <= sel.ticksLeft)
                continue;   // item before selection or on ticksLeft border (border case handled above)

            if(measureItem->tickPosition >= sel.ticksRight)
                break;  // item after selection

            // Older clients require their conductor properties on bar boundaries.
            if(!exactTempo && docRoot->roundDownTicksToMeasureBorder(measureItem->tickPosition) != measureItem->tickPosition)
                continue;

            // item within selection, serialize it
            measureItemsToSerializeList.append(new DocMeasureItem(*measureItem));
        }

        // Serialize the list and delete contents
        dataStream << qint32(measureItemsToSerializeList.size());
        for(int i=0; i < measureItemsToSerializeList.size(); ++i)
        {
            DocMeasureItem* item=measureItemsToSerializeList[i];
            const int denominator=docRoot->ticksToMeasure(item->tickPosition).measureProperties.timeSignatureDenominator;
            item->serialize(dataStream,sel.ticksLeft,exactTempo,denominator,meterMetadata && exactTempo);
            delete measureItemsToSerializeList[i];
        }
        measureItemsToSerializeList.clear();
    }

    // for each selected track ...
    for(int trackIndex=getEditorState().firstSelectedTrack();
            trackIndex <= getEditorState().lastSelectedTrack(docRoot);
            ++trackIndex)
    {
        DocTrack* track=docRoot->trackList[trackIndex];

        // serialize track properties
        track->serialize(dataStream);

        // editor track state
        getEditorState().trackStateList[trackIndex].serialize(dataStream);

        // count events that must be serialized
        int eventCount=0;
        DocEvent* event=track->firstEvent;
        while(event)
        {
            if(event->mustSerialize(sel.ticksLeft, sel.ticksRight) &&
               (extended || event->type <= DocEvent::E_Meta)) ++eventCount;
            event=event->nextEvent;
        }

        dataStream << eventCount;

        // serialize list of events (cut-down) to selected range
        event=track->firstEvent;
        while(event)
        {
            if(event->mustSerialize(sel.ticksLeft, sel.ticksRight) &&
               (extended || event->type <= DocEvent::E_Meta))
                event->serialize(dataStream, sel.ticksLeft, sel.ticksRight,extended);
            event=event->nextEvent;
        }
    }
}

void CS_Clipboard::pasteFromClipboard(bool scaleToSelection)
{
    QClipboard* clipboard=QApplication::clipboard();
    const QMimeData* mimeData=clipboard->mimeData();
    if(!mimeData->hasFormat(CS_CLIPBOARD_V4_MIME_TYPE) && !mimeData->hasFormat(CS_CLIPBOARD_V3_MIME_TYPE) && !mimeData->hasFormat(CS_CLIPBOARD_V2_MIME_TYPE) && !mimeData->hasFormat(CS_CLIPBOARD_MIME_TYPE))
        return; // Wrong format on clipboard. Silent failure.

    QByteArray clipboardData=mimeData->data(mimeData->hasFormat(CS_CLIPBOARD_V4_MIME_TYPE) ? CS_CLIPBOARD_V4_MIME_TYPE :
                                          mimeData->hasFormat(CS_CLIPBOARD_V3_MIME_TYPE) ? CS_CLIPBOARD_V3_MIME_TYPE :
                                          mimeData->hasFormat(CS_CLIPBOARD_V2_MIME_TYPE) ? CS_CLIPBOARD_V2_MIME_TYPE : CS_CLIPBOARD_MIME_TYPE);
    QDataStream dataStream(&clipboardData, QIODevice::ReadOnly);

    deserializeAndPasteIntoSelection(dataStream, scaleToSelection);
}

void CS_Clipboard::deserializeAndPasteIntoSelection(QDataStream& dataStream, bool scaleToSelection)
{
    // Editor selection mode
    SelectionModeType selMode=getEditorState().selection.getSelectionMode();

    // Clipboard:
    SelectionModeType clipboardSelMode;
    int clipboardNumberOfMeasures;
    int clipboardNumberOfTracks;
    int clipboardTickRange;

    int rawSelectionMode=0;
    dataStream >> rawSelectionMode;
    const bool meterMetadata=rawSelectionMode == -4;
    const bool exactTempo=rawSelectionMode == -3 || meterMetadata;
    if(exactTempo)dataStream >> rawSelectionMode;
    if(dataStream.status() != QDataStream::Ok ||
       (rawSelectionMode != S_LocalCells && rawSelectionMode != S_GlobalMeasure && rawSelectionMode != S_GlobalTrack))
        return;
    clipboardSelMode=static_cast<SelectionModeType>(rawSelectionMode);

    if(clipboardSelMode == S_GlobalMeasure)dataStream >> clipboardNumberOfMeasures;
    else clipboardNumberOfMeasures=0;

    dataStream >> clipboardNumberOfTracks;
    dataStream >> clipboardTickRange;
    if(dataStream.status() != QDataStream::Ok ||
       (clipboardSelMode != S_GlobalMeasure && clipboardSelMode != S_GlobalTrack && clipboardSelMode != S_LocalCells) ||
       clipboardNumberOfTracks <= 0 || clipboardNumberOfTracks > dataStream.device()->bytesAvailable() ||
       (clipboardSelMode != S_GlobalTrack && clipboardTickRange <= 0) ||
       (clipboardSelMode == S_GlobalMeasure && clipboardNumberOfMeasures <= 0))
        return;

    // ------------------------------------------------------------------------------------------------

    // If document has no tracks, then regardless of current editor state all tracks from clipboard
    //  are appended to document at tick position zero, the track properties are taken from clipboard.

    QString selModeStringWithQuantity,selModeStringWithoutQuantity;
    switch(clipboardSelMode)
    {
    case S_GlobalMeasure:
        selModeStringWithQuantity=tr("%n whole measure(s)","",clipboardNumberOfMeasures);
        selModeStringWithoutQuantity=tr("measure(s) by their measure header(s)","",clipboardNumberOfMeasures);
        break;
    case S_GlobalTrack:
        selModeStringWithQuantity=tr("%n whole track(s)","",clipboardNumberOfTracks);
        selModeStringWithoutQuantity=tr("track(s) by their track header(s)","",clipboardNumberOfTracks);
        break;
    case S_LocalCells:
        selModeStringWithQuantity=tr("a range of cells in %n track(s)","",clipboardNumberOfTracks);
        selModeStringWithoutQuantity=tr("a range of cells");
        break;
    }

    // Paste and scale to selection makes no sense with global tracks on clipboard or currently selected
    if(scaleToSelection)
    {
        QString msg;
        if(selMode == S_GlobalTrack)
        {
            msg=tr("Scaling whole tracks is undefined.");
        }
        else if(clipboardSelMode == S_GlobalTrack)
        {
            msg=tr("The clipboard contains %1, but scaling whole tracks is undefined.")
                .arg(selModeStringWithQuantity);
        }
        if(!msg.isEmpty())
        {
            QMessageBox::warning(mainWindow,
                                 tr("Paste and Scale to Selection"),
                                 msg + "\n\n" +
                                 tr("In order to scale, copy a finite time range to the clipboard.\n"
                                    "Then select a different finite time range "
                                    "to paste the clipboard data to."),
                                 QMessageBox::Ok);
            return;
        }
    }
    else if(docRoot->hasTracks() && selMode != clipboardSelMode)
    {
        // Current selection mode must match selection mode of clipboard data
        QMessageBox::warning(mainWindow,
                             tr("Paste"),
                             tr("The clipboard contains %1.\n"
                                "Please select %2 as location for pasting.")
                             .arg(selModeStringWithQuantity).arg(selModeStringWithoutQuantity),
                             QMessageBox::Ok);
        return;
    }

    // ------------------------------------------------------------------------------------------------
    // Deserialize rest of clipboard data

    DocRoot* clipboardDoc=new DocRoot;

    // tick resolution
    dataStream >> clipboardDoc->midiTicksPerWholeNote;
    if(dataStream.status() != QDataStream::Ok || clipboardDoc->midiTicksPerWholeNote <= 0)
    {
        delete clipboardDoc;
        return;
    }

    // selection mode S_GlobalMeasure only:
    //  Properties of first selected measure, and further measureItems within selected range
    if(clipboardSelMode == S_GlobalMeasure)
    {
        // Deserialize list of measure items
        int serializedMeasureItemsListSize;
        dataStream >> serializedMeasureItemsListSize;
        if(dataStream.status() != QDataStream::Ok || serializedMeasureItemsListSize <= 0 ||
           serializedMeasureItemsListSize > dataStream.device()->bytesAvailable())
        {
            delete clipboardDoc;
            return;
        }

        for(int i=0; i < serializedMeasureItemsListSize; ++i)
        {
            DocMeasureItem* measureItem=new DocMeasureItem;
            measureItem->deserialize(dataStream,exactTempo,meterMetadata);
            clipboardDoc->measureItemList.append(measureItem);
            if(i == 0)
            {
                // Older payloads stored a preceding change's relative tick,
                // sometimes negative. These effective properties start at zero.
                measureItem->tickPosition=0;
                measureItem->setRequiredFlagsFirstMeasure();
                if(!measureItem->hasValidProperties())
                    dataStream.setStatus(QDataStream::ReadCorruptData);
            }
            else if(measureItem->tickPosition <= clipboardDoc->measureItemList[i-1]->tickPosition ||
                    measureItem->tickPosition >= clipboardTickRange)
                dataStream.setStatus(QDataStream::ReadCorruptData);
            if(dataStream.status() != QDataStream::Ok)
            {
                delete clipboardDoc;
                return;
            }
        }

        // Legacy BPM counts denominator beats. Resolve it once before meter edits/scaling.
        int denominator=4;
        int numerator=4;
        int meterAnchor=0;
        for(DocMeasureItem* item : clipboardDoc->measureItemList)
        {
            const qint64 meterLength=qint64(numerator) * clipboardDoc->midiTicksPerWholeNote / denominator;
            const bool onBar=meterLength > 0 && (item->tickPosition - meterAnchor) % meterLength == 0;
            if((item->setTimeSignature && !clipboardDoc->canRepresentTimeSignature(*item)) ||
               (!onBar && (item->setTimeSignature || item->setRehearsalMarker || item->setPlaybackOptions)))
                dataStream.setStatus(QDataStream::ReadCorruptData);
            if(item->setTimeSignature)
            {
                denominator=item->timeSignatureDenominator;
                numerator=item->timeSignatureNominator;
                meterAnchor=item->tickPosition;
            }
            if(item->setTempo && item->microsecondsPerQuarter == 0)
                item->microsecondsPerQuarter=item->tempoMicrosecondsPerQuarter(denominator);
            if(item->setTempo && item->microsecondsPerQuarter <= 0)
                dataStream.setStatus(QDataStream::ReadCorruptData);
        }
        if(dataStream.status() != QDataStream::Ok)
        {
            delete clipboardDoc;
            return;
        }
    }

    QList<EditorTrackState> clipboardTrackStateList;

    // deserialize tracks with EditorTrackState and events
    for(int i=0; i < clipboardNumberOfTracks; ++i)
    {
        DocTrack* track=new DocTrack;
        track->deserialize(dataStream);
        clipboardDoc->trackList.append(track);

        // track state
        EditorTrackState trackState;
        trackState.deserialize(dataStream);
        clipboardTrackStateList.append(trackState);

        // events
        int eventCount;
        dataStream >> eventCount;
        if(dataStream.status() != QDataStream::Ok || eventCount < 0 ||
           eventCount > dataStream.device()->bytesAvailable())
        {
            delete clipboardDoc;
            return;
        }

        for(int j=0; j < eventCount; ++j)
        {
            DocEvent* event=new DocEvent;
            event->deserialize(dataStream);
            if(dataStream.status() != QDataStream::Ok)
            {
                delete event;
                delete clipboardDoc;
                return;
            }
            track->insertEvent(event);
        }
    }

    // Scale tick resolution to resolution of current document. The selection
    // range is serialized separately from the events, so it must be converted
    // with the same ratio as the clipboard document before it is used for paste.
    const int clipboardTicksPerWholeNote=clipboardDoc->midiTicksPerWholeNote;
    const int documentTicksPerWholeNote=docRoot->midiTicksPerWholeNote;
    if(documentTicksPerWholeNote <= 0)
    {
        delete clipboardDoc;
        return;
    }

    const qint64 scaledClipboardTickRange=
            qint64(clipboardTickRange) * documentTicksPerWholeNote / clipboardTicksPerWholeNote;
    // Validate the staged document before narrowing scaled positions to int.
    // A valid interval at the source resolution may exceed the target domain.
    const qint64 maxTick=std::numeric_limits<int>::max();
    const auto scaledTick=[&](qint64 tick) {
        return tick * documentTicksPerWholeNote / clipboardTicksPerWholeNote;
    };
    bool valid=clipboardSelMode == S_GlobalTrack || scaledClipboardTickRange <= maxTick;
    for(const DocMeasureItem* measure : clipboardDoc->measureItemList)
        valid=valid && scaledTick(measure->tickPosition) >= 0 && scaledTick(measure->tickPosition) <= maxTick;
    for(const DocTrack* track : clipboardDoc->trackList)
    {
        for(const SmfMetaEvent* meta : track->metaEventList)
            valid=valid && scaledTick(meta->tickPosition) <= maxTick;
        for(const DocEvent* event=track->firstEvent; event; event=event->nextEvent)
        {
            const qint64 start=scaledTick(event->tickPosition);
            const qint64 end=scaledTick(event->tickPositionEnd());
            valid=valid && start >= 0 && start < maxTick && end <= maxTick &&
                    (clipboardSelMode == S_GlobalTrack || event->tickPositionEnd() <= clipboardTickRange);
        }
    }
    if(!valid)
    {
        delete clipboardDoc;
        return;
    }
    clipboardDoc->scaleTickResolution(documentTicksPerWholeNote);
    // Integer resolution conversion can move a formerly valid meter anchor
    // off its target grid. Reject before any grid lookup or undo mutation.
    if(clipboardSelMode == S_GlobalMeasure)
    {
        int denominator=4,numerator=4,meterAnchor=0;
        for(const DocMeasureItem* item : clipboardDoc->measureItemList)
        {
            const qint64 length=qint64(numerator) * documentTicksPerWholeNote / denominator;
            if(length <= 0 || (item->setTimeSignature && !clipboardDoc->canRepresentTimeSignature(*item)) ||
               ((item->setTimeSignature || item->setRehearsalMarker || item->setPlaybackOptions) &&
                              (item->tickPosition - meterAnchor) % length != 0))
            {
                delete clipboardDoc;
                return;
            }
            if(item->setTimeSignature)
            {
                denominator=item->timeSignatureDenominator;
                numerator=item->timeSignatureNominator;
                meterAnchor=item->tickPosition;
            }
        }
    }
    if(scaledClipboardTickRange <= 0)
        clipboardTickRange=1;
    else if(scaledClipboardTickRange > std::numeric_limits<int>::max())
        clipboardTickRange=std::numeric_limits<int>::max();
    else
        clipboardTickRange=static_cast<int>(scaledClipboardTickRange);

    // Check destination addition and global insertion before opening an undo
    // macro. Failure leaves both document data and the undo stack unchanged.
    if(!scaleToSelection && clipboardSelMode != S_GlobalTrack)
    {
        qint64 start=getEditorState().selection.ticksLeft;
        qint64 length=clipboardTickRange;
        qint64 shift=0;
        if(clipboardSelMode == S_GlobalMeasure)
        {
            const int first=getFirstSelectedMeasureIndex();
            const int count=qMin(getNumberOfSelectedMeasures(),clipboardNumberOfMeasures);
            start=docRoot->measureToTicks(first);
            length=clipboardDoc->measureToTicks(count);
            shift=length-(qint64(docRoot->measureToTicks(first+count))-start);
        }
        else if(!getEditorState().selection.oneCellSelectedPerTrack())
            length=qMin(length,qint64(getEditorState().selection.ticksRight)-start);
        valid=start >= 0 && length > 0 && start+length <= maxTick;
        if(shift > 0)
        {
            for(const DocMeasureItem* measure : docRoot->measureItemList)
                valid=valid && qint64(measure->tickPosition)+shift <= maxTick;
            for(const DocTrack* track : docRoot->trackList)
                for(const DocEvent* event=track->firstEvent; event; event=event->nextEvent)
                    valid=valid && qint64(event->tickPositionEnd())+shift <= maxTick;
        }
        if(!valid)
        {
            delete clipboardDoc;
            return;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Merge deserialized data with document data

    if(scaleToSelection)
    {
        pasteWithScaling(clipboardDoc, clipboardTrackStateList, clipboardTickRange);
    }
    else    // non-scaling paste
    {
        switch(clipboardSelMode)
        {
        case S_GlobalMeasure:
            pasteGlobalMeasures(clipboardDoc, clipboardTrackStateList, clipboardNumberOfMeasures);
            break;
        case S_GlobalTrack:
            pasteGlobalTracks(clipboardDoc, clipboardTrackStateList);
            break;
        case S_LocalCells:
            pasteLocalCells(clipboardDoc, clipboardTrackStateList, clipboardTickRange);
            break;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Delete deserialized contents

    delete clipboardDoc;
}

void CS_Clipboard::pasteGlobalMeasures(DocRoot* clipboardDoc, const QList<EditorTrackState>& clipboardTrackStateList, int clipboardNumberOfMeasures)
{
    // Either selection modes match or document has no tracks,
    //  in this case current selection mode is also S_GlobalMeasure
    Q_ASSERT(getEditorState().selection.getSelectionMode() == S_GlobalMeasure);
    Q_ASSERT(clipboardDoc->measureItemList.size() >= 1);

    // Calculate edit ranges in document
    int firstSelectedMeasureIndex = getFirstSelectedMeasureIndex();
    int numberOfSelectedMeasures  = getNumberOfSelectedMeasures();

    int numberOfMeasuresToOverwrite=qMin(numberOfSelectedMeasures, clipboardNumberOfMeasures);

    int documentOverwriteTicksLeft=
            docRoot->measureToTicks(firstSelectedMeasureIndex);
    int documentOverwriteTicksRight=
            docRoot->measureToTicks(firstSelectedMeasureIndex + numberOfMeasuresToOverwrite);

    // calculate actual number of tracks to paste
    int numberOfTracksToPaste;
    if(!docRoot->hasTracks())
        numberOfTracksToPaste=clipboardDoc->trackList.size();
    else
        numberOfTracksToPaste=qMin(clipboardDoc->trackList.size(), docRoot->trackList.size());

    // ------------------------------------------------------------------------------------------------
    // Determine measure properties before paste region
    DocMeasureItem BeforeRegionMeasureProperties;
    if(documentOverwriteTicksLeft == 0)
    {
        // Beginning of piece: take properties of first measure
        BeforeRegionMeasureProperties=docRoot->ticksToMeasure(documentOverwriteTicksLeft).measureProperties;
    }
    else    // in the middle of the piece
    {
        // determine measure properties before documentOverwriteTicksLeft
        BeforeRegionMeasureProperties=docRoot->ticksToMeasure(documentOverwriteTicksLeft - 1).measureProperties;
    }

    // Determine measure properties after paste region
    DocMeasureItem AfterRegionMeasureProperties=
            docRoot->ticksToMeasure(documentOverwriteTicksRight).measureProperties;

    // first measure item from clipboard provides necessary measure properties
    DocMeasureItem firstPastedMeasureProperties(*clipboardDoc->measureItemList[0]);
    if(documentOverwriteTicksLeft == 0)
        firstPastedMeasureProperties.setRequiredFlagsFirstMeasure();
    else
        firstPastedMeasureProperties.enforceChangedProperties(BeforeRegionMeasureProperties);

    // after first measure item from clipboard was prepared, calculate paste tick range (from clipboardDoc!)
    clipboardDoc->measureItemList[0]->setRequiredFlagsFirstMeasure();
    int overwriteTickLength=clipboardDoc->measureToTicks(numberOfMeasuresToOverwrite);

    // ------------------------------------------------------------------------------------------------
    // Start operations

    beginMacro(tr("Paste %n Measure(s)","",numberOfMeasuresToOverwrite),
               EditorRange(documentOverwriteTicksLeft, -1, documentOverwriteTicksRight, -1));

    EditorState newState=getEditorState();

    if(!docRoot->hasTracks())
    {
        // create all tracks listed in clipboardDoc
        for(int trackIndex=0; trackIndex < numberOfTracksToPaste; ++trackIndex)
        {
            // copy track attributes to new track object (no events are copied by constructor)
            DocTrack* clipboardTrack=clipboardDoc->trackList[trackIndex];
            addCommand(new Command_InsertTrack(trackIndex, new DocTrack(*clipboardTrack)));

            // modify also editor state
            newState.trackStateList.insert(trackIndex, clipboardTrackStateList[trackIndex]);

            // (don't copy clipboard events)
        }
    }
    else
    {
        // Delete measures that will be overwritten
        deleteCells_(EditorRange(documentOverwriteTicksLeft, 0,
                                documentOverwriteTicksRight, docRoot->trackList.size()-1));
    }

    deleteMeasureItems_(documentOverwriteTicksLeft, documentOverwriteTicksRight);

    // ------------------------------------------------------------------------------------------------
    // Insert space for events
    insertCells_(documentOverwriteTicksLeft, overwriteTickLength, 0, docRoot->trackList.size()-1);

    // Shift measure items after paste range (if at least one exists)
    for(int i=0; i < docRoot->measureItemList.size(); ++i)
    {
        if(docRoot->measureItemList[i]->tickPosition >= documentOverwriteTicksLeft)
        {
            addCommand(new Command_ShiftMeasureItems(documentOverwriteTicksLeft, overwriteTickLength));
            break;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Merge clipboard measureItems into document measureItems

    if(firstPastedMeasureProperties.measureItemRequired())
    {
        // insert first measure item if required
        firstPastedMeasureProperties.tickPosition=documentOverwriteTicksLeft;
        addCommand(new Command_InsertMeasureItem(new DocMeasureItem(firstPastedMeasureProperties)));
    }

    // subsequent measure items
    DocMeasureItem effectiveClipboardMeasureProperties=firstPastedMeasureProperties;
    for(int i=1; i < clipboardDoc->measureItemList.size(); ++i)
    {
        DocMeasureItem* clipboardMeasureItem=clipboardDoc->measureItemList[i];
        if(clipboardMeasureItem->tickPosition >= overwriteTickLength)continue;

        effectiveClipboardMeasureProperties.makeEffectiveMeasureProperties(*clipboardMeasureItem);
        effectiveClipboardMeasureProperties.tickPosition+=documentOverwriteTicksLeft;

        addCommand(new Command_InsertMeasureItem(new DocMeasureItem(effectiveClipboardMeasureProperties)));
    }

    // Check which measure properties should be changed additionally to those changed by
    //  a measure item maybe existing at the end of the paste region.
    AfterRegionMeasureProperties.enforceChangedProperties(effectiveClipboardMeasureProperties);
    AfterRegionMeasureProperties.tickPosition = documentOverwriteTicksLeft + overwriteTickLength;

    // Does a measure item already exist directly after paste region?
    DocMeasureItem* currentAfterRegionMeasureItem=
            docRoot->getMeasureItemAtExact(AfterRegionMeasureProperties.tickPosition);

    // Check if we actually need to have a measure item at the end of the paste region.
    if(AfterRegionMeasureProperties.measureItemRequired())
    {
        if(currentAfterRegionMeasureItem != NULL)
        {
            // modify the existing item
            addCommand(new Command_MeasureItemProperties(
                    currentAfterRegionMeasureItem,
                    AfterRegionMeasureProperties));
        }
        else
        {
            // create a new item
            addCommand(new Command_InsertMeasureItem(new DocMeasureItem(AfterRegionMeasureProperties)));
        }
    }
    else    // no measure item required
    {
        if(currentAfterRegionMeasureItem != NULL)
        {
            // delete the existing item
            addCommand(new Command_DeleteMeasureItem(currentAfterRegionMeasureItem));
        }
    }

    // ------------------------------------------------------------------------------------------------
    // For all existing tracks in document and on clipboard ...
    for(int trackIndex=0; trackIndex < numberOfTracksToPaste; ++trackIndex)
    {
        // ... insert clipboard events
        DocTrack* clipboardTrack=clipboardDoc->trackList[trackIndex];
        DocEvent* clipboardEvent=clipboardTrack->firstEvent;
        while(clipboardEvent)
        {
            // cut down to overwriteTickLength
            if(clipboardEvent->tickPosition >= overwriteTickLength)
            {
                clipboardEvent=clipboardEvent->nextEvent;
                continue;
            }
            if(clipboardEvent->tickPositionEnd() > overwriteTickLength)
            {
                clipboardEvent->tickLength=overwriteTickLength - clipboardEvent->tickPosition;
            }

            // shift to absolute tick position
            clipboardEvent->tickPosition += documentOverwriteTicksLeft;

            // insert a copy of this event
            addCommand(new Command_InsertEvent(trackIndex, new DocEvent(*clipboardEvent)));

            clipboardEvent=clipboardEvent->nextEvent;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Modify editor state

    newState.setGlobalMeasureSelection(firstSelectedMeasureIndex, numberOfMeasuresToOverwrite, docRoot);

    endMacro(newState, EditorRange(newState.selection.ticksLeft,-1,
                                   newState.selection.ticksRight,-1));
}

void CS_Clipboard::pasteGlobalTracks(DocRoot* clipboardDoc, const QList<EditorTrackState>& clipboardTrackStateList)
{
    int pasteTopTrackIndex;
    int numberOfTracksToPaste;
    EditorRange scrollToRangeUndo;

    if(!docRoot->hasTracks())
    {
        Q_ASSERT(getEditorState().selection.getSelectionMode() == S_GlobalMeasure);

        pasteTopTrackIndex=0;
        numberOfTracksToPaste=clipboardDoc->trackList.size();
        // scrollToRangeUndo: leave invalid
    }
    else
    {
        int numberOfSelectedTracks=
                getEditorState().lastSelectedTrack(docRoot) -
                getEditorState().firstSelectedTrack() + 1;

        pasteTopTrackIndex=getEditorState().selection.trackTop;
        numberOfTracksToPaste=qMin(numberOfSelectedTracks, clipboardDoc->trackList.size());

        scrollToRangeUndo=EditorRange(-1, pasteTopTrackIndex, -1, pasteTopTrackIndex + numberOfTracksToPaste - 1);
    }

    // ------------------------------------------------------------------------------------------------
    // Start operations

    beginMacro(tr("Paste %n Track(s)","",numberOfTracksToPaste), scrollToRangeUndo);

    EditorState newState=getEditorState();

    if(docRoot->hasTracks())
    {
        // Remove tracks that are being replaced
        for(int i=0; i < numberOfTracksToPaste; ++i)
        {
            addCommand(new Command_DeleteTrack(pasteTopTrackIndex, docRoot->trackList[pasteTopTrackIndex]));

            // modify also editor state
            newState.trackStateList.removeAt(pasteTopTrackIndex);
        }
    }

    for(int clipboardTrackIndex=0; clipboardTrackIndex < numberOfTracksToPaste; ++clipboardTrackIndex)
    {
        int documentTrackIndex=pasteTopTrackIndex + clipboardTrackIndex;

        // copy track
        DocTrack* clipboardTrack=clipboardDoc->trackList[clipboardTrackIndex];
        addCommand(new Command_InsertTrack(documentTrackIndex, new DocTrack(*clipboardTrack)));

        // modify also editor state
        newState.trackStateList.insert(documentTrackIndex, clipboardTrackStateList[clipboardTrackIndex]);

        // copy clipboard events
        DocEvent* clipboardEvent=clipboardTrack->firstEvent;
        while(clipboardEvent)
        {
            // insert a copy of this event
            addCommand(new Command_InsertEvent(documentTrackIndex, new DocEvent(*clipboardEvent)));

            clipboardEvent=clipboardEvent->nextEvent;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Select inserted tracks: Set global track selection

    newState.setGlobalTrackSelection(pasteTopTrackIndex, numberOfTracksToPaste, docRoot);

    endMacro(newState, EditorRange(-1, pasteTopTrackIndex,
                                   -1, pasteTopTrackIndex + numberOfTracksToPaste - 1));
}

void CS_Clipboard::pasteLocalCells(DocRoot* clipboardDoc, const QList<EditorTrackState>& clipboardTrackStateList, int clipboardTickRange)
{
    int selectedTickRange=getEditorState().selection.ticksRight - getEditorState().selection.ticksLeft;
    int overwriteTickLength;

    if(getEditorState().selection.oneCellSelectedPerTrack())
    {
        // only 1 cell per track selected:
        //  take horizontal paste range from clipboard, ignore selection tick range
        overwriteTickLength = clipboardTickRange;
    }
    else
    {
        // multiple cells per track selected:
        //  take horizontal paste as minimum of selection tick range and clipboard tick range
        overwriteTickLength=qMin(selectedTickRange, clipboardTickRange);
    }

    int pasteTopTrackIndex;
    int numberOfTracksToPasteInto;
    EditorRange scrollToRangeUndo;

    if(!docRoot->hasTracks())
    {
        Q_ASSERT(getEditorState().selection.getSelectionMode() == S_GlobalMeasure);

        pasteTopTrackIndex=0;
        numberOfTracksToPasteInto=clipboardDoc->trackList.size();
        // scrollToRangeUndo: leave invalid
    }
    else
    {
        int numberOfSelectedTracks=
                getEditorState().lastSelectedTrack(docRoot) -
                getEditorState().firstSelectedTrack() + 1;

        pasteTopTrackIndex=getEditorState().selection.trackTop;

        if(numberOfSelectedTracks == 1 && getEditorState().selection.oneCellSelectedPerTrack())
        {
            // only one track and one cell selected:
            //  paste all tracks from clipboard, but do not create any additional tracks if
            //  clipboard contains too much tracks
            int maxAvailableTracksForPasting = docRoot->trackList.size() - pasteTopTrackIndex;
            numberOfTracksToPasteInto=qMin(maxAvailableTracksForPasting, clipboardDoc->trackList.size());
        }
        else
        {
            numberOfTracksToPasteInto=qMin(numberOfSelectedTracks, clipboardDoc->trackList.size());
        }

        scrollToRangeUndo=getEditorState().selection;
    }

    // ------------------------------------------------------------------------------------------------
    // Start operations

    beginMacro(tr("Paste Cell Range"),scrollToRangeUndo);

    EditorState newState=getEditorState();

    if(!docRoot->hasTracks())
    {
        // create all tracks listed in clipboardDoc
        for(int trackIndex=0; trackIndex < numberOfTracksToPasteInto; ++trackIndex)
        {
            // copy track
            DocTrack* clipboardTrack=clipboardDoc->trackList[trackIndex];
            addCommand(new Command_InsertTrack(trackIndex, new DocTrack(*clipboardTrack)));

            // modify also editor state
            newState.trackStateList.insert(trackIndex, clipboardTrackStateList[trackIndex]);

            // (don't copy clipboard events)
        }
    }
    else
    {
        // Clear cells that will be overwritten.
        //  Last cell may be only partly cleared if ticksLeft+overwriteTickLength is not on a cell boundary.
        for(int documentTrackIndex=pasteTopTrackIndex;
            documentTrackIndex < pasteTopTrackIndex + numberOfTracksToPasteInto;
            ++documentTrackIndex)
        {
            clearTrackRange_(documentTrackIndex,
                             getEditorState().selection.ticksLeft,
                             getEditorState().selection.ticksLeft + overwriteTickLength, false);
        }
    }

    // copy cell range of size (overwriteTickLength, numberOfTracksToPasteInto)
    for(int clipboardTrackIndex=0; clipboardTrackIndex < numberOfTracksToPasteInto; ++clipboardTrackIndex)
    {
        int documentTrackIndex=pasteTopTrackIndex + clipboardTrackIndex;

        DocTrack* clipboardTrack=clipboardDoc->trackList[clipboardTrackIndex];
        DocEvent* clipboardEvent=clipboardTrack->firstEvent;
        while(clipboardEvent)
        {
            // cut down to overwriteTickLength
            if(clipboardEvent->tickPosition >= overwriteTickLength)
            {
                clipboardEvent=clipboardEvent->nextEvent;
                continue;
            }
            if(clipboardEvent->tickPositionEnd() > overwriteTickLength)
            {
                clipboardEvent->tickLength=overwriteTickLength - clipboardEvent->tickPosition;
            }

            // shift to absolute tick position
            clipboardEvent->tickPosition += getEditorState().selection.ticksLeft;

            // insert a copy of this event
            addCommand(new Command_InsertEvent(documentTrackIndex, new DocEvent(*clipboardEvent)));

            clipboardEvent=clipboardEvent->nextEvent;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Change right border of local cell selection to last cell that was touched by overwriteTickLength

    newState.selection.trackTop          = pasteTopTrackIndex;
    newState.selection.ticksRight        =
            docRoot->roundUpTicksToCellBorder(newState.selection.ticksLeft + overwriteTickLength,
                                              newState.writeLength);
    newState.selection.trackBottom       = pasteTopTrackIndex + numberOfTracksToPasteInto - 1;
    newState.selection.anchor.ticksLeft  = newState.selection.ticksLeft;
    newState.selection.anchor.ticksRight = docRoot->roundUpTicksToCellBorder(newState.selection.ticksLeft + 1,
                                                                             newState.writeLength);
    newState.selection.anchor.track      = pasteTopTrackIndex;

    endMacro(newState, EditorRange(newState.selection.ticksLeft,
                                   pasteTopTrackIndex,
                                   newState.selection.ticksRight,
                                   pasteTopTrackIndex + numberOfTracksToPasteInto - 1));
}

void CS_Clipboard::pasteWithScaling(DocRoot* clipboardDoc, const QList<EditorTrackState>& clipboardTrackStateList, int clipboardTickRange)
{
    int selectedTickRange=getEditorState().selection.ticksRight - getEditorState().selection.ticksLeft;

    int pasteTopTrackIndex;
    int numberOfTracksToPasteInto;
    EditorRange scrollToRangeUndo;

    if(!docRoot->hasTracks())
    {
        Q_ASSERT(getEditorState().selection.getSelectionMode() == S_GlobalMeasure);

        pasteTopTrackIndex=0;
        numberOfTracksToPasteInto=clipboardDoc->trackList.size();
        // scrollToRangeUndo: leave invalid
    }
    else
    {
        int numberOfSelectedTracks=
                getEditorState().lastSelectedTrack(docRoot) -
                getEditorState().firstSelectedTrack() + 1;

        pasteTopTrackIndex=getEditorState().selection.trackTop;
        numberOfTracksToPasteInto=qMin(numberOfSelectedTracks, clipboardDoc->trackList.size());

        scrollToRangeUndo=getEditorState().selection;
    }

    // ------------------------------------------------------------------------------------------------
    // Start operations

    beginMacro(tr("Paste and Scale to Selection"),scrollToRangeUndo);

    EditorState newState=getEditorState();

    if(!docRoot->hasTracks())
    {
        // create all tracks listed in clipboardDoc
        for(int trackIndex=0; trackIndex < numberOfTracksToPasteInto; ++trackIndex)
        {
            // copy track
            DocTrack* clipboardTrack=clipboardDoc->trackList[trackIndex];
            addCommand(new Command_InsertTrack(trackIndex, new DocTrack(*clipboardTrack)));

            // modify also editor state
            newState.trackStateList.insert(trackIndex, clipboardTrackStateList[trackIndex]);

            // (don't copy clipboard events)
        }
    }
    else
    {
        // Clear cells that will be overwritten.
        for(int documentTrackIndex=pasteTopTrackIndex;
            documentTrackIndex < pasteTopTrackIndex + numberOfTracksToPasteInto;
            ++documentTrackIndex)
        {
            clearTrackRange_(documentTrackIndex,
                             getEditorState().selection.ticksLeft,
                             getEditorState().selection.ticksRight, false);
        }
    }

    // Copy and scale clipboardTickRange from clipboard to selectedTickRange in document
    for(int clipboardTrackIndex=0; clipboardTrackIndex < numberOfTracksToPasteInto; ++clipboardTrackIndex)
    {
        int documentTrackIndex=pasteTopTrackIndex + clipboardTrackIndex;

        DocTrack* clipboardTrack=clipboardDoc->trackList[clipboardTrackIndex];
        DocEvent* clipboardEvent=clipboardTrack->firstEvent;
        while(clipboardEvent)
        {
            // Apply scaling factor and offset to start and end position.
            //  Use 64-bit calculations for this purpose to retain full integer precision.
            int originalEndTicks=clipboardEvent->tickPositionEnd();

            clipboardEvent->tickPosition =
                    getEditorState().selection.ticksLeft +
                    (int)((qint64)clipboardEvent->tickPosition * selectedTickRange / clipboardTickRange);

            // only notes have a defined length, for other events leave length at value
            //  DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS
            if(clipboardEvent->type == DocEvent::E_Note)
            {
                clipboardEvent->tickLength =
                        getEditorState().selection.ticksLeft +
                        (int)((qint64)originalEndTicks * selectedTickRange / clipboardTickRange) -
                        clipboardEvent->tickPosition;

                // respect minimum tick length
                if(clipboardEvent->tickLength < DOCUMENT_MIN_EVENT_LENGTH_TICKS)
                    clipboardEvent->tickLength = DOCUMENT_MIN_EVENT_LENGTH_TICKS;
            }
            else if(clipboardEvent->type == DocEvent::E_Meta &&
                    clipboardEvent->metaEventData.metaEvent->metaEventType == SMF_META_EVENT_TYPE_END_OF_TRACK)
            {
                const int scaledEnd=(int)(qint64(originalEndTicks) * selectedTickRange / clipboardTickRange);
                clipboardEvent->tickPosition=getEditorState().selection.ticksLeft + qMax(1,scaledEnd)-1;
                clipboardEvent->tickLength=DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS;
            }

            // insert a copy of this event
            addCommand(new Command_InsertEvent(documentTrackIndex, new DocEvent(*clipboardEvent)));

            clipboardEvent=clipboardEvent->nextEvent;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Selected tick range remains unchanged. Adjust track indices only.

    newState.selection.trackTop          = pasteTopTrackIndex;
    newState.selection.trackBottom       = pasteTopTrackIndex + numberOfTracksToPasteInto - 1;
    newState.selection.anchor.track      = pasteTopTrackIndex;

    endMacro(newState, EditorRange(newState.selection.ticksLeft,
                                   pasteTopTrackIndex,
                                   newState.selection.ticksRight,
                                   pasteTopTrackIndex + numberOfTracksToPasteInto - 1));
}
