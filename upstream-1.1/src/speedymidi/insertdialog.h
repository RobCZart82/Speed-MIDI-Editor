/***************************************************************************
 *  insertdialog.h - Dialog: Insert
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

#ifndef INSERTDIALOG_H
#define INSERTDIALOG_H

#include "global.h"
#include <QtGui/QDialog>
#include "editorstate.h"

namespace Ui {
    class InsertDialog;
}

class InsertDialog : public QDialog {
    Q_OBJECT
public:
    InsertDialog(CS_LocalMassEdit* csLocalMassEdit);
    ~InsertDialog();
    int exec();

protected:
    void changeEvent(QEvent *e);

    virtual void accept();
    virtual void reject();
    void enableAndDisable();

    QButtonGroup* buttonGroupInsertionType;

    void setPreviewSelection();

    // Dialog data
    bool updateData(bool saveAndValidate = true);
public:
    bool noCellInsert;
    int numberOfMeasuresToInsert;
    int numberOfTracksToInsert;

    enum InsertionTypeSelection { IS_Cells,IS_Measures,IS_Tracks };
    InsertionTypeSelection insertionType;

protected slots:
    void buttonGroupInsertionTypeClicked(int id);

private:
    Ui::InsertDialog *ui;

    CS_LocalMassEdit* csLocalMassEdit;
    const DocRoot* docRoot;
    EditorState backupEditorState;
};

#endif // INSERTDIALOG_H
