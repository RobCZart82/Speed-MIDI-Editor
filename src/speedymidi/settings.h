/***************************************************************************
 *  settings.h - Application Settings and LRU Values
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include "global.h"
#include <QSettings>

class Settings
{
public:
    enum PlaybackStartPositionType { PSP_fromBeginning, PSP_fromLeftmostVisibleMeasure, PSP_fromCursorPosition };

    Settings();

    void read();
    void write();
    bool execPreferencesDialog(bool startWithPageMidiDevices);

protected:
    QSettings s;

public:

    QString messageTranslationLocaleName;
    QString musicalTranslationLocaleName;

    QStringList recentFileList;
    QByteArray mainWindowGeometry;
    QByteArray mainWindowState;
    bool mainWindowToolbarVisible;

    QString selectedInputDevice;    // empty string if "none" was selected
    QString selectedOutputDevice;   // empty string if "none" was selected
    bool midiThru;

    bool checkForChannelCollisions;
    bool checkFileCompatibility;
    bool scrollingPlayback;
    PlaybackStartPositionType playbackStartPosition;
    int newNoteMidiVelocity;

    struct MousePianoType
    {
        bool visible;
        QPoint dockWidgetFramePos;
        QSize size;

        int leftWhiteKey;

        bool percussionMode;
        int midiChannelChromatic;
        int midiChannelPercussion;
        int midiVelocity;
    };
    MousePianoType mousePiano;

    // GUI dialog LRU values
    struct LRU_Type
    {
        double scaleNoteLengthPercent;
        int transposeOctaveSteps;
        int transposeDiatonicSteps;
        int transposeChromaticSteps;
        int addSwingDialogSwingHardness;

        bool trackWizardAssignPatches;

        bool conversionOptionsConvertRelativePlaybackSpeed;
        bool conversionOptionsConvertSwing;

        bool extractPartsGenerateInverseParts;
        QString extractPartsFileNameFormat;
        bool extractPartsSeparateTrackNamesByComma;
        bool extractPartsSaveCompatibleFiles;
    };
    LRU_Type LRU;
};

#endif // SETTINGS_H
