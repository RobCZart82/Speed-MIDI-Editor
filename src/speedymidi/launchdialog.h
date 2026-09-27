/***************************************************************************
 *  launchdialog.h - Dialog: Launch Dialog
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

#ifndef LAUNCHDIALOG_H
#define LAUNCHDIALOG_H

#include "global.h"
#include <QtWidgets/QDialog>

namespace Ui {
    class LaunchDialog;
}

class LaunchDialog : public QDialog {
    Q_OBJECT
public:
    LaunchDialog(QWidget *parent = 0);
    ~LaunchDialog();

    int exec();

protected slots:
    void pushButtonCreateNewWithTrackWizardClicked();
    void pushButtonCreateNewDefaultDocumentClicked();
    void pushButtonOpenMidiFileClicked();
    void pushButtonOpenRecentFileClicked();
    void listWidgetRecentFiles_ItemActivated(QListWidgetItem* item);

protected:
    void changeEvent(QEvent *e);

    virtual void accept();
    virtual void reject();
    void enableAndDisable();

    SpeedyMidiApp* getApp() const { return (SpeedyMidiApp*)qApp; }

    // Dialog data
    bool updateData(bool saveAndValidate = true);
public:
    enum LaunchDialogResultType { LDR_NewDocumentWithWizard,
                                  LDR_NewDefaultDocument,
                                  LDR_OpenDocument,
                                  LDR_Cancel } result;
    QString openFilePath;

private:
    Ui::LaunchDialog *ui;
};

#endif // LAUNCHDIALOG_H
