/***************************************************************************
 *  trackpropertiesdialog.cpp - Dialog: Track Attributes
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

#include "trackpropertiesdialog.h"
#include "ui_trackpropertiesdialog.h"
#include "cs_localmassedit.h"
#include "mainwindow.h"
#include "doc_root.h"
#include <QButtonGroup>
#include <QMessageBox>

TrackPropertiesDialog::TrackPropertiesDialog(CS_LocalMassEdit* csLocalMassEdit, int trackIndex)
    : QDialog(csLocalMassEdit->getMainWindow()), ui(new Ui::TrackPropertiesDialog)
{
    ui->setupUi(this);
    firstPaintEvent=true;

    this->csLocalMassEdit=csLocalMassEdit;
    docRoot=csLocalMassEdit->getDocRoot();

    backupEditorState=csLocalMassEdit->getEditorState();
    restoreBackupEditorState=true;

    // If trackIndex == docRoot->trackList.size() then the user clicked the append-track button
    currentTrackIndex=trackIndex;

    connect(ui->spinBoxTrackNumber,SIGNAL(valueChanged(int)),SLOT(spinBoxTrackNumberValueChanged(int)));

    buttonGroupEventColor=new QButtonGroup(this);
    buttonGroupEventColor->addButton(ui->colorButton01, 0);
    buttonGroupEventColor->addButton(ui->colorButton02, 1);
    buttonGroupEventColor->addButton(ui->colorButton03, 2);
    buttonGroupEventColor->addButton(ui->colorButton04, 3);
    buttonGroupEventColor->addButton(ui->colorButton05, 4);
    buttonGroupEventColor->addButton(ui->colorButton06, 5);
    buttonGroupEventColor->addButton(ui->colorButton07, 6);
    buttonGroupEventColor->addButton(ui->colorButton08, 7);
    buttonGroupEventColor->addButton(ui->colorButton09, 8);
    buttonGroupEventColor->addButton(ui->colorButton10, 9);
    buttonGroupEventColor->addButton(ui->colorButton11,10);
    buttonGroupEventColor->addButton(ui->colorButton12,11);
    buttonGroupEventColor->addButton(ui->colorButton13,12);
    buttonGroupEventColor->addButton(ui->colorButton14,13);
    buttonGroupEventColor->addButton(ui->colorButton15,14);
    buttonGroupEventColor->addButton(ui->colorButton16,15);

    connect(ui->pushButtonMoveTrackUp,SIGNAL(clicked()),SLOT(pushButtonMoveTrackUpClicked()));
    connect(ui->pushButtonMoveTrackDown,SIGNAL(clicked()),SLOT(pushButtonMoveTrackDownClicked()));

    connect(ui->buttonBox,SIGNAL(clicked(QAbstractButton*)),SLOT(buttonBoxButtonClicked(QAbstractButton*)));
    connect(ui->spinBoxPanorama,SIGNAL(valueChanged(int)),SLOT(spinBoxPanoramaValueChanged(int)));
    connect(ui->sliderPanorama,SIGNAL(valueChanged(int)),SLOT(sliderPanoramaValueChanged(int)));
    connect(ui->pushButtonCenter,SIGNAL(clicked()),SLOT(pushButtonCenterClicked()));

    // Populate patch name combo box. Patch index is 1-based.
    QStringList patchNameList;
    for(int i=0; i < MIDI_N_PATCH_NAMES; ++i)
        patchNameList << QString("%1: %2").arg(i+1,-3).arg(MIDI_PATCH_NAME[i]);
    ui->comboBoxPatch->insertItems(0,patchNameList);

    // Create icons for color buttons
    for(int i=0; i < DOCUMENT_N_EVENT_COLORS; ++i)
    {
        // increase the contrast of the displayed color as the screen colors are relatively bright
        QColor pixmapFillColor=DOCUMENT_EVENT_COLORS[i];
        pixmapFillColor.setRed  (4 * qMax(pixmapFillColor.red  () - 0x40, 0) / 3);
        pixmapFillColor.setGreen(4 * qMax(pixmapFillColor.green() - 0x40, 0) / 3);
        pixmapFillColor.setBlue (4 * qMax(pixmapFillColor.blue () - 0x40, 0) / 3);

        QPixmap pixmap(14,14);
        pixmap.fill(pixmapFillColor);

        QToolButton* button=static_cast<QToolButton*>(buttonGroupEventColor->button(i));
        button->setIcon(QIcon(pixmap));
    }
}

TrackPropertiesDialog::~TrackPropertiesDialog()
{
    delete ui;
}

void TrackPropertiesDialog::changeEvent(QEvent *e)
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

void TrackPropertiesDialog::paintEvent(QPaintEvent *e)
{
    // Set the global track selection in the background after dialog has been painted.
    //  This removes the feeling of sluggish window repaint.
    if(firstPaintEvent)
    {
        firstPaintEvent=false;
        setGlobalTrackSelection(currentTrackIndex);
    }
    QDialog::paintEvent(e);
}

int TrackPropertiesDialog::exec()
{
    // ------------------------------------------------------------------------------------------
    // initialize dialog data

    retrieveTrackProperties(currentTrackIndex);
    updateData(false);
    enableAndDisable();

    // ------------------------------------------------------------------------------------------

    return QDialog::exec();
}

void TrackPropertiesDialog::enableAndDisable()
{
    ui->pushButtonMoveTrackUp->setEnabled(currentTrackIndex > 0);
    ui->pushButtonMoveTrackDown->setEnabled(currentTrackIndex < docRoot->trackList.size() - 1);
}

void TrackPropertiesDialog::accept()
{
    if(updateData())
    {
        applyTrackProperties(currentTrackIndex);
        QDialog::accept();
    }
}

void TrackPropertiesDialog::reject()
{
    // Restore editor state if no OK, Apply, NextTrack, PrevTrack, MoveTrackUp, or MoveTrackDown took place
    if(restoreBackupEditorState)csLocalMassEdit->applyStateAndUpdate(backupEditorState);

    QDialog::reject();
}

void TrackPropertiesDialog::buttonBoxButtonClicked(QAbstractButton* button)
{
    // Apply button
    if(ui->buttonBox->buttonRole(button) == QDialogButtonBox::ApplyRole)
    {
        if(!updateData())return;
        applyTrackProperties(currentTrackIndex);
    }
}

void TrackPropertiesDialog::spinBoxPanoramaValueChanged(int value)
{
    // synchronize panorama spin box and panorama scroll bar
    ui->sliderPanorama->setValue(value);
}

void TrackPropertiesDialog::sliderPanoramaValueChanged(int value)
{
    // synchronize panorama spin box and panorama scroll bar
    ui->spinBoxPanorama->setValue(value);
}

void TrackPropertiesDialog::pushButtonCenterClicked()
{
    if(!updateData())return;

    currentTrackProperties.midiPanorama=MIDI_PANORAMA_CENTER;

    updateData(false);
}

bool TrackPropertiesDialog::updateData(bool saveAndValidate)
{
    if(saveAndValidate)
    {
        currentTrackIndex = ui->spinBoxTrackNumber->value() - 1;

        currentTrackProperties.name            =   ui->lineEditTrackName->text();

        currentTrackProperties.midiVolume      =   ui->spinBoxVolume->value();
        currentTrackProperties.midiPanorama    =   ui->spinBoxPanorama->value();
        currentTrackProperties.midiChannel     =   ui->spinBoxChannel->value();
        currentTrackProperties.midiPatch       =   ui->comboBoxPatch->currentIndex() + 1;

        // Handle color buttons
        currentTrackProperties.eventColor      =   DOCUMENT_EVENT_COLORS[buttonGroupEventColor->checkedId()];
    }
    else
    {
        ui->spinBoxTrackNumber->blockSignals(true);
        ui->spinBoxTrackNumber->setValue(currentTrackIndex + 1);
        ui->spinBoxTrackNumber->blockSignals(false);

        ui->lineEditTrackName->setText(currentTrackProperties.name);

        ui->spinBoxVolume->setValue(currentTrackProperties.midiVolume);
        ui->spinBoxPanorama->setValue(currentTrackProperties.midiPanorama);
        ui->sliderPanorama->setValue(currentTrackProperties.midiPanorama);
        ui->spinBoxChannel->setValue(currentTrackProperties.midiChannel);
        ui->comboBoxPatch->setCurrentIndex(currentTrackProperties.midiPatch - 1);

        // Handle color buttons
        int stdColorIndex=currentTrackProperties.getEventColorStandardIndex();
        if(stdColorIndex == -1)
        {
            buttonGroupEventColor->button(0)->setChecked(true);
            buttonGroupEventColor->button(0)->setChecked(false);    // disable all buttons
        }
        else
        {
            // enable correct button
            buttonGroupEventColor->button(stdColorIndex)->setChecked(true);
        }

        // The range of the spin box is one more than the current number of tracks (displayed value is 1-based).
        //  This allows to append a new track. The +2 ensures that when editing a to-append-track we can click
        //  the up button to append even a further track.
        ui->spinBoxTrackNumber->setMaximum( qMax(docRoot->trackList.size() + 1, currentTrackIndex + 2));
    }
    return true;
}

void TrackPropertiesDialog::pushButtonMoveTrackUpClicked()
{
    if(!updateData())return;
    applyTrackProperties(currentTrackIndex);

    csLocalMassEdit->moveTrack(currentTrackIndex,currentTrackIndex-1);
    --currentTrackIndex;                // follow the moved track

    setGlobalTrackSelection(currentTrackIndex);
    retrieveTrackProperties(currentTrackIndex);
    updateData(false);
    enableAndDisable();
}

void TrackPropertiesDialog::pushButtonMoveTrackDownClicked()
{
    if(!updateData())return;
    applyTrackProperties(currentTrackIndex);

    csLocalMassEdit->moveTrack(currentTrackIndex,currentTrackIndex+1);
    ++currentTrackIndex;    // follow the moved track
    
    setGlobalTrackSelection(currentTrackIndex);
    retrieveTrackProperties(currentTrackIndex);
    updateData(false);
    enableAndDisable();
}

void TrackPropertiesDialog::spinBoxTrackNumberValueChanged(int value)
{
    UNUSED(value);

    int oldTrackIndex=currentTrackIndex;

    if(!updateData())return;    // updateData will also update currentTrackIndex
    
    applyTrackProperties(oldTrackIndex);

    // Does the user want to append a track?
    if(currentTrackIndex == docRoot->trackList.size())
    {
        // select last track during message box execution
        setGlobalTrackSelection(docRoot->trackList.size() - 1);

        if(QMessageBox::question(this, tr("Append track"), tr("Append a new track?"),
                                 QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes) == QMessageBox::No)
        {
            // No, stay on current track.
            currentTrackIndex=oldTrackIndex;
            updateData(false);
            return;
        }
    }

    setGlobalTrackSelection(currentTrackIndex);
    retrieveTrackProperties(currentTrackIndex);
    updateData(false);
    enableAndDisable();
}

void TrackPropertiesDialog::applyTrackProperties(int trackIndex)
{
    restoreBackupEditorState=false;

    if(trackIndex == docRoot->trackList.size())
    {
        // append a new track with the entered properties
        csLocalMassEdit->insertTrack(trackIndex,new DocTrack(currentTrackProperties),true);
    }
    else
    {
        // modify an existing track
        csLocalMassEdit->modifyTrack(trackIndex,currentTrackProperties);
    }
}

void TrackPropertiesDialog::retrieveTrackProperties(int trackIndex)
{
    if(trackIndex < docRoot->trackList.size())
    {
        currentTrackProperties=*docRoot->trackList[trackIndex];
    }
    else
    {
        // user clicked the append button or added a new track by using the spin box
        currentTrackProperties.setDefaultProperties(docRoot);
    }
}

void TrackPropertiesDialog::setGlobalTrackSelection(int trackIndex)
{
    // when appending, do not change current selection
    if(trackIndex == docRoot->trackList.size())return;

    // Change selection to global track selection
    EditorState newState=csLocalMassEdit->getEditorState();
    newState.setGlobalTrackSelection(trackIndex, 1, docRoot);

    csLocalMassEdit->scrollRangeIntoView(newState, newState.selection);
    csLocalMassEdit->applyStateAndUpdate(newState);
}
