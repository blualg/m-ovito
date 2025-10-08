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


#include <ovito/core/Core.h>
#include <ovito/opengl/OffscreenOpenGLRenderingJob.h>

namespace Ovito {

/**
 * \brief A rendering job that uses OpenGL to render the picking pass into an offscreen buffer.
 */
class OVITO_OPENGLRENDERERWINDOW_EXPORT PickingOpenGLRenderingJob : public OffscreenOpenGLRenderingJob
{
    OVITO_CLASS(PickingOpenGLRenderingJob)

public:

    /// Returns an instance of this rendering job class that is shared among all viewport windows.
    static OORef<PickingOpenGLRenderingJob> createSharedInstance(UserInterface& userInterface);
};

}   // End of namespace
