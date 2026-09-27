/***************************************************************************
 *  trackwizarddialog.cpp - Dialog: Wizard for New Tracks
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

#include "trackwizarddialog.h"
#include "ui_trackwizarddialog.h"
#include "doc_root.h"

#include <QMessageBox>

TrackWizardDialog::TrackWizardDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TrackWizardDialog)
{
    ui->setupUi(this);

    assignPatches=false;
}

TrackWizardDialog::~TrackWizardDialog()
{
    delete ui;
}

void TrackWizardDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

int TrackWizardDialog::exec()
{
    // ------------------------------------------------------------------------------------------
    // initialize dialog data

    updateData(false);
    enableAndDisable();

    // ------------------------------------------------------------------------------------------

    return QDialog::exec();
}

void TrackWizardDialog::enableAndDisable()
{
}

void TrackWizardDialog::accept()
{
    if(updateData())QDialog::accept();
}

void TrackWizardDialog::reject()
{
    QDialog::reject();
}

bool TrackWizardDialog::updateData(bool saveAndValidate)
{
    if(saveAndValidate)
    {
        tracksToAdd   = ui->lineEditTracksToAdd->text();

        // validate string
        if(!DocRoot::isValidTrackWizardString(tracksToAdd))
        {
            QMessageBox::warning(this,
                                 tr("Track Wizard"),
                                 tr("Invalid track types specified.\n\n"
                                    "Please refer to given track types and examples."));
            return false;
        }

        assignPatches = ui->checkBoxAssignPatches->isChecked();
    }
    else
    {
        ui->lineEditTracksToAdd->setText(tracksToAdd);
        ui->checkBoxAssignPatches->setChecked(assignPatches);
    }
    return true;
}
