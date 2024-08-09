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


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include "ObjectTemplates.h"

namespace Ovito {

/**
 * \brief Manages the application-wide list of modifier templates.
 */
class OVITO_GUIBASE_EXPORT ModifierTemplates : public ObjectTemplates
{
    Q_OBJECT

private:

    /// \brief Constructor.
    ModifierTemplates(QObject* parent = nullptr);

public:

    /// \brief Returns the singleton instance of this class.
    static ModifierTemplates* get();

    /// \brief Creates a new template on the basis of the given modifier(s).
    /// \param templateName The name of the new template. If a template with the same name exists, it is overwritten.
    /// \param modifiers The list of one or more modifiers from which the template should be created.
    /// \return The index of the created template.
    int createTemplate(const QString& templateName, const QVector<OORef<Modifier>>& modifiers) {
        QVector<OORef<RefTarget>> objects;
        for(auto& mod : modifiers)
            objects.push_back(mod);
        return ObjectTemplates::createTemplate(templateName, objects);
    }

    /// \brief Instantiates the objects that are stored under the given template name.
    QVector<OORef<Modifier>> instantiateTemplate(const QString& templateName) {
        QVector<OORef<Modifier>> modifiers;
        for(auto& obj : ObjectTemplates::instantiateTemplate(templateName)) {
            if(OORef<Modifier> modifier = dynamic_object_cast<Modifier>(std::move(obj)))
                modifiers.push_back(std::move(modifier));
        }
        return modifiers;
    }
};

}   // End of namespace
