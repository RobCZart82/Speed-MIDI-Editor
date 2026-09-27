/***************************************************************************
 *  speedymidiapp.h - Singleton Application Class
 *                    Owner of MIDI Interface, and Mouse Piano
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

#ifndef SPEEDYMIDIAPP_H
#define SPEEDYMIDIAPP_H

#include "global.h"
#include "qtsingleapplication/qtsingleapplication.h"

class SpeedyMidiApp : public QtSingleApplication
{
    Q_OBJECT
public:
    SpeedyMidiApp(int &argc, char **argv);

    int exec();

    void execLaunchDialog();
    MainWindow* createDocumentByWizard();
    void stopAnyPlayback();
    void execPreferencesDialog(bool startWithPageMidiDevices);
    void quit();

    // -----------------------------------------------------------------------------------------------
    // Mouse piano

    void reattachMousePiano();

    // -----------------------------------------------------------------------------------------------
    // Selectors

    QString getAppExecutablePath() const { return appExecutablePath; }
    Settings* getSettings() const { return settings; }

    QAction* getActionView_MousePiano() const { return actionView_MousePiano; }
    QAction* getActionView_Toolbar() const { return actionView_Toolbar; }
    QAction* getActionOptions_MousePianoPercussionMode() const { return actionOptions_MousePianoPercussionMode; }
    QAction* getActionOptions_MidiThru() const { return actionOptions_MidiThru; }

    MousePianoDockWidget* getMousePianoDockWidget() const { return mousePianoDockWidget; }
    MousePianoWidget* getMousePianoWidget() const { return mousePianoWidget; }

    MidiInterface* getMidiInterface() const { return midiInterface; }
    bool isMidiInputAvailable() const { return midiInputAvailable; }
    bool isMidiOutputAvailable() const { return midiOutputAvailable; }

    quint32 getKeypressSerialNo(int noteNumber) const
    {
        Q_ASSERT(noteNumber >= 0 && noteNumber < MIDI_N_NOTE_NUMBERS);
        return keypressSerialNumbersArray[noteNumber];
    }

public slots:
    void showToolBar(bool visible);

protected slots:
    void handleInstanceMessage(QString message);

    void midiKeyPressed(int noteNumber);
    void midiKeyReleased(int noteNumber);
    void mousePianoKeyPressed(int noteNumber);
    void mousePianoKeyReleased(int noteNumber);

    void showMousePiano(bool visible);
    void actionOptions_MousePianoPercussionMode_Triggered(bool checked);
    void actionOptions_MidiThru_Triggered(bool checked);

    void aboutToQuitCleanup();

protected:
    void setupTranslators();
    void setupAppGlobalUi();
    void retranslateAppGlobalUi();
    virtual bool event(QEvent *e);

    bool mainWindowExists() const;
    MainWindow* getActiveOrFallbackMainWindow() const;

    QString appExecutablePath;
    Settings* settings;

    QTranslator* messageTranslator;
    QTranslator* qtMessageTranslator;
    QTranslator* musicalTranslator;

    bool launchDialogDisplayed;

    QStringList filesToOpenList;
    QString filesToOpenMessage;

    // ------------------------------------------------------------
    // App-global UI elements

    QAction* actionView_MousePiano;
    QAction* actionView_Toolbar;
    QAction* actionOptions_MousePianoPercussionMode;
    QAction* actionOptions_MidiThru;

    MousePianoDockWidget* mousePianoDockWidget;
    MousePianoWidget* mousePianoWidget;

    // ------------------------------------------------------------
    // Midi interface
    MidiInterface* midiInterface;

    void initMidi();
    void openMidiInput();
    void openMidiOutput();

    bool midiInputAvailable;
    bool midiOutputAvailable;

    // MIDI key state arrays (do not use asynchronous state array of MidiInterface)
    bool midiKeyPressedArray[MIDI_N_NOTE_NUMBERS];            // MIDI input only
    quint32 keypressSerialNumbersArray[MIDI_N_NOTE_NUMBERS];  // MIDI input mixed with mouse piano
                                                              // 0 (=invalid serial number) indicates "not pressed"
    void updateMousePiano(int noteNumber);

    quint32 nextKeypressSerialNumber;
};

#endif // SPEEDYMIDIAPP_H
