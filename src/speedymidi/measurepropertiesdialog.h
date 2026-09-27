/***************************************************************************
 *  measurepropertiesdialog.h - Dialog: Measure Attributes
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

#ifndef MEASUREPROPERTIESDIALOG_H
#define MEASUREPROPERTIESDIALOG_H

#include "global.h"
#include <QtWidgets/QDialog>
#include "editorstate.h"
#include "doc_measureitem.h"

namespace Ui {
    class MeasurePropertiesDialog;
}

class MeasurePropertiesDialog : public QDialog {
    Q_OBJECT
public:
    // constructor for document setup wizard
    MeasurePropertiesDialog();

    // constructor for normal editor
    MeasurePropertiesDialog(CS_LocalMassEdit* csLocalMassEdit, int measureIndex);

    ~MeasurePropertiesDialog();
    int exec();

    const DocMeasureItem& getSetupWizardMeasureProperties()
    {
        Q_ASSERT(documentSetupWizardMode);
        return currentMeasureProperties;
    }

protected:
    void init();

    virtual void changeEvent(QEvent *e);
    virtual void paintEvent(QPaintEvent *e);

    virtual void accept();
    virtual void reject();
    void enableAndDisable();

    bool applyMeasureProperties(int measureIndex);
    void retrieveMeasureProperties(int measureIndex);
    void setGlobalMeasureSelection(int measureIndex);

    class RehearsalMarkerTemplate
    {
    public:
        RehearsalMarkerTemplate(const QString& text, QColor color)
        {
            this->text=text;
            this->color=color;
        }
        QString text;
        QColor color;
    };
    QList<RehearsalMarkerTemplate*> rehearsalMarkerTemplateList;

    QButtonGroup* buttonGroupKeySignatureScale;

    // Dialog data
    bool updateData(bool saveAndValidate = true);
    int currentMeasureIndex;
    DocMeasureItem currentMeasureProperties;
    DocMeasureItem previousMeasureProperties;

    bool documentSetupWizardMode;

protected slots:
    void spinBoxMeasureNumberValueChanged(int value);
    void groupBoxSetRehearsalMarkerToggled(bool on);
    void groupBoxSetTimeSignatureClicked(bool bChecked);
    void groupBoxSetTimeSignatureToggled(bool on);
    void groupBoxSetKeySignatureClicked(bool bChecked);
    void groupBoxSetKeySignatureToggled(bool on);
    void groupBoxSetTempoClicked(bool bChecked);
    void groupBoxSetTempoToggled(bool on);
    void groupBoxSetPlaybackOptionsToggled(bool on);
    void menuRehearsalMarkerTemplateTriggered();
    void comboBoxKeySignatureMajorIndexChanged(int index);
    void comboBoxKeySignatureMinorIndexChanged(int index);
    void buttonGroupKeySignatureScaleButtonClicked(int id);
    void checkBoxSwingToggled(bool on);
    void menuSwingHardnessTriggered();
    void buttonBoxButtonClicked(QAbstractButton* button);
private:
    Ui::MeasurePropertiesDialog *ui;
    bool firstPaintEvent;

    CS_LocalMassEdit* csLocalMassEdit;
    const DocRoot* docRoot;

    EditorState backupEditorState;
    bool restoreBackupEditorState;

    QMenu* toolButtonMarkerLetterTemplateMenu;
    QMenu* toolButtonMarkerSectionTemplateMenu;
    QMenu* toolButtonSwingHardnessMenu;
};

#endif // MEASUREPROPERTIESDIALOG_H
