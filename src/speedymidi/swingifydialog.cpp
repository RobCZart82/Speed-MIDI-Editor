/***************************************************************************
 *  swingifydialog.cpp - Dialog: Add Swing
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

#include "swingifydialog.h"
#include "ui_swingifydialog.h"
#include <QButtonGroup>
#include <QMenu>

SwingifyDialog::SwingifyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SwingifyDialog)
{
    ui->setupUi(this);

    // ---------------------------------------------------------------------------------------------
    // Set minimum and maximum values defined by constants not available in .ui-file

    ui->spinBoxSwingHardness->setMinimum(DOCUMENT_MIN_SWING_HARDNESS);
    ui->spinBoxSwingHardness->setMaximum(DOCUMENT_MAX_SWING_HARDNESS);

    // ---------------------------------------------------------------------------------------------

    swingHardness=100;

    // -------------------------------------------------------------------------------------------------------
    // Define swing types for tool button menu

    toolButtonSwingHardnessMenu=new QMenu(this);

    for(int i=0; i < DOCUMENT_N_SWING_TYPES; ++i)
    {
        // store the swing stretch factor as user data in QAction object
        QAction* action=new QAction(getSwingTypeName(i),this);
        action->setData(QVariant(DOCUMENT_SWING_TYPES[i].swingHardness));

        toolButtonSwingHardnessMenu->addAction(action);
        connect(action,SIGNAL(triggered()),SLOT(menuSwingHardnessTriggered()));
    }
    ui->toolButtonSwingHardness->setMenu(toolButtonSwingHardnessMenu);
}

SwingifyDialog::~SwingifyDialog()
{
    delete ui;
}

void SwingifyDialog::changeEvent(QEvent *e)
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

int SwingifyDialog::exec()
{
    // ------------------------------------------------------------------------------------------
    // initialize dialog data

    updateData(false);
    enableAndDisable();

    // ------------------------------------------------------------------------------------------

    return QDialog::exec();
}

void SwingifyDialog::enableAndDisable()
{
}

void SwingifyDialog::accept()
{
    if(updateData())
        QDialog::accept();
}

void SwingifyDialog::reject()
{
    QDialog::reject();
}

bool SwingifyDialog::updateData(bool saveAndValidate)
{
    if(saveAndValidate)
    {
        swingHardness = ui->spinBoxSwingHardness->value();
    }
    else
    {
        ui->spinBoxSwingHardness->setValue(swingHardness);
    }
    return true;
}

void SwingifyDialog::menuSwingHardnessTriggered()
{
    if(!updateData())return;

    // get the action that sent this message, then get associated swing hardness value
    QAction* action=(QAction*)sender();
    swingHardness = action->data().toInt();

    updateData(false);
    enableAndDisable();
}
