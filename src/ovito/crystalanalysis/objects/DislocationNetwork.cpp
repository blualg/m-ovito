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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/stdobj/table/DataTable.h>
#include "DislocationNetwork.h"
#include "DislocationVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DislocationNetwork);
OVITO_CLASSINFO(DislocationNetwork, "ClassNameAlias", "DislocationNetworkObject");  // For backward compatibility with OVITO 3.10
OVITO_CLASSINFO(DislocationNetwork, "DisplayName", "Dislocations");
DEFINE_VECTOR_REFERENCE_FIELD(DislocationNetwork, crystalStructures);
DEFINE_REFERENCE_FIELD(DislocationNetwork, clusterGraph);
SET_PROPERTY_FIELD_LABEL(DislocationNetwork, crystalStructures, "Crystal structures");
SET_PROPERTY_FIELD_LABEL(DislocationNetwork, clusterGraph, "Cluster graph");

/******************************************************************************
* Constructor.
******************************************************************************/
void DislocationNetwork::initializeObject(ObjectInitializationFlags flags)
{
    PeriodicDomainObject::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {

        if(!flags.testFlag(ObjectInitializationFlag::DontCreateVisElement)) {
            // Attach a visualization element for rendering the dislocation lines.
            setVisElement(OORef<DislocationVis>::create(flags));
        }

        // Create the "unidentified" structure.
        if(crystalStructures().empty()) {
            DataOORef<MicrostructurePhase> defaultStructure = DataOORef<MicrostructurePhase>::create(flags);
            defaultStructure->setName(tr("Unidentified structure"));
            defaultStructure->setColor(Color(1,1,1));
            defaultStructure->createBurgersVectorFamily();
            addCrystalStructure(std::move(defaultStructure));
        }

        // Create an empty cluster graph.
        setClusterGraph(DataOORef<ClusterGraph>::create(flags));
    }
}

/******************************************************************************
* Creates a copy of the object.
******************************************************************************/
OORef<RefTarget> DislocationNetwork::clone(bool deepCopy, CloneHelper& cloneHelper) const
{
    // Let the base class create an instance of this class.
    OORef<DislocationNetwork> clone = static_object_cast<DislocationNetwork>(PeriodicDomainObject::clone(deepCopy, cloneHelper));

    for(int lineIndex = 0; lineIndex < lines().size(); lineIndex++) {
        DislocationLine* oldLine = lines()[lineIndex];
        OVITO_ASSERT(oldLine->replacedWith == nullptr);
        OVITO_ASSERT(oldLine->id == lineIndex);
        DislocationLine* newLine = clone->createLine(oldLine->burgersVector);
        newLine->vertices = oldLine->vertices;
        newLine->coreSize = oldLine->coreSize;
        OVITO_ASSERT(newLine->id == oldLine->id);
    }

    for(int lineIndex = 0; lineIndex < lines().size(); lineIndex++) {
        DislocationLine* oldLine = lines()[lineIndex];
        DislocationLine* newLine = clone->lines()[lineIndex];
        for(int nodeIndex = 0; nodeIndex < 2; nodeIndex++) {
            DislocationNode* oldNode = oldLine->nodes[nodeIndex];
            if(oldNode->isDangling())
                continue;
            DislocationNode* oldSecondNode = oldNode->junctionRing;
            DislocationNode* newNode = newLine->nodes[nodeIndex];
            newNode->junctionRing = clone->lines()[oldNode->junctionRing->line->id]->nodes[oldSecondNode->isForwardNode() ? 0 : 1];
        }
    }

#ifdef OVITO_DEBUG
    for(int lineIndex = 0; lineIndex < lines().size(); lineIndex++) {
        DislocationLine* oldLine = lines()[lineIndex];
        DislocationLine* newLine = clone->lines()[lineIndex];
        for(int nodeIndex = 0; nodeIndex < 2; nodeIndex++) {
            OVITO_ASSERT(oldLine->nodes[nodeIndex]->countJunctionArms() == newLine->nodes[nodeIndex]->countJunctionArms());
        }
    }
#endif

    return clone;
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void DislocationNetwork::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(clusterGraph)) {
        // If the cluster graph gets replaced with a new copy, update our cluster references
        // by remapping them to the new graph.
        if(oldTarget && newTarget) {
            ClusterGraph* newGraph = static_object_cast<ClusterGraph>(newTarget);
            for(DislocationLine* line : lines()) {
                if(line->burgersVector.cluster()) {
                    OVITO_ASSERT(static_object_cast<ClusterGraph>(oldTarget)->findCluster(line->burgersVector.cluster()->id));
                    line->burgersVector = ClusterVector(line->burgersVector.localVec(), newGraph->findCluster(line->burgersVector.cluster()->id));
                }
            }
        }
    }

    PeriodicDomainObject::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Allocates a new dislocation line terminated by two nodes.
******************************************************************************/
DislocationLine* DislocationNetwork::createLine(const ClusterVector& burgersVector)
{
    DislocationNode* forwardNode  = _nodePool.construct();
    DislocationNode* backwardNode = _nodePool.construct();

    DislocationLine* line = _linePool.construct(burgersVector, forwardNode, backwardNode);
    line->id = _lines.size();
    _lines.push_back(line);

    return line;
}

/******************************************************************************
* Removes a line from the list of lines.
******************************************************************************/
void DislocationNetwork::discardLine(DislocationLine* line)
{
    OVITO_ASSERT(line != nullptr);
    auto i = std::find(_lines.begin(), _lines.end(), line);
    OVITO_ASSERT(i != _lines.end());
    _lines.erase(i);
}

/******************************************************************************
* Smoothens and coarsens the dislocation lines.
******************************************************************************/
void DislocationNetwork::smoothDislocationLines(int lineSmoothingLevel, FloatType linePointInterval, TaskProgress& progress)
{
    progress.setMaximum(lines().size());

    for(DislocationLine* line : lines()) {
        progress.incrementValue();
        std::deque<Point3> vertices;
        coarsenDislocationLine(linePointInterval, line->vertices, line->coreSize, vertices, line->isClosedLoop(), line->isInfiniteLine());
        smoothDislocationLine(lineSmoothingLevel, vertices, line->isClosedLoop());
        line->vertices = std::move(vertices);
        line->coreSize.clear(); // core size info is no longer valid after smoothing
    }
}

/******************************************************************************
* Removes some of the sampling points from a dislocation line.
******************************************************************************/
void DislocationNetwork::coarsenDislocationLine(FloatType linePointInterval, const std::deque<Point3>& input, const std::deque<int>& coreSize, std::deque<Point3>& output, bool isClosedLoop, bool isInfiniteLine)
{
    OVITO_ASSERT(input.size() >= 2);
    OVITO_ASSERT(input.size() == coreSize.size() || coreSize.empty());
    constexpr int ConstantCoreSize = 6;

    if(linePointInterval <= 0) {
        output = input;
        return;
    }

    // Special handling for infinite lines.
    if(isInfiniteLine && input.size() >= 3) {
        int count = input.size() - 1;
        int coreSizeSum = !coreSize.empty() ? std::accumulate(coreSize.cbegin(), coreSize.cend() - 1, 0) : (ConstantCoreSize * count);
        if(coreSizeSum * linePointInterval > count * count) {
            // Make it a straight line.
            Vector3 com = Vector3::Zero();
            for(auto p = input.cbegin(); p != input.cend() - 1; ++p)
                com += *p - input.front();
            output.push_back(input.front() + com / count);
            output.push_back(input.back() + com / count);
            return;
        }
    }

    // Special handling for very short segments.
    if(input.size() < 4) {
        output = input;
        return;
    }

    // Always keep the end points of linear segments fixed to not break junctions.
    if(!isClosedLoop) {
        output.push_back(input.front());
    }

    // Resulting line must contain at least two points (the end points).
    int minNumPoints = 2;

    // If the dislocation forms a loop, keep at least four points, because two points do not make a proper loop.
    if(input.front().equals(input.back(), CA_ATOM_VECTOR_EPSILON))
        minNumPoints = 4;

    auto inputPtr = input.cbegin();
    auto inputCoreSizePtr = coreSize.cbegin();

    int sum = 0;
    int count = 0;

    // Average over a half interval, starting from the beginning of the segment.
    Vector3 com = Vector3::Zero();
    do {
        if(!coreSize.empty())
            sum += *inputCoreSizePtr++;
        else
            sum += ConstantCoreSize;
        com += *inputPtr++ - input.front();
        count++;
    }
    while(2*count*count < (int)(linePointInterval * sum) && count+1 < input.size() / minNumPoints / 2);

    // Average over a half interval, starting from the end of the segment.
    auto inputPtrEnd = input.cend() - 1;
    auto inputCoreSizePtrEnd = coreSize.rbegin();
    OVITO_ASSERT(inputPtr < inputPtrEnd);
    while(count*count < (int)(linePointInterval * sum) && count < input.size() / minNumPoints) {
        if(!coreSize.empty())
            sum += *inputCoreSizePtrEnd++;
        else
            sum += ConstantCoreSize;
        com += *inputPtrEnd-- - input.back();
        count++;
    }
    OVITO_ASSERT(inputPtr < inputPtrEnd);

    if(isClosedLoop) {
        output.push_back(input.front() + com / count);
    }

    while(inputPtr < inputPtrEnd)
    {
        int sum = 0;
        int count = 0;
        Vector3 com = Vector3::Zero();
        do {
            if(!coreSize.empty())
                sum += *inputCoreSizePtr++;
            else
                sum += ConstantCoreSize;
            com.x() += inputPtr->x();
            com.y() += inputPtr->y();
            com.z() += inputPtr->z();
            ++inputPtr;
            count++;
        }
        while(count*count < (int)(linePointInterval * sum) && count+1 < input.size() / minNumPoints && inputPtr != inputPtrEnd);
        output.push_back(Point3::Origin() + com / count);
    }

    if(!isClosedLoop) {
        // Always keep the end points of linear segments to not break junctions.
        output.push_back(input.back());
    }
    else {
        output.push_back(input.back() + com / count);
    }

    OVITO_ASSERT(output.size() >= minNumPoints);
    OVITO_ASSERT(!isClosedLoop || isInfiniteLine || output.size() >= 3);
}

/******************************************************************************
* Smoothes the sampling points of a dislocation line.
******************************************************************************/
void DislocationNetwork::smoothDislocationLine(int smoothingLevel, std::deque<Point3>& line, bool isLoop)
{
    if(smoothingLevel <= 0)
        return; // Nothing to do.

    if(line.size() <= 2)
        return; // Nothing to do.

    if(line.size() <= 4 && line.front().equals(line.back(), CA_ATOM_VECTOR_EPSILON))
        return; // Do not smooth loops consisting of very few segments.

    // This is the 2d implementation of the mesh smoothing algorithm:
    //
    // Gabriel Taubin
    // A Signal Processing Approach To Fair Surface Design
    // In SIGGRAPH 95 Conference Proceedings, pages 351-358 (1995)

    FloatType k_PB = 0.1f;
    FloatType lambda = 0.5f;
    FloatType mu = 1.0f / (k_PB - 1.0f/lambda);
    const FloatType prefactors[2] = { lambda, mu };

    std::vector<Vector3> laplacians(line.size());
    for(int iteration = 0; iteration < smoothingLevel; iteration++) {

        for(int pass = 0; pass <= 1; pass++) {
            // Compute discrete Laplacian for each point.
            auto l = laplacians.begin();
            if(isLoop == false)
                (*l++).setZero();
            else
                (*l++) = ((*(line.end()-2) - *(line.end()-3)) + (*(line.begin()+1) - line.front())) * FloatType(0.5);

            auto p1 = line.cbegin();
            auto p2 = line.cbegin() + 1;
            for(;;) {
                auto p0 = p1;
                ++p1;
                ++p2;
                if(p2 == line.cend())
                    break;
                *l++ = ((*p0 - *p1) + (*p2 - *p1)) * FloatType(0.5);
            }

            *l++ = laplacians.front();
            OVITO_ASSERT(l == laplacians.end());

            auto lc = laplacians.cbegin();
            for(Point3& p : line) {
                p += prefactors[pass] * (*lc++);
            }
        }
    }
}

/******************************************************************************
* Aligns the directions of dislocation lines as much as possible.
******************************************************************************/
void DislocationNetwork::alignDislocationLineDirections()
{
    for(DislocationLine* line : lines()) {
        const std::deque<Point3>& vertices = line->vertices;
        OVITO_ASSERT(vertices.size() >= 2);

        Vector3 dir = vertices.back() - vertices.front();
        if(dir.isZero(CA_ATOM_VECTOR_EPSILON))
            continue;

        if(std::abs(dir.x()) > std::abs(dir.y())) {
            if(std::abs(dir.x()) > std::abs(dir.z())) {
                if(dir.x() >= 0.0) continue;
            }
            else {
                if(dir.z() >= 0.0) continue;
            }
        }
        else {
            if(std::abs(dir.y()) > std::abs(dir.z())) {
                if(dir.y() >= 0.0) continue;
            }
            else {
                if(dir.z() >= 0.0) continue;
            }
        }

        line->flipOrientation();
    }
}

/******************************************************************************
* Computes statistical information on the identified dislocation lines and
* outputs it to the pipeline as data tables and global attributes.
******************************************************************************/
FloatType DislocationNetwork::generateDislocationStatistics(const OOWeakRef<const PipelineNode>& pipelineNode, PipelineFlowState& state, bool replaceDataObjects, const MicrostructurePhase* defaultStructure) const
{
    std::map<const BurgersVectorFamily*, FloatType> dislocationLengths;
    std::map<const BurgersVectorFamily*, int> segmentCounts;
    std::map<const BurgersVectorFamily*, const MicrostructurePhase*> dislocationCrystalStructures;

    const BurgersVectorFamily* defaultFamily = nullptr;
    if(defaultStructure) {
        defaultFamily = defaultStructure->defaultBurgersVectorFamily();
        for(const BurgersVectorFamily* family : defaultStructure->burgersVectorFamilies()) {
            dislocationLengths[family] = 0;
            segmentCounts[family] = 0;
            dislocationCrystalStructures[family] = defaultStructure;
        }
    }

    // Classify, count and measure length of dislocation segments.
    FloatType totalLineLength = 0;
    for(const DislocationLine* line : lines()) {
        FloatType len = line->calculateLength();
        totalLineLength += len;

        Cluster* cluster = line->burgersVector.cluster();
        OVITO_ASSERT(cluster != nullptr);
        const MicrostructurePhase* structure = structureById(cluster->structure);
        if(structure == nullptr) continue;
        const BurgersVectorFamily* family = defaultFamily;
        if(structure == defaultStructure) {
            family = structure->defaultBurgersVectorFamily();
            for(const BurgersVectorFamily* f : structure->burgersVectorFamilies()) {
                if(f->isMember(line->burgersVector.localVec(), structure)) {
                    family = f;
                    break;
                }
            }
        }
        if(family) {
            segmentCounts[family]++;
            dislocationLengths[family] += len;
            dislocationCrystalStructures[family] = structure;
        }
    }

    // Output a data table with the dislocation line lengths.
    int maxId = 0;
    for(const auto& entry : dislocationLengths) maxId = std::max(maxId, entry.first->numericId());
    PropertyPtr dislocationLengthsProperty = DataTable::OOClass().createUserProperty(
        DataBuffer::Initialized, maxId + 1, DataBuffer::FloatDefault, 1, tr("Total line length"));
    BufferWriteAccess<FloatType, access_mode::write> dislocationLengthsAccess(dislocationLengthsProperty);
    for(const auto& entry : dislocationLengths) dislocationLengthsAccess[entry.first->numericId()] = entry.second;
    dislocationLengthsAccess.reset();
    PropertyPtr dislocationTypeIds = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, maxId + 1, DataBuffer::Int32, 1,
                                                                             tr("Dislocation type"));
    boost::algorithm::iota_n(BufferWriteAccess<int32_t, access_mode::discard_write>(dislocationTypeIds).begin(), 0,
                             dislocationTypeIds->size());

    for(const auto& entry : dislocationLengths) dislocationTypeIds->addElementType(entry.first);

    DataTable* lengthTableObj =
        replaceDataObjects ? state.getMutableLeafObject<DataTable>(DataTable::OOClass(), QStringLiteral("disloc-lengths")) : nullptr;
    if(!lengthTableObj) {
        lengthTableObj = state.createObject<DataTable>(QStringLiteral("disloc-lengths"), pipelineNode, DataTable::BarChart,
                                                       tr("Dislocation lengths"),
                                                       std::move(dislocationLengthsProperty), std::move(dislocationTypeIds));
        lengthTableObj->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(DataTable::plotMode)});
    }
    else {
        ConstPropertyPtr x = std::move(dislocationTypeIds);
        ConstPropertyPtr y = std::move(dislocationLengthsProperty);
        lengthTableObj->setContent(maxId + 1, DataRefVector<Property>{{y, x}});
        lengthTableObj->setX(std::move(x));
        lengthTableObj->setY(std::move(y));
    }

    // Output a data table with the dislocation segment counts.
    PropertyPtr dislocationCountsProperty = DataTable::OOClass().createUserProperty(
        DataBuffer::Initialized, maxId + 1, DataBuffer::Int32, 1, tr("Dislocation count"));
    BufferWriteAccessAndRef<int32_t, access_mode::write> dislocationCountsAccess(dislocationCountsProperty);
    for(const auto& entry : segmentCounts) dislocationCountsAccess[entry.first->numericId()] = entry.second;
    dislocationCountsAccess.reset();

    DataTable* countTableObj =
        replaceDataObjects ? state.getMutableLeafObject<DataTable>(DataTable::OOClass(), QStringLiteral("disloc-counts")) : nullptr;
    if(!countTableObj) {
        countTableObj =
            state.createObject<DataTable>(QStringLiteral("disloc-counts"), pipelineNode, DataTable::BarChart,
                                          tr("Dislocation counts"), std::move(dislocationCountsProperty));
        countTableObj->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(DataTable::plotMode)});
    }
    else
        countTableObj->setContent(maxId + 1, DataRefVector<Property>{{std::move(dislocationCountsProperty)}});
    countTableObj->insertProperty(0, lengthTableObj->x());
    countTableObj->setX(lengthTableObj->x());

    if(replaceDataObjects)
        state.setAttribute(QStringLiteral("DislocationAnalysis.total_line_length"), QVariant::fromValue(totalLineLength), pipelineNode);
    else
        state.addAttribute(QStringLiteral("DislocationAnalysis.total_line_length"), QVariant::fromValue(totalLineLength), pipelineNode);

    for(const auto& dlen : dislocationLengths) {
        const MicrostructurePhase* structure = dislocationCrystalStructures[dlen.first];
        QString bstr;
        if(dlen.first->burgersVector() != Vector3::Zero()) {
            bstr = DislocationVis::formatBurgersVector(dlen.first->burgersVector(), structure);
            bstr.remove(QChar(' '));
            bstr.replace(QChar('['), QChar('<'));
            bstr.replace(QChar(']'), QChar('>'));
        }
        else
            bstr = "other";
        if(replaceDataObjects)
            state.setAttribute(QStringLiteral("DislocationAnalysis.length.%1").arg(bstr), QVariant::fromValue(dlen.second), pipelineNode);
        else
            state.addAttribute(QStringLiteral("DislocationAnalysis.length.%1").arg(bstr), QVariant::fromValue(dlen.second), pipelineNode);
    }

    return totalLineLength;
}

}   // End of namespace
