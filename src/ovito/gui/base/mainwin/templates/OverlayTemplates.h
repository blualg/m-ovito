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
#include <ovito/core/viewport/overlays/ViewportOverlay.h>
#include "ObjectTemplates.h"

namespace Ovito {

/**
 * \brief Manages the application-wide list of viewport layer templates.
 */
class OVITO_GUIBASE_EXPORT OverlayTemplates : public ObjectTemplates
{
    Q_OBJECT

private:

    /// \brief Constructor.
    OverlayTemplates(QObject* parent = nullptr);

public:

    /// \brief Returns the singleton instance of this class.
    static OverlayTemplates* get();

    /// \brief Creates a new template on the basis of the given overlays(s).
    /// \param templateName The name of the new template. If a template with the same name exists, it is overwritten.
    /// \param modifiers The list of one or more overlays from which the template should be created.
    /// \return The index of the created template.
    int createTemplate(const QString& templateName, const QVector<OORef<ViewportOverlay>>& overlays) {
        QVector<OORef<RefTarget>> objects;
        for(auto& ov : overlays)
            objects.push_back(ov);
        return ObjectTemplates::createTemplate(templateName, objects);
    }

    /// \brief Instantiates the objects that are stored under the given template name.
    QVector<OORef<ViewportOverlay>> instantiateTemplate(const QString& templateName) {
        QVector<OORef<ViewportOverlay>> overlays;
        for(auto& obj : ObjectTemplates::instantiateTemplate(templateName)) {
            if(OORef<ViewportOverlay> ov = dynamic_object_cast<ViewportOverlay>(std::move(obj)))
                overlays.push_back(std::move(ov));
        }
        return overlays;
    }
};

}   // End of namespace
