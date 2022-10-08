////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/BondsObject.h>
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito::Particles {

/**
 * \brief A visualization element for rendering bonds.
 */
class OVITO_PARTICLES_EXPORT BondsVis : public DataVis
{
	OVITO_CLASS(BondsVis)
	Q_CLASSINFO("DisplayName", "Bonds");

    Q_PROPERTY(Ovito::Particles::BondsVis::ShadingMode shadingMode READ shadingMode WRITE setShadingMode)

public:

	/// The shading modes supported by the bonds vis element.
	enum ShadingMode {
		NormalShading = CylinderPrimitive::ShadingMode::NormalShading,
		FlatShading = CylinderPrimitive::ShadingMode::FlatShading
	};
	Q_ENUM(ShadingMode);

	/// The coloring modes supported by the vis element.
	enum ColoringMode {
		UniformColoring,
		ByTypeColoring,
		ParticleBasedColoring
	};
	Q_ENUMS(ColoringMode);	

	/// Constructor.
	Q_INVOKABLE BondsVis(ObjectCreationParams params);

	/// Renders the visual element.
	virtual PipelineStatus render(TimePoint time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode) override;

	/// Computes the bounding box of the visual element.
	virtual Box3 boundingBox(TimePoint time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, TimeInterval& validityInterval) override;

	/// Returns the display color used for selected bonds.
	Color selectionBondColor() const { return Color(1,0,0); }

	/// Determines the display colors of half-bonds.
	/// Returns an array with two colors per full bond, because the two half-bonds may have different colors.
	std::vector<Color> halfBondColors(const ParticlesObject* particles, bool highlightSelection, ColoringMode coloringMode, bool ignoreBondColorProperty) const;

	/// Determines the bond widths used for rendering.
	ConstPropertyPtr bondWidths(const BondsObject* bonds) const;

protected:

	/// Allows the object to parse the serialized contents of a property field in a custom way.
	virtual bool loadPropertyFieldFromStream(ObjectLoadStream& stream, const RefMakerClass::SerializedClassInfo::PropertyFieldInfo& serializedField) override;

protected:

	/// Display width of bonds.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, bondWidth, setBondWidth, PROPERTY_FIELD_MEMORIZE);

	/// Uniform display color of the bonds.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, bondColor, setBondColor, PROPERTY_FIELD_MEMORIZE);

	/// Shading mode for bond rendering.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(BondsVis::ShadingMode, shadingMode, setShadingMode, PROPERTY_FIELD_MEMORIZE);

	/// Determines how the bonds are colored.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(BondsVis::ColoringMode, coloringMode, setColoringMode, PROPERTY_FIELD_MEMORIZE);
};

/**
 * \brief This information record is attached to the bonds by the BondsVis when rendering
 * them in the viewports. It facilitates the picking of bonds with the mouse.
 */
class OVITO_PARTICLES_EXPORT BondPickInfo : public ObjectPickInfo
{
	OVITO_CLASS(BondPickInfo)

public:

	/// Constructor.
	BondPickInfo(DataOORef<const ParticlesObject> particles, DataOORef<const SimulationCellObject> simulationCell) : _particles(std::move(particles)), _simulationCell(std::move(simulationCell)) {}

	/// Returns the particles object.
	const DataOORef<const ParticlesObject>& particles() const { OVITO_ASSERT(_particles); return _particles; }

	/// Returns the simulation cell.
	const DataOORef<const SimulationCellObject>& simulationCell() const { return _simulationCell; }

	/// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
	virtual QString infoString(PipelineSceneNode* objectNode, quint32 subobjectId) override;

private:

	/// The particles object.
	DataOORef<const ParticlesObject> _particles;

	/// The simulation cell object.
	DataOORef<const SimulationCellObject> _simulationCell;
};

}	// End of namespace
