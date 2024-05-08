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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/dataset/data/DataObjectReference.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "SimulationCell.h"
#include "SimulationCellVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SimulationCell);
OVITO_CLASSINFO(SimulationCell, "ClassNameAlias", "SimulationCellObject");  // For backward compatibility with OVITO 3.9.2
OVITO_CLASSINFO(SimulationCell, "DisplayName", "Simulation cell");
DEFINE_PROPERTY_FIELD(SimulationCell, cellMatrix);
DEFINE_PROPERTY_FIELD(SimulationCell, pbcX);
DEFINE_PROPERTY_FIELD(SimulationCell, pbcY);
DEFINE_PROPERTY_FIELD(SimulationCell, pbcZ);
DEFINE_PROPERTY_FIELD(SimulationCell, is2D);
DEFINE_SHADOW_PROPERTY_FIELD(SimulationCell, pbcX);
DEFINE_SHADOW_PROPERTY_FIELD(SimulationCell, pbcY);
DEFINE_SHADOW_PROPERTY_FIELD(SimulationCell, pbcZ);
DEFINE_SHADOW_PROPERTY_FIELD(SimulationCell, is2D);
SET_PROPERTY_FIELD_LABEL(SimulationCell, cellMatrix, "Cell matrix");
SET_PROPERTY_FIELD_LABEL(SimulationCell, pbcX, "Periodic boundary conditions (X)");
SET_PROPERTY_FIELD_LABEL(SimulationCell, pbcY, "Periodic boundary conditions (Y)");
SET_PROPERTY_FIELD_LABEL(SimulationCell, pbcZ, "Periodic boundary conditions (Z)");
SET_PROPERTY_FIELD_LABEL(SimulationCell, is2D, "2D");
SET_PROPERTY_FIELD_UNITS(SimulationCell, cellMatrix, WorldParameterUnit);

/******************************************************************************
* Computes the inverse of the cell matrix.
******************************************************************************/
void SimulationCell::computeInverseMatrix() const
{
    if(!is2D()) {
        cellMatrix().inverse(_reciprocalSimulationCell);
    }
    else {
        _reciprocalSimulationCell.setIdentity();
        FloatType det = cellMatrix()(0,0) * cellMatrix()(1,1) - cellMatrix()(0,1) * cellMatrix()(1,0);
        bool isValid = (std::abs(det) > FLOATTYPE_EPSILON);
        if(isValid) {
            _reciprocalSimulationCell(0,0) = cellMatrix()(1,1) / det;
            _reciprocalSimulationCell(1,0) = -cellMatrix()(1,0) / det;
            _reciprocalSimulationCell(0,1) = -cellMatrix()(0,1) / det;
            _reciprocalSimulationCell(1,1) = cellMatrix()(0,0) / det;
            _reciprocalSimulationCell.translation().x() = -(_reciprocalSimulationCell(0,0) * cellMatrix().translation().x() + _reciprocalSimulationCell(0,1) * cellMatrix().translation().y());
            _reciprocalSimulationCell.translation().y() = -(_reciprocalSimulationCell(1,0) * cellMatrix().translation().x() + _reciprocalSimulationCell(1,1) * cellMatrix().translation().y());
        }
    }
    _isReciprocalMatrixValid = true;
}

/******************************************************************************
* Is called when the value of a non-animatable field of this object changes.
******************************************************************************/
void SimulationCell::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(cellMatrix) || field == PROPERTY_FIELD(is2D)) {
        invalidateReciprocalCellMatrix();

        // Ensure that a 2D cell has always a finite extent along Z.
        if(is2D() && (cellMatrix()(0,2) != 0.0 || cellMatrix()(1,2) != 0.0 || cellMatrix()(2,2) == 0.0)) {
            AffineTransformation m = cellMatrix();
            m(0,2) = 0.0;
            m(1,2) = 0.0;
            if(m(2,2) == 0.0) m(2,2) = 1.0;
            setCellMatrix(m);
        }
    }
    DataObject::propertyChanged(field);
}

/******************************************************************************
* Creates an editable proxy object for this DataObject and synchronizes its parameters.
******************************************************************************/
void SimulationCell::updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath) const
{
    OVITO_ASSERT(this == dataPath.back());

    if(SimulationCell* proxy = static_object_cast<SimulationCell>(editableProxy())) {
        // Synchronize the actual data object with the editable proxy object.

        // Box size changes of the actual simulation cell are adopted by the proxy cell object.
        proxy->setCellMatrix(cellMatrix());

        // Changes made by the user to the PBC flags or dimensionality setting of the proxy cell object are adopted by the actual simulation cell object.
        if(pbcFlags() != proxy->pbcFlags() || is2D() != proxy->is2D()) {
            // Make this data object mutable first.
            SimulationCell* self = static_object_cast<SimulationCell>(state.makeMutableInplace(dataPath));

            self->setPbcFlags(proxy->pbcFlags());
            self->setIs2D(proxy->is2D());
        }
    }
    else {
        // Create and initialize a new proxy.
        OORef<SimulationCell> newProxy = OORef<SimulationCell>::create(ObjectInitializationFlag::DontCreateVisElement);
        newProxy->setPbcFlags(pbcFlags());
        newProxy->setIs2D(is2D());
        newProxy->setCellMatrix(cellMatrix());

        // Make this data object mutable and attach the proxy object to it.
        state.makeMutableInplace(dataPath)->setEditableProxy(std::move(newProxy));
    }

    DataObject::updateEditableProxies(state, dataPath);
}

/******************************************************************************
* Wraps the input coordinates at the periodic boundaries of the cell.
* The wrapped coordinates are returned as a new DataBuffer object.
******************************************************************************/
ConstPropertyPtr SimulationCell::wrapPoints(const Property* inputPositions) const
{
    // Check the input data type and component count.
    OVITO_ASSERT(inputPositions);
    OVITO_ASSERT(inputPositions->dataType() == DataBuffer::FloatDefault);
    OVITO_ASSERT(inputPositions->componentCount() == 3);

    // If PBCs are turned off, we have nothing to do and can return the input coordinates as is.
    if(!hasPbcCorrected())
        return inputPositions;

    // Create a new buffer to store the wrapped coordinates.
    PropertyPtr outputPositions = inputPositions->cloneWithoutData(inputPositions->size());

    const AffineTransformation cellMatrix = this->cellMatrix();
    const AffineTransformation reciprocalCellMatrix = this->reciprocalCellMatrix();
    const auto pbcFlags = this->pbcFlagsCorrected();

#ifdef OVITO_USE_SYCL
    if(inputPositions->size() != 0) {
        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
            SyclBufferAccess<Point3, access_mode::read> posInAcc{inputPositions, cgh};
            SyclBufferAccess<Point3, access_mode::discard_write> posOutAcc{outputPositions, cgh};
            OVITO_SYCL_PARALLEL_FOR(cgh, SimulationCell_wrapPoints)(sycl::range(posInAcc.size()), [=](size_t i) {
                const Point3 p = posInAcc[i];
                const Point3 rp = reciprocalCellMatrix * p;
                const Vector3 rv(
                    pbcFlags[0] * sycl::floor(rp.x()),
                    pbcFlags[1] * sycl::floor(rp.y()),
                    pbcFlags[2] * sycl::floor(rp.z())
                );
                posOutAcc[i] = p - cellMatrix * rv;
            });
        });
    }
#else
    BufferReadAccess<Point3> inputPosAcc{inputPositions};
    BufferWriteAccess<Point3, access_mode::discard_write> outputPosAcc{outputPositions};
    boost::transform(inputPosAcc, outputPosAcc.begin(), [&](const Point3& p) {
        const Point3 rp = reciprocalCellMatrix * p;
        const Vector3 rv(
            pbcFlags[0] * std::floor(rp.x()),
            pbcFlags[1] * std::floor(rp.y()),
            pbcFlags[2] * std::floor(rp.z())
        );
        return p - cellMatrix * rv;
    });
#endif

    return outputPositions;
}

}   // End of namespace
