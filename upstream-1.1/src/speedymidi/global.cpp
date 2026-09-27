/***************************************************************************
 *  global.cpp - Includes, Predeclarations, and Constants for all Components
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

#include "global.h"

const QColor DOCUMENT_EVENT_COLORS[DOCUMENT_N_EVENT_COLORS]=
{
    QColor(0xff,0x50,0x50), //  0 red
    QColor(0xff,0xa0,0x40), //  1 orange
    QColor(0xff,0xff,0x40), //  2 yellow
    QColor(0xb0,0xff,0x50), //  3 yellow-green
    QColor(0x60,0xf0,0x60), //  4 green
    QColor(0x90,0x90,0xff), //  5 blue
    QColor(0xa8,0xa8,0xff), //  6 violet-blue
    QColor(0xd8,0x40,0xff), //  7 violet
    QColor(0xff,0x40,0xc0), //  8 purple
    QColor(0xd0,0x78,0x78), //  9 red-gray
    QColor(0xe0,0xa0,0x60), // 10 brown
    QColor(0x60,0xc0,0x60), // 11 green-gray
    QColor(0x70,0xa8,0xa8), // 12 dark-turquoise
    QColor(0x80,0x80,0xd8), // 13 blue-gray
    QColor(0x50,0xe8,0xe8), // 14 light-turquoise
    QColor(0xff,0x90,0xff), // 15 pink
};

const QColor DOCUMENT_MARKER_COLORS[DOCUMENT_N_MARKER_COLORS]=
{
    QColor(Qt::darkRed),
    QColor(Qt::darkYellow),
    QColor(Qt::darkGreen),
    QColor(Qt::darkBlue),
    QColor(Qt::darkMagenta),
    QColor(Qt::darkCyan),
    QColor(0x80,0x40,0), // brown
    QColor(Qt::black),
};

const SwingType DOCUMENT_SWING_TYPES[]={
    { QT_TRANSLATE_NOOP("GlobalConstants","&Light"), 75 },
    { QT_TRANSLATE_NOOP("GlobalConstants","&Standard"), 100 },
    { QT_TRANSLATE_NOOP("GlobalConstants","&Heavy"), 125 },
    { QT_TRANSLATE_NOOP("GlobalConstants","&Dotted 8th + 16th"), 150 },
};
const int DOCUMENT_N_SWING_TYPES=sizeof(DOCUMENT_SWING_TYPES) / sizeof(DOCUMENT_SWING_TYPES[0]);

QString getSwingTypeName(int index)
{
    return QApplication::translate("GlobalConstants", DOCUMENT_SWING_TYPES[index].description);
}

const char* MIDI_PATCH_NAME[MIDI_N_PATCH_NAMES]={
    "Acoustic Grand Piano",
    "Bright Acoustic Piano",
    "Electric Grand Piano",
    "Honky-tonk Piano",
    "Rhodes Piano",
    "Chorused Piano",
    "Harpsichord",
    "Clavinet",
    "Celesta",
    "Glockenspiel",
    "Music Box",
    "Vibraphone",
    "Marimba",
    "Xylophone",
    "Tubular Bells",
    "Dulcimer",
    "Hammond Organ",
    "Percussive Organ",
    "Rock Organ",
    "Church Organ",
    "Reed Organ",
    "Accordion",
    "Harmonica",
    "Tango Accordion",
    "Acoustic Guitar (nylon)",
    "Acoustic Guitar (steel)",
    "Electric Guitar (jazz)",
    "Electric Guitar (clean)",
    "Electric Guitar (muted)",
    "Overdriven Guitar",
    "Distortion Guitar",
    "Guitar Harmonics",
    "Acoustic Bass",
    "Electric Bass (finger)",
    "Electric Bass (pick)",
    "Fretless Bass",
    "Slap Bass 1",
    "Slap Bass 2",
    "Synth Bass 1",
    "Synth Bass 2",
    "Violin",
    "Viola",
    "Cello",
    "Contrabass",
    "Tremolo Strings",
    "Pizzicato Strings",
    "Orchestral Harp",
    "Timpani",
    "String Ensemble 1",
    "String Ensemble 2",
    "SynthStrings 1",
    "SynthStrings 2",
    "Choir Aahs",
    "Voice Oohs",
    "Synth Voice",
    "Orchestra Hit",
    "Trumpet",
    "Trombone",
    "Tuba",
    "Muted Trumpet",
    "French Horn",
    "Brass Section",
    "Synth Brass 1",
    "Synth Brass 2",
    "Soprano Sax",
    "Alto Sax",
    "Tenor Sax",
    "Baritone Sax",
    "Oboe",
    "English Horn",
    "Bassoon",
    "Clarinet",
    "Piccolo",
    "Flute",
    "Recorder",
    "Pan Flute",
    "Bottle Blow",
    "Shakuhachi",
    "Whistle",
    "Ocarina",
    "Lead 1 (square)",
    "Lead 2 (sawtooth)",
    "Lead 3 (calliope lead)",
    "Lead 4 (chiff lead)",
    "Lead 5 (charang)",
    "Lead 6 (voice)",
    "Lead 7 (fifths)",
    "Lead 8 (bass + lead)",
    "Pad 1 (new age)",
    "Pad 2 (warm)",
    "Pad 3 (polysynth)",
    "Pad 4 (choir)",
    "Pad 5 (bowed)",
    "Pad 6 (metallic)",
    "Pad 7 (halo)",
    "Pad 8 (sweep)",
    "FX 1 (rain)",
    "FX 2 (soundtrack)",
    "FX 3 (crystal)",
    "FX 4 (atmosphere)",
    "FX 5 (brightness)",
    "FX 6 (goblins)",
    "FX 7 (echoes)",
    "FX 8 (sci-fi)",
    "Sitar",
    "Banjo",
    "Shamisen",
    "Koto",
    "Kalimba",
    "Bagpipe",
    "Fiddle",
    "Shanai",
    "Tinkle Bell",
    "Agogo",
    "Steel Drums",
    "Woodblock",
    "Taiko Drum",
    "Melodic Tom",
    "Synth Drum",
    "Reverse Cymbal",
    "Guitar Fret Noise",
    "Breath Noise",
    "Seashore",
    "Bird Tweet",
    "Telephone Ring",
    "Helicopter",
    "Applause",
    "Gunshot",
};
