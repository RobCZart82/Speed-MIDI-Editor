/***************************************************************************
 *  cs_file.h - Controller Subsystem: File
 *              (New, Open, Save, Close, Quit ...)
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

#ifndef CS_FILE_H
#define CS_FILE_H

#include "cs_common.h"

class CS_File : public CS_Common
{
    Q_OBJECT
public:
    CS_File(Controller* controller);

protected:
    virtual bool closeEvent();
    
    bool maybeSave();
    bool showCompatibilityWarning();

protected slots:
    void actionFile_ShowLaunchDialog_Triggered();
    void actionFile_NewWithWizard_Triggered();
    void actionFile_NewDefaultDocument_Triggered();
    void actionFile_Open_Triggered();
    void actionFile_OpenRecentFile_Triggered();
    bool actionFile_Save_Triggered();
    bool actionFile_SaveAs_Triggered();
    bool actionFile_SaveCompatibleFile_Triggered();
    void actionFile_Close_Triggered();
    void actionFile_ExtractParts_Triggered();
    void actionFile_Quit_Triggered();

    void actionOptions_Preferences_Triggered();

    void actionHelp_About_Triggered();
};

#endif // CS_FILE_H
