////////////////////////////////////////////////////////////////////////////////////////
//
//  Ring Finder modifier editor for m-ovito.
//
//  This file is distributed under the GNU General Public License version 3 only.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/analysis/ring/RingFinderModifier.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <QLabel>
#include <QSizePolicy>
#include "RingFinderModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(RingFinderModifierEditor);
SET_OVITO_OBJECT_EDITOR(RingFinderModifier, RingFinderModifierEditor);

void RingFinderModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Ring Finder"), rolloutParams, "manual:m-ovito.ring_finder");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setColumnStretch(1, 1);

    IntegerParameterUI* minimumSizeUI =
        createParamUI<IntegerParameterUI>(PROPERTY_FIELD(RingFinderModifier::minimumRingSize));
    gridLayout->addWidget(minimumSizeUI->label(), 0, 0);
    gridLayout->addLayout(minimumSizeUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* maximumSizeUI =
        createParamUI<IntegerParameterUI>(PROPERTY_FIELD(RingFinderModifier::maximumRingSize));
    gridLayout->addWidget(maximumSizeUI->label(), 1, 0);
    gridLayout->addLayout(maximumSizeUI->createFieldLayout(), 1, 1);

    BooleanParameterUI* createPolygonsUI =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(RingFinderModifier::createPolygons));
    gridLayout->addWidget(createPolygonsUI->checkBox(), 2, 0, 1, 2);

    layout->addLayout(gridLayout);

    _summaryLabel = new QLabel(rollout);
    _summaryLabel->setWordWrap(true);
    _summaryLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    _summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(_summaryLabel);

    layout->addWidget(new QLabel(tr("Ring size histogram:")));
    _histogramPlot = new DataTablePlotWidget();
    _histogramPlot->setMinimumHeight(180);
    _histogramPlot->setMaximumHeight(180);
    layout->addWidget(_histogramPlot);

    layout->addWidget(new OpenDataInspectorButton(
        this, tr("Show histogram in data inspector"), RingFinderModifier::HistogramTableIdentifier, 1));

    StatusWidget* statusWidget = createParamUI<ObjectStatusDisplay>()->statusWidget();
    statusWidget->setMinimumHeight(64);
    statusWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->addWidget(statusWidget);

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &RingFinderModifierEditor::updatePlot);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &RingFinderModifierEditor::updateSummary);
}

void RingFinderModifierEditor::updatePlot()
{
    handleExceptions([&]() {
        DataOORef<const DataTable> table =
            getPipelineOutput().getObjectBy<DataTable>(modificationNode(), RingFinderModifier::HistogramTableIdentifier);
        _histogramPlot->setTable(std::move(table));
    });
}

void RingFinderModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        const PipelineFlowState& state = getPipelineOutput();
        const QVariant total = state.getAttributeValue(modificationNode(), QStringLiteral("RingCount"));
        if(!total.isValid()) {
            _summaryLabel->setText(tr("No ring results for the current frame. A bond topology is required."));
            return;
        }

        const int minimumSize =
            state.getAttributeValue(modificationNode(), QStringLiteral("RingFinder.minimum_ring_size")).toInt();
        const int maximumSize =
            state.getAttributeValue(modificationNode(), QStringLiteral("RingFinder.maximum_ring_size")).toInt();
        const int polygonCount =
            state.getAttributeValue(modificationNode(), QStringLiteral("RingFinder.polygon_count")).toInt();

        QStringList terms;
        for(int size = minimumSize; size <= maximumSize; ++size) {
            const int count = state.getAttributeValue(modificationNode(), QStringLiteral("%1-RingCount").arg(size)).toInt();
            terms << tr("%1-ring: %2").arg(size).arg(count);
        }

        _summaryLabel->setText(tr("Rings: %1 total (%2). Polygon facets: %3.")
                                   .arg(total.toInt())
                                   .arg(terms.join(tr(", ")))
                                   .arg(polygonCount));
    });
}

}  // namespace Ovito
