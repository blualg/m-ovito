////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

#pragma once


#include <ovito/gui/qml/GUI.h>
#include <ovito/gui/base/mainwin/PipelineListItem.h>
#include <ovito/gui/base/mainwin/PipelineListModel.h>
#include <ovito/gui/base/mainwin/ModifierListModel.h>
#include <ovito/core/app/ApplicationService.h>
#include <ovito/core/app/StandaloneApplication.h>

#include <QQmlApplicationEngine>

namespace Ovito {

/**
 * \brief The application object used when running on the WebAssembly platform.
 */
class OVITO_GUI_EXPORT WasmApplication : public StandaloneApplication
{
    Q_OBJECT

public:

    /// Constructor.
    WasmApplication();

    /// Create the global instance of the right QCoreApplication derived class.
    virtual void createQtApplication(int& argc, char** argv) override;

    /// This is called from main() before the application exits.
    void shutdown();

    /// Handler function for exceptions.
    virtual void reportError(const Exception& exception, bool blocking) override;

    /// Returns a pointer to the main dataset container.
    WasmDataSetContainer* datasetContainer() const;

protected:

    /// Defines the program's command line parameters.
    virtual void registerCommandLineParameters(QCommandLineParser& parser) override;

    /// Prepares application to start running.
    virtual bool startupApplication() override;

    /// Is called at program startup once the event loop is running.
    virtual void postStartupInitialization() override;

    /// Creates the global FileManager class instance.
    virtual FileManager* createFileManager() override;

private:

    /// The global Qml engine.
    QQmlApplicationEngine* _qmlEngine = nullptr;
};

#if 0
// Registration of classes from the Core and GuiBase modules as QML types.
// See https://doc.qt.io/qt-6/qtqml-cppintegration-definetypes.html
#define OVITO_REGISTER_FOREIGN_QML_TYPE(name) \
    struct name##_ForeignQMLType \
    { \
        Q_GADGET \
        QML_FOREIGN(name) \
        QML_NAMED_ELEMENT(name) \
        QML_UNCREATABLE("") \
    };
OVITO_REGISTER_FOREIGN_QML_TYPE(RefTarget)
OVITO_REGISTER_FOREIGN_QML_TYPE(Viewport)
OVITO_REGISTER_FOREIGN_QML_TYPE(ViewportSettings)
OVITO_REGISTER_FOREIGN_QML_TYPE(ParameterUnit)
OVITO_REGISTER_FOREIGN_QML_TYPE(PipelineListItem)
OVITO_REGISTER_FOREIGN_QML_TYPE(PipelineListModel)
OVITO_REGISTER_FOREIGN_QML_TYPE(ModifierListModel)
#endif

}   // End of namespace
