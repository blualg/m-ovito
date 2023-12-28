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
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/oo/CloneHelper.h>

namespace Ovito {

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
PipelineFlowState::~PipelineFlowState()
{
}
#endif

/******************************************************************************
* Returns the data collection of this pipeline state after making sure it is
* safe to modify it.
******************************************************************************/
DataCollection* PipelineFlowState::mutableData()
{
    OVITO_ASSERT(_data);
    if(_data && !_data->isSafeToModify()) {
        _data = CloneHelper::cloneSingleObject(_data.get(), false);
        OVITO_ASSERT(_data->isSafeToModify());
        OVITO_ASSERT(_data->dataReferenceCount() == 1);
    }
    return const_cast<DataCollection*>(_data.get());
}

/******************************************************************************
* Makes the last object in the data path mutable and returns a pointer to the mutable copy.
* Also update the data path to point to the new object.
******************************************************************************/
DataObject* PipelineFlowState::makeMutableInplace(ConstDataObjectPath& path)
{
    OVITO_ASSERT(path.empty() == false);
    OVITO_ASSERT(path.front() == data());
    DataObject* parent = mutableData();
    path.front() = parent;
    for(auto obj = std::next(path.begin()); obj != path.end(); ++obj) {
        *obj = parent = parent->makeMutable(*obj);
    }
    return parent;
}

}   // End of namespace
