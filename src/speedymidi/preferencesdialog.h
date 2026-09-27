/***************************************************************************
 *  preferencesdialog.h - Dialog: Preferences
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

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include "global.h"
#include <QtWidgets/QDialog>
#include "settings.h"

namespace Ui {
    class PreferencesDialog;
}

class QListWidget;
class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    PreferencesDialog(QWidget *parent = 0);
    ~PreferencesDialog();
    int exec(bool startWithPageMidiDevices);

    // Dialog data
    bool updateData(bool saveAndValidate = true);

    // Page 1: Language
    QString messageTranslationLocaleName;
    QString musicalTranslationLocaleName;

    // Page 2: MIDI devices
    QString selectedInputDevice;    // empty string if "none" was selected
    QString selectedOutputDevice;   // empty string if "none" was selected

    // Page 3: Playback options
    bool scrollingPlayback;
    Settings::PlaybackStartPositionType playbackStartPosition;
    bool checkForChannelCollisions;
    bool checkFileCompatibility;

    // Page 4: Note entry
    int newNoteMidiVelocity;

    // Page 5: Mouse piano
    int mousePianoMidiChannelChromatic;
    int mousePianoMidiChannelPercussion;
    int mousePianoMidiVelocity;

protected:
    void changeEvent(QEvent *e);
    virtual void accept();
    virtual void reject();

    void enableAndDisable();
    int listWidgetSelectedRow(QListWidget* listWidget);
    void listWidgetSelectText(QListWidget* listWidget, const QString& text, Qt::MatchFlags flags = Qt::MatchExactly);

    QButtonGroup* buttonGroupPlaybackStartPosition;

    QStringList inputDeviceList;
    QStringList outputDeviceList;

    QStringList messageLocaleNameList;
    QStringList musicalLocaleNameList;

    QStringList messageLanguageCountryList;
    QStringList musicalLanguageCountryList;

protected slots:
    void listWidgetPageSelectorSelectionChanged();

private:
    Ui::PreferencesDialog *ui;
};

#endif // PREFERENCESDIALOG_H
