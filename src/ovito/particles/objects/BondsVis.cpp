////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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
DEFINE_PROPERTY_FIELD(BondsVis, visualizeBondOrder);
SET_PROPERTY_FIELD_LABEL(BondsVis, bondWidth, "Bond width");
SET_PROPERTY_FIELD_LABEL(BondsVis, bondColor, "Uniform bond color");
SET_PROPERTY_FIELD_LABEL(BondsVis, shadingMode, "Shading mode");
SET_PROPERTY_FIELD_LABEL(BondsVis, coloringMode, "Coloring mode");
SET_PROPERTY_FIELD_LABEL(BondsVis, filledSegments, "Number of filled segments");
SET_PROPERTY_FIELD_LABEL(BondsVis, filledFraction, "Filled segment fraction");
SET_PROPERTY_FIELD_LABEL(BondsVis, visualizeBondOrder, "Visualize bond order property");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(BondsVis, bondWidth, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(BondsVis, filledSegments, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(BondsVis, filledFraction, PercentParameterUnit, 0, 1);

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
 * Clamps the OVITO bond order to the range [0, 3] as only up to 3 bonds will be
 * displayed at a time.
 ******************************************************************************/
inline GraphicsFloatType clampBondOrder(GraphicsFloatType bondOrder)
{
    return std::clamp(bondOrder, (GraphicsFloatType)0, (GraphicsFloatType)3);
}

/******************************************************************************
 * Check whether the bond order is a fractional or integer
 ******************************************************************************/
inline bool isFractionalBond(GraphicsFloatType bondOrder)
{
    bondOrder = clampBondOrder(bondOrder);
    return std::abs(std::round(bondOrder) - bondOrder) > Ovito::epsilon_v<GraphicsFloatType>;
}

/******************************************************************************
 * Calculate the out of plane vector (normal) for each bond and its neighbors
 * to orient the bond cylinders.
 * The normal is estimated as the accumulated normal (cross product)
 * of all neighbor bond combinations.
 * For bond "networks" perfectly aligned with the z-axis alignment along x is
 * taken as fallback.
 * Estimate the center of the current partial bond "network" for fractional
 * bonds to draw the dashed bond always towards the ring center.
 ******************************************************************************/
std::pair<Vector3G, Vector3G> getBondNormalVector(const ParticleIndexPair& particleIndices,
                                                  const ParticleBondMap& bondsMap,
                                                  const BufferReadAccess<Point3>& positions,
                                                  const BufferReadAccess<GraphicsFloatType>& bondOrder,
                                                  const SimulationCellDataG& cell)
{
    OVITO_ASSERT(bondOrder);

    // Normals towards x and z-axis
    Vector3G normal_z = Vector3G::Zero();
    Vector3G normal_x = Vector3G::Zero();
    // Estimate the center of the current partial bond "network"
    Vector3G direction = Vector3G::Zero();

    std::array<Vector3G, 2> neighbors;
    for(size_t index : particleIndices) {
        size_t count = 0;

        // Loop over all bonded neighbors of the two particles making up the bond
        for(const BondWithIndex& bond : bondsMap.bondsOfParticle(index)) {
            OVITO_ASSERT(bond.index1 == index);
            OVITO_ASSERT(bond.index2 != index);

            // Compute unwrapped bond vector.
            const Point3G p1 = positions[bond.index1].toDataType<GraphicsFloatType>();
            const Point3G p2 = positions[bond.index2].toDataType<GraphicsFloatType>();
            Vector3G delta1 = p2 - p1;
            delta1 += cell.cellMatrix() * bond.pbcShift.toDataType<GraphicsFloatType>();
            neighbors[count % neighbors.size()] = delta1;

            // Center only relevant for fractional bonds
            if(isFractionalBond(bondOrder[bond.bondIndex])) {
                direction += delta1;
            }

            // Compute and accumulate normals
            if(count >= neighbors.size() - 1) {
                Vector3G localNormal = neighbors[0].cross(neighbors[1]);
                localNormal.normalizeSafely();
                if(localNormal.z() > 0)
                    normal_z += localNormal;
                else
                    normal_z -= localNormal;
                if(localNormal.x() > 0)
                    normal_x += localNormal;
                else
                    normal_x -= localNormal;
            }
            count++;
        }
    }
    if(normal_z.squaredLength() <= (Ovito::epsilon_v<GraphicsFloatType> * Ovito::epsilon_v<GraphicsFloatType>))
        return {normal_x, direction};
    else
        return {normal_z, direction};
}

/******************************************************************************
 * Calculate the (reduced) bond width based on the bond order
 ******************************************************************************/
inline GraphicsFloatType getEffectiveBondWidth(const BufferReadAccess<GraphicsFloatType>& bondInputWidths, size_t bondIndex, FloatType bondDiameter, int bondRepCount)
{
    OVITO_ASSERT(bondRepCount >= 0 && bondRepCount <= 3);
    GraphicsFloatType bondWidthValue = (bondInputWidths && bondInputWidths[bondIndex] > GraphicsFloatType(0)) ? bondInputWidths[bondIndex] : static_cast<GraphicsFloatType>(bondDiameter);
    if(bondRepCount > 1 && bondRepCount <= 2) {
        bondWidthValue *= (GraphicsFloatType)0.8;
    }
    else if(bondRepCount > 2) {
        bondWidthValue *= (GraphicsFloatType)0.63;
    }
    return bondWidthValue;
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
                                    const GraphicsFloatType bondWidth)
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
        Vector3G cross = gvec.cross(normal).safelyNormalized();
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
inline size_t filledSegmentsToCylinderCount(const int filledSegments, const FloatType filledFraction)
{
    if(filledSegments <= 0 || filledFraction <= 0) {
        return 0;
    }

    if(filledFraction >= 1) {
        return 2;
    }

    // If the number of filled segments is odd, we need one additional cylinder, because the center line is split in half.
    if(filledSegments % 2 == 1) {
        return filledSegments + 1;
    }
    else {
        return filledSegments;
    }
}

}  // namespace

/******************************************************************************
 * Determines the number of cylinders to be rendered for the given bond topology.
 * Used to preallocated all the output arrays.
 ******************************************************************************/
size_t BondsVis::getCylinderCount(const Property* bondTopologyProperty,
                                  const Property* bondOrderProperty,
                                  const int filledSegments,
                                  const FloatType filledFraction)
{
    // No bond topology
    if(!bondTopologyProperty) {
        return 0;
    }
    // Simple case, no bond order property --> two half-bond cylinders per bond
    if(!bondOrderProperty) {
        return 2 * bondTopologyProperty->size();
    }
    OVITO_ASSERT(bondTopologyProperty->size() == bondOrderProperty->size());
    OVITO_ASSERT(filledSegments >= 0);

    // Number of cylinders that will be required per dashed bond
    const size_t cylindersPerDashedBond = filledSegmentsToCylinderCount(filledSegments, filledFraction);

    // Count the total number of cylinders that will be required for all bonds.
    size_t count = 0;
    BufferReadAccess<GraphicsFloatType> bondOrderAcc(bondOrderProperty);
    for(GraphicsFloatType bondOrder : bondOrderAcc) {
        // Determine how many full and dashed bonds we need to draw for this bond based on its order.
        GraphicsFloatType intPart;
        const GraphicsFloatType remainder = std::modf(clampBondOrder(bondOrder), &intPart);
        count += 2 * (size_t)intPart;
        if(remainder > Ovito::epsilon_v<GraphicsFloatType>) {
            count += cylindersPerDashedBond;
        }
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
    const Property* bondOrderProperty = visualizeBondOrder() ? bonds->getProperty(Bonds::OrderProperty) : nullptr;

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
                                         FloatType,           // Filled fraction
                                         Color,               // Bond uniform color
                                         ColoringMode,        // Bond coloring mode
                                         ShadingMode,         // Bond shading mode
                                         bool                 // Render nodal vertices
                                         >;

    // Decide whether to render additional spheres at the nodal positions of the bonds.
    const bool renderNodalVertices = !transparencyProperty && !bondWidthProperty &&
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
                     filledSegments(),
                     filledFraction(),
                     bondColor(),
                     coloringMode(),
                     shadingMode(),
                     renderNodalVertices),
            [&](CylinderPrimitive& cylinders, ParticlePrimitive& vertices, OORef<BondPickInfo>& pickInfo) {
                const SimulationCellDataG cellData(simulationCell);
                const FloatType uniformBondWidth = bondWidth();

                // Are bond orders in play, which require multi-cylinder rendering for double / triple bonds and dashed bonds for fractional bond orders?
                const bool useBondOrder = bondOrderProperty ? std::ranges::any_of(BufferReadAccess<GraphicsFloatType>(bondOrderProperty),
                                                                                [](GraphicsFloatType bondOrder) { return bondOrder != 1; })
                                                            : false;

                // Count the number of cylinders that will be required per dashed bond.
                const size_t dashedCylinderCount = useBondOrder ? filledSegmentsToCylinderCount(filledSegments(), filledFraction()) : 0;
                OVITO_ASSERT(dashedCylinderCount % 2 == 0); // Always an even number of cylinders, because in case of an odd number of filled segments, the central segment is split in half, resulting in an even number of cylinders.

                // Count the total number of cylinders that will be required for all bonds.
                const size_t totalCylinderCount = getCylinderCount(bondTopologyProperty, useBondOrder ? bondOrderProperty : nullptr, filledSegments(), filledFraction());

                // Make sure we don't exceed our internal limits.
                if(totalCylinderCount > (size_t)std::numeric_limits<int>::max()) {
                    throw Exception(tr("This version of OVITO cannot render more than %1 bonds.").arg(std::numeric_limits<int>::max() / 2));
                }

                // Mapping from cylinder indices to original bonds
                BufferFactory<int32_t> subobjectToBondMapping;

                if(bondTopologyProperty && positionProperty && uniformBondWidth > 0) {

                    // Mapping from cylinder indices to original bonds
                    if(useBondOrder)
                        subobjectToBondMapping = BufferFactory<int32_t>(totalCylinderCount);

                    // Allocate buffers for the cylinders geometry.
                    BufferFactory<Point3G> cylinderPositions1(totalCylinderCount);
                    BufferFactory<Point3G> cylinderPositions2(totalCylinderCount);
                    BufferFactory<ColorG> cylinderColors(totalCylinderCount);
                    BufferFactory<GraphicsFloatType> cylinderTransparencies = transparencyProperty ? BufferFactory<GraphicsFloatType>(totalCylinderCount) : BufferFactory<GraphicsFloatType>{};
                    BufferFactory<GraphicsFloatType> cylinderWidths = bondWidthProperty || bondOrderProperty
                                                                      ? BufferFactory<GraphicsFloatType>(totalCylinderCount)
                                                                      : BufferFactory<GraphicsFloatType>{};

                    // Allocate buffers for the nodal vertices.
                    BufferFactory<ColorG> nodalColors = renderNodalVertices ? BufferFactory<ColorG>(positionProperty->size()) : BufferFactory<ColorG>{};
                    BufferFactory<GraphicsFloatType> nodalTransparencies = (renderNodalVertices && transparencyProperty)
                                                                               ? BufferFactory<GraphicsFloatType>(positionProperty->size())
                                                                               : BufferFactory<GraphicsFloatType>{};
                    BufferFactory<int32_t> nodalIndices = renderNodalVertices ? BufferFactory<int32_t>(0) : BufferFactory<int32_t>{};
                    boost::dynamic_bitset<> visitedParticles(renderNodalVertices ? positionProperty->size() : 0);
                    OVITO_ASSERT(nodalColors || !nodalTransparencies);

                    // Access particle coords.
                    BufferReadAccess<Point3> positions(positionProperty);
                    const size_t particleCount = positions.size();

                    // Obtain the radii of the particles.
                    BufferReadAccessAndRef<GraphicsFloatType> particleRadii;
                    if(particleVis) particleRadii = particleVis->particleRadii(particles, true);
                    // Make sure the particle radius array has the correct length.
                    if(particleRadii && particleRadii.size() != particleCount) particleRadii.reset();

                    // Determine half-bond colors.
                    const size_t bondCount = bondTopologyProperty->size();
                    std::vector<ColorG> hbondColors = halfBondColors(particles, frameGraph.isInteractive(), coloringMode(), false);
                    OVITO_ASSERT(hbondColors.size() == 2 * bondCount);

                    // Bonds enumerator used to determine the normal vector to align double / triple bonds.
                    std::optional<ParticleBondMap> bondsMap;
                    if(useBondOrder)
                        bondsMap.emplace(bondTopologyProperty, bondPeriodicImageProperty);

                    // Number of on and off segments for dashed bonds.
                    const int numOnSegments = filledSegments();
                    const int numOffSegments = filledSegments() + 1;
                    OVITO_ASSERT(numOnSegments > 0);
                    OVITO_ASSERT(numOffSegments > 0);

                    size_t cylinderIndex = 0;
                    BufferReadAccess<ParticleIndexPair> bonds(bondTopologyProperty);
                    BufferReadAccess<Vector3I> bondPeriodicImages(bondPeriodicImageProperty);
                    BufferReadAccess<GraphicsFloatType> bondInputTransparency(transparencyProperty);
                    BufferReadAccess<GraphicsFloatType> bondInputWidths(bondWidthProperty);
                    BufferReadAccess<GraphicsFloatType> bondInputOrders(bondOrderProperty);
                    for(size_t bondIndex = 0; bondIndex < bondCount; bondIndex++) {
                        // Determine how many full and dashed bond cylinders we need to draw for this bond based on its order.
                        int bondRepCount;
                        size_t cylinderCount;
                        bool lastBondRepIsDashed = false;
                        if(!useBondOrder) {
                            bondRepCount = 1;
                            cylinderCount = 2; // Two half-bonds per bond
                        }
                        else {
                            GraphicsFloatType intPart;
                            const GraphicsFloatType remainder = std::modf(clampBondOrder(bondInputOrders[bondIndex]), &intPart);
                            bondRepCount = (int)intPart;
                            cylinderCount = 2 * bondRepCount;
                            if(remainder > Ovito::epsilon_v<GraphicsFloatType>) {
                                bondRepCount += 1;
                                cylinderCount += dashedCylinderCount;
                                lastBondRepIsDashed = filledFraction() < 1;
                            }

                            // Skip bonds with zero / negative order. That's safe because these are not included in the cylinder count
                            if(bondRepCount <= 0)
                                continue;
                        }
                        OVITO_ASSERT(cylinderCount % 2 == 0); // Always an even number of cylinders, because we render double bonds as two half-bonds and dashed bonds always have an even effective number of segments.

                        size_t oldCylinderIndex = cylinderIndex; // Only used for debugging purposes

                        bool isDegenerateBond = false;
                        const ParticleIndexPair& particleIndices = bonds[bondIndex];
                        // Handle invalid (out-of-range) particle indices in the bond topology.
                        if(particleIndices[0] >= particleCount || particleIndices[1] >= particleCount) {
                            isDegenerateBond = true;
                        }
                        else {
                            // Get the effective cylinder width for this bond, which depends on the bond order and the option "Width" bond property.
                            const GraphicsFloatType bondWidth = getEffectiveBondWidth(bondInputWidths, bondIndex, uniformBondWidth, bondRepCount);

                            // Determine the normal vector and local center direction to align double/triple bonds.
                            Vector3G normal = Vector3G::Zero(), centerVec = Vector3G::Zero();
                            if(useBondOrder && bondRepCount > 1) {
                                std::tie(normal, centerVec) = getBondNormalVector(particleIndices, *bondsMap, positions, bondInputOrders, cellData);
                                // Fallback, normal cannot be zero
                                if(!useBondOrder || bondRepCount <= 1 || !normal.normalizeSafely()) {
                                    normal = Vector3G{0, 0, 1};
                                }
                            }

                            // Compute unwrapped bond vector.
                            Vector3G bondVecG = positions[particleIndices[1]].toDataType<GraphicsFloatType>() - positions[particleIndices[0]].toDataType<GraphicsFloatType>();
                            bool isSplitBond = false;
                            if(bondPeriodicImageProperty) {
                                for(size_t k = 0; k < 3; k++) {
                                    if(auto d = bondPeriodicImages[bondIndex][k]) {
                                        bondVecG += cellData.cellMatrix().column(k) * d;
                                        isSplitBond = true;
                                    }
                                }
                            }

                            // Skip degenerate bonds with length 0.
                            const GraphicsFloatType blen = bondVecG.length();
                            if(blen <= Ovito::epsilon_v<GraphicsFloatType>) {
                                isDegenerateBond = true;
                            }
                            else {
                                // This calculation determines the point where to split the bond into the two half-bonds such that the border appears
                                // halfway between the two particles, which may have two different sizes.
                                GraphicsFloatType t = GraphicsFloatType(0.5);
                                if(particleRadii && blen > Ovito::epsilon_v<GraphicsFloatType>) {
                                    const GraphicsFloatType blen2 = blen * GraphicsFloatType(2);
                                    t = GraphicsFloatType(0.5) + std::min(GraphicsFloatType(0.5), particleRadii[particleIndices[0]] / blen2) -
                                        std::min(GraphicsFloatType(0.5), particleRadii[particleIndices[1]] / blen2);
                                }

                                for(size_t bondRepIdx = 0; bondRepIdx < bondRepCount; bondRepIdx++) {

                                    // Displace individual cylinders based on the current bond order.
                                    const Vector3G offSetVec = getBondOffsetVector(bondRepCount, bondRepIdx, bondVecG, normal, centerVec, bondWidth);

                                    if(!lastBondRepIsDashed || bondRepIdx < bondRepCount - 1) {
                                        // Generate a solid bond made of two cylinders, each representing a half-bond.
                                        for(int cylinderIdx = 0; cylinderIdx < 2; cylinderIdx++) {
                                            OVITO_ASSERT(cylinderIndex < totalCylinderCount);
                                            cylinderColors[cylinderIndex] = hbondColors[2 * bondIndex + cylinderIdx];
                                            if(nodalColors && bondRepCount == 1 && !visitedParticles.test(particleIndices[cylinderIdx])) {
                                                nodalColors[particleIndices[cylinderIdx]] = cylinderColors[cylinderIndex];
                                                if(nodalTransparencies)
                                                    nodalTransparencies[particleIndices[cylinderIdx]] = bondInputTransparency[bondIndex];
                                                visitedParticles.set(particleIndices[cylinderIdx]);
                                                nodalIndices.push_back(particleIndices[cylinderIdx]);
                                            }
                                            if(cylinderTransparencies) cylinderTransparencies[cylinderIndex] = bondInputTransparency[bondIndex];
                                            if(cylinderWidths) cylinderWidths[cylinderIndex] = bondWidth;
                                            const Point3G startPos = positions[particleIndices[cylinderIdx]].toDataType<GraphicsFloatType>() + offSetVec;
                                            cylinderPositions1[cylinderIndex] = startPos;
                                            cylinderPositions2[cylinderIndex] = (cylinderIdx == 0) ? (startPos + bondVecG * t) : (startPos - bondVecG * (GraphicsFloatType(1) - t));
                                            if(isSplitBond) swap(cylinderPositions1[cylinderIndex], cylinderPositions2[cylinderIndex]);
                                            if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                                            cylinderIndex++;
                                        }
                                    }
                                    else if(dashedCylinderCount > 0) {
                                        // Generate a dashed bond.
                                        const Vector3G bondDirG = bondVecG / blen;

                                        // Reduce bond length by particle radii to get effective bond length.
                                        GraphicsFloatType effectiveBondLength = blen;
                                        if(particleRadii) {
                                            effectiveBondLength -= particleRadii[particleIndices[0]];
                                            effectiveBondLength -= particleRadii[particleIndices[1]];
                                        }

                                        // Length of each on and off segment
                                        GraphicsFloatType onSegmentLength = (GraphicsFloatType)filledFraction() * effectiveBondLength / numOnSegments;
                                        GraphicsFloatType offSegmentLength = ((GraphicsFloatType)1 - (GraphicsFloatType)filledFraction()) * effectiveBondLength / numOffSegments;

                                        // Bond start point is based on particle radius.
                                        Point3G currentPos1 = positions[particleIndices[0]].toDataType<GraphicsFloatType>() + offSetVec;
                                        currentPos1 += (particleRadii ? particleRadii[particleIndices[0]] : 0) * bondDirG;

                                        // Bond end point is based on particle radius.
                                        Point3G currentPos2 = positions[particleIndices[1]].toDataType<GraphicsFloatType>() + offSetVec;
                                        currentPos2 -= (particleRadii ? particleRadii[particleIndices[1]] : 0) * bondDirG;

                                        // Generate cylinders for segments, always in symmetric pairs.
                                        int numSegmentsPerSide = dashedCylinderCount / 2; // Number of segments to generate per half-bond
                                        for(int segment = 0; segment < numSegmentsPerSide; segment++) {
                                            if(segment == numSegmentsPerSide - 1 && filledSegments() % 2 == 1) {
                                                // If the number of filled segments is odd, the last segment is split in half, so we need to adjust the segment length for the last segment.
                                                onSegmentLength *= (GraphicsFloatType)0.5;
                                            }

                                            currentPos1 += bondDirG * offSegmentLength;
                                            currentPos2 -= bondDirG * offSegmentLength;
                                            Point3G nextPos1 = currentPos1 + bondDirG * onSegmentLength;
                                            Point3G nextPos2 = currentPos2 - bondDirG * onSegmentLength;

                                            OVITO_ASSERT(cylinderIndex < totalCylinderCount);
                                            cylinderPositions1[cylinderIndex] = currentPos1;
                                            cylinderPositions2[cylinderIndex] = nextPos1;
                                            cylinderColors[cylinderIndex] = hbondColors[2 * bondIndex + 0];
                                            if(cylinderWidths) cylinderWidths[cylinderIndex] = bondWidth;
                                            if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                                            cylinderIndex++;

                                            OVITO_ASSERT(cylinderIndex < totalCylinderCount);
                                            cylinderPositions1[cylinderIndex] = currentPos2;
                                            cylinderPositions2[cylinderIndex] = nextPos2;
                                            cylinderColors[cylinderIndex] = hbondColors[2 * bondIndex + 1];
                                            if(cylinderWidths) cylinderWidths[cylinderIndex] = bondWidth;
                                            if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                                            cylinderIndex++;

                                            currentPos1 = nextPos1;
                                            currentPos2 = nextPos2;
                                        }
                                    }
                                }
                            }
                        }

                        if(isDegenerateBond) {
                            // Generate dummy cylinders (invisible in the viewport) if the bond is degenerate - only to maintain the original cylinder count.
                            for(size_t i = 0; i < cylinderCount; i++) {
                                OVITO_ASSERT(cylinderIndex < totalCylinderCount);
                                cylinderColors[cylinderIndex].setBlack();
                                if(cylinderTransparencies) cylinderTransparencies[cylinderIndex] = 0;
                                if(cylinderWidths) cylinderWidths[cylinderIndex] = 0;
                                cylinderPositions1[cylinderIndex] = Point3G::Origin();
                                cylinderPositions2[cylinderIndex] = Point3G::Origin();
                                if(subobjectToBondMapping) subobjectToBondMapping[cylinderIndex] = (int32_t)bondIndex;
                                cylinderIndex++;
                            }
                        }

                        OVITO_ASSERT(cylinderIndex - oldCylinderIndex == cylinderCount);
                    }
                    OVITO_ASSERT(cylinderIndex == totalCylinderCount);

                    cylinders.setShape(CylinderPrimitive::CylinderShape);
                    cylinders.setShadingMode(static_cast<CylinderPrimitive::ShadingMode>(shadingMode()));
                    cylinders.setRenderSingleCylinderCap(transparencyProperty != nullptr);
                    cylinders.setUniformWidth(uniformBondWidth);
                    cylinders.setWidths(cylinderWidths.take());
                    cylinders.setPositions(cylinderPositions1.take(), cylinderPositions2.take());
                    cylinders.setColors(cylinderColors.take());
                    cylinders.setTransparencies(cylinderTransparencies.take());

                    if(renderNodalVertices) {
                        OVITO_ASSERT(positionProperty);
                        vertices.setParticleShape(ParticlePrimitive::SphericalShape);
                        vertices.setShadingMode((shadingMode() == NormalShading) ? ParticlePrimitive::NormalShading
                                                                                 : ParticlePrimitive::FlatShading);
                        vertices.setRenderingQuality(ParticlePrimitive::HighQuality);
                        vertices.setPositions(positionProperty);
                        vertices.setUniformRadius(0.5 * uniformBondWidth);
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
* Given an sub-object ID returned by the Viewport::pick() method, looks up the
* corresponding bond index.
******************************************************************************/
size_t BondPickInfo::bondIndexFromSubObjectID(uint32_t subobjectId) const
{
    OVITO_ASSERT(_subobjectToBondMapping ? subobjectId < _subobjectToBondMapping->size() : true);
    if(_subobjectToBondMapping && subobjectId < _subobjectToBondMapping->size()) {
        // Look up the bond index in the subobject to bond mapping.
        return BufferReadAccess<int32_t>(_subobjectToBondMapping)[subobjectId];
    }
    else {
        // Default case, only single bonds.
        return subobjectId / 2;
    }
}

/******************************************************************************
* Returns a human-readable string describing the picked object,
* which will be displayed in the status bar by OVITO.
******************************************************************************/
QString BondPickInfo::infoString(const Pipeline* pipeline, uint32_t subobjectId)
{
    QString str;

    if(particles()->bonds()) {
        size_t bondIndex = bondIndexFromSubObjectID(subobjectId);
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
* Provides a custom function that takes are of the deserialization of a
* serialized property field.
* This is needed for file backward compatibility with OVITO 3.5.4.
******************************************************************************/
RefTarget::SerializedPropertyField::CustomDeserializationFunctionPtr BondsVis::OOMetaClass::overrideFieldDeserialization(LoadStream& stream, const SerializedPropertyField& field) const
{
    // For backward compatibility with OVITO 3.5.4.

    // Parse the "useParticleColors" field, which has been replaced by the "coloringMode" parameter field in later versions.
    if(field.definingClass == &BondsVis::OOClass() && field.identifier == "useParticleColors") {
        return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
            bool useParticleColors;
            stream >> useParticleColors;
            static_cast<BondsVis&>(owner).setColoringMode(useParticleColors ? ParticleBasedColoring : ByTypeColoring);
        };
    }

    return DataVis::OOMetaClass::overrideFieldDeserialization(stream, field);
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
