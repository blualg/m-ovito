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


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/viewport/Viewport.h>

namespace Ovito {

/**
 * \brief Utility class that supports the picking of bonds in the viewports.
 */
class OVITO_PARTICLESGUI_EXPORT BondPickingHelper
{
public:

    struct PickResult {

        /// The index of the picked bond.
        size_t bondIndex;

        /// The pipeline that produced the picked bond.
        OORef<Pipeline> pipeline;
    };

    /// \brief Finds the bond under the mouse cursor.
    /// \param vpwin The viewport window to perform hit testing in.
    /// \param clickPoint The position of the mouse cursor in the viewport.
    /// \param time The animation at which hit testing is performed.
    /// \param result The output structure that receives information about the picked bond.
    /// \return \c true if a bond has been picked; \c false otherwise.
    bool pickBond(ViewportWindow* vpwin, const QPoint& clickPoint, PickResult& result);
};

}   // End of namespace
