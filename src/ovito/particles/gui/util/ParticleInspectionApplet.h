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
#include <ovito/particles/util/ParticleExpressionEvaluator.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/gui/util/ParticlePickingHelper.h>
#include <ovito/stdobj/gui/properties/PropertyInspectionApplet.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/core/viewport/ViewportGizmo.h>

namespace Ovito {

/**
 * \brief Data inspector page for particles.
 */
class ParticleInspectionApplet : public PropertyInspectionApplet
{
    OVITO_CLASS(ParticleInspectionApplet)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject() { PropertyInspectionApplet::initializeObject(Particles::OOClass()); }

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 0; }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;

    /// Lets the applet update the contents displayed in the inspector.
    virtual void updateDisplay() override;

    /// This is called when the applet is no longer visible.
    virtual void deactivate() override;

    /// Returns the help topic ID for the documentation page of this applet.
    virtual QString helpTopicId() const override { return QStringLiteral("manual:data_inspector.particles"); }

protected:

    /// Creates the evaluator object for filter expressions.
    virtual std::unique_ptr<PropertyExpressionEvaluator> createExpressionEvaluator() override {
        return std::make_unique<ParticleExpressionEvaluator>();
    }

    /// Determines whether the given property represents a color.
    virtual bool isColorProperty(const Property* property) const override {
        return PropertyInspectionApplet::isColorProperty(property)
                || property->typeId() == Particles::VectorColorProperty;
    }

private Q_SLOTS:

    /// Computes the inter-particle distances for the selected particles.
    void updateDistanceTable();

    /// Computes the angles formed by selected particles.
    void updateAngleTable();

private:

    /// Viewport input mode that lets the user pick particles.
    class PickingMode : public ViewportInputMode, ParticlePickingHelper, ViewportGizmo
    {
    public:

        /// Constructor.
        void initializeObject(ParticleInspectionApplet* applet) {
            ViewportInputMode::initializeObject();
            _applet = applet;
        }

        /// Handles the mouse up events for a viewport.
        virtual void mouseReleaseEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

        /// Handles the mouse move event for the given viewport.
        virtual void mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

        /// Lets the input mode render its overlay content in a viewport.
        virtual void renderOverlay(Viewport* vp, ViewportWindow* vpWin, FrameGraph& frameGraph, DataSet* dataset) override;

        /// Clears the list of picked particles.
        void resetSelection() {
            if(!_pickedElements.empty()) {
                _pickedElements.clear();
                requestViewportUpdate();
            }
        }

    protected:

        /// This is called by the system after the input handler has become the active handler.
        virtual void activated(bool temporaryActivation) override {
            ViewportInputMode::activated(temporaryActivation);
            inputManager()->addViewportGizmo(this);
        }

        /// This is called by the system after the input handler is no longer the active handler.
        virtual void deactivated(bool temporary) override {
            if(!temporary) {
                inputManager()->removeViewportGizmo(this);
                _pickedElements.clear();
            }
            ViewportInputMode::deactivated(temporary);
        }

    private:

        /// The owner object.
        ParticleInspectionApplet* _applet;

        /// The list of picked particles.
        std::vector<PickResult> _pickedElements;
    };

private:

    /// The viewport input for picking particles.
    OORef<PickingMode> _pickingMode;

    /// UI action that controls the display of inter-particle distances.
    QAction* _measuringModeAction;

    /// The table displaying the inter-particle distances.
    QTableWidget* _distanceTable;

    /// The table displaying the angles formed by selected particles.
    QTableWidget* _angleTable;
};

}   // End of namespace
