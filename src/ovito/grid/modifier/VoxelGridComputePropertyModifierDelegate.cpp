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

#include <ovito/grid/Grid.h>
#include "VoxelGridComputePropertyModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VoxelGridComputePropertyModifierDelegate);
OVITO_CLASSINFO(VoxelGridComputePropertyModifierDelegate, "DisplayName", "Voxel grids");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> VoxelGridComputePropertyModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all VoxelGrid objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(VoxelGrid::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Initializes an expression evaluator.
******************************************************************************/
std::unique_ptr<PropertyExpressionEvaluator> VoxelGridComputePropertyModifierDelegate::initializeExpressionEvaluator(const ComputePropertyModifier* modifier, const PipelineFlowState& originalState, int frame) const
{
    const ConstDataObjectPath containerPath = originalState.expectObject(inputContainerRef());
    const VoxelGrid* voxelGrid = containerPath.lastAs<VoxelGrid>();

    auto evaluator = std::make_unique<PropertyExpressionEvaluator>();
    evaluator->initialize(modifier->expressions(), originalState, containerPath, frame);

    // A helper object, which computes the spatial coordinate of a voxel upon request.
    struct GridPositionHelper {
        const VoxelGrid::GridDimensions shape;
        AffineTransformation tm = AffineTransformation::Zero();
        GridPositionHelper(const VoxelGrid* voxelGrid) : shape(voxelGrid->shape()) {
            voxelGrid->verifyIntegrity();
            switch(voxelGrid->gridType()) {
            case VoxelGrid::GridType::CellData:
                if(shape[0] != 0 && shape[1] != 0 && shape[2] != 0) {
                    tm = voxelGrid->domain()->cellMatrix() *
                        Matrix3::diagonal(FloatType(1) / shape[0], FloatType(1) / shape[1], FloatType(1) / shape[2]) *
                        AffineTransformation::translation(Vector3(FloatType(0.5), FloatType(0.5), voxelGrid->domain()->is2D() ? FloatType(0.5) : FloatType(0)));
                }
                break;
            case VoxelGrid::GridType::PointData:
                const int nx = ((voxelGrid->domain()->pbcFlags()[0] || shape[0] <= 1) ? shape[0] : (shape[0] - 1));
                const int ny = ((voxelGrid->domain()->pbcFlags()[1] || shape[1] <= 1) ? shape[1] : (shape[1] - 1));
                const int nz = ((voxelGrid->domain()->pbcFlags()[2] || shape[2] <= 1) ? shape[2] : (shape[2] - 1));
                if(nx != 0 && ny != 0 && nz != 0) {
                    tm = voxelGrid->domain()->cellMatrix() * Matrix3::diagonal(FloatType(1) / nx, FloatType(1) / ny, FloatType(1) / nz);
                }
                break;
            }
        }
        Point3 operator()(size_t voxelIndex) const {
            const size_t coords[3] = { voxelIndex % shape[0], (voxelIndex / shape[0]) % shape[1], voxelIndex / (shape[0] * shape[1]) };
            return tm * Point3(coords[0], coords[1], coords[2]);
        }
    };

    evaluator->registerComputedVariable("SpatialPosition.X", [helper=GridPositionHelper(voxelGrid)](size_t voxelIndex) -> double {
        return helper(voxelIndex).x();
    },
    tr("Cartesian voxel coord"));

    evaluator->registerComputedVariable("SpatialPosition.Y", [helper=GridPositionHelper(voxelGrid)](size_t voxelIndex) -> double {
        return helper(voxelIndex).y();
    },
    tr("Cartesian voxel coord"));

    evaluator->registerComputedVariable("SpatialPosition.Z", [helper=GridPositionHelper(voxelGrid)](size_t voxelIndex) -> double {
        return helper(voxelIndex).z();
    },
    tr("Cartesian voxel coord"));

    evaluator->registerComputedVariable("VoxelCoordinate.X", [shape=voxelGrid->shape()](size_t voxelIndex) -> double {
        return voxelIndex % shape[0];
    },
    tr("Logical coordinate"));

    evaluator->registerComputedVariable("VoxelCoordinate.Y", [shape=voxelGrid->shape()](size_t voxelIndex) -> double {
        return (voxelIndex / shape[0]) % shape[1];
    },
    tr("Logical coordinate"));

    evaluator->registerComputedVariable("VoxelCoordinate.Z", [shape=voxelGrid->shape()](size_t voxelIndex) -> double {
        return voxelIndex / (shape[0] * shape[1]);
    },
    tr("Logical coordinate"));

    return evaluator;
}

}   // End of namespace
