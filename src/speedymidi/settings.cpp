/***************************************************************************
 *  settings.cpp - Application Settings and LRU Values
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

#include "settings.h"
#include "preferencesdialog.h"

Settings::Settings()
        : s("HoHo", "SpeedyMIDI")
{
    // First time startup settings

    // use system locale as default locale for messages and musical names
    messageTranslationLocaleName=QLocale::system().name();
    musicalTranslationLocaleName=QLocale::system().name();

    mainWindowToolbarVisible=true;

    midiThru=false;

    checkForChannelCollisions=true;
    checkFileCompatibility=true;
    scrollingPlayback=true;
    playbackStartPosition=PSP_fromCursorPosition;
    newNoteMidiVelocity=80;

    // mouse piano
    mousePiano.visible=false;
    mousePiano.dockWidgetFramePos=QPoint(300,400);
    mousePiano.size=QSize(600, 80);
    mousePiano.leftWhiteKey=4*7;
    mousePiano.percussionMode=false;
    mousePiano.midiChannelChromatic=MIDI_MIN_CHANNEL;
    mousePiano.midiChannelPercussion=MIDI_PERCUSSION_CHANNEL;
    mousePiano.midiVelocity=80;

    // dialog LRU values
    LRU.scaleNoteLengthPercent=100.;
    LRU.transposeOctaveSteps=1;
    LRU.transposeDiatonicSteps=1;
    LRU.transposeChromaticSteps=1;
    LRU.addSwingDialogSwingHardness=100;

    LRU.trackWizardAssignPatches=true;

    LRU.conversionOptionsConvertRelativePlaybackSpeed=true;
    LRU.conversionOptionsConvertSwing=true;

    LRU.extractPartsGenerateInverseParts=false;
    LRU.extractPartsFileNameFormat="%s-%t";
    LRU.extractPartsSeparateTrackNamesByComma=true;
    LRU.extractPartsSaveCompatibleFiles=true;

    read();
}

void Settings::read()
{
    // Read settings from registry or equivalent. Do not trust ANY setting as external modifications are possible.

    messageTranslationLocaleName=s.value("Translation/messageLocale", messageTranslationLocaleName).toString();
    musicalTranslationLocaleName=s.value("Translation/musicalLocale", musicalTranslationLocaleName).toString();

    mainWindowGeometry=s.value("MainWindow/Geometry").toByteArray();
    mainWindowState=s.value("MainWindow/State").toByteArray();
    mainWindowToolbarVisible=s.value("MainWindow/toolbarVisible",mainWindowToolbarVisible).toBool();

    // Use special character 0x01 to indicate "not set"
    selectedInputDevice=s.value("MIDI/inputDevice","\x1").toString();
    selectedOutputDevice=s.value("MIDI/outputDevice","\x1").toString();
    midiThru=s.value("MIDI/thru",midiThru).toBool();

    scrollingPlayback=s.value("Playback/scrolling",scrollingPlayback).toBool();
    playbackStartPosition=(PlaybackStartPositionType)
                          s.value("Playback/startPosition",(int)playbackStartPosition).toInt();
    if(playbackStartPosition != PSP_fromBeginning &&    // Assure enum value is valid
       playbackStartPosition != PSP_fromLeftmostVisibleMeasure &&
       playbackStartPosition != PSP_fromCursorPosition)
        playbackStartPosition=PSP_fromLeftmostVisibleMeasure;

    checkForChannelCollisions=s.value("Playback/checkForChannelCollisions",checkForChannelCollisions).toBool();
    checkFileCompatibility = s.value("Playback/checkFileCompatibility",checkFileCompatibility).toBool();

    newNoteMidiVelocity=s.value("NoteEntry/newNoteMidiVelocity",newNoteMidiVelocity).toInt();
    if(newNoteMidiVelocity < 1                  )newNoteMidiVelocity=1;
    if(newNoteMidiVelocity > MIDI_MAX_DATA_VALUE)newNoteMidiVelocity=MIDI_MAX_DATA_VALUE;

    // --------------------------------------------------------------------------------------------
    // Mouse piano

    mousePiano.visible=s.value("MousePiano/visible",mousePiano.visible).toBool();
    mousePiano.dockWidgetFramePos = s.value("MousePiano/dockWidgetFramePos",mousePiano.dockWidgetFramePos).toPoint();
    mousePiano.size=s.value("MousePiano/size", mousePiano.size).toSize();
    mousePiano.leftWhiteKey=s.value("MousePiano/leftWhiteKey",mousePiano.leftWhiteKey).toInt();
    mousePiano.percussionMode=s.value("MousePiano/percussionMode",mousePiano.percussionMode).toBool();
    mousePiano.midiChannelChromatic=s.value("MousePiano/midiChannelChromatic",mousePiano.midiChannelChromatic).toInt();
    mousePiano.midiChannelPercussion=s.value("MousePiano/midiChannelPercussion",mousePiano.midiChannelPercussion).toInt();
    mousePiano.midiVelocity=s.value("MousePiano/midiVelocity",mousePiano.midiVelocity).toInt();

    if(mousePiano.leftWhiteKey < 0)mousePiano.leftWhiteKey=0;
    if(mousePiano.leftWhiteKey > MOUSEPIANO_MAX_WHITE_KEY)mousePiano.leftWhiteKey=MOUSEPIANO_MAX_WHITE_KEY;

    if(mousePiano.midiChannelChromatic < MIDI_MIN_CHANNEL)mousePiano.midiChannelChromatic=MIDI_MIN_CHANNEL;
    if(mousePiano.midiChannelChromatic > MIDI_MAX_CHANNEL)mousePiano.midiChannelChromatic=MIDI_MAX_CHANNEL;

    if(mousePiano.midiChannelPercussion < MIDI_MIN_CHANNEL)mousePiano.midiChannelPercussion=MIDI_MIN_CHANNEL;
    if(mousePiano.midiChannelPercussion > MIDI_MAX_CHANNEL)mousePiano.midiChannelPercussion=MIDI_MAX_CHANNEL;

    if(mousePiano.midiVelocity < 0                  )mousePiano.midiVelocity=0;
    if(mousePiano.midiVelocity > MIDI_MAX_DATA_VALUE)mousePiano.midiVelocity=MIDI_MAX_DATA_VALUE;

    // --------------------------------------------------------------------------------------------
    // LRU values

    recentFileList=s.value("LRU/recentFileList").toStringList();

    LRU.scaleNoteLengthPercent  = s.value("LRU/scaleNoteLengthPercent",LRU.scaleNoteLengthPercent).toDouble();
    LRU.transposeOctaveSteps    = s.value("LRU/transposeOctaveSteps",LRU.transposeOctaveSteps).toInt();
    LRU.transposeDiatonicSteps  = s.value("LRU/transposeDiatonicSteps",LRU.transposeDiatonicSteps).toInt();
    LRU.transposeChromaticSteps = s.value("LRU/transposeChromaticSteps",LRU.transposeChromaticSteps).toInt();
    LRU.addSwingDialogSwingHardness = s.value("LRU/addSwingDialogSwingHardness",LRU.addSwingDialogSwingHardness).toInt();
    LRU.trackWizardAssignPatches = s.value("LRU/trackWizardAssignPatches",LRU.trackWizardAssignPatches).toBool();
    LRU.conversionOptionsConvertRelativePlaybackSpeed  = s.value("LRU/conversionOptionsConvertRelativePlaybackSpeed",LRU.conversionOptionsConvertRelativePlaybackSpeed).toBool();
    LRU.conversionOptionsConvertSwing  = s.value("LRU/conversionOptionsConvertSwing",LRU.conversionOptionsConvertSwing).toBool();
    LRU.extractPartsGenerateInverseParts = s.value("LRU/extractPartsGenerateInverseParts",LRU.extractPartsGenerateInverseParts).toBool();
    LRU.extractPartsFileNameFormat = s.value("LRU/extractPartsFileNameFormat",LRU.extractPartsFileNameFormat).toString();
    LRU.extractPartsSeparateTrackNamesByComma = s.value("LRU/extractPartsSeparateTrackNamesByComma",LRU.extractPartsSeparateTrackNamesByComma).toBool();
    LRU.extractPartsSaveCompatibleFiles = s.value("LRU/extractPartsSaveCompatibleFiles",LRU.extractPartsSaveCompatibleFiles).toBool();

    // most LRU values need not to be checked, the GUI will do this automatically

    if(LRU.addSwingDialogSwingHardness < DOCUMENT_MIN_SWING_HARDNESS)
        LRU.addSwingDialogSwingHardness=DOCUMENT_MIN_SWING_HARDNESS;
    if(LRU.addSwingDialogSwingHardness > DOCUMENT_MAX_SWING_HARDNESS)
        LRU.addSwingDialogSwingHardness=DOCUMENT_MAX_SWING_HARDNESS;
}

void Settings::write()
{
    s.setValue("Translation/messageLocale", messageTranslationLocaleName);
    s.setValue("Translation/musicalLocale", musicalTranslationLocaleName);

    s.setValue("MainWindow/Geometry", mainWindowGeometry);
    s.setValue("MainWindow/State", mainWindowState);
    s.setValue("MainWindow/toolbarVisible",mainWindowToolbarVisible);

    s.setValue("MIDI/inputDevice", selectedInputDevice);
    s.setValue("MIDI/outputDevice", selectedOutputDevice);
    s.setValue("MIDI/thru", midiThru);

    s.setValue("Playback/scrolling", scrollingPlayback);
    s.setValue("Playback/startPosition", (int)playbackStartPosition);
    s.setValue("Playback/checkForChannelCollisions", checkForChannelCollisions);
    s.setValue("Playback/checkFileCompatibility",checkFileCompatibility);

    s.setValue("NoteEntry/newNoteMidiVelocity",newNoteMidiVelocity);

    // --------------------------------------------------------------------------------------------
    // Mouse piano

    s.setValue("MousePiano/visible",mousePiano.visible);
    s.setValue("MousePiano/dockWidgetFramePos",mousePiano.dockWidgetFramePos);
    s.setValue("MousePiano/size", mousePiano.size);
    s.setValue("MousePiano/leftWhiteKey",mousePiano.leftWhiteKey);
    s.setValue("MousePiano/percussionMode",mousePiano.percussionMode);
    s.setValue("MousePiano/midiChannelChromatic",mousePiano.midiChannelChromatic);
    s.setValue("MousePiano/midiChannelPercussion",mousePiano.midiChannelPercussion);
    s.setValue("MousePiano/midiVelocity",mousePiano.midiVelocity);

    // --------------------------------------------------------------------------------------------
    // LRU values

    s.setValue("LRU/recentFileList", recentFileList);

    s.setValue("LRU/scaleNoteLengthPercent", LRU.scaleNoteLengthPercent);
    s.setValue("LRU/transposeOctaveSteps", LRU.transposeOctaveSteps);
    s.setValue("LRU/transposeDiatonicSteps", LRU.transposeDiatonicSteps);
    s.setValue("LRU/transposeChromaticSteps", LRU.transposeChromaticSteps);
    s.setValue("LRU/addSwingDialogSwingHardness",LRU.addSwingDialogSwingHardness);
    s.setValue("LRU/trackWizardAssignPatches", LRU.trackWizardAssignPatches);
    s.setValue("LRU/conversionOptionsConvertRelativePlaybackSpeed",LRU.conversionOptionsConvertRelativePlaybackSpeed);
    s.setValue("LRU/conversionOptionsConvertSwing",LRU.conversionOptionsConvertSwing);
    s.setValue("LRU/extractPartsGenerateInverseParts",LRU.extractPartsGenerateInverseParts);
    s.setValue("LRU/extractPartsFileNameFormat",LRU.extractPartsFileNameFormat);
    s.setValue("LRU/extractPartsSeparateTrackNamesByComma",LRU.extractPartsSeparateTrackNamesByComma);
    s.setValue("LRU/extractPartsSaveCompatibleFiles",LRU.extractPartsSaveCompatibleFiles);
}

bool Settings::execPreferencesDialog(bool startWithPageMidiDevices)
{
    PreferencesDialog dlg(NULL);    // application modal dialog

    dlg.messageTranslationLocaleName    = messageTranslationLocaleName;
    dlg.musicalTranslationLocaleName    = musicalTranslationLocaleName;
    dlg.selectedInputDevice             = selectedInputDevice;
    dlg.selectedOutputDevice            = selectedOutputDevice;
    dlg.checkForChannelCollisions       = checkForChannelCollisions;
    dlg.checkFileCompatibility          = checkFileCompatibility;
    dlg.scrollingPlayback               = scrollingPlayback;
    dlg.playbackStartPosition           = playbackStartPosition;
    dlg.newNoteMidiVelocity             = newNoteMidiVelocity;
    dlg.mousePianoMidiChannelChromatic  = mousePiano.midiChannelChromatic;
    dlg.mousePianoMidiChannelPercussion = mousePiano.midiChannelPercussion;
    dlg.mousePianoMidiVelocity          = mousePiano.midiVelocity;

    if(dlg.exec(startWithPageMidiDevices) == QDialog::Accepted)
    {
        messageTranslationLocaleName     = dlg.messageTranslationLocaleName;
        musicalTranslationLocaleName     = dlg.musicalTranslationLocaleName;
        selectedInputDevice              = dlg.selectedInputDevice;
        selectedOutputDevice             = dlg.selectedOutputDevice;
        checkForChannelCollisions        = dlg.checkForChannelCollisions;
        checkFileCompatibility           = dlg.checkFileCompatibility;
        scrollingPlayback                = dlg.scrollingPlayback;
        playbackStartPosition            = dlg.playbackStartPosition;
        newNoteMidiVelocity              = dlg.newNoteMidiVelocity;
        mousePiano.midiChannelChromatic  = dlg.mousePianoMidiChannelChromatic;
        mousePiano.midiChannelPercussion = dlg.mousePianoMidiChannelPercussion;
        mousePiano.midiVelocity          = dlg.mousePianoMidiVelocity;

        write();
        return true;
    }

    return false;
}
