////////////////////////////////////////////////////////////////////////////////////////
//
//  Atomic noising modifier editor for m-ovito.
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

class AtomicNoisingModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(AtomicNoisingModifierEditor)

public:

    Q_INVOKABLE AtomicNoisingModifierEditor() {}

protected:

    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    void updateSummary();

    QPointer<QLabel> _summaryLabel;
};

}  // namespace Ovito
