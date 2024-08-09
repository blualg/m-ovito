////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/dialogs/ApplicationSettingsDialog.h>
#include <ovito/gui/base/mainwin/templates/ObjectTemplates.h>

namespace Ovito {

/**
 * Abstract base class for pages in the application settings dialog, which allows the user to manage object templates, e.g. modifier and viewport layer templates.
 */
class OVITO_GUI_EXPORT TemplatesPageBase : public ApplicationSettingsDialogPage
{
    OVITO_CLASS(TemplatesPageBase)
    Q_OBJECT

protected:

    // UI title of the settings page.
    virtual QString settingsPageTitle() = 0;

    // Informational text shown on the page.
    virtual QString settingsPageDescription() = 0;

    /// Help topic to open when the user presses the help button.
    virtual QString helpTopicId() = 0;

    /// The kind of objects for which templates are managed on this page.
    virtual QString objectTypeName() = 0;

    /// The file type and suffix used for saving and loading templates on this.
    virtual QString templateFileFilter() = 0;

    /// The object that manages the templates shown on this page.
    virtual ObjectTemplates* templateManager() = 0;

    /// The kind of objects for which templates are managed on this page.
    QString objectTypeNameLC() { return objectTypeName().toLower(); }

    /// When the user is creating a new template, this method populates the list of available objects,
    /// which the the user can select to be included in the template.
    virtual QVector<QTreeWidgetItem*> populateAvailableObjectsList(QTreeWidget* objectListWidget, QComboBox* nameBox) = 0;

public:

    /// \brief Creates the widget.
    virtual void insertSettingsDialogPage(QTabWidget* tabWidget) override;

    /// \brief Lets the settings page to save all values entered by the user.
    virtual void saveValues(QTabWidget* tabWidget) override;

    /// \brief Lets the settings page restore the original values of changed settings when the user presses the Cancel button.
    virtual void restoreValues(QTabWidget* tabWidget) override;

private Q_SLOTS:

    /// Is invoked when the user presses the "Create template" button.
    void onCreateTemplate();

    /// Is invoked when the user presses the "Delete template" button.
    void onDeleteTemplate();

    /// Is invoked when the user presses the "Rename template" button.
    void onRenameTemplate();

    /// Is invoked when the user presses the "Export templates" button.
    void onExportTemplates();

    /// Is invoked when the user presses the "Import templates" button.
    void onImportTemplates();

private:

    QListView* _listWidget;
    bool _dirtyFlag = false;
};

}   // End of namespace
