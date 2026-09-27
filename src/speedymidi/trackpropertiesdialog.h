/***************************************************************************
 *  trackpropertiesdialog.h - Dialog: Track Attributes
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

#ifndef TRACKPROPERTIESDIALOG_H
#define TRACKPROPERTIESDIALOG_H

#include "global.h"
#include <QtWidgets/QDialog>
#include "editorstate.h"
#include "doc_track.h"

namespace Ui {
    class TrackPropertiesDialog;
}

class TrackPropertiesDialog : public QDialog {
    Q_OBJECT
public:
    TrackPropertiesDialog(CS_LocalMassEdit* csLocalMassEdit, int trackIndex);
    ~TrackPropertiesDialog();
    int exec();

protected:
    virtual void changeEvent(QEvent *e);
    virtual void paintEvent(QPaintEvent *e);

    virtual void accept();
    virtual void reject();
    void enableAndDisable();

    void applyTrackProperties(int trackIndex);
    void retrieveTrackProperties(int trackIndex);
    void setGlobalTrackSelection(int trackIndex);

    QButtonGroup* buttonGroupEventColor;

    // Dialog data
    bool updateData(bool saveAndValidate = true);
    int currentTrackIndex;
    DocTrack currentTrackProperties;

protected slots:
    void pushButtonMoveTrackUpClicked();
    void pushButtonMoveTrackDownClicked();
    void spinBoxTrackNumberValueChanged(int value);
    void buttonBoxButtonClicked(QAbstractButton* button);
    void spinBoxPanoramaValueChanged(int value);
    void sliderPanoramaValueChanged(int value);
    void pushButtonCenterClicked();

protected:
    bool firstPaintEvent;

    CS_LocalMassEdit* csLocalMassEdit;
    const DocRoot* docRoot;
    EditorState backupEditorState;
    bool restoreBackupEditorState;

private:
    Ui::TrackPropertiesDialog *ui;
};

#endif // TRACKPROPERTIESDIALOG_H
