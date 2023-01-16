////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2020 OVITO GmbH, Germany
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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/TransformingDataVis.h>

namespace Ovito {

/**
 * \brief Base class for transient data objects that are dynamically generated from other data objects
 *        by an asynchronous DataVis implementation.
 */
class OVITO_CORE_EXPORT TransformedDataObject : public DataObject
{
    OVITO_CLASS(TransformedDataObject)

public:

    /// Constructor.
    TransformedDataObject(ObjectCreationParams params, TransformingDataVis* creator = nullptr, const DataObject* sourceData = nullptr) : DataObject(params), 
        _sourceDataObject(sourceData), _visElementRevision(creator ? creator->revisionNumber() : 0) {}

private:

    /// Stores a reference to the original DataObject this TransformedDataObject was generated from.
    /// We use it to detect changes to the source object and avoid unnecessary regeneration
    /// of the transient data object.
    DECLARE_RUNTIME_PROPERTY_FIELD(DataOORef<const DataObject>, sourceDataObject, setSourceDataObject);

    /// Stores a revision version number of the TransformingDataVis that created this TransformedDataObject.
    /// We use this to detect changes to the TransformingDataVis's parameters that require a re-generation of the
    /// transient data object.
    DECLARE_RUNTIME_PROPERTY_FIELD(int, visElementRevision, setVisElementRevision);
};

}   // End of namespace
