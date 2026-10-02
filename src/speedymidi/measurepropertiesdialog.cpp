/***************************************************************************
 *  measurepropertiesdialog.cpp - Dialog: Measure Attributes
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

#include "measurepropertiesdialog.h"
#include "ui_measurepropertiesdialog.h"
#include "doc_root.h"
#include "mainwindow.h"
#include "cs_localmassedit.h"

#include <QButtonGroup>
#include <QMessageBox>
#include <QMenu>
#include <cmath>

MeasurePropertiesDialog::MeasurePropertiesDialog() :
    QDialog(NULL),
    ui(new Ui::MeasurePropertiesDialog)
{
    ui->setupUi(this);
    csLocalMassEdit=NULL;
    docRoot=NULL;

    documentSetupWizardMode=true;   // enable document setup wizard mode
    restoreBackupEditorState=false;

    currentMeasureIndex=0;          // behaviour like on first measure in piece

    init();

    // disable measure number spin box
    ui->spinBoxMeasureNumber->setEnabled(false);

    // remove apply button
    ui->buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    setWindowTitle(tr("First measure attributes"));
}

MeasurePropertiesDialog::MeasurePropertiesDialog(CS_LocalMassEdit* csLocalMassEdit, int measureIndex) :
    QDialog(csLocalMassEdit->getMainWindow()),
    ui(new Ui::MeasurePropertiesDialog)
{
    ui->setupUi(this);
    this->csLocalMassEdit=csLocalMassEdit;
    docRoot=csLocalMassEdit->getDocRoot();

    documentSetupWizardMode=false;
    restoreBackupEditorState=true;
    backupEditorState=csLocalMassEdit->getEditorState();

    currentMeasureIndex=measureIndex;

    // clamp measure index to maximum allowed value (.ui file contains the same max value too in 1-based form)
    if(currentMeasureIndex > CS_NAVIGATION_MAX_FIRST_MEASURE)
        currentMeasureIndex=CS_NAVIGATION_MAX_FIRST_MEASURE;

    init();
}

void MeasurePropertiesDialog::init()
{
    // ---------------------------------------------------------------------------------------------
    // Set minimum and maximum values defined by constants not available in .ui-file

    ui->spinBoxMeasureNumber->setMaximum(CS_NAVIGATION_MAX_FIRST_MEASURE + 1);
    ui->spinBoxTimeSignatureNominator->setMaximum(EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR);
    ui->spinBoxBPM->setMinimum(MIDI_MIN_BPM);
    ui->spinBoxBPM->setMaximum(MIDI_MAX_BPM);
    ui->spinBoxBPM->setDecimals(6);
    connect(ui->spinBoxBPM,qOverload<double>(&QDoubleSpinBox::valueChanged),this,
            [this](double) { tempoValueEdited=true; });

    // ---------------------------------------------------------------------------------------------

    firstPaintEvent=true;

    connect(ui->spinBoxMeasureNumber,SIGNAL(valueChanged(int)),SLOT(spinBoxMeasureNumberValueChanged(int)));

    connect(ui->groupBoxSetRehearsalMarker,SIGNAL(toggled(bool)),SLOT(groupBoxSetRehearsalMarkerToggled(bool)));
    connect(ui->groupBoxSetTimeSignature,SIGNAL(clicked(bool)),SLOT(groupBoxSetTimeSignatureClicked(bool)));
    connect(ui->groupBoxSetTimeSignature,SIGNAL(toggled(bool)),SLOT(groupBoxSetTimeSignatureToggled(bool)));
    connect(ui->groupBoxSetKeySignature,SIGNAL(clicked(bool)),SLOT(groupBoxSetKeySignatureClicked(bool)));
    connect(ui->groupBoxSetKeySignature,SIGNAL(toggled(bool)),SLOT(groupBoxSetKeySignatureToggled(bool)));
    connect(ui->groupBoxSetTempo,SIGNAL(clicked(bool)),SLOT(groupBoxSetTempoClicked(bool)));
    connect(ui->groupBoxSetTempo,SIGNAL(toggled(bool)),SLOT(groupBoxSetTempoToggled(bool)));
    connect(ui->groupBoxSetPlaybackOptions,SIGNAL(toggled(bool)),SLOT(groupBoxSetPlaybackOptionsToggled(bool)));

    connect(ui->buttonBox,SIGNAL(clicked(QAbstractButton*)),SLOT(buttonBoxButtonClicked(QAbstractButton*)));

    // -------------------------------------------------------------------------------------------------------
    // Define rehearsal marker templates (text and color)

    for(char c='A'; c <= 'Z'; ++c)
    {
        rehearsalMarkerTemplateList.append(
                new RehearsalMarkerTemplate(QString(c),
                                            DOCUMENT_MARKER_COLORS[ (c-'A') % DOCUMENT_N_MARKER_COLORS]));
    }

    int sectionTemplateStartIndex=rehearsalMarkerTemplateList.size();

    rehearsalMarkerTemplateList.append(new RehearsalMarkerTemplate(tr("Intro"), DOCUMENT_MARKER_COLORS[5]));
    rehearsalMarkerTemplateList.append(new RehearsalMarkerTemplate(tr("Verse"), DOCUMENT_MARKER_COLORS[3]));
    for(int i=1; i <= 10; ++i)
        rehearsalMarkerTemplateList.append(new RehearsalMarkerTemplate(
                QString(tr("Verse %1").arg(i)), DOCUMENT_MARKER_COLORS[3]));
    rehearsalMarkerTemplateList.append(new RehearsalMarkerTemplate(tr("Chorus"), DOCUMENT_MARKER_COLORS[2]));
    for(int i=1; i <= 4; ++i)
        rehearsalMarkerTemplateList.append(new RehearsalMarkerTemplate(
                QString(tr("Chorus %1").arg(i)), DOCUMENT_MARKER_COLORS[2]));
    rehearsalMarkerTemplateList.append(new RehearsalMarkerTemplate(tr("Bridge"), DOCUMENT_MARKER_COLORS[1]));
    for(int i=1; i <= 2; ++i)
        rehearsalMarkerTemplateList.append(new RehearsalMarkerTemplate(
                QString(tr("Bridge %1").arg(i)), DOCUMENT_MARKER_COLORS[1]));
    rehearsalMarkerTemplateList.append(new RehearsalMarkerTemplate(tr("Interlude"), DOCUMENT_MARKER_COLORS[6]));
    rehearsalMarkerTemplateList.append(new RehearsalMarkerTemplate(tr("Outro"), DOCUMENT_MARKER_COLORS[0]));
    rehearsalMarkerTemplateList.append(new RehearsalMarkerTemplate(tr("End"), DOCUMENT_MARKER_COLORS[7]));

    // Create tool button menus
    toolButtonMarkerLetterTemplateMenu=new QMenu(this);
    toolButtonMarkerSectionTemplateMenu=new QMenu(this);

    for(int i=0; i < rehearsalMarkerTemplateList.size(); ++i)
    {
        QPixmap pixmap(14,14);
        // draw the icon lighter than the real base color
        pixmap.fill(rehearsalMarkerTemplateList[i]->color.lighter(150));

        QAction* action=new QAction(QIcon(pixmap),rehearsalMarkerTemplateList[i]->text,this);
        action->setData(QVariant(i));   // store the index as user data

        if(i < sectionTemplateStartIndex) toolButtonMarkerLetterTemplateMenu->addAction(action);
        else toolButtonMarkerSectionTemplateMenu->addAction(action);

        connect(action,SIGNAL(triggered()),SLOT(menuRehearsalMarkerTemplateTriggered()));
    }
    ui->toolButtonMarkerLetterTemplate->setMenu(toolButtonMarkerLetterTemplateMenu);
    ui->toolButtonMarkerSectionTemplate->setMenu(toolButtonMarkerSectionTemplateMenu);

    // -------------------------------------------------------------------------------------------------------
    // Populate combo box "rehearsal marker color"
    for(int i=0; i < DOCUMENT_N_MARKER_COLORS; ++i)
    {
        QPixmap pixmap(12,12);
        // draw the icon lighter than the real base color
        pixmap.fill(DOCUMENT_MARKER_COLORS[i].lighter(150));

        ui->comboBoxMarkerColor->insertItem(i,QIcon(pixmap),"");
    }

    // -------------------------------------------------------------------------------------------------------
    // Populate time signature denominator combo box
    int denominator=1,index=0;
    while(denominator <= EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR)
    {
        ui->comboBoxTimeSignatureDenominator->insertItem(index,QString("%1").arg(denominator));
        ++index;
        denominator*=2;
    }

    // -------------------------------------------------------------------------------------------------------
    // Populate key signature combo boxes
    index=0;
    DocMeasureItem majorModeItem; majorModeItem.keySignatureScale=DocMeasureItem::KSS_Major;
    DocMeasureItem minorModeItem; minorModeItem.keySignatureScale=DocMeasureItem::KSS_Minor;

    for(int keySignature = -MIDI_MAX_KEY_SIGNATURE; keySignature <= MIDI_MAX_KEY_SIGNATURE; ++keySignature)
    {
        majorModeItem.keySignature=keySignature;
        minorModeItem.keySignature=keySignature;

        ui->comboBoxKeySignatureMajor->insertItem(index,majorModeItem.keySignatureName());
        ui->comboBoxKeySignatureMinor->insertItem(index,minorModeItem.keySignatureName());
        ++index;
    }

    buttonGroupKeySignatureScale=new QButtonGroup(this);
    buttonGroupKeySignatureScale->addButton(ui->radioButtonMajorMode, 0);
    buttonGroupKeySignatureScale->addButton(ui->radioButtonMinorMode, 1);
    connect(buttonGroupKeySignatureScale,SIGNAL(idClicked(int)),SLOT(buttonGroupKeySignatureScaleButtonClicked(int)));

    connect(ui->comboBoxKeySignatureMajor,SIGNAL(currentIndexChanged(int)),SLOT(comboBoxKeySignatureMajorIndexChanged(int)));
    connect(ui->comboBoxKeySignatureMinor,SIGNAL(currentIndexChanged(int)),SLOT(comboBoxKeySignatureMinorIndexChanged(int)));

    // -------------------------------------------------------------------------------------------------------
    // Playback options

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

    connect(ui->checkBoxSwing,SIGNAL(toggled(bool)),SLOT(checkBoxSwingToggled(bool)));
    connect(ui->comboBoxTimeSignatureDenominator,qOverload<int>(&QComboBox::currentIndexChanged),this,
            [this](int) {
        // Preserve a pending edit in the beat unit in which it was entered.
        if(tempoValueEdited)
        {
            DocMeasureItem edited(currentMeasureProperties);
            edited.setTempoBPM(ui->spinBoxBPM->value(),currentMeasureProperties.timeSignatureDenominator);
            if(edited.tempoMicrosecondsPerQuarter(currentMeasureProperties.timeSignatureDenominator) <= 0)return;
            currentMeasureProperties=edited;
            tempoValueEdited=false;
        }
        if(updateData())updateTempoField();
    });
}

MeasurePropertiesDialog::~MeasurePropertiesDialog()
{
    delete ui;

    for(int i=0; i < rehearsalMarkerTemplateList.size(); ++i)
        delete rehearsalMarkerTemplateList[i];
    rehearsalMarkerTemplateList.clear();
}

void MeasurePropertiesDialog::changeEvent(QEvent *e)
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

void MeasurePropertiesDialog::paintEvent(QPaintEvent *e)
{
    // Set the global measure selection in the background after dialog has been painted.
    //  This removes the feeling of sluggish window repaint.
    if(firstPaintEvent)
    {
        firstPaintEvent=false;
        if(!documentSetupWizardMode)setGlobalMeasureSelection(currentMeasureIndex);
    }
    QDialog::paintEvent(e);
}

int MeasurePropertiesDialog::exec()
{
    // ------------------------------------------------------------------------------------------
    // initialize dialog data

    if(documentSetupWizardMode)
    {
        // set default measure properties for setup wizard
        currentMeasureProperties.setFirstMeasureItemDefaults();
        previousMeasureProperties.invalidate();
    }
    else    // normal editor mode
    {
        retrieveMeasureProperties(currentMeasureIndex);
    }

    updateData(false);
    enableAndDisable();

    // ------------------------------------------------------------------------------------------

    return QDialog::exec();
}

void MeasurePropertiesDialog::enableAndDisable()
{
    // Enable correct key signature combo box depending on scale type
    ui->comboBoxKeySignatureMajor->setEnabled(currentMeasureProperties.setKeySignature &&
                                                currentMeasureProperties.keySignatureScale == DocMeasureItem::KSS_Major);
    ui->comboBoxKeySignatureMinor->setEnabled(currentMeasureProperties.setKeySignature &&
                                                currentMeasureProperties.keySignatureScale == DocMeasureItem::KSS_Minor);

    // enable the template buttons even if the corresponding group box is disabled
    ui->toolButtonMarkerLetterTemplate->setEnabled(true);
    ui->toolButtonMarkerSectionTemplate->setEnabled(true);
    ui->toolButtonSwingHardness->setEnabled(true);

    bool swingCheckBoxChecked=currentMeasureProperties.swingHardness > 0;
    ui->spinBoxSwingHardness->setEnabled(currentMeasureProperties.setPlaybackOptions &&
                                           swingCheckBoxChecked);
}

void MeasurePropertiesDialog::accept()
{
    if(updateData())
    {
        if(!documentSetupWizardMode)
        {
            if(applyMeasureProperties(currentMeasureIndex))
                QDialog::accept();
        }
        else QDialog::accept();
    }
}

void MeasurePropertiesDialog::reject()
{
    // Restore editor state if no OK, Apply, NextMeasure, or PrevMeasure took place
    if(!documentSetupWizardMode && restoreBackupEditorState)
        csLocalMassEdit->applyStateAndUpdate(backupEditorState);

    QDialog::reject();
}

void MeasurePropertiesDialog::buttonBoxButtonClicked(QAbstractButton* button)
{
    // Apply button
    if(ui->buttonBox->buttonRole(button) == QDialogButtonBox::ApplyRole)
    {
        Q_ASSERT(!documentSetupWizardMode); // not possible in setup wizard mode

        if(!updateData())return;
        applyMeasureProperties(currentMeasureIndex);

        // reload properties inherited from previous measure items
        retrieveMeasureProperties(currentMeasureIndex);
        updateData(false);
        enableAndDisable();
    }
}

bool MeasurePropertiesDialog::updateData(bool saveAndValidate)
{
    if(saveAndValidate)
    {
        currentMeasureIndex = ui->spinBoxMeasureNumber->value() - 1;

        currentMeasureProperties.setRehearsalMarker     = ui->groupBoxSetRehearsalMarker->isChecked();
        currentMeasureProperties.setTimeSignature       = ui->groupBoxSetTimeSignature->isChecked();
        currentMeasureProperties.setKeySignature        = ui->groupBoxSetKeySignature->isChecked();
        currentMeasureProperties.setTempo               = ui->groupBoxSetTempo->isChecked();
        currentMeasureProperties.setPlaybackOptions     = ui->groupBoxSetPlaybackOptions->isChecked();

        currentMeasureProperties.rehearsalMarkerText    = ui->lineEditMarkerText->text();
        if(ui->comboBoxMarkerColor->currentIndex() >= 0)
            currentMeasureProperties.rehearsalMarkerColor = DOCUMENT_MARKER_COLORS[ui->comboBoxMarkerColor->currentIndex()];

        currentMeasureProperties.timeSignatureNominator = ui->spinBoxTimeSignatureNominator->value();
        currentMeasureProperties.timeSignatureDenominator=1;
        for(int exponent=ui->comboBoxTimeSignatureDenominator->currentIndex(); exponent > 0; --exponent)
            currentMeasureProperties.timeSignatureDenominator*=2;

        currentMeasureProperties.keySignatureScale = (DocMeasureItem::KeySignatureScaleType)buttonGroupKeySignatureScale->checkedId();
        if(currentMeasureProperties.keySignatureScale == DocMeasureItem::KSS_Major)
        {
            currentMeasureProperties.keySignature       = ui->comboBoxKeySignatureMajor->currentIndex()
                                                          - MIDI_MAX_KEY_SIGNATURE;
        }
        else
        {
            currentMeasureProperties.keySignature       = ui->comboBoxKeySignatureMinor->currentIndex()
                                                          - MIDI_MAX_KEY_SIGNATURE;
        }

        // A displayed decimal is only a view of the exact MIDI tempo. Replacing
        // it on every Apply would round imported values during unrelated edits.
        if(tempoValueEdited)
        {
            DocMeasureItem edited(currentMeasureProperties);
            edited.setTempoBPM(ui->spinBoxBPM->value(),currentMeasureProperties.timeSignatureDenominator);
            if(edited.tempoMicrosecondsPerQuarter(currentMeasureProperties.timeSignatureDenominator) <= 0)
            {
                QMessageBox::warning(this,tr("Tempo"),tr("This tempo cannot be represented in a MIDI file."));
                return false;
            }
            currentMeasureProperties=edited;
            tempoValueEdited=false;
        }

        currentMeasureProperties.swingHardness          = ui->spinBoxSwingHardness->value();

        bool swingCheckBoxChecked=currentMeasureProperties.swingHardness > 0;
        ui->spinBoxSwingHardness->setMinimum(swingCheckBoxChecked ? DOCUMENT_MIN_SWING_HARDNESS : 0);
        ui->spinBoxSwingHardness->setMaximum(swingCheckBoxChecked ? DOCUMENT_MAX_SWING_HARDNESS : 0);
    }
    else
    {
        ui->spinBoxMeasureNumber->blockSignals(true);
        ui->spinBoxMeasureNumber->setValue(currentMeasureIndex + 1);
        ui->spinBoxMeasureNumber->blockSignals(false);

        ui->groupBoxSetRehearsalMarker->blockSignals(true);
        ui->groupBoxSetRehearsalMarker->setChecked(currentMeasureProperties.setRehearsalMarker);
        ui->groupBoxSetRehearsalMarker->blockSignals(false);
        ui->groupBoxSetTimeSignature->blockSignals(true);
        ui->groupBoxSetTimeSignature->setChecked(currentMeasureProperties.setTimeSignature);
        ui->groupBoxSetTimeSignature->blockSignals(false);
        ui->groupBoxSetKeySignature->blockSignals(true);
        ui->groupBoxSetKeySignature->setChecked(currentMeasureProperties.setKeySignature);
        ui->groupBoxSetKeySignature->blockSignals(false);
        ui->groupBoxSetTempo->blockSignals(true);
        ui->groupBoxSetTempo->setChecked(currentMeasureProperties.setTempo);
        ui->groupBoxSetTempo->blockSignals(false);
        ui->groupBoxSetPlaybackOptions->blockSignals(true);
        ui->groupBoxSetPlaybackOptions->setChecked(currentMeasureProperties.setPlaybackOptions);
        ui->groupBoxSetPlaybackOptions->blockSignals(false);

        ui->lineEditMarkerText->setText(currentMeasureProperties.rehearsalMarkerText);

        // special case markerColor: if the combo box is disabled, set it to an invalid selection
        //                           so there will be no strange gray icon
        if(currentMeasureProperties.setRehearsalMarker)
            ui->comboBoxMarkerColor->setCurrentIndex(currentMeasureProperties.getMarkerStdColorIndex());
        else
            ui->comboBoxMarkerColor->setCurrentIndex(-1);

        ui->spinBoxTimeSignatureNominator->setValue(currentMeasureProperties.timeSignatureNominator);
        int exponent=0;
        for(int d=currentMeasureProperties.timeSignatureDenominator; d > 1; d/=2)
            ++exponent;
        ui->comboBoxTimeSignatureDenominator->blockSignals(true);
        ui->comboBoxTimeSignatureDenominator->setCurrentIndex(exponent);
        ui->comboBoxTimeSignatureDenominator->blockSignals(false);

        buttonGroupKeySignatureScale->blockSignals(true);
        buttonGroupKeySignatureScale->button((int)currentMeasureProperties.keySignatureScale)->setChecked(true);
        buttonGroupKeySignatureScale->blockSignals(false);

        ui->comboBoxKeySignatureMajor->blockSignals(true);
        ui->comboBoxKeySignatureMajor->setCurrentIndex(
                currentMeasureProperties.keySignature + MIDI_MAX_KEY_SIGNATURE);
        ui->comboBoxKeySignatureMajor->blockSignals(false);

        ui->comboBoxKeySignatureMinor->blockSignals(true);
        ui->comboBoxKeySignatureMinor->setCurrentIndex(
                currentMeasureProperties.keySignature + MIDI_MAX_KEY_SIGNATURE);
        ui->comboBoxKeySignatureMinor->blockSignals(false);

        updateTempoField();

        bool swingCheckBoxChecked=currentMeasureProperties.swingHardness > 0;
        ui->spinBoxSwingHardness->setMinimum(swingCheckBoxChecked ? 10 : 0);
        ui->spinBoxSwingHardness->setMaximum(swingCheckBoxChecked ? DOCUMENT_MAX_SWING_HARDNESS : 0);
        ui->spinBoxSwingHardness->setValue(currentMeasureProperties.swingHardness);

        ui->checkBoxSwing->blockSignals(true);
        ui->checkBoxSwing->setChecked(swingCheckBoxChecked);
        ui->checkBoxSwing->blockSignals(false);
    }
    return true;
}

void MeasurePropertiesDialog::updateTempoField()
{
    const int denominator=currentMeasureProperties.timeSignatureDenominator;
    const double bpm=currentMeasureProperties.tempoBPM(denominator);
    const double minimum=std::ceil((15000000.0 * denominator / 0xffffff) * 1000000.0) / 1000000.0;
    const double maximum=15000000.0 * denominator;
    ui->spinBoxBPM->blockSignals(true);
    ui->spinBoxBPM->setRange(qMax(minimum,qMin(double(MIDI_MIN_BPM),bpm)),
                            qMin(maximum,qMax(double(MIDI_MAX_BPM),bpm)));
    ui->spinBoxBPM->setValue(bpm);
    ui->spinBoxBPM->blockSignals(false);
    tempoValueEdited=false;
}

void MeasurePropertiesDialog::spinBoxMeasureNumberValueChanged(int value)
{
    UNUSED(value);

    Q_ASSERT(!documentSetupWizardMode);

    int oldMeasureIndex=currentMeasureIndex;

    if(!updateData())return;    // updateData will also update currentMeasureIndex

    if(!applyMeasureProperties(oldMeasureIndex))
    {
        // User break, stay on current measure
        currentMeasureIndex=oldMeasureIndex;
        updateData(false);
        return;
    }

    setGlobalMeasureSelection(currentMeasureIndex);
    retrieveMeasureProperties(currentMeasureIndex);
    updateData(false);
    enableAndDisable();
}

void MeasurePropertiesDialog::groupBoxSetRehearsalMarkerToggled(bool on)
{
    UNUSED(on);

    if(!updateData())return;

    // if deselected, reset the values to inherited values from previous measure
    if(!currentMeasureProperties.setRehearsalMarker && currentMeasureIndex >= 1)
    {
        currentMeasureProperties.rehearsalMarkerText  = previousMeasureProperties.rehearsalMarkerText;
        currentMeasureProperties.rehearsalMarkerColor = previousMeasureProperties.rehearsalMarkerColor;
    }
    else
    {
        // if newly selected and no previous marker available, set default color
        if(!currentMeasureProperties.rehearsalMarkerColor.isValid())
            currentMeasureProperties.rehearsalMarkerColor=DOCUMENT_MARKER_COLORS[0];
    }

    updateData(false);
    enableAndDisable();
}

void MeasurePropertiesDialog::groupBoxSetTimeSignatureClicked(bool bChecked)
{
    UNUSED(bChecked);

    // In the first measure, the check cannot be removed
    if(currentMeasureIndex == 0)ui->groupBoxSetTimeSignature->setChecked(true);
}

void MeasurePropertiesDialog::groupBoxSetTimeSignatureToggled(bool on)
{
    UNUSED(on);

    if(!updateData())return;

    // if deselected, reset the values to inherited values from previous measure
    if(!currentMeasureProperties.setTimeSignature && currentMeasureIndex >= 1)
    {
        currentMeasureProperties.timeSignatureNominator   = previousMeasureProperties.timeSignatureNominator;
        currentMeasureProperties.timeSignatureDenominator = previousMeasureProperties.timeSignatureDenominator;
        currentMeasureProperties.midiClocksPerMetronomeClick = previousMeasureProperties.midiClocksPerMetronomeClick;
        currentMeasureProperties.notated32ndNotesPerQuarter = previousMeasureProperties.notated32ndNotesPerQuarter;
    }

    updateData(false);
    enableAndDisable();
}

void MeasurePropertiesDialog::groupBoxSetKeySignatureClicked(bool bChecked)
{
    UNUSED(bChecked);

    // In the first measure, the check cannot be removed
    if(currentMeasureIndex == 0)ui->groupBoxSetKeySignature->setChecked(true);
}

void MeasurePropertiesDialog::groupBoxSetKeySignatureToggled(bool on)
{
    UNUSED(on);

    if(!updateData())return;

    // if deselected, reset the values to inherited values from previous measure
    if(!currentMeasureProperties.setKeySignature && currentMeasureIndex >= 1)
    {
        currentMeasureProperties.keySignatureScale = previousMeasureProperties.keySignatureScale;
        currentMeasureProperties.keySignature      = previousMeasureProperties.keySignature;
    }

    updateData(false);
    enableAndDisable();
}

void MeasurePropertiesDialog::groupBoxSetTempoClicked(bool bChecked)
{
    UNUSED(bChecked);

    // In the first measure, the check cannot be removed
    if(currentMeasureIndex == 0)ui->groupBoxSetTempo->setChecked(true);
}

void MeasurePropertiesDialog::groupBoxSetTempoToggled(bool on)
{
    UNUSED(on);

    if(!updateData())return;

    // if deselected, reset the values to inherited values from previous measure
    if(!currentMeasureProperties.setTempo && currentMeasureIndex >= 1)
    {
        currentMeasureProperties.BPM = previousMeasureProperties.BPM;
        currentMeasureProperties.microsecondsPerQuarter = previousMeasureProperties.microsecondsPerQuarter;
        currentMeasureProperties.precedingTempoValues.clear();
    }

    updateData(false);
    enableAndDisable();
}

void MeasurePropertiesDialog::groupBoxSetPlaybackOptionsToggled(bool on)
{
    UNUSED(on);

    if(!updateData())return;

    // if deselected, reset the values to inherited values from previous measure
    if(!currentMeasureProperties.setPlaybackOptions && currentMeasureIndex >= 1)
    {
        currentMeasureProperties.swingHardness = previousMeasureProperties.swingHardness;
    }

    updateData(false);
    enableAndDisable();
}

void MeasurePropertiesDialog::menuRehearsalMarkerTemplateTriggered()
{
    if(!updateData())return;

    // get the action that sent this message, then find the corresponding index
    QAction* action=(QAction*)sender();
    int templateIndex=action->data().toInt();

    currentMeasureProperties.setRehearsalMarker = true;

    currentMeasureProperties.rehearsalMarkerText  = rehearsalMarkerTemplateList[templateIndex]->text;
    currentMeasureProperties.rehearsalMarkerColor = rehearsalMarkerTemplateList[templateIndex]->color;

    updateData(false);
    enableAndDisable();
}

void MeasurePropertiesDialog::comboBoxKeySignatureMajorIndexChanged(int index)
{
    UNUSED(index);

    if(!updateData())return;

    updateData(false);  // transfer selection also to minor key mode combo box
    enableAndDisable();
}

void MeasurePropertiesDialog::comboBoxKeySignatureMinorIndexChanged(int index)
{
    UNUSED(index);

    if(!updateData())return;

    updateData(false);  // transfer selection also to major key mode combo box
    enableAndDisable();
}

void MeasurePropertiesDialog::buttonGroupKeySignatureScaleButtonClicked(int id)
{
    UNUSED(id);

    if(!updateData())return;
    enableAndDisable();     // update enable status of the two key signature combo boxes
}

void MeasurePropertiesDialog::checkBoxSwingToggled(bool on)
{
    if(!updateData())return;

    if(on)
        currentMeasureProperties.swingHardness = 100;
    else
        currentMeasureProperties.swingHardness = 0;

    updateData(false);
    enableAndDisable();     // update enable status of swing hardness value spin box
}

void MeasurePropertiesDialog::menuSwingHardnessTriggered()
{
    if(!updateData())return;

    // get the action that sent this message, then get associated swing hardness value
    QAction* action=(QAction*)sender();
    int swingHardnessValue=action->data().toInt();

    currentMeasureProperties.setPlaybackOptions = true;
    currentMeasureProperties.swingHardness = swingHardnessValue;

    updateData(false);
    enableAndDisable();
}

bool MeasurePropertiesDialog::applyMeasureProperties(int measureIndex)
{
    Q_ASSERT(!documentSetupWizardMode);

    // If time signature changed such that the number of ticks per measure is modified,
    //  ask user if we should re-bar all events up to the next time signature set-flag.

    int measureStartTicks=docRoot->measureToTicks(measureIndex);
    int oldTicksPerMeasure=docRoot->ticksPerMeasure(docRoot->ticksToMeasure(measureStartTicks).measureProperties);
    int newTicksPerMeasure=docRoot->ticksPerMeasure(currentMeasureProperties);

    bool rebarEvents=false;

    if(newTicksPerMeasure != oldTicksPerMeasure)
    {
        switch(QMessageBox::question(this,
                                     tr("Time signature changed"),
                                     tr("Rebar all events up to next time signature change?"),
                                     QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,
                                     QMessageBox::Yes))
        {
        case QMessageBox::Cancel:return false;
        case QMessageBox::Yes:
            rebarEvents=true;
            break;
        default:
            break;
        }
    }

    // insert, modify or delete a measure item
    if(!csLocalMassEdit->setMeasureProperties(measureIndex,currentMeasureProperties,
                                             oldTicksPerMeasure,newTicksPerMeasure,rebarEvents))
    {
        QMessageBox::warning(this,tr("Time signature change too large"),
                             tr("This change would move events or the selection beyond the supported document length. Choose a smaller time signature or a shorter range."));
        return false;
    }
    restoreBackupEditorState=false;
    return true;
}

void MeasurePropertiesDialog::retrieveMeasureProperties(int measureIndex)
{
    Q_ASSERT(!documentSetupWizardMode);

    int measureStartTicks=docRoot->measureToTicks(measureIndex);
    currentMeasureProperties=docRoot->ticksToMeasure(measureStartTicks).measureProperties;

    // Copy inherited properties to another properties object.
    //   If we edit the first measure, invalidate the object
    if(measureIndex == 0)previousMeasureProperties.invalidate();
    else
    {
        // Tempo may have changed inside the preceding measure.
        previousMeasureProperties=docRoot->ticksToMeasure(measureStartTicks - 1).measureProperties;
    }
}

void MeasurePropertiesDialog::setGlobalMeasureSelection(int measureIndex)
{
    Q_ASSERT(!documentSetupWizardMode);

    // Change selection to global measure selection
    EditorState newState=csLocalMassEdit->getEditorState();
    newState.setGlobalMeasureSelection(measureIndex, 1, docRoot);

    csLocalMassEdit->scrollRangeIntoView(newState,
            EditorRange(newState.selection.ticksLeft,-1,newState.selection.ticksRight,-1));

    csLocalMassEdit->applyStateAndUpdate(newState);
}
