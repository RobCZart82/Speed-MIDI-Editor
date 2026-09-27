/***************************************************************************
 *  writelengthdialog.h - Dialog: Cell Length
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

#ifndef WRITELENGTHDIALOG_H
#define WRITELENGTHDIALOG_H

#include "global.h"
#include <QtGui/QDialog>
#include "editorstate.h"

namespace Ui {
    class WriteLengthDialog;
}

class QButtonGroup;

class WriteLengthDialog : public QDialog {
    Q_OBJECT
public:
    WriteLengthDialog(QWidget *parent = 0);
    ~WriteLengthDialog();
    int exec();
    static int greatestCommonDivisor(int a, int b);

protected:
    void changeEvent(QEvent *e);

    virtual void accept();
    void enableAndDisable();

    QButtonGroup* buttonGroupBaseLength;
    QButtonGroup* buttonGroupTuplet;
    enum TupletSelection { TS_None,TS_Duplet,TS_Triplet,TS_Other };

    // Dialog data
    bool updateData(bool saveAndValidate = true);
public:
    WriteLength writeLength;
    bool setFocusToOtherTuplet;

protected slots:
    void buttonGroupBaseLengthButtonClicked(int id);
    void buttonGroupTupletButtonClicked(int id);

private:
    Ui::WriteLengthDialog *ui;
};

#endif // WRITELENGTHDIALOG_H
