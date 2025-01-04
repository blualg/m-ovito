////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/gui/base/mainwin/templates/ModifierTemplates.h>
#include "TemplatesPageBase.h"

namespace Ovito {

/**
 * Page of the application settings dialog, which allows the user to manage the user-defined modifier templates.
 */
class OVITO_GUI_EXPORT ModifierTemplatesPage : public TemplatesPageBase
{
    OVITO_CLASS(ModifierTemplatesPage)
    Q_OBJECT

protected:

    // UI title of the settings page.
    virtual QString settingsPageTitle() override {
        return tr("Modifier Templates");
    }

    // Informational text shown on the page.
    virtual QString settingsPageDescription() override {
        return tr(
            "Modifier templates you define here will appear in the drop-down list of available modifiers, from where you can quickly insert them into a data pipeline. "
            "Templates may consist of several modifiers, making your life easier if you repeatedly need to use the same modifier sequence.");
    }

    /// Help topic to open when the user presses the help button.
    virtual QString helpTopicId() override {
        return QStringLiteral("manual:modifier_templates");
    }

    /// The kind of objects for which templates are managed on this page.
    virtual QString objectTypeName() override {
        return tr("Modifier");
    }

    /// The file type and suffix used for saving and loading templates on this.
    virtual QString templateFileFilter() override {
        return tr("OVITO Modifier Templates (*.ovmod)");
    }

    /// The object that manages the templates shown on this page.
    virtual ObjectTemplates* templateManager() override {
        return ModifierTemplates::get();
    }

    /// When the user is creating a new template, this method populates the list of available objects,
    /// which the the user can select to be included in the template.
    virtual QVector<QTreeWidgetItem*> populateAvailableObjectsList(QTreeWidget* objectListWidget, QComboBox* nameBox) override;

public:

    /// \brief Returns an integer value that is used to sort the dialog pages in ascending order.
    virtual int pageSortingKey() const override { return 3; }
};

}   // End of namespace
