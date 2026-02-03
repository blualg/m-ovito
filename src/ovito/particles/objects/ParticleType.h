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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/ParticlesVis.h>
#include <ovito/stdobj/properties/ElementType.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>

namespace Ovito {

/**
 * \brief Stores the properties of a particle type, e.g. name, color, and radius.
 */
class OVITO_PARTICLES_EXPORT ParticleType : public ElementType
{
    /// Define a new metaclass.
    class ParticleTypeClass : public ElementTypeClass
    {
    public:
        /// Inherit constructor from base class.
        using ElementTypeClass::ElementTypeClass;

        /// Returns a list of column names to be displayed in the data inspector for element types of this class.
        [[nodiscard]] virtual QStringList dataInspectorColumns() const override;

        /// Returns the Qt table model data for the given element type to be displayed in the data inspector.
        virtual QVariant dataInspectorModelData(int columnIndex, const QString& columnName, const ElementType* elementType, int role) const override;
    };
    OVITO_CLASS_META(ParticleType, ParticleTypeClass);

public:
    // clang-format off
    enum class ChemicalElement: uint_fast8_t
    {
         // "X" is placeholder for unspecified type
        X, H, He, Li, Be, B, C, N, O, F, Ne, Na, Mg, Al, Si, P, S, Cl, Ar, K,
        Ca, Sc, Ti, V, Cr, Mn, Fe, Co, Ni, Cu, Zn, Ga, Ge, As, Se, Br, Kr, Rb,
        Sr, Y, Zr, Nb, Mo, Tc, Ru, Rh, Pd, Ag, Cd, In, Sn, Sb, Te, I, Xe, Cs,
        Ba, La, Ce, Pr, Nd, Pm, Sm, Eu, Gd, Tb, Dy, Ho, Er, Tm, Yb, Lu, Hf, Ta,
        W, Re, Os, Ir, Pt, Au, Hg, Tl, Pb, Bi, Po, At, Rn, Fr, Ra, Ac, Th, Pa, U,
        Np, Pu, Am, Cm, Bk, Cf, Es, Fm, Md, No, Lr, Rf, Db, Sg, Bh, Hs, Mt, Ds, Rg,
        Cn, Nh, Fl, Mc, Lv, Ts, Og, D,  // Deuterium is given a special index to separate it from Hydrogen
        NUMBER_OF_PREDEFINED_CHEMICAL_TYPES
    };
    // clang-format on

    enum class PredefinedStructureType : uint_fast8_t
    {
        OTHER = 0,                   //< Unidentified structure
        FCC,                         //< Face-centered cubic
        HCP,                         //< Hexagonal close-packed
        BCC,                         //< Body-centered cubic
        ICO,                         //< Icosahedral structure
        CUBIC_DIAMOND,               //< Cubic diamond structure
        CUBIC_DIAMOND_FIRST_NEIGH,   //< First neighbor of a cubic diamond atom
        CUBIC_DIAMOND_SECOND_NEIGH,  //< Second neighbor of a cubic diamond atom
        HEX_DIAMOND,                 //< Hexagonal diamond structure
        HEX_DIAMOND_FIRST_NEIGH,     //< First neighbor of a hexagonal diamond atom
        HEX_DIAMOND_SECOND_NEIGH,    //< Second neighbor of a hexagonal diamond atom
        SC,                          //< Simple cubic structure
        GRAPHENE,                    //< Graphene structure
        HEXAGONAL_ICE,
        CUBIC_ICE,
        INTERFACIAL_ICE,
        HYDRATE,
        INTERFACIAL_HYDRATE,

        NUMBER_OF_PREDEFINED_STRUCTURE_TYPES
    };

    enum class RadiusVariant : uint_fast8_t
    {
        DisplayRadius,
        VanDerWaalsRadius
    };

public:

    //////////////////////////////////// Utility methods ////////////////////////////////

    /// Builds a map from type identifiers to particle radii.
    static std::map<int, FloatType> typeRadiusMap(const Property* typeProperty) {
        std::map<int, FloatType> m;
        for(const ElementType* type : typeProperty->elementTypes())
            if(const ParticleType* particleType = dynamic_object_cast<ParticleType>(type))
                m.insert({ type->numericId(), particleType->radius() });
        return m;
    }

    /// Builds a map from type identifiers to particle masses.
    static std::map<int, FloatType> typeMassMap(const Property* typeProperty) {
        std::map<int, FloatType> m;
        for(const ElementType* type : typeProperty->elementTypes())
            if(const ParticleType* particleType = dynamic_object_cast<ParticleType>(type))
                m.insert({ type->numericId(), particleType->mass() });
        return m;
    }

    /// Loads a mesh-based shape from a geometry file (but doesn't yet assign it to the ParticleType).
    [[nodiscard]] Future<DataOORef<TriangleMesh>> loadShapeMesh(QUrl sourceUrl,
                                                                const FileImporterClass* importerClass = nullptr,
                                                                const QString& importerFormat = {}) const;

    //////////////////////////////////// Default parameters ////////////////////////////////

    /// Returns the hard-coded symbol of a predefined chemical element (one or two characters).
    static const QString& getChemicalElementSymbol(ChemicalElement el) {
        OVITO_ASSERT((int)el >= 0 && (int)el < (int)ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES);
        return _PredefinedChemicalTypes[(size_t)el].symbol;
    }

    /// Performs a reverse lookup. Given a chemical element symbol, find the corresponding ChemicalElement enum value.
    template<typename StringType>
        requires (std::same_as<StringType, QString> || std::same_as<StringType, QStringView> || std::same_as<StringType, QLatin1String>)
    static ChemicalElement getChemicalElementFromSymbol(const StringType& symbol) {
        const auto it = std::ranges::find(_PredefinedChemicalTypes, symbol, &PredefinedChemicalType::symbol);
        if(it != _PredefinedChemicalTypes.end()) {
            OVITO_ASSERT(std::distance(_PredefinedChemicalTypes.begin(), it) > 0 &&
                         std::distance(_PredefinedChemicalTypes.begin(), it) < (int)ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES);
            return (ChemicalElement)std::distance(_PredefinedChemicalTypes.begin(), it);
        }
        return ChemicalElement::X;
    }

    /// Returns the hard-coded full name of a predefined chemical element.
    static const QString& getChemicalElementFullName(ChemicalElement el) {
        OVITO_ASSERT((int)el >= 0 && (int)el < (int)ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES);
        return _PredefinedChemicalTypes[(size_t)el].fullName;
    }

    /// Returns the hard-coded color of a predefined chemical element.
    static const Color& getChemicalElementColor(ChemicalElement el) {
        OVITO_ASSERT((int)el >= 0 && (int)el < (int)ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES);
        return _PredefinedChemicalTypes[(size_t)el].color;
    }

    /// Returns the name string of a predefined structure type.
    static const QString& getPredefinedStructureTypeName(PredefinedStructureType predefType) {
        OVITO_ASSERT((int)predefType >= 0 && (int)predefType < (int)PredefinedStructureType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES);
        return _predefinedStructureTypes[(size_t)predefType].name;
    }

    /// Returns the hard-coded color of a predefined structure type.
    static const Color& getPredefinedStructureTypeColor(PredefinedStructureType predefType) {
        OVITO_ASSERT((int)predefType >= 0 && (int)predefType < (int)PredefinedStructureType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES);
        return _predefinedStructureTypes[(size_t)predefType].color;
    }

    /// Returns the default radius for a named particle type.
    static FloatType getDefaultParticleRadius(Particles::Type typeClass,
                                              const QString& particleTypeName,
                                              int particleTypeId,
                                              bool loadUserDefaults,
                                              RadiusVariant radiusVariant = RadiusVariant::DisplayRadius);

    /// Changes the default radius for a named particle type.
    static void setDefaultParticleRadius(Particles::Type typeClass,
                                         const QString& particleTypeName,
                                         FloatType radius,
                                         RadiusVariant radiusVariant = RadiusVariant::DisplayRadius);

    /// Returns the default mass for a named particle type.
    static FloatType getDefaultParticleMass(Particles::Type typeClass, const QString& particleTypeName, int particleTypeId, bool loadUserDefaults);

    /// Determines the chemical element represented by a particle type (if any).
    static ChemicalElement getDefaultChemicalElementForType(Particles::Type typeClass, const QString& particleTypeName, int particleTypeId, bool loadUserDefaults);

    /// Performs a reverse lookup. Given a mass value, find the corresponding standard particle type name.
    /// Currently, this method only considers chemical elements from the hard-coded table, because
    /// mass presets cannot be configured by the user.
    static QString guessTypeNameFromMass(FloatType mass);

protected:

    /// Initializes the type's parameters to default values based on the type's name or numeric ID.
    virtual void initializeTypeInternal(const QString& typeName, const OwnerPropertyRef& property, bool loadUserDefaults) override;

    /// Is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// The radius used for rendering particles of this type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, radius, setRadius);
    DECLARE_SHADOW_PROPERTY_FIELD(radius);

    /// Indicates that the type's radius was read from loaded input file and the value should be considered immutable.
    /// If this flag is set by the file reader, the user is no longer allowed to modify the radius value in the GUI.
    /// Currently, this flag is only set by the GSDImporter for types with shape type "Sphere", whose
    /// radius may vary with simulation time.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, radiusIsPrescribed, setRadiusIsPrescribed);

    /// The van der Waals radius of this particle type, which is used for generating bonds between particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, vdwRadius, setVdwRadius);
    DECLARE_SHADOW_PROPERTY_FIELD(vdwRadius);

    /// The visualization shape for particles of this type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ParticlesVis::ParticleShape{ParticlesVis::ParticleShape::Default}, shape, setShape);
    DECLARE_SHADOW_PROPERTY_FIELD(shape);

    /// An optional user-defined shape used for rendering particles of this type.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const TriangleMesh>, shapeMesh, setShapeMesh, PROPERTY_FIELD_NO_SUB_ANIM);

    /// Activates the highlighting of the polygonal edges of the user-defined shape assigned to this particle type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, highlightShapeEdges, setHighlightShapeEdges, PROPERTY_FIELD_MEMORIZE);
    DECLARE_SHADOW_PROPERTY_FIELD(highlightShapeEdges);

    /// Activates the culling of back-facing mesh faces of the user-defined shape assigned to this particle type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, shapeBackfaceCullingEnabled, setShapeBackfaceCullingEnabled, PROPERTY_FIELD_MEMORIZE);
    DECLARE_SHADOW_PROPERTY_FIELD(shapeBackfaceCullingEnabled);

    /// Use the mesh colors instead of particle colors when rendering the user-defined shape.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, shapeUseMeshColor, setShapeUseMeshColor);
    DECLARE_SHADOW_PROPERTY_FIELD(shapeUseMeshColor);

    /// The mass of this particle type (maybe zero if not set).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, mass, setMass);
    DECLARE_SHADOW_PROPERTY_FIELD(mass);

    /// The chemical element this particle type represents.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ChemicalElement::X, chemicalElement, setChemicalElement);
    DECLARE_SHADOW_PROPERTY_FIELD(chemicalElement);

private:

    /// Data structure that holds the name, color, and radius of a chemical atom type.
    struct PredefinedChemicalType {
        QString symbol; // Chemical symbol (one or two characters)
        QString fullName; // Full name of the element (e.g., "Hydrogen")
        Color color;
        FloatType displayRadius;
        FloatType vdwRadius;
        FloatType mass;
        FloatType alternativeMass = 0; // Used if the official atomic weight of the element has changed over the years (e.g. Zn).
    };

    /// Data structure that holds the name and display color of a structural particle type.
    struct PredefinedStructuralType {
        QString name;
        Color color;
    };

    /// Default names, colors, and radii for some predefined particle types (chemical elements)
    static const std::array<PredefinedChemicalType, (size_t)ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES> _PredefinedChemicalTypes;

    /// Default names, colors, and radii for the predefined structure types (fcc, bcc, hcp, etc.)
    static const std::array<PredefinedStructuralType, (size_t)PredefinedStructureType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES>
        _predefinedStructureTypes;
};

}   // End of namespace
