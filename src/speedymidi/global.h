/***************************************************************************
 *  global.h - Includes, Predeclarations, and Constants for all Components
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

#ifndef GLOBAL_H
#define GLOBAL_H

#define UNUSED(x) ((void)x)

// __________________________________________ Qt includes ____________________________________________

// include only some basic classes here

#include <QApplication>
#include <QAction>

#include <QList>
#include <QStringList>
#include <QMap>
#include <QColor>

#include <QResizeEvent>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <QDebug>

class QMenu;
class QScrollBar;
class QSpinBox;
class QSlider;
class QPushButton;

class QUndoStack;
class QUndoCommand;
class QListWidgetItem;
class QButtonGroup;
class QAbstractButton;
class QDomElement;
class QFile;

// ____________________________________________ Constants ____________________________________________

// Application constants
#if defined(Q_OS_MACOS)
#define APP_TRANSLATOR_PATH_PREFIX       "../Resources/translations/"
#else
#define APP_TRANSLATOR_PATH_PREFIX       "translations/"
#endif
#define APP_TRANSLATOR_MSG_PREFIX        "msg_"
#define APP_TRANSLATOR_QT_PREFIX         "qt_"
#define APP_TRANSLATOR_MUSIC_PREFIX      "music_"

// -----------------------------------------------------------------------------------------------
// MainWindow constants

#define MAINWINDOW_CASCADE_OFFSET               40

// -----------------------------------------------------------------------------------------------
// Zoom slider constants

#define ZOOM_GLASS_LABEL_MINIMUM_SIZE               16
#define ZOOM_GLASS_LABEL_HOLD_TRIGGER_INTERVAL      20

#define ZOOM_SLIDER_WIDGET_MAX_VALUE                200
#define ZOOM_SLIDER_WIDGET_LABEL_CLICK_STEP         2
#define ZOOM_SLIDER_WIDGET_KEY_PRESSED_STEP         8

// -----------------------------------------------------------------------------------------------
// Universal MIDI constants

#define MIDI_MIN_CHANNEL              1
#define MIDI_MAX_CHANNEL             16
#define MIDI_PERCUSSION_CHANNEL      10

#define MIDI_MAX_KEY_SIGNATURE        7      // SMF FF59: seven flats through seven sharps
#define MIDI_MAX_DATA_VALUE         127
#define MIDI_N_NOTE_NUMBERS   (MIDI_MAX_DATA_VALUE+1) // count
#define MIDI_MAX_OCTAVE              10      // [0;10] inclusive
#define MIDI_PANORAMA_CENTER         64

#define MIDI_MIN_BPM                  1
#define MIDI_MAX_BPM                  960

#define MIDI_N_PATCH_NAMES  128
extern const char* MIDI_PATCH_NAME[MIDI_N_PATCH_NAMES];

// -----------------------------------------------------------------------------------------------
// Document constants

#define DOCUMENT_N_EVENT_COLORS       16
#define DOCUMENT_N_MARKER_COLORS      8
extern const QColor DOCUMENT_EVENT_COLORS [DOCUMENT_N_EVENT_COLORS ];
extern const QColor DOCUMENT_MARKER_COLORS[DOCUMENT_N_MARKER_COLORS];

#define DOCUMENT_SWING_PLAYBACK_BASE_NOTE_DENOMINATOR     8   // eighth note
#define DOCUMENT_MIN_SWING_HARDNESS                      10
#define DOCUMENT_MAX_SWING_HARDNESS                     290

struct SwingType
{
    const char* description;
    int swingHardness;
};

extern const SwingType DOCUMENT_SWING_TYPES[];
extern const int DOCUMENT_N_SWING_TYPES;
QString getSwingTypeName(int index);

#define DOCUMENT_MIN_EVENT_LENGTH_TICKS            1
#define DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS        1
#define DOCUMENT_DEFAULT_TICKS_PER_QUARTER_NOTE  480     // usual default in Standard MIDI files
#define DOCUMENT_DEFAULT_TICKS_PER_WHOLE_NOTE    (4*DOCUMENT_DEFAULT_TICKS_PER_QUARTER_NOTE)
#define DOCUMENT_MIN_TICKS_PER_WHOLE_NOTE        DOCUMENT_DEFAULT_TICKS_PER_WHOLE_NOTE
// all loaded standard MIDI files are forced to have this resolution (the resolution is scaled appropriately)
// Should not be reduced to avoid cells with zero length in extremely small write length settings
//  current smallest setting: 128th with tuplet setting EDITOR_MAX_TUPLET_FRACTIONAL_COMPONENT=15
//                            notes in the space of 2, so
//                            4 * 480 / 128 / 15 * 2 = 2 >= 1

// -----------------------------------------------------------------------------------------------
// Editor specific constants

#define EDITOR_MAX_WRITELENGTH_DENOMINATOR       128
#define EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR     32  // less than 128, avoiding pathological drawing and editing effects (also tick value overflows), if changed: modify also EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR_EXP
#define EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR_EXP  5  // =log2(EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR)
#define EDITOR_MAX_TUPLET_FRACTIONAL_COMPONENT    15  // if changed: modify also writelengthdialog.ui, 2 x setMaximum(...)

// -----------------------------------------------------------------------------------------------
// Controller and controller subsystem constants

#define CS_NAVIGATION_MOUSE_AUTOSCROLL_TIMER_INTERVAL          30

/* Maximum measure index for scrolling:

  make a conservative estimation:
   max. 2^31 ticks, divided by max. measure length of 32 whole notes
   at 4 times the default resolution (480 ticks per quarter) => 8738 measures
  In pathological cases however, application may crash because of tick overflow.
  This might happen when the resolution is VERY high (up to 32767 per quarter is possible in MIDI)
  and we have huge measures of 32 whole notes. Well, just don't worry about that...
*/
#define CS_NAVIGATION_MAX_FIRST_MEASURE                  8191

// -----------------------------------------------------------------------------------------------
// View constants

#define VIEW_TRACK_SEPARATOR_X_LEFT               3
#define VIEW_TRACK_SEPARATOR_Y_INBETWEEN          4
#define VIEW_TRACK_HEADER_PANEL_WIDTH             120
#define VIEW_TRACK_HEADER_NOTE_RANGE_SLIDER_WIDTH 20
#define VIEW_TRACK_HEADER_WIDTH    (VIEW_TRACK_HEADER_PANEL_WIDTH + VIEW_TRACK_HEADER_NOTE_RANGE_SLIDER_WIDTH)

#define VIEW_MEASURE_HEADER_HEIGHT                60
#define VIEW_MEASURE_HEADER_ITEMS_CELL_HEIGHT     27
#define VIEW_MEASURE_HEADER_MARKER_CELL_HEIGHT    18

#define VIEW_CELL_AREA_LEFT        (VIEW_TRACK_SEPARATOR_X_LEFT + VIEW_TRACK_HEADER_WIDTH)
#define VIEW_CELL_AREA_TOP         (VIEW_MEASURE_HEADER_HEIGHT + VIEW_TRACK_SEPARATOR_Y_INBETWEEN)

#define VIEW_MIN_VIEW_SIZE_XY                     50
#define VIEW_END_OF_SCREEN_GRADIENT_SIZE          9
#define VIEW_FONT_NAME                            "Arial"
#define VIEW_DEFAULT_FONT_PIXEL_SIZE              13
#define VIEW_SELECTION_FRAME_THICKNESS            1

#define VIEW_NOTE_HEIGHT_IN_PIXELS_MIN            3
#define VIEW_NOTE_HEIGHT_IN_PIXELS_MAX            25
#define VIEW_NOTE_HEIGHT_IN_PIXELS_EXP_BASE       1.5

// Limit definitions:
// value =   0 => 480 ticks (1 quarter in default resolution) in  5 pixels => TicksPerPixel = 96
// value = max =>  15 ticks (1 / 128th in default resolution) in 15 pixels => TicksPerPixel = 1
#define VIEW_TICKS_PER_PIXEL_MIN_DEFAULT_RESOLUTION   1
#define VIEW_TICKS_PER_PIXEL_MAX_DEFAULT_RESOLUTION   96
#define VIEW_TICKS_PER_PIXEL_EXP_BASE                 50

#define VIEW_MIN_STD_CELL_LENGTH_IN_PIXELS_AUTO_ZOOM          5
#define VIEW_MIN_STD_CELL_LENGTH_IN_PIXELS_MANUAL_ZOOM        3

#define VIEW_MIN_TRACK_HEIGHT_IN_PIXELS           43
#define VIEW_MIN_TRACK_HEIGHT_IN_NOTES            1
#define VIEW_MAX_TRACK_HEIGHT_IN_NOTES            MIDI_N_NOTE_NUMBERS

// -----------------------------------------------------------------------------------------------
// Mouse piano constants

extern const QColor MOUSEPIANO_OCTAVE_COLORS[MIDI_MAX_OCTAVE+1];
extern const int MOUSEPIANO_NOTE_INDEX_TO_WHITE_KEY_INDEX[12];
extern const int MOUSEPIANO_NOTE_INDEX_TO_BLACK_KEY_INDEX[12];
extern const int MOUSEPIANO_WHITE_KEY_INDEX_TO_NOTE_INDEX[ 7];

#define MOUSEPIANO_BLACK_KEY_RELATIVE_SIZE_X      0.7    // relative to size of a white key
#define MOUSEPIANO_BLACK_KEY_RELATIVE_SIZE_Y      0.65   // relative to size of a white key

#define MOUSEPIANO_MIN_YSIZE                        70
#define MOUSEPIANO_MAX_YSIZE                        140
#define MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE            12
#define MOUSEPIANO_WHITE_KEY_Y_TO_X_SIZE_FACTOR     0.3     // size ratio of a white key

#define MOUSEPIANO_MAX_WHITE_KEY  (10*7+5) // white key index corresponding to MIDI_INTERFACE_N_NOTE_NUMBERS-1

// -----------------------------------------------------------------------------------------------
// Serialization via XML

#define XML_CONFIG_CURRENT_VERSION 1

#define XML_TAG_SPEEDY_MIDI_CONFIG "speedy_midi_config"
#define XML_TAG_TRACK_CONFIG       "track_config"

#define XML_TAG_MEASURE_ITEM       "measure_item"

#define XML_TAG_REHEARSAL_MARKER   "rehearsal_marker"
#define XML_ATTR_COLOR             "color"

#define XML_TAG_PLAYBACK_OPTIONS   "playback_options"
#define XML_ATTR_SWING             "swing"

#define XML_ATTR_EVENT_COLOR       "event_color"

#define XML_TAG_EDITOR_STATE       "editor_state"
#define XML_TAG_MAPPING            "mapping"
#define XML_TAG_SELECTION          "selection"
#define XML_TAG_ANCHOR             "anchor"
#define XML_TAG_RANGE              "range"
#define XML_TAG_TRACK_STATES       "track_states"
#define XML_TAG_TRACK              "track"

#define XML_ATTR_VERSION                        "version"
#define XML_ATTR_FIRST_MEASURE                  "first_measure"
#define XML_ATTR_FIRST_TRACK                    "first_track"
#define XML_ATTR_CELL_LENGTH_DENOMINATOR        "cell_d"
#define XML_ATTR_CELL_LENGTH_TUPLET_NOMINATOR   "cell_tn"
#define XML_ATTR_CELL_LENGTH_TUPLET_DENOMINATOR "cell_td"
#define XML_ATTR_XZOOM                          "xzoom"
#define XML_ATTR_YZOOM                          "yzoom"
#define XML_ATTR_MODE                           "mode"
#define XML_VALUE_MEASURES                      "measures"
#define XML_VALUE_TRACKS                        "tracks"
#define XML_VALUE_CELLS                         "cells"
#define XML_ATTR_LEFT                           "left"
#define XML_ATTR_RIGHT                          "right"
#define XML_ATTR_TRACK                          "track"
#define XML_ATTR_TOP_TRACK                      "top_track"
#define XML_ATTR_BOTTOM_TRACK                   "bottom_track"
#define XML_ATTR_SOLO                           "solo"
#define XML_ATTR_MUTE                           "mute"
#define XML_ATTR_RECORD                         "record"
#define XML_ATTR_HEIGHT_IN_NOTES                "height_in_notes"
#define XML_ATTR_CENTER_NOTE                    "center_note"

// _____________________________________ Predeclare class names ______________________________________

// The application
class SpeedyMidiApp;
class Settings;

// Windows/widgets
class MainWindow;
namespace Ui { class MainWindow; }
class EditorScrollBar;
class ToolBarSpinBox;
class ZoomSliderWidget;
class MousePianoWidget;
class MousePianoDockWidget;
class View;
class EditorMapper;
class DisplayedMeasure;
class DisplayedCell;
class DisplayedTrack;

// Controller
class Controller;
class ControllerSubsystem;
class CS_Common;
class CS_File;
class CS_Navigation;
class CS_Clipboard;
class CS_LocalMassEdit;
class CS_Write;
class CS_Utilities;
class CS_Playback;

// Model/Document
class DocRoot;
class DocMeasureItem;
class DocTrack;
class DocEvent;
class TicksToMeasureResult;
class ConversionOptions;

// Commands
class EditorUndoCommand;

// Editor state
class WriteLength;
class EditorRange;
class EditorSelectionSupportPoint;
class EditorSelection;
class EditorTrackState;
class EditorState;
class VolatileEditorState;

// Standard Midi Files
class SmfDocument;
class SmfTrack;
class SmfEvent;
class SmfMidiEvent;
class SmfMetaEvent;
class SmfSysExEvent;
class SmfImporter;
class SmfExporter;
class SmfExporterMidiEvent;

// Dialogs
class InsertDialog;
class LaunchDialog;
class MeasurePropertiesDialog;
class PartExtractionDialog;
class PreferencesDialog;
class ConversionOptionsDialog;
class SwingifyDialog;
class TrackPropertiesDialog;
class TrackWizardDialog;
class WriteLengthDialog;

// Midi interface
class MidiInterface;
class MidiInterfaceThread;
class MidiShortMsg;

#endif // GLOBAL_H
