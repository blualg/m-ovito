import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.ovito

import "qrc:/gui/ui" as Ui

Ui.RolloutPanel {
	title: qsTr("Triangle mesh")
	helpTopicId: "manual:visual_elements.triangle_mesh"

	GridLayout {
		anchors.fill: parent
		columns: 2

		Ui.ParameterLabel { propertyField: "color" }
		Ui.ColorParameter {
			propertyField: "color" // PROPERTY_FIELD(TriangleMeshVis::color)
			Layout.fillWidth: true
		}

		Ui.ParameterLabel { propertyField: "transparencyController" }
		Ui.FloatParameter {
			propertyField: "transparencyController" // PROPERTY_FIELD(TriangleMeshVis::transparencyController)
			Layout.fillWidth: true
		}

		Ui.BooleanCheckBoxParameter {
			propertyField: "highlightEdges" // PROPERTY_FIELD(TriangleMeshVis::highlightEdges)
			Layout.columnSpan: 2
		}
	}
}