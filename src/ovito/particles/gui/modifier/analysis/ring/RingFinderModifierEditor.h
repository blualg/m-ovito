////////////////////////////////////////////////////////////////////////////////////////
//
//  Ring Finder modifier editor for m-ovito.
//
//  This file is distributed under the GNU General Public License version 3 only.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>

namespace Ovito {

class RingFinderModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(RingFinderModifierEditor)

public:

    Q_INVOKABLE RingFinderModifierEditor() {}

protected:

    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    void updatePlot();
    void updateSummary();

    QPointer<DataTablePlotWidget> _histogramPlot;
    QPointer<QLabel> _summaryLabel;
};

}  // namespace Ovito
