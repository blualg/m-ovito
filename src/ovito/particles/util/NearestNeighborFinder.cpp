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

#include <ovito/particles/Particles.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include "NearestNeighborFinder.h"

namespace Ovito {

#define TREE_DEPTH_LIMIT 17

/******************************************************************************
* Constructor.
******************************************************************************/
NearestNeighborFinder::NearestNeighborFinder(int numNeighbors, BufferReadAccess<Point3> posProperty, const SimulationCellData& cellData, BufferReadAccess<SelectionIntType> selectionProperty) :
    _numNeighbors(numNeighbors),
    _bucketSize(std::max(numNeighbors / 2, 8)),
    _simCell(cellData)
{
    OVITO_ASSERT(posProperty);
    OVITO_ASSERT(this_task::get());

    OVITO_ASSERT(!_simCell.is2D() || !_simCell.cellMatrix().column(2).isZero());
    if(_simCell.volume3D() <= FLOATTYPE_EPSILON || _simCell.isDegenerate())
        throw Exception("Simulation cell is degenerate.");

    _cellVectorLengthsSquared[0] = _simCell.cellMatrix().column(0).squaredLength();
    _cellVectorLengthsSquared[1] = _simCell.cellMatrix().column(1).squaredLength();
    _cellVectorLengthsSquared[2] = _simCell.cellMatrix().column(2).squaredLength();

    // Compute normal vectors of simulation cell faces.
    _planeNormals[0] = _simCell.cellNormalVector(0);
    _planeNormals[1] = _simCell.cellNormalVector(1);
    _planeNormals[2] = _simCell.cellNormalVector(2);
    OVITO_ASSERT(_planeNormals[0] != Vector3::Zero());
    OVITO_ASSERT(_planeNormals[1] != Vector3::Zero());
    OVITO_ASSERT(_planeNormals[2] != Vector3::Zero());

    // For small simulation cells it cannot hurt much to consider more periodic images.
    // At the very least, consider one periodic image in each direction (when cell is orthogonal),
    // and two periodic images if cell is tilted.
    int nimages = 200 / qBound<size_t>(50, posProperty.size(), 200);
    if(nimages < 2 && !_simCell.isAxisAligned())
        nimages = 2;

    // Create list of periodic image shift vectors.
    int nx = _simCell.hasPbc(0) ? nimages : 0;
    int ny = _simCell.hasPbc(1) ? nimages : 0;
    int nz = _simCell.hasPbc(2) ? nimages : 0;
    _pbcImages.reserve((2*nx+1) * (2*ny+1) * (2*nz+1));
    for(int iz = -nz; iz <= nz; iz++) {
        for(int iy = -ny; iy <= ny; iy++) {
            for(int ix = -nx; ix <= nx; ix++) {
                _pbcImages.push_back(_simCell.cellMatrix() * Vector3(ix,iy,iz));
            }
        }
    }
    // Sort PBC images by distance from the primary image.
    std::ranges::sort(_pbcImages, [](const Vector3& a, const Vector3& b) {
        return a.squaredLength() < b.squaredLength();
    });

    // Compute bounding box of all particles (only for non-periodic directions).
    Box3 boundingBox(Point3(0,0,0), Point3(1,1,1));
    if(_simCell.hasPbc(0) == false || _simCell.hasPbc(1) == false || _simCell.hasPbc(2) == false) {
        for(const Point3& p : posProperty) {
            Point3 reducedp = _simCell.absoluteToReduced(p);
            if(_simCell.hasPbc(0) == false) {
                if(reducedp.x() < boundingBox.minc.x()) boundingBox.minc.x() = reducedp.x();
                else if(reducedp.x() > boundingBox.maxc.x()) boundingBox.maxc.x() = reducedp.x();
            }
            if(_simCell.hasPbc(1) == false) {
                if(reducedp.y() < boundingBox.minc.y()) boundingBox.minc.y() = reducedp.y();
                else if(reducedp.y() > boundingBox.maxc.y()) boundingBox.maxc.y() = reducedp.y();
            }
            if(_simCell.hasPbc(2) == false) {
                if(reducedp.z() < boundingBox.minc.z()) boundingBox.minc.z() = reducedp.z();
                else if(reducedp.z() > boundingBox.maxc.z()) boundingBox.maxc.z() = reducedp.z();
            }
        }
    }

    // Create root node.
    _root = _nodePool.construct();
    _root->bounds = boundingBox;
    _numLeafNodes++;

    // Create first level of child nodes by splitting in X direction.
    splitLeafNode(_root, 0);

    // Create second level of child nodes by splitting in Y direction.
    splitLeafNode(_root->children[0], 1);
    splitLeafNode(_root->children[1], 1);

    // Create third level of child nodes by splitting in Z direction.
    splitLeafNode(_root->children[0]->children[0], 2);
    splitLeafNode(_root->children[0]->children[1], 2);
    splitLeafNode(_root->children[1]->children[0], 2);
    splitLeafNode(_root->children[1]->children[1], 2);

    // Insert particles into tree structure. Refine tree as needed.
    const auto* p = posProperty.cbegin();
    const auto* sel = selectionProperty ? selectionProperty.cbegin() : nullptr;
    _atoms.resize(posProperty.size());
    for(NeighborListAtom& a : _atoms) {
        this_task::throwIfCanceled();
        a.pos = *p;
        // Wrap atomic positions back into simulation box.
        Point3 rp = _simCell.absoluteToReduced(a.pos);
        for(size_t k = 0; k < 3; k++) {
            if(_simCell.hasPbc(k)) {
                if(auto s = std::floor(rp[k])) {
                    rp[k] -= s;
                    a.pos -= s * _simCell.cellMatrix().column(k);
                }
            }
        }
        if(!sel || *sel++) {
            insertParticle(&a, rp, _root, 0);
        }
        ++p;
    }

    _root->convertToAbsoluteCoordinates(_simCell.cellMatrix());

    this_task::throwIfCanceled();
}

/******************************************************************************
* Inserts an atom into the binary tree.
******************************************************************************/
void NearestNeighborFinder::insertParticle(NeighborListAtom* atom, const Point3& p, TreeNode* node, int depth)
{
    if(node->isLeaf()) {
        OVITO_ASSERT(node->bounds.classifyPoint(p) != -1);
        // Insert atom into leaf node.
        atom->nextInBin = node->atoms;
        node->atoms = atom;
        node->numAtoms++;
        if(depth > _maxTreeDepth)
            _maxTreeDepth = depth;
        // If leaf node becomes too large, split it in the largest dimension.
        if(node->numAtoms > _bucketSize && depth < TREE_DEPTH_LIMIT) {
            splitLeafNode(node, determineSplitDirection(node));
        }
    }
    else {
        // Decide on which side of the splitting plane the atom is located.
        if(p[node->splitDim] < node->splitPos)
            insertParticle(atom, p, node->children[0], depth+1);
        else
            insertParticle(atom, p, node->children[1], depth+1);
    }
}

/******************************************************************************
* Determines in which direction to split the given leaf node.
******************************************************************************/
int NearestNeighborFinder::determineSplitDirection(TreeNode* node)
{
    FloatType dmax = 0.0;
    int dmax_dim = -1;
    for(int dim = 0; dim < 3; dim++) {
        FloatType size = node->bounds.size(dim);
        FloatType d = _cellVectorLengthsSquared[dim] * size * size;
        if(d > dmax) {
            dmax = d;
            dmax_dim = dim;
        }
    }
    OVITO_ASSERT(dmax_dim >= 0);
    return dmax_dim;
}

/******************************************************************************
* Splits a leaf node into two new leaf nodes and redistributes the atoms to the child nodes.
******************************************************************************/
void NearestNeighborFinder::splitLeafNode(TreeNode* node, int splitDim)
{
    // Copy the atoms pointer from the union before it gets overwritten when setting the children.
    NeighborListAtom* atom = node->atoms;

    node->splitDim = splitDim;
    node->splitPos = (node->bounds.minc[splitDim] + node->bounds.maxc[splitDim]) * FloatType(0.5);

    // Create child nodes and define their bounding boxes.
    node->children[0] = _nodePool.construct();
    node->children[1] = _nodePool.construct();
    node->children[0]->bounds = node->bounds;
    node->children[1]->bounds = node->bounds;
    node->children[0]->bounds.maxc[splitDim] = node->children[1]->bounds.minc[splitDim] = node->splitPos;

    FloatType a = _simCell.reciprocalCellMatrix()(splitDim, 0);
    FloatType b = _simCell.reciprocalCellMatrix()(splitDim, 1);
    FloatType c = _simCell.reciprocalCellMatrix()(splitDim, 2);
    FloatType d = _simCell.reciprocalCellMatrix()(splitDim, 3);

    // Redistribute atoms to child nodes.
    while(atom != nullptr) {
        NeighborListAtom* next = atom->nextInBin;
        FloatType p = a * atom->pos.x() + b * atom->pos.y() + c * atom->pos.z() + d;
        if(p < node->splitPos) {
            atom->nextInBin = node->children[0]->atoms;
            node->children[0]->atoms = atom;
        }
        else {
            atom->nextInBin = node->children[1]->atoms;
            node->children[1]->atoms = atom;
        }
        atom = next;
    }

    _numLeafNodes++;
}

}   // End of namespace
