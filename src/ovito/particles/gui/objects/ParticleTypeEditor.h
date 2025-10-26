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

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the ParticleType class.
 */
class ParticleTypeEditor : public PropertiesEditor
{
    OVITO_CLASS(ParticleTypeEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    /// Creates a button that opens a menu for managing the presets for a particle type parameter.
    QToolButton* createPresetsMenuButton(const QString& parameterName, std::function<void(ParticleType*)> resetFunc, std::function<void(const ParticleType*)> setDefaultFunc, std::function<bool(const ParticleType*)> isUnchangedFunc);

private Q_SLOTS:

    /// Called when the contents of the editor have been replaced with a new object.
    void onContentsReplaced();

    /// Called when the user wants to pick and load a mesh-based particle shape from disk.
    void onLoadParticleShape();

private:

    QToolButton* _colorPresetsMenuButton;
    QToolButton* _displayRadiusPresetsMenuButton;
    QToolButton* _vdwRadiusPresetsMenuButton;
    QPushButton* _loadShapeBtn;
};

}   // End of namespace
