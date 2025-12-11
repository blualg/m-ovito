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
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/particles/objects/ParticleBondMap.h>
#include "BondsVis.h"
#include "ParticlesVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(BondsVis);
OVITO_CLASSINFO(BondsVis, "DisplayName", "Bonds");
IMPLEMENT_ABSTRACT_OVITO_CLASS(BondPickInfo);
DEFINE_PROPERTY_FIELD(BondsVis, bondWidth);
DEFINE_PROPERTY_FIELD(BondsVis, bondColor);
DEFINE_PROPERTY_FIELD(BondsVis, shadingMode);
DEFINE_PROPERTY_FIELD(BondsVis, coloringMode);
DEFINE_PROPERTY_FIELD(BondsVis, filledSegments);
DEFINE_PROPERTY_FIELD(BondsVis, filledFraction);
SET_PROPERTY_FIELD_LABEL(BondsVis, bondWidth, "Bond width");
SET_PROPERTY_FIELD_LABEL(BondsVis, bondColor, "Uniform bond color");
SET_PROPERTY_FIELD_LABEL(BondsVis, shadingMode, "Shading mode");
SET_PROPERTY_FIELD_LABEL(BondsVis, coloringMode, "Coloring mode");
SET_PROPERTY_FIELD_LABEL(BondsVis, filledSegments, "Number of filled segments");
SET_PROPERTY_FIELD_LABEL(BondsVis, filledFraction, "Filled segment fraction");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(BondsVis, bondWidth, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(BondsVis, filledSegments, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(BondsVis, filledFraction, FloatParameterUnit, 0, 1);

/******************************************************************************
 * Computes the bounding box of the visual element.
 ******************************************************************************/
Box3 BondsVis::boundingBoxImmediate(AnimationTime time,
                                    const ConstDataObjectPath& path,
                                    const Pipeline* pipeline,
                                    const PipelineFlowState& flowState,
                                    TimeInterval& validityInterval)
{
    const Bonds* bonds = path.lastAs<Bonds>();
    const Particles* particles = path.nextToLastAs<Particles>();
    if(!bonds || !particles) return {};
    particles->verifyIntegrity();
    bonds->verifyIntegrity();
    const Property* bondTopologyProperty = bonds->getProperty(Bonds::TopologyProperty);
    const Property* bondPeriodicImageProperty = bonds->getProperty(Bonds::PeriodicImageProperty);
    const Property* bondWidthProperty = bonds->getProperty(Bonds::WidthProperty);
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    const SimulationCell* simulationCell = flowState.getObject<SimulationCell>();

    // Compute bounding box from bond data.
    if(bondTopologyProperty && positionProperty) {
        Box3 bbox;

        BufferReadAccess<ParticleIndexPair> bondTopology(bondTopologyProperty);
        BufferReadAccess<Vector3I> bondPeriodicImages(bondPeriodicImageProperty);
        BufferReadAccess<Point3> positions(positionProperty);

        size_t particleCount = positions.size();
        const AffineTransformation cell = simulationCell ? simulationCell->cellMatrix() : AffineTransformation::Zero();

        for(size_t bondIndex = 0; bondIndex < bondTopology.size(); bondIndex++) {
            size_t index1 = bondTopology[bondIndex][0];
            size_t index2 = bondTopology[bondIndex][1];
            if(index1 >= particleCount || index2 >= particleCount) continue;

            bbox.addPoint(positions[index1]);
            bbox.addPoint(positions[index2]);
            if(bondPeriodicImages && bondPeriodicImages[bondIndex] != Vector3I::Zero()) {
                Vector3 vec = positions[index2] - positions[index1];
                const Vector3I& pbcShift = bondPeriodicImages[bondIndex];
                for(size_t k = 0; k < 3; k++) {
                    if(pbcShift[k] != 0) vec += cell.column(k) * (FloatType)pbcShift[k];
                }
                bbox.addPoint(positions[index1] + (vec * FloatType(0.5)));
                bbox.addPoint(positions[index2] - (vec * FloatType(0.5)));
            }
        }

        // Extend box to account for width of bonds.
        GraphicsFloatType maxBondWidth = std::max(static_cast<GraphicsFloatType>(bondWidth()), GraphicsFloatType(0));
        if(bondWidthProperty && bondWidthProperty->size() != 0) {
            BufferReadAccess<GraphicsFloatType> widthArray(bondWidthProperty);
            auto minmax = std::minmax_element(widthArray.cbegin(), widthArray.cend());
            if(*minmax.first <= 0)
                maxBondWidth = std::max(maxBondWidth, *minmax.second);
            else
                maxBondWidth = *minmax.second;
        }

        return bbox.padBox(maxBondWidth / FloatType(2) * std::sqrt(FloatType(3)));
    }
    return {};
}

namespace {

/******************************************************************************
 * Check whether the bond order is a fractional or integer
 ******************************************************************************/
inline bool isFractionalBond(GraphicsFloatType bondOrder)
{
    OVITO_ASSERT(bondOrder >= (GraphicsFloatType)0 && bondOrder <= GraphicsFloatType(3 + 1e-9));
    return std::abs(std::round(bondOrder) - bondOrder) > (GraphicsFloatType)1e-9;
}

/******************************************************************************
 * Calculate the out of plane vector (normal) for each bond and its neighbors
 * to orient the bond cylinders
 * The normal is estimated as the accumulated normal (cross product)
 * of all neihbor bond combinations
 * For bond "networks" perfectly aligned with the z-axis alginment along x is
 * taken as fallback
 * Estimate the center of the current partial bond "network" for fractional
 * bonds to draw the dashed bond always towards the ring center
 ******************************************************************************/
std::pair<Vector3G, Vector3G> getBondNormalVector(const ParticleIndexPair& particleIndices,
                                                  const std::optional<ParticleBondMap>& bondsMap,
                                                  const BufferReadAccess<Point3>& positions,
                                                  const BufferReadAccess<GraphicsFloatType>& bondOrder,
                                                  const SimulationCell* cell)
{
    if(bondsMap) {
        // Normals towards x and z-axis
        Vector3G normal_z = Vector3G::Zero();
        Vector3G normal_x = Vector3G::Zero();
        // Estimate the center of the current partial bond "network"
        Vector3G direction = Vector3G::Zero();

        std::array<Vector3G, 2> neighbors;
        for(size_t index : particleIndices) {
            size_t count = 0;

            const Point3& p1 = positions[index];
            // Loop over all bonded neighbors of the two particles making up the bond
            for(const BondWithIndex& bond : bondsMap->bondsOfParticle(index)) {
                OVITO_ASSERT(bond.index1 == index);
                OVITO_ASSERT(bond.index2 != index);

                // Compute bond vector and its length.
                const Point3& p1 = positions[bond.index1];
                const Point3& p2 = positions[bond.index2];
                Vector3G delta1 = (p2 - p1).toDataType<GraphicsFloatType>();
                if(cell) {
                    delta1 += (cell * bond.pbcShift).toDataType<GraphicsFloatType>();
                }
                neighbors[count % neighbors.size()] = delta1;

                // Center only relevant for fractional bonds
                if(bondOrder && isFractionalBond(bondOrder[bond.bondIndex])) {
                    direction += delta1;
                }

                // Compute and accumulate normals
                if(count >= neighbors.size() - 1) {
                    Vector3G localNormal = neighbors[0].cross(neighbors[1]);
                    if(!localNormal.normalizeSafely()) {
                        localNormal = Vector3G{0, 0, 1};
                    }
                    GraphicsFloatType dot = localNormal.dot(Vector3G(0, 0, 1));
                    if(dot > (GraphicsFloatType)0) {
                        normal_z += localNormal;
                    }
                    else {
                        normal_z -= localNormal;
                    }
                    dot = localNormal.dot(Vector3G(1, 0, 0));
                    if(dot > (GraphicsFloatType)0) {
                        normal_x += localNormal;
                    }
                    else {
                        normal_x -= localNormal;
                    }
                }
                count++;
            }
        }
        return {normal_z.squaredLength() <= 1e-18 ? normal_x : normal_z, direction};
    }
    return {{0, 0, 1}, Vector3G::Zero()};
}

/******************************************************************************
 * Calculate the (reduced) bond width based on the bond order
 ******************************************************************************/
inline GraphicsFloatType getBondWidth(const BufferReadAccess<GraphicsFloatType>& bondInputWidths,
                                      size_t bondIndex,
                                      FloatType bondDiameter,
                                      size_t bondRepCount)
{
    OVITO_ASSERT(bondRepCount >= 0 && bondRepCount <= 3);
    FloatType bondWidthValue = bondDiameter;
    bondWidthValue = (bondInputWidths && bondInputWidths[bondIndex] > GraphicsFloatType(0)) ? bondInputWidths[bondIndex] : bondDiameter;
    if(bondRepCount > 1 && bondRepCount <= 2) {
        bondWidthValue *= (GraphicsFloatType)0.8;
    }
    else if(bondRepCount > 2) {
        bondWidthValue *= (GraphicsFloatType)0.63;
    }
    return (GraphicsFloatType)bondWidthValue;
}

/******************************************************************************
 * Shift bonds based on the bond order to avoid overlapping bond cylinders
 ******************************************************************************/
inline GraphicsFloatType getBondShiftFactor(size_t bondRepCount)
{
    OVITO_ASSERT(bondRepCount >= 0 && bondRepCount <= 3);
    constexpr static std::array<GraphicsFloatType, 4> bondShiftFactors = {0, 1, 1.25, 2.5};
    return bondShiftFactors[bondRepCount];
}

/******************************************************************************
 Determine where to shift vector for each bond to avoid overlapping bond cylinders
 ******************************************************************************/
inline Vector3G getBondOffsetVector(size_t bondRepCount,
                                    size_t bondRepIdx,
                                    const Vector3G& gvec,
                                    const Vector3G& normal,
                                    const Vector3G& shiftDir,
                                    GraphicsFloatType bondWidth)
{
    OVITO_ASSERT(bondRepCount >= 0 && bondRepCount <= 3);
    OVITO_ASSERT(bondRepIdx >= 0 && bondRepIdx < 3);

    constexpr static std::array<std::array<FloatType, 3>, 3> bondPosOffset{
        {{{0.0, std::numeric_limits<FloatType>::quiet_NaN(), std::numeric_limits<FloatType>::quiet_NaN()}},
         {{0.5, -0.5, std::numeric_limits<FloatType>::quiet_NaN()}},
         {{0.0, 0.5, -0.5}}}};

    const FloatType offset = bondPosOffset[bondRepCount - 1][bondRepIdx];
    OVITO_ASSERT(!std::isnan(offset));
    Vector3G offSetVec = Vector3G::Zero();
    if(bondRepCount > 1) {
        // Shift vector points towards the local center of fractional bonds
        // -> used to have the dashed bond on the inside of rings
        Vector3G cross = gvec.cross(normal).normalized();
        if(shiftDir != Vector3G::Zero() && cross.dot(shiftDir) >= 0) {
            cross = -cross;
        }
        offSetVec = cross * (GraphicsFloatType)offset * getBondShiftFactor(bondRepCount) * bondWidth;
    }
    return offSetVec;
}

/******************************************************************************
 Determine the number of cylinders to be rendered for the given dashed bond based on the number of filled segments
 Used to preallocated all the output arrays.
 ******************************************************************************/
inline size_t filledSegmentsToCylinderCount(const int filledSegments)
{
    if(filledSegments == 0) {
        return 0;
    }

    // Needs to be odd for center line symmetry in the dashes
    OVITO_ASSERT(filledSegments % 2 == 1);

    // Use hard coded values to estimate the number of segments
    constexpr GraphicsFloatType totalLength = 1;
    constexpr GraphicsFloatType filledFraction = 0.5;

    // Number of odd indices (on)
    const GraphicsFloatType numOnSegments = filledSegments / 2;
    // Lenght of on segment
    const GraphicsFloatType onLength = (numOnSegments > 0) ? filledFraction * totalLength / numOnSegments : 0;

    //  Number of even indices (off)
    const GraphicsFloatType numOffSegments = (filledSegments + 1) / 2;
    // Length of off segment
    const GraphicsFloatType offLength = (numOffSegments > 0) ? (1 - filledFraction) * totalLength / numOffSegments : 0;

    GraphicsFloatType currentPos = 0;
    GraphicsFloatType accumulatedLength = 0;
    size_t count = 0;
    for(size_t i = 0; i < filledSegments; i++) {
        const bool on = (i % 2 == 1);
        if(on) {
            // On segment
            if(accumulatedLength < 0.5 * totalLength && accumulatedLength + onLength >= 0.5 * totalLength) {
                // Extra cylinder is needed to handle color change in on region
                count += 2;
            }
            else {
                // On segment
                count += 1;
            }
        }
        accumulatedLength += on ? onLength : offLength;
    }

    return count;
}

/******************************************************************************
 * Determines the number of cylinders to be rendered for the given bond order
 * based on the number of filled segments.
 * Used to preallocated all the output arrays.
 ******************************************************************************/
inline size_t bondOrderToCylinderCount(const GraphicsFloatType bondOrder, const size_t cylindersPerFilledSegment)
{
    GraphicsFloatType intPart;
    const GraphicsFloatType remainder = std::modf(bondOrder, &intPart);
    size_t count = 2 * (size_t)std::clamp(intPart, (GraphicsFloatType)0, (GraphicsFloatType)3);
    if(remainder > (GraphicsFloatType)1e-9) {
        count += cylindersPerFilledSegment;
    }
    return count;
}

}  // namespace

/******************************************************************************
 * Determines the number of cylinders to be rendered for the given bond topology.
 * Used to preallocated all the output arrays.
 ******************************************************************************/
size_t BondsVis::getCylinderCount(const Property* bondTopologyProperty, const Property* bondOrderProperty, const int filledSegments)
{
    // No bond topology
    if(!bondTopologyProperty) {
        return 0;
    }
    // Default case, no bond order
    if(!bondOrderProperty) {
        return 2 * bondTopologyProperty->size();
    }
    OVITO_ASSERT(bondTopologyProperty->size() == bondOrderProperty->size());
    OVITO_ASSERT(filledSegments >= 0);

    // Estimate the number of cylinders that will be required per dashed bond
    const size_t cylindersPerFilledSegment = filledSegmentsToCylinderCount(filledSegments);

    // Count the number of cylinders that will be required
    size_t count = 0;
    BufferReadAccess<GraphicsFloatType> bondOrderAcc(bondOrderProperty);
    for(GraphicsFloatType bondOrder : bondOrderAcc) {
        // Bond order can be in the range from 0 to 3
        count += bondOrderToCylinderCount(bondOrder, cylindersPerFilledSegment);
    }
    return count;
}

/******************************************************************************
 * Lets the visualization element render the data object.
 ******************************************************************************/
std::variant<PipelineStatus, Future<PipelineStatus>> BondsVis::render(const ConstDataObjectPath& path,
                                                                      const PipelineFlowState& flowState,
                                                                      FrameGraph& frameGraph,
                                                                      const SceneNode* sceneNode)
{
    const Bonds* bonds = path.lastAs<Bonds>();
    const Particles* particles = path.nextToLastAs<Particles>();
    if(!bonds || !particles) return {};
    particles->verifyIntegrity();
    bonds->verifyIntegrity();
    const Property* bondTopologyProperty = bonds->getProperty(Bonds::TopologyProperty);
    const Property* bondPeriodicImageProperty = bonds->getProperty(Bonds::PeriodicImageProperty);
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    const SimulationCell* simulationCell = flowState.getObject<SimulationCell>();
    const Property* bondTypeProperty = (coloringMode() == ByTypeColoring) ? bonds->getProperty(Bonds::TypeProperty) : nullptr;
    const Property* bondColorProperty = bonds->getProperty(Bonds::ColorProperty);
    const Property* bondWidthProperty = bonds->getProperty(Bonds::WidthProperty);
    const Property* bondSelectionProperty = frameGraph.isInteractive() ? bonds->getProperty(Bonds::SelectionProperty) : nullptr;
    const Property* transparencyProperty = bonds->getProperty(Bonds::TransparencyProperty);
    const Property* bondOrderProperty = bonds->getProperty(Bonds::OrderProperty);

    // Obtain particle-related properties and the vis element.
    const ParticlesVis* particleVis = particles->visElement<ParticlesVis>();
    const Property* particleRadiusProperty = particles->getProperty(Particles::RadiusProperty);
    const Property* particleTransparencyProperty = particles->getProperty(Particles::TransparencyProperty);
    const Property* particleColorProperty = nullptr;
    const Property* particleTypeProperty = nullptr;
    if(coloringMode() == ParticleBasedColoring && particleVis) {
        particleColorProperty = particles->getProperty(Particles::ColorProperty);
        particleTypeProperty = particleVis->getParticleTypeColorProperty(particles);
    }

    // Needs to be odd for center line symmetry in the dashes
    const int numFilledSegments = (2 * filledSegments()) + 1;

    // Make sure the primitive for the nodal vertices gets created if particles display is turned off or if particles are semi-transparent.
    const bool useBondOrder = bondOrderProperty ? std::ranges::any_of(BufferReadAccess<GraphicsFloatType>(bondOrderProperty),
                                                                      [](GraphicsFloatType bondOrder) { return bondOrder != 1; })
                                                : false;

    // Count the number of cylinders that will be required for each dashed bond
    const size_t dashedCylinderCount = useBondOrder ? filledSegmentsToCylinderCount(numFilledSegments) : 0;

    // Make sure we don't exceed our internal limits.
    const size_t cylinderCount = getCylinderCount(bondTopologyProperty, bondOrderProperty, numFilledSegments);

    if(bondTopologyProperty && cylinderCount > (size_t)std::numeric_limits<int>::max()) {
        throw Exception(tr("This version of OVITO cannot render more than %1 bonds.").arg(std::numeric_limits<int>::max() / 2));
    }

    // The key type used for caching the rendering primitive:
    using CacheKey = RendererResourceKey<struct BondsVisCache,
                                         ConstDataObjectRef,  // Bond topology property
                                         ConstDataObjectRef,  // Bond PBC vector property
                                         ConstDataObjectRef,  // Particle position property
                                         ConstDataObjectRef,  // Particle color property
                                         ConstDataObjectRef,  // Particle type property
                                         ConstDataObjectRef,  // Particle radius property
                                         ConstDataObjectRef,  // Bond color property
                                         ConstDataObjectRef,  // Bond type property
                                         ConstDataObjectRef,  // Bond selection property
                                         ConstDataObjectRef,  // Bond transparency
                                         ConstDataObjectRef,  // Bond order
                                         ConstDataObjectRef,  // Bond width
                                         ConstDataObjectRef,  // Simulation cell
                                         FloatType,           // Bond width
                                         FloatType,           // Radius scaling factor
                                         int,                 // Filled segments
                                         FloatType,           // Filled faction
                                         Color,               // Bond uniform color
                                         ColoringMode,        // Bond coloring mode
                                         ShadingMode,         // Bond shading mode
                                         bool                 // Render nodal vertices
                                         >;

    const bool renderNodalVertices = !useBondOrder && !transparencyProperty && !bondWidthProperty &&
                                     (!particleVis || !particleVis->isEnabled() || particleTransparencyProperty != nullptr);

    // Lookup the rendering primitive in the vis cache.
    const auto& [cylinders, vertices, pickInfo] =
        frameGraph.visCache().lookup<std::tuple<CylinderPrimitive, ParticlePrimitive, OORef<BondPickInfo>>>(
            CacheKey(bondTopologyProperty,
                     bondPeriodicImageProperty,
                     positionProperty,
                     particleColorProperty,
                     particleTypeProperty,
                     particleRadiusProperty,
                     bondColorProperty,
                     bondTypeProperty,
                     bondSelectionProperty,
                     transparencyProperty,
                     bondOrderProperty,
                     bondWidthProperty,
                     simulationCell,
                     bondWidth(),
                     particleVis->radiusScaleFactor(),
                     numFilledSegments,
                     filledFraction(),
                     bondColor(),
                     coloringMode(),
                     shadingMode(),
                     renderNodalVertices),
            [&](CylinderPrimitive& cylinders, ParticlePrimitive& vertices, OORef<BondPickInfo>& pickInfo) {
                const FloatType bondDiameter = bondWidth();
                // Remap cylinders to bonds
                auto subobjectToBondMapping = (useBondOrder) ? BufferFactory<int32_t>(cylinderCount) : BufferFactory<int32_t>();

                if(bondTopologyProperty && positionProperty && bondDiameter > 0) {
                    // Allocate buffers for the bonds geometry.
                    BufferFactory<Point3G> bondPositions1(cylinderCount);
                    BufferFactory<Point3G> bondPositions2(cylinderCount);
                    BufferFactory<ColorG> bondColors(cylinderCount);
                    BufferFactory<GraphicsFloatType> bondTransparencies =
                        transparencyProperty ? BufferFactory<GraphicsFloatType>(cylinderCount) : BufferFactory<GraphicsFloatType>{};
                    BufferFactory<GraphicsFloatType> bondWidths = bondWidthProperty || bondOrderProperty
                                                                      ? BufferFactory<GraphicsFloatType>(cylinderCount)
                                                                      : BufferFactory<GraphicsFloatType>{};

                    // Allocate buffers for the nodal vertices.
                    BufferFactory<ColorG> nodalColors =
                        renderNodalVertices ? BufferFactory<ColorG>(positionProperty->size()) : BufferFactory<ColorG>{};
                    BufferFactory<GraphicsFloatType> nodalTransparencies = (renderNodalVertices && transparencyProperty)
                                                                               ? BufferFactory<GraphicsFloatType>(positionProperty->size())
                                                                               : BufferFactory<GraphicsFloatType>{};
                    BufferFactory<int32_t> nodalIndices = renderNodalVertices ? BufferFactory<int32_t>(0) : BufferFactory<int32_t>{};
                    boost::dynamic_bitset<> visitedParticles(renderNodalVertices ? positionProperty->size() : 0);
                    OVITO_ASSERT(nodalColors || !nodalTransparencies);

                    // Cache some values.
                    BufferReadAccess<Point3> positions(positionProperty);
                    size_t particleCount = positions.size();
                    const AffineTransformation cell = simulationCell ? simulationCell->cellMatrix() : AffineTransformation::Zero();

                    // Obtain the radii of the particles.
                    BufferReadAccessAndRef<GraphicsFloatType> particleRadii;
                    if(particleVis) particleRadii = ConstDataBufferPtr(particleVis->particleRadii(particles, true));
                    // Make sure the particle radius array has the correct
                    // length.
                    if(particleRadii && particleRadii.size() != particleCount) particleRadii.reset();

                    // Determine half-bond colors.
                    std::vector<ColorG> colors = halfBondColors(particles, frameGraph.isInteractive(), coloringMode(), false);
                    OVITO_ASSERT(colors.size() == 2 * bonds->elementCount());

                    // Bonds enumerator used to determine the the normal vector to align double / triple bonds.
                    std::optional<ParticleBondMap> bondsMap;
                    if(useBondOrder) {
                        bondsMap.emplace(bondTopologyProperty, bondPeriodicImageProperty);
                    }

                    size_t cylinderIndex = 0;
                    BufferReadAccess<ParticleIndexPair> bonds(bondTopologyProperty);
                    BufferReadAccess<Vector3I> bondPeriodicImages(bondPeriodicImageProperty);
                    BufferReadAccess<GraphicsFloatType> bondInputTransparency(transparencyProperty);
                    BufferReadAccess<GraphicsFloatType> bondInputWidths(bondWidthProperty);
                    BufferReadAccess<GraphicsFloatType> bondInputOrders(bondOrderProperty);
                    for(size_t bondIndex = 0; bondIndex < bonds.size(); bondIndex++) {
                        // How many bonds do we need to draw for this bond
                        // based on its order?
                        const size_t bondRepCount =
                            useBondOrder
                                ? (size_t)std::clamp(std::ceil(bondInputOrders[bondIndex]), (GraphicsFloatType)0, (GraphicsFloatType)3)
                                : 1;
                        // Skip bonds with zero / negative order- safe
                        // because these are not included in the cylinder
                        // count
                        if(bondInputOrders && bondRepCount <= 0) {
                            continue;
                        }
                        // Get the bond width
                        const GraphicsFloatType bondWidth = getBondWidth(bondInputWidths, bondIndex, bondDiameter, bondRepCount);

                        const ParticleIndexPair& particleIndices = bonds[bondIndex];
                        if(particleIndices[0] < particleCount && particleIndices[1] < particleCount) {
                            // Determine the normal vector and local center direction to align double / triple bonds.
                            auto [normal, centerVec] =
                                (useBondOrder && bondRepCount > 1)
                                    ? getBondNormalVector(particleIndices, bondsMap, positions, bondInputOrders, simulationCell)
                                    : std::make_pair(Vector3G::Zero(), Vector3G::Zero());
                            // Fallback, normal cannot be zero
                            if(!useBondOrder || bondRepCount <= 1 || !normal.normalizeSafely()) {
                                normal = Vector3G{0, 0, 1};
                            }
                            // Bond vector
                            Vector3 bondVec = positions[particleIndices[1]] - positions[particleIndices[0]];
                            bool isSplitBond = false;
                            if(bondPeriodicImageProperty) {
                                for(size_t k = 0; k < 3; k++) {
                                    if(int d = bondPeriodicImages[bondIndex][k]) {
                                        bondVec += cell.column(k) * (FloatType)d;
                                        isSplitBond = true;
                                    }
                                }
                            }
                            const Vector3G& bondVecG = bondVec.toDataType<GraphicsFloatType>();
                            GraphicsFloatType t = 0.5;
                            const GraphicsFloatType blen = bondVecG.length() * GraphicsFloatType(2);
                            if(particleRadii && blen != 0) {
                                // This calculation determines the point
                                // where to split the bond into the two
                                // half-bonds such that the border appears
                                // halfway between the two particles, which
                                // may have two different sizes.
                                t = GraphicsFloatType(0.5) + std::min(GraphicsFloatType(0.5), particleRadii[particleIndices[0]] / blen) -
                                    std::min(GraphicsFloatType(0.5), particleRadii[particleIndices[1]] / blen);
                            }

                            const size_t colorIndex = 2 * bondIndex;
                            OVITO_ASSERT(colorIndex + 1 < colors.size());
                            // Cylinder count can be greater due to dashed bonds
                            const size_t cylinderCount =
                                (useBondOrder) ? bondOrderToCylinderCount(bondInputOrders[bondIndex], numFilledSegments) : 2 * bondRepCount;

                            for(size_t bondRepIdx = 0; bondRepIdx < bondRepCount; bondRepIdx++) {
                                const Vector3G& offSetVec =
                                    getBondOffsetVector(bondRepCount, bondRepIdx, bondVecG, normal, centerVec, bondWidth);
                                if(cylinderCount == 2 * bondRepCount || bondRepIdx != bondRepCount - 1) {
                                    // Solid bond with two cylinders each representing a single half bond
                                    for(uint_fast8_t cylinderIdx = 0; cylinderIdx < 2; cylinderIdx++) {
                                        // Shift individual bonds base on the current bond order
                                        OVITO_ASSERT(cylinderIndex < bondColors.size());
                                        bondColors[cylinderIndex] = colors[colorIndex + cylinderIdx];
                                        if(nodalColors && !visitedParticles.test(particleIndices[cylinderIdx])) {
                                            nodalColors[particleIndices[cylinderIdx]] = bondColors[cylinderIndex];
                                            if(nodalTransparencies)
                                                nodalTransparencies[particleIndices[cylinderIdx]] = bondInputTransparency[bondIndex];
                                            visitedParticles.set(particleIndices[cylinderIdx]);
                                            nodalIndices.push_back(particleIndices[cylinderIdx]);
                                        }
                                        if(bondTransparencies) bondTransparencies[cylinderIndex] = bondInputTransparency[bondIndex];
                                        if(bondWidths) {
                                            bondWidths[cylinderIndex] = bondWidth;
                                        }
                                        bondPositions1[cylinderIndex] =
                                            positions[particleIndices[cylinderIdx]].toDataType<GraphicsFloatType>() + offSetVec;
                                        bondPositions2[cylinderIndex] =
                                            (cylinderIdx == 0) ? bondPositions1[cylinderIndex] + bondVecG * t
                                                               : bondPositions1[cylinderIndex] - bondVecG * (GraphicsFloatType(1) - t);
                                        if(isSplitBond) swap(bondPositions1[cylinderIndex], bondPositions2[cylinderIndex]);
                                        if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                                        cylinderIndex++;
                                    }
                                }
                                else {
                                    // Dashed bond
                                    OVITO_ASSERT(dashedCylinderCount > 0);
                                    std::optional<Vector3> bondVecNorm = bondVec.safelyNormalized<true>();
                                    // Bond has no lenght
                                    if(!bondVecNorm) {
                                        // Bonds of length 0 are skipped
                                        for(size_t cylinderIdx = 0; cylinderIdx < dashedCylinderCount; cylinderIdx++) {
                                            bondColors[cylinderIndex] = colors[colorIndex];
                                            bondPositions1[cylinderIndex] = Point3G::Origin();
                                            bondPositions2[cylinderIndex] = Point3G::Origin();
                                            if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                                            cylinderIndex++;
                                        }
                                    }
                                    else {
                                        const Vector3G bondVecG = bondVec.toDataType<GraphicsFloatType>();
                                        GraphicsFloatType bondLength = bondVecG.length();
                                        const Vector3G bondDirG = bondVecG / bondLength;

                                        // Bond starting position based on particle radius
                                        Point3G currentPos = positions[particleIndices[0]].toDataType<GraphicsFloatType>() + offSetVec;
                                        currentPos += (particleRadii ? particleRadii[particleIndices[0]] * bondDirG : Vector3G{0, 0, 0});
                                        const Point3G startPos = currentPos;

                                        // Bond ending position based on particle radius
                                        Point3G endPos = currentPos + bondVecG;
                                        endPos -= (particleRadii ? particleRadii[particleIndices[1]] * bondDirG : Vector3G{0, 0, 0});

                                        // Reduce bond length by particle radii
                                        bondLength -=
                                            (particleRadii ? (particleRadii[particleIndices[0]] + particleRadii[particleIndices[1]]) : 0);

                                        // Number of on and off segments
                                        const int numOnSegments = filledSegments();
                                        const int numOffSegments = filledSegments() + 1;

                                        // Length of each on and off segment
                                        const GraphicsFloatType onSegmentLength =
                                            numOnSegments > 0
                                                ? (GraphicsFloatType)filledFraction() * bondLength / (GraphicsFloatType)numOnSegments
                                                : 0;
                                        const GraphicsFloatType offSegmentLength =
                                            numOffSegments > 0 ? ((GraphicsFloatType)1 - (GraphicsFloatType)filledFraction()) * bondLength /
                                                                     (GraphicsFloatType)numOffSegments
                                                               : 0;

                                        ColorG& currentColor = colors[colorIndex];
                                        // Length already filled by current dashed segments
                                        GraphicsFloatType accumulatedLength = 0;
                                        const GraphicsFloatType halfBondLength = (GraphicsFloatType)0.5 * bondLength;

                                        // Loop over all bond to draw dashed segments
                                        for(size_t cylinderIdx = 0; cylinderIdx < (numOnSegments + numOffSegments); cylinderIdx++) {
                                            const bool on = cylinderIdx % 2 == 1;
                                            // End position of current segment
                                            endPos = currentPos + bondDirG * (on ? onSegmentLength : offSegmentLength);

                                            if(on) {
                                                // Color transition across half bond
                                                if(accumulatedLength < halfBondLength &&
                                                   accumulatedLength + onSegmentLength >= halfBondLength) {
                                                    const Point3G centerPoint = startPos + bondDirG * halfBondLength;
                                                    // First half of split segment (first color)
                                                    bondPositions1[cylinderIndex] = currentPos;
                                                    bondPositions2[cylinderIndex] = centerPoint;
                                                    bondColors[cylinderIndex] = currentColor;
                                                    if(bondWidths) {
                                                        bondWidths[cylinderIndex] = bondWidth;
                                                    }
                                                    if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                                                    cylinderIndex++;
                                                    // Second half of split segment (second color)
                                                    currentColor = colors[colorIndex + 1];
                                                    bondPositions1[cylinderIndex] = centerPoint;
                                                    bondPositions2[cylinderIndex] = endPos;
                                                    bondColors[cylinderIndex] = currentColor;
                                                    if(bondWidths) {
                                                        bondWidths[cylinderIndex] = bondWidth;
                                                    }
                                                    if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                                                    cylinderIndex++;
                                                }
                                                else {
                                                    // Segment without color change -> full color across full length
                                                    bondPositions1[cylinderIndex] = currentPos;
                                                    bondPositions2[cylinderIndex] = endPos;
                                                    bondColors[cylinderIndex] = currentColor;
                                                    if(bondWidths) {
                                                        bondWidths[cylinderIndex] = bondWidth;
                                                    }
                                                    if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                                                    cylinderIndex++;
                                                }
                                            }
                                            else {
                                                // Bond off - still allow for color transition
                                                if(accumulatedLength < halfBondLength &&
                                                   accumulatedLength + offSegmentLength >= halfBondLength) {
                                                    currentColor = colors[colorIndex + 1];
                                                }
                                            }
                                            // Increase accumulated length by current cylinder length and update current position
                                            accumulatedLength += on ? onSegmentLength : offSegmentLength;
                                            currentPos = endPos;
                                        }
                                    }
                                }
                            }
                        }
                        else {
                            OVITO_ASSERT(2 * bondIndex + 1 < colors.size());
                            bondColors[cylinderIndex] = colors[2 * bondIndex];
                            if(bondTransparencies) bondTransparencies[cylinderIndex] = 0;
                            if(bondWidths) bondWidths[cylinderIndex] = 0;
                            bondPositions1[cylinderIndex] = Point3G::Origin();
                            bondPositions2[cylinderIndex] = Point3G::Origin();
                            if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                            cylinderIndex++;

                            bondColors[cylinderIndex] = colors[2 * bondIndex + 1];
                            if(bondTransparencies) bondTransparencies[cylinderIndex] = 0;
                            if(bondWidths) bondWidths[cylinderIndex] = 0;
                            bondPositions1[cylinderIndex] = Point3G::Origin();
                            bondPositions2[cylinderIndex] = Point3G::Origin();
                            if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                            cylinderIndex++;
                        }
                    }

                    cylinders.setShape(CylinderPrimitive::CylinderShape);
                    cylinders.setShadingMode(static_cast<CylinderPrimitive::ShadingMode>(shadingMode()));
                    cylinders.setRenderSingleCylinderCap(transparencyProperty != nullptr);
                    cylinders.setUniformWidth(bondDiameter);
                    cylinders.setWidths(bondWidths.take());
                    cylinders.setPositions(bondPositions1.take(), bondPositions2.take());
                    cylinders.setColors(bondColors.take());
                    cylinders.setTransparencies(bondTransparencies.take());

                    if(renderNodalVertices) {
                        OVITO_ASSERT(!useBondOrder);
                        OVITO_ASSERT(positionProperty);
                        OVITO_ASSERT(!nodalColors || positionProperty->size() == nodalColors.size());
                        OVITO_ASSERT(!nodalIndices || (positionProperty->size() == nodalIndices.size()));
                        OVITO_ASSERT(!nodalTransparencies || (positionProperty->size() == nodalTransparencies.size()));
                        vertices.setParticleShape(ParticlePrimitive::SphericalShape);
                        vertices.setShadingMode((shadingMode() == NormalShading) ? ParticlePrimitive::NormalShading
                                                                                 : ParticlePrimitive::FlatShading);
                        vertices.setRenderingQuality(ParticlePrimitive::HighQuality);
                        vertices.setPositions(positionProperty);
                        vertices.setUniformRadius(0.5 * bondDiameter);
                        vertices.setColors(nodalColors.take());
                        vertices.setIndices(nodalIndices.take());
                        vertices.setTransparencies(nodalTransparencies.take());
                    }
                }

                pickInfo = OORef<BondPickInfo>::create(particles, simulationCell, subobjectToBondMapping.take());
            });

    if(!cylinders.basePositions()) return {};

    FrameGraph::RenderingCommandGroup& commandGroup = frameGraph.addCommandGroup(FrameGraph::SceneLayer);
    frameGraph.addPrimitive(commandGroup, std::make_unique<CylinderPrimitive>(cylinders), sceneNode, pickInfo);

    if(renderNodalVertices) frameGraph.addPrimitive(commandGroup, std::make_unique<ParticlePrimitive>(vertices), sceneNode);

    return {};
}

/******************************************************************************
 * Determines the display colors of half-bonds.
 * Returns an array with two colors per full bond, because the two half-bonds
 * may have different colors.
 ******************************************************************************/
std::vector<ColorG> BondsVis::halfBondColors(const Particles* particles,
                                             bool highlightSelection,
                                             ColoringMode coloringMode,
                                             bool ignoreBondColorProperty) const
{
    OVITO_ASSERT(particles != nullptr);
    particles->verifyIntegrity();
    const Bonds* bonds = particles->bonds();
    if(!bonds) return {};
    bonds->verifyIntegrity();

    // Get bond-related properties which determine the bond coloring.
    BufferReadAccess<ParticleIndexPair> topologyProperty = bonds->getProperty(Bonds::TopologyProperty);
    BufferReadAccess<ColorG> bondColorProperty = !ignoreBondColorProperty ? bonds->getProperty(Bonds::ColorProperty) : nullptr;
    const Property* bondTypeProperty = (coloringMode == ByTypeColoring) ? bonds->getProperty(Bonds::TypeProperty) : nullptr;
    BufferReadAccess<SelectionIntType> bondSelectionProperty = highlightSelection ? bonds->getProperty(Bonds::SelectionProperty) : nullptr;

    // Get particle-related properties and the vis element.
    const ParticlesVis* particleVis = particles->visElement<ParticlesVis>();

    std::vector<ColorG> output(bonds->elementCount() * 2);
    const ColorG defaultColor = bondColor().toDataType<GraphicsFloatType>();
    if(bondColorProperty && bondColorProperty.size() * 2 == output.size()) {
        // Take bond colors directly from the color property.
        auto bc = output.begin();
        for(const auto& c : bondColorProperty) {
            *bc++ = c;
            *bc++ = c;
        }
    }
    else if(coloringMode == ParticleBasedColoring && particleVis) {
        // Derive bond colors from particle colors.
        size_t particleCount = particles->elementCount();
        BufferReadAccessAndRef<ColorG> particleColors = particleVis->particleColors(particles, false);
        OVITO_ASSERT(particleColors.size() == particleCount);
        auto bc = output.begin();
        for(const auto& bond : topologyProperty) {
            if((size_t)bond[0] < particleCount && (size_t)bond[1] < particleCount) {
                *bc++ = particleColors[bond[0]];
                *bc++ = particleColors[bond[1]];
            }
            else {
                *bc++ = defaultColor;
                *bc++ = defaultColor;
            }
        }
    }
    else {
        if(bondTypeProperty && bondTypeProperty->size() * 2 == output.size()) {
            // Assign colors based on bond types.
            // Generate a lookup map for bond type colors.
            const std::map<int, ColorG>& colorMap = bondTypeProperty->typeColorMap();
            std::array<ColorG, 16> colorArray;
            // Check if all type IDs are within a small, non-negative range.
            // If yes, we can use an array lookup strategy. Otherwise we have to use a dictionary lookup strategy, which is slower.
            if(boost::algorithm::all_of(colorMap,
                                        [&colorArray](const auto& i) { return i.first >= 0 && i.first < (int)colorArray.size(); })) {
                colorArray.fill(defaultColor);
                for(const auto& entry : colorMap) colorArray[entry.first] = entry.second;
                // Fill color array.
                BufferReadAccess<int32_t> bondTypeData(bondTypeProperty);
                const int32_t* t = bondTypeData.cbegin();
                for(auto c = output.begin(); c != output.end(); ++t) {
                    if(*t >= 0 && (size_t)*t < colorArray.size()) {
                        *c++ = colorArray[*t];
                        *c++ = colorArray[*t];
                    }
                    else {
                        *c++ = defaultColor;
                        *c++ = defaultColor;
                    }
                }
            }
            else {
                // Fill color array.
                BufferReadAccess<int32_t> bondTypeData(bondTypeProperty);
                const int32_t* t = bondTypeData.cbegin();
                for(auto c = output.begin(); c != output.end(); ++t) {
                    if(auto it = colorMap.find(*t); it != colorMap.end()) {
                        *c++ = it->second;
                        *c++ = it->second;
                    }
                    else {
                        *c++ = defaultColor;
                        *c++ = defaultColor;
                    }
                }
            }
        }
        else {
            // Assign a uniform color to all bonds.
            std::ranges::fill(output, defaultColor);
        }
    }

    // Highlight selected bonds.
    if(bondSelectionProperty && bondSelectionProperty.size() * 2 == output.size()) {
        const ColorG selColor = selectionBondColor();
        const SelectionIntType* t = bondSelectionProperty.cbegin();
        for(auto c = output.begin(); c != output.end(); ++t) {
            if(*t) {
                *c++ = selColor;
                *c++ = selColor;
            }
            else
                c += 2;
        }
    }

    return output;
}

/******************************************************************************
 * Returns a human-readable string describing the picked object,
 * which will be displayed in the status bar by OVITO.
 ******************************************************************************/
QString BondPickInfo::infoString(const Pipeline* pipeline, uint32_t subobjectId)
{
    QString str;

    size_t bondIndex;
    OVITO_ASSERT(_subobjectToBondMapping ? subobjectId < _subobjectToBondMapping->size() : true);
    if(_subobjectToBondMapping && subobjectId < _subobjectToBondMapping->size()) {
        // Look up the bond index in the subobject to bond mapping.
        bondIndex = BufferReadAccess<int32_t>(_subobjectToBondMapping)[subobjectId];
    }
    else {
        // Default case, no no duoble / triple / dashed bonds
        bondIndex = subobjectId / 2;
    }
    if(particles()->bonds()) {
        BufferReadAccess<ParticleIndexPair> topologyProperty = particles()->bonds()->getTopology();
        if(topologyProperty && topologyProperty.size() > bondIndex) {
            size_t index1 = topologyProperty[bondIndex][0];
            size_t index2 = topologyProperty[bondIndex][1];
            str = tr("Bond: ");

            // Bond length
            BufferReadAccess<Point3> posProperty = particles()->getProperty(Particles::PositionProperty);
            if(posProperty && posProperty.size() > index1 && posProperty.size() > index2) {
                const Point3& p1 = posProperty[index1];
                const Point3& p2 = posProperty[index2];
                Vector3 delta = p2 - p1;
                if(BufferReadAccess<Vector3I> periodicImageProperty = particles()->bonds()->getProperty(Bonds::PeriodicImageProperty)) {
                    if(simulationCell()) {
                        delta += simulationCell()->cellMatrix() * periodicImageProperty[bondIndex].toDataType<FloatType>();
                    }
                }
                str += QString("<key>Length:</key> <val>%1</val><sep><key>Delta:</key> <val>%2, %3, %4</val>")
                           .arg(delta.length())
                           .arg(delta.x())
                           .arg(delta.y())
                           .arg(delta.z());
            }

            // Bond properties.
            str += QStringLiteral("<sep>");
            str += particles()->bonds()->elementInfoString(bondIndex);

            // Pair type info.
            const Property* typeProperty = particles()->getProperty(Particles::TypeProperty);
            if(typeProperty && typeProperty->size() > index1 && typeProperty->size() > index2) {
                BufferReadAccess<int32_t> typeData(typeProperty);
                const ElementType* type1 = typeProperty->elementType(typeData[index1]);
                const ElementType* type2 = typeProperty->elementType(typeData[index2]);
                if(type1 && type2) {
                    str += QString("<sep><key>Particles:</key> <val>%1 - %2</val>").arg(type1->nameOrNumericId(), type2->nameOrNumericId());
                }
            }
        }
    }
    return str;
}

/******************************************************************************
 * Allows the object to parse the serialized contents of a property field in a custom way.
 ******************************************************************************/
bool BondsVis::loadPropertyFieldFromStream(ObjectLoadStream& stream,
                                           const RefMakerClass::SerializedClassInfo::PropertyFieldInfo& serializedField)
{
    // For backward compatibility with OVITO 3.5.4:
    // Parse the "useParticleColors" field, which has been replaced by the "coloringMode" parameter field in later versions.
    if(serializedField.definingClass == &BondsVis::OOClass() && serializedField.identifier == "useParticleColors") {
        bool useParticleColors;
        stream >> useParticleColors;
        setColoringMode(useParticleColors ? ParticleBasedColoring : ByTypeColoring);
        return true;
    }
    return DataVis::loadPropertyFieldFromStream(stream, serializedField);
}

/******************************************************************************
 * Determines the display bond widths.
 ******************************************************************************/
ConstPropertyPtr BondsVis::bondWidths(const Bonds* bonds) const
{
    bonds->verifyIntegrity();

    // Take bond widths directly from the 'Width' property if available.
    ConstPropertyPtr output = bonds->getProperty(Bonds::WidthProperty);
    if(output) {
        // Check if the width array contains any zero entries.
        BufferReadAccess<GraphicsFloatType> widthArray(output);
        if(std::ranges::contains(widthArray, GraphicsFloatType(0))) {
            widthArray.reset();

            // Replace zero entries in the "Width" array with the uniform default width.
            std::ranges::replace(BufferWriteAccess<GraphicsFloatType, access_mode::read_write>(output.makeMutableInplace()),
                                 GraphicsFloatType(0),
                                 static_cast<GraphicsFloatType>(bondWidth()));
        }
    }
    else {
        // Allocate output array.
        output.reset(Bonds::OOClass().createStandardProperty(DataBuffer::Uninitialized, bonds->elementCount(), Bonds::WidthProperty));

        // Assign the uniform default width to all bonds.
        output.makeMutableInplace()->fill<GraphicsFloatType>((GraphicsFloatType)bondWidth());
    }

    return output;
}

}  // namespace Ovito
