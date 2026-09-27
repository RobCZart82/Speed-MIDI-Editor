/***************************************************************************
 *  conversionoptionsdialog.cpp - Dialog: 
 *                                Options when Saving Compatible MIDI File
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

#include "conversionoptionsdialog.h"
#include "ui_conversionoptionsdialog.h"
#include "mainwindow.h"
#include "doc_root.h"

ConversionOptionsDialog::ConversionOptionsDialog(QWidget *parent, MainWindow* mainWindow, const DocRoot* docRoot) :
    QDialog(parent),
    ui(new Ui::ConversionOptionsDialog)
{
    ui->setupUi(this);

    this->docRoot=docRoot;
    this->mainWindow=mainWindow;

    relativePlaybackSpeedInPercent = mainWindow->getSpinBoxRelativePlaybackSpeed()->value();
    swingPresent                   = docRoot->swingPresent();

    convertRelativePlaybackSpeed=false;
    convertSwing=false;
}

ConversionOptionsDialog::~ConversionOptionsDialog()
{
    delete ui;
}

void ConversionOptionsDialog::changeEvent(QEvent *e)
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

int ConversionOptionsDialog::exec()
{
    // ------------------------------------------------------------------------------------------
    // initialize dialog data

    enableAndDisable(); // first disable check boxes if conversion option is not applicable
    updateData(false);

    ui->checkBoxRelativePlaybackSpeed->setText(
            ui->checkBoxRelativePlaybackSpeed->text()
            + tr(" = %1%").arg(relativePlaybackSpeedInPercent));

    // ------------------------------------------------------------------------------------------

    return QDialog::exec();
}

void ConversionOptionsDialog::enableAndDisable()
{
    // disable check boxes if conversion option is not applicable
    ui->checkBoxRelativePlaybackSpeed->setEnabled(relativePlaybackSpeedInPercent != 100);
    ui->checkBoxSwing                ->setEnabled(swingPresent);
}

void ConversionOptionsDialog::accept()
{
    if(updateData())
        QDialog::accept();
}

void ConversionOptionsDialog::reject()
{
    QDialog::reject();
}

bool ConversionOptionsDialog::updateData(bool saveAndValidate)
{
    // transfer data only if items are enabled

    if(saveAndValidate)
    {
        if(ui->checkBoxRelativePlaybackSpeed->isEnabled())
            convertRelativePlaybackSpeed = ui->checkBoxRelativePlaybackSpeed->isChecked();

        if(ui->checkBoxSwing->isEnabled())
            convertSwing                 = ui->checkBoxSwing                ->isChecked();
    }
    else
    {
        if(ui->checkBoxRelativePlaybackSpeed->isEnabled())
            ui->checkBoxRelativePlaybackSpeed->setChecked(convertRelativePlaybackSpeed);

        if(ui->checkBoxSwing->isEnabled())
            ui->checkBoxSwing                ->setChecked(convertSwing);
    }
    return true;
}

