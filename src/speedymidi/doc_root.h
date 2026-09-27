/***************************************************************************
 *  doc_root.h - Model class: Model Root
 *               (cf. Model–View–Controller Architecture)
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

#ifndef DOC_ROOT_H
#define DOC_ROOT_H

#include "global.h"

class DocRoot
{
private:
    DocRoot(const DocRoot& rhs) { UNUSED(rhs); }   // disable copy constructor
public:
    DocRoot();
    ~DocRoot();
    void makeDeepCopy(const DocRoot& rhs);

    bool hasTracks() const { return trackList.size() >= 1; }

    DocMeasureItem* getMeasureItemAtExact(int ticks) const;         // returns NULL if no such item exists

    // Measure and cell grid utility functions
    int ticksPerBeat(const DocMeasureItem& measureProperties) const;
    int ticksPerMeasure(const DocMeasureItem& measureProperties) const;
    int getNextCellMeasureInternalTickPosition(int measureInternalCellIndex, const WriteLength& writeLength) const;
    DocMeasureItem getFirstMeasureEffectiveProperties() const;
    TicksToMeasureResult ticksToMeasure(int ticks) const;
    int measureToTicks(int measureIndex) const;
    int roundDownTicksToCellBorder(int ticks, const WriteLength& writeLength) const;
    int roundUpTicksToCellBorder(int ticks, const WriteLength& writeLength) const;
    int roundDownTicksToMeasureBorder(int ticks) const;
    int roundUpTicksToMeasureBorder(int ticks) const;
    int getMaxTicks() const;
    int getMaxMeasure() const;

    QString getNewTrackIdentifier() const;
    int getNewTrackChannel() const;
    QColor getNewTrackEventColor() const;

    EditorState prepareDefaultDocument();
    EditorState prepareDocumentByWizardSettings(const DocMeasureItem& firstMeasureProperties, QString trackWizardString, bool assignPatches);
    static bool isValidTrackWizardString(const QString& tracksToAdd);
    DocTrack* processTrackWizardString(bool assignPatch, EditorTrackState& newTrackState, QString& trackWizardString) const;

    bool swingPresent() const;
    QString collectNonStandardPlaybackOptionsDescription(int relativePlaybackSpeed) const;

    bool load(QFile* smfFile, EditorState* loadedEditorState);
    bool save(QFile* smfFile, const EditorState& editorStateToSave, bool saveEditorState) const;
    bool save(QFile* smfFile, const EditorState& editorStateToSave, const ConversionOptions& conversionOptions) const;
    bool save(QFile* smfFile, const EditorState& editorStateToSave, const ConversionOptions& conversionOptions, const QList<int> trackIndexList) const;
    void makeCompatible(const ConversionOptions& conversionOptions);
    void scaleTickResolution(int newTicksPerWholeNote);

    // Subobjects
    QList<DocMeasureItem*> measureItemList;
    QList<DocTrack*> trackList;

    // non-time-shiftable global (conductor track) meta events from SMF_Document (not editable)
    QList<SmfMetaEvent*> metaEventList;

    // flag is true if this is a default document that was never modified (undo does not reset this flag!)
    bool unmodifiedDefaultDocument;

    int midiTicksPerWholeNote;                  // Tick resolution

    // -----------------------------------------------------------------------------
    // Properties covered by undo/redo

    // (none)
};

class ConversionOptions
{
public:
    explicit ConversionOptions(bool savingOriginalFile)   // default constructor for standard save document command
    {
        this->savingOriginalFile=savingOriginalFile;

        convertRelativePlaybackSpeed=false;
        relativePlaybackSpeedInPercent=100;

        convertSwing=false;
    }

    // advanced constructor for special options to generate a copy of the file for playback in
    //  other MIDI playback software
    ConversionOptions(bool convertRelativePlaybackSpeed, int relativePlaybackSpeedInPercent, bool convertSwing)
    {
        savingOriginalFile=false;

        this->convertRelativePlaybackSpeed=convertRelativePlaybackSpeed;
        this->relativePlaybackSpeedInPercent=relativePlaybackSpeedInPercent;
        this->convertSwing=convertSwing;
    }

    bool savingOriginalFile;

    bool convertRelativePlaybackSpeed;
    int relativePlaybackSpeedInPercent;

    bool convertSwing;
};

#endif // DOC_ROOT_H
