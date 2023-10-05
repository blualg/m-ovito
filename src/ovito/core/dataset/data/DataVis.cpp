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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/DataSet.h>
#include "DataVis.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(DataVis);

/******************************************************************************
* Returns all pipelines that produced this visualization element.
******************************************************************************/
QSet<Pipeline*> DataVis::pipelines(bool onlyScenePipelines) const
{
    OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "DataVis::pipelines", "This function may only be called from the main thread.");

    QSet<Pipeline*> pipelinesList;
    visitDependents([&](RefMaker* dependent) {
        if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent)) {
            if(pipeline->visElements().contains(const_cast<DataVis*>(this))) {
                if(!onlyScenePipelines || pipeline->isInScene())
                    pipelinesList.insert(pipeline);
            }
        }
    });
    return pipelinesList;
}

}   // End of namespace
