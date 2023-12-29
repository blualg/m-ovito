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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdmod/modifiers/AssignColorModifier.h>

namespace Ovito {

/**
 * \brief Function for the AssignColorModifier that operates on particles.
 */
class ParticlesAssignColorModifierDelegate : public AssignColorModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class ParticlesAssignColorModifierDelegateClass : public AssignColorModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AssignColorModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Particles::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("particles"); }
    };

    OVITO_CLASS_META(ParticlesAssignColorModifierDelegate, ParticlesAssignColorModifierDelegateClass)
    OVITO_CLASSINFO("DisplayName", "Particles");

public:

    /// Constructor.
    using AssignColorModifierDelegate::AssignColorModifierDelegate;

protected:

    /// \brief returns the ID of the standard property that will receive the assigned colors.
    virtual int outputColorPropertyId() const override { return Particles::ColorProperty; }
};

/**
 * \brief Function for the AssignColorModifier that operates on particle vectors.
 */
class ParticleVectorsAssignColorModifierDelegate : public AssignColorModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public AssignColorModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AssignColorModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Particles::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("vectors"); }
    };

    OVITO_CLASS_META(ParticleVectorsAssignColorModifierDelegate, OOMetaClass)
    OVITO_CLASSINFO("DisplayName", "Particle vectors");

public:

    /// Constructor.
    using AssignColorModifierDelegate::AssignColorModifierDelegate;

protected:

    /// \brief returns the ID of the standard property that will receive the assigned colors.
    virtual int outputColorPropertyId() const override { return Particles::VectorColorProperty; }
};

/**
 * \brief Function for the AssignColorModifier that operates on bonds.
 */
class BondsAssignColorModifierDelegate : public AssignColorModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class BondsAssignColorModifierDelegateClass : public AssignColorModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AssignColorModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Bonds::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("bonds"); }
    };

    OVITO_CLASS_META(BondsAssignColorModifierDelegate, BondsAssignColorModifierDelegateClass)
    OVITO_CLASSINFO("DisplayName", "Bonds");

public:

    /// Constructor.
    using AssignColorModifierDelegate::AssignColorModifierDelegate;

protected:

    /// \brief returns the ID of the standard property that will receive the computed colors.
    virtual int outputColorPropertyId() const override { return Bonds::ColorProperty; }
};

}   // End of namespace
