/***************************************************************************
 *  insertdialog.cpp - Dialog: Insert
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

#include "insertdialog.h"
#include "ui_insertdialog.h"
#include "doc_root.h"
#include "mainwindow.h"
#include "cs_localmassedit.h"

#include <QButtonGroup>

InsertDialog::InsertDialog(CS_LocalMassEdit* csLocalMassEdit) :
    QDialog(csLocalMassEdit->getMainWindow()),
    ui(new Ui::InsertDialog)
{
    ui->setupUi(this);
    this->csLocalMassEdit=csLocalMassEdit;
    docRoot=csLocalMassEdit->getDocRoot();

    noCellInsert=false;
    numberOfMeasuresToInsert=1;
    numberOfTracksToInsert=1;
    insertionType=IS_Cells;

    backupEditorState=csLocalMassEdit->getEditorState();

    buttonGroupInsertionType=new QButtonGroup(this);
    buttonGroupInsertionType->addButton(ui->radioButtonCells,0);
    buttonGroupInsertionType->addButton(ui->radioButtonMeasures,1);
    buttonGroupInsertionType->addButton(ui->radioButtonTracks,2);
    connect(buttonGroupInsertionType,SIGNAL(idClicked(int)),SLOT(buttonGroupInsertionTypeClicked(int)));
}

InsertDialog::~InsertDialog()
{
    delete ui;
}

void InsertDialog::changeEvent(QEvent *e)
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

int InsertDialog::exec()
{
    // ------------------------------------------------------------------------------------------
    // initialize dialog data

    updateData(false);
    enableAndDisable();
    setPreviewSelection();

    switch(insertionType)
    {
    case IS_Cells:      ui->radioButtonCells->setFocus();break;
    case IS_Measures:   ui->spinBoxHowManyMeasures->setFocus();break;
    case IS_Tracks:     ui->spinBoxHowManyTracks->setFocus();break;
    }

    // ------------------------------------------------------------------------------------------

    return QDialog::exec();
}

void InsertDialog::enableAndDisable()
{
    ui->radioButtonCells->setEnabled(!noCellInsert);
    ui->spinBoxHowManyMeasures->setEnabled(insertionType == IS_Measures);
    ui->spinBoxHowManyTracks->setEnabled(insertionType == IS_Tracks);
}

void InsertDialog::accept()
{
    // Do NOT restore editor state in accept(). The new selection will be needed
    //  for a correct insertion point in CS_LocalMassEdit::insert...()

    if(updateData())
        QDialog::accept();
}

void InsertDialog::reject()
{
    // Restore editor state
    csLocalMassEdit->applyStateAndUpdate(backupEditorState);
    QDialog::reject();
}

bool InsertDialog::updateData(bool saveAndValidate)
{
    if(saveAndValidate)
    {
        insertionType=(InsertionTypeSelection)buttonGroupInsertionType->checkedId();

        numberOfMeasuresToInsert = ui->spinBoxHowManyMeasures->value();
        numberOfTracksToInsert   = ui->spinBoxHowManyTracks  ->value();
    }
    else
    {
        buttonGroupInsertionType->button(insertionType)->setChecked(true);

        ui->spinBoxHowManyMeasures->setValue(numberOfMeasuresToInsert);
        ui->spinBoxHowManyTracks  ->setValue(numberOfTracksToInsert);
    }
    return true;
}

void InsertDialog::buttonGroupInsertionTypeClicked(int id)
{
    UNUSED(id);

    if(!updateData())return;
    enableAndDisable();

    setPreviewSelection();
    updateData(false);
}

void InsertDialog::setPreviewSelection()
{
    EditorState previewSelectionState=backupEditorState;

    switch(insertionType)
    {
    case IS_Cells:
        csLocalMassEdit->scrollRangeIntoView(previewSelectionState, previewSelectionState.selection);
        break;
    case IS_Measures:
        {
            int measureStartTicks=
                    docRoot->roundDownTicksToMeasureBorder(backupEditorState.selection.ticksLeft);

            // Change selection to global measure selection
            previewSelectionState.setGlobalMeasureSelection(
                    docRoot->ticksToMeasure(measureStartTicks).measureIndex, 1, docRoot);

            csLocalMassEdit->scrollRangeIntoView(previewSelectionState,
                    EditorRange(previewSelectionState.selection.ticksLeft,-1,
                                previewSelectionState.selection.ticksRight,-1));
        }
        break;
    case IS_Tracks:
        {
            if(docRoot->hasTracks())
            {
                // Change selection to global track selection
                previewSelectionState.setGlobalTrackSelection(backupEditorState.firstSelectedTrack(),
                                                              1,
                                                              docRoot);

                csLocalMassEdit->scrollRangeIntoView(previewSelectionState, previewSelectionState.selection);
            }
        }
        break;
    }

    csLocalMassEdit->applyStateAndUpdate(previewSelectionState);
}
