/***************************************************************************
 *  writelengthdialog.cpp - Dialog: Cell Length
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

#include "writelengthdialog.h"
#include "ui_writelengthdialog.h"
#include <QButtonGroup>
#include <QMessageBox>

WriteLengthDialog::WriteLengthDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WriteLengthDialog)
{
    ui->setupUi(this);

    buttonGroupBaseLength=new QButtonGroup(this);
    buttonGroupBaseLength->addButton(ui->pushButtonNote1,0);
    buttonGroupBaseLength->addButton(ui->pushButtonNote2,1);
    buttonGroupBaseLength->addButton(ui->pushButtonNote4,2);
    buttonGroupBaseLength->addButton(ui->pushButtonNote8,3);
    buttonGroupBaseLength->addButton(ui->pushButtonNote16,4);
    buttonGroupBaseLength->addButton(ui->pushButtonNote32,5);
    buttonGroupBaseLength->addButton(ui->pushButtonNote64,6);
    buttonGroupBaseLength->addButton(ui->pushButtonNote128,7);
    connect(buttonGroupBaseLength,SIGNAL(buttonClicked(int)),SLOT(buttonGroupBaseLengthButtonClicked(int)));

    buttonGroupTuplet=new QButtonGroup(this);
    buttonGroupTuplet->addButton(ui->radioButtonNoTuplet,TS_None);
    buttonGroupTuplet->addButton(ui->radioButtonDuplet,TS_Duplet);
    buttonGroupTuplet->addButton(ui->radioButtonTriplet,TS_Triplet);
    buttonGroupTuplet->addButton(ui->radioButtonOtherTuplet,TS_Other);
    connect(buttonGroupTuplet,SIGNAL(buttonClicked(int)),SLOT(buttonGroupTupletButtonClicked(int)));
}

int WriteLengthDialog::exec()
{
    // ------------------------------------------------------------------------------------------
    // initialize dialog data

    updateData(false);

    if(setFocusToOtherTuplet)
    {
        buttonGroupTuplet->button(TS_Other)->setChecked(true);
        ui->spinBoxDenominator->setFocus();
    }
    else
    {
        int checkId=TS_None;
        if(writeLength.tupletDenominator == 1 && writeLength.tupletNominator == 1)
            checkId=TS_None;
        else if(writeLength.tupletDenominator == 2 && writeLength.tupletNominator == 3)
            checkId=TS_Duplet;
        else if(writeLength.tupletDenominator == 3 && writeLength.tupletNominator == 2)
            checkId=TS_Triplet;
        else
            checkId=TS_Other;

        buttonGroupTuplet->button(checkId)->setChecked(true);
        buttonGroupTuplet->button(checkId)->setFocus();
    }

    enableAndDisable();

    // ------------------------------------------------------------------------------------------

    return QDialog::exec();
}

WriteLengthDialog::~WriteLengthDialog()
{
    delete ui;
}

void WriteLengthDialog::changeEvent(QEvent *e)
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

void WriteLengthDialog::enableAndDisable()
{
    ui->spinBoxDenominator->setEnabled(buttonGroupTuplet->checkedId() == TS_Other);
    ui->spinBoxNominator->setEnabled(buttonGroupTuplet->checkedId() == TS_Other);
}

void WriteLengthDialog::accept()
{
    if(updateData())
    {
        if(writeLength.tupletDenominator != 1 || writeLength.tupletNominator != 1)
        {
            // Check tuplet settings

            // Reduce the fraction tupletNominator/tupletDenominator
            int gcd=greatestCommonDivisor(writeLength.tupletNominator,writeLength.tupletDenominator);
            writeLength.tupletDenominator/=gcd;
            writeLength.tupletNominator/=gcd;

            // Do not allow de-/nominator to be 1
            if(writeLength.tupletDenominator == 1 || writeLength.tupletNominator == 1)
            {
                updateData(false);

                QMessageBox::warning(this, tr("Error"), tr("Tuplet parameters must be coprime."));

                if(writeLength.tupletDenominator == 1)
                    ui->spinBoxDenominator->setFocus();
                else
                    ui->spinBoxNominator->setFocus();

                return;
            }
        }

        // Finally accept
        QDialog::accept();
    }
}

int WriteLengthDialog::greatestCommonDivisor(int a, int b)
{
    // Use the euclidean algorithm.
    while(a != b)
    {
        if(a < b)b-=a;
        else a-=b;
    }
    return a;
}

bool WriteLengthDialog::updateData(bool saveAndValidate)
{
    if(saveAndValidate)
    {
        writeLength.denominator=1;
        for(int i=0; i < buttonGroupBaseLength->checkedId(); ++i)
            writeLength.denominator*=2;

        writeLength.tupletDenominator=ui->spinBoxDenominator->value();
        writeLength.tupletNominator=ui->spinBoxNominator->value();
    }
    else
    {
        int checkId=0;
        int i=writeLength.denominator;
        while(i > 1)
        {
            ++checkId;
            i/=2;
        }
        buttonGroupBaseLength->button(checkId)->setChecked(true);

        ui->spinBoxDenominator->setValue(writeLength.tupletDenominator);
        ui->spinBoxNominator->setValue(writeLength.tupletNominator);
    }
    return true;
}

void WriteLengthDialog::buttonGroupBaseLengthButtonClicked(int id)
{
    UNUSED(id);

    if(!updateData())return;
}

void WriteLengthDialog::buttonGroupTupletButtonClicked(int id)
{
    if(!updateData())return;
    enableAndDisable();

    switch(id)
    {
    case TS_None: // no tuplet
        writeLength.tupletDenominator=1;
        writeLength.tupletNominator=1;
        break;
    case TS_Duplet:
        writeLength.tupletDenominator=2;
        writeLength.tupletNominator=3;
        break;
    case TS_Triplet:
        writeLength.tupletDenominator=3;
        writeLength.tupletNominator=2;
        break;
    case TS_Other:
        ui->spinBoxDenominator->setFocus();
        break;
    }

    updateData(false);
}
