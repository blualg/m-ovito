////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef OVITO_QML_GUI
    #include <ovito/gui/desktop/GUI.h>
    #include <ovito/gui/desktop/app/GuiApplication.h>
#else
    #include <ovito/core/Core.h>
    #include <ovito/gui/qml/app/WasmApplication.h>
#endif

#if defined(OVITO_BUILD_PLUGIN_PYSCRIPT) && !defined(OVITO_BUILD_BASIC)
    // Explicitly build 'ovito' executable against Python library.
    // The following include directive will pull in the Python headers.
    #include <ovito/pyscript/PyScript.h>
#endif

/**
 * This is the main entry point for the graphical desktop application.
 *
 * Note that most of the application logic is found in the Core and the Gui
 * library modules of OVITO, not in this executable module.
 */
int main(int argc, char** argv)
{
#if defined(OVITO_BUILD_PLUGIN_PYSCRIPT) && !defined(OVITO_BUILD_BASIC)
    // This (useless) call to a Python C API function is needed to force-link the Python library into the executable.
    // We have to make sure the Python lib gets loaded into process memory before any of the OVITO plugin Python modules 
    // are loaded, because they depend on the Python lib but were not explicitly linked to it.
    if(Py_IsInitialized())
        return 1;
#endif

    // Initialize the application.
#ifndef OVITO_QML_GUI
    Ovito::GuiApplication app;
#else
    Ovito::WasmApplication app;
#endif
    if(!app.initialize(argc, argv))
        return 1;

    // The Application::initialize() method may return with success but without creating a Qt application
    // object. This happens, for example, when the --version command line parameter was specified to the user.
    // In this case, we terminate the program immediately without even entering the event loop.
    if(!QCoreApplication::instance())
        return 0;

    // Enter event loop if a Qt application has been created.
    int result = app.runApplication();

    // Shut application down.
    app.shutdown();

    return result;
}
