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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "PickingOpenGLRenderingJob.h"
#include "OpenGLViewportWindow.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(PickingOpenGLRenderingJob);

/******************************************************************************
* Returns an instance of this rendering job class that is shared among all viewport windows.
******************************************************************************/
OORef<PickingOpenGLRenderingJob> PickingOpenGLRenderingJob::createSharedInstance(UserInterface& userInterface)
{
    OORef<PickingOpenGLRenderingJob> job;

    // Check if any of the existing viewport windows already created a shared instance of this rendering job.
    for(ViewportWindow* window : userInterface.viewportWindows()) {
        if(OpenGLViewportWindow* openglWindow = dynamic_object_cast<OpenGLViewportWindow>(window)) {
            if(openglWindow->pickingRenderingJob()) {
                job = openglWindow->pickingRenderingJob();
                break;
            }
        }
    }

    // Create a new shared instance if no existing one was found.
    if(!job)
        job = OORef<PickingOpenGLRenderingJob>::create(userInterface.datasetContainer().visCache(), nullptr); // Note: It's valid to use the global vis cache here, because the OpenGL renderer runs in the main thread.

    // Sanity checks to make sure the existing job is compatible with our requirements.
    OVITO_ASSERT(!job->sceneRenderer());
    OVITO_ASSERT(job->visCache() == userInterface.datasetContainer().visCache());
    return job;
}

}   // End of namespace
