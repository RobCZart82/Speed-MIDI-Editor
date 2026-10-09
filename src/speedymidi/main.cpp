/***************************************************************************
 *  main.cpp - main() Function
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

#include "speedymidiapp.h"
#include <QScopeGuard>

#ifdef Q_OS_WIN32
#include "windows.h"    // for call to CreateMutex
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN32
    // On Windows, create a mutex preventing uninstalling the application while running.
    const HANDLE uninstallMutex=CreateMutexA(NULL, FALSE, "SpeedyMidiAntiUninstall");
    const auto closeUninstallMutex=qScopeGuard([uninstallMutex]() {
        if(uninstallMutex)CloseHandle(uninstallMutex);
    });
    if(!uninstallMutex)
        qWarning("Cannot create the installer protection mutex (Windows error %lu)",GetLastError());
#endif

    SpeedyMidiApp a(argc, argv);
    return a.exec();
}
