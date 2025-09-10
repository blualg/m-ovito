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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/core/oo/RefTargetListener.h>
#include <ovito/core/dataset/scene/ScenePreparation.h>
#include "DataInspectionApplet.h"

namespace Ovito {

/**
 * The data inspection panel.
 */
class OVITO_GUI_EXPORT DataInspectorPanel : public QWidget, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    DataInspectorPanel(MainWindowUI& ui);

    /// Selects a specific data object in the data inspector.
    bool selectDataObject(const PipelineNode* createdByNode, const QString& objectIdentifierHint, const QVariant& modeHint);

    /// Returns the currently selected pipeline whose output is being shown by the data inspector.
    Pipeline* selectedPipeline() const { return _selectedPipeline; }

    /// Returns the currently selected pipeline scene node.
    SceneNode* selectedSceneNode() const { return _selectedSceneNode; }

    /// Returns the most recent output data of the selected pipeline, which is displayed in the data inspector panel.
    const PipelineFlowState& pipelineOutput() const { return _pipelineOutput; }

protected:

    /// Handle state changes of the QWidget.
    virtual void changeEvent(QEvent* event) override;

public Q_SLOTS:

    /// Hides the inspector panel.
    void collapse();

    /// Shows the inspector panel.
    void open();

    /// Expands or collapses the panel depending on the current state.
    void toggle() { onTabBarClicked(-1); }

protected Q_SLOTS:

    /// This is called whenever the scene node selection has changed.
    void onSceneSelectionChanged(SelectionSet* selection);

    /// Is emitted whenever the scene of the current dataset has been changed and is being made ready for rendering.
    void onScenePreparationStarted();

    /// Is called whenever the scene became ready for rendering.
    void onScenePreparationFinished();

    /// Updates the contents displayed in the data inspector.
    void updateInspector();

    /// Is called when the user clicked on the tab bar.
    void onTabBarClicked(int index);

    /// Is called when the user selects a new tab.
    void onCurrentTabChanged(int index);

    /// Is called whenever the user has switched to a different page of the inspector.
    void onCurrentPageChanged(int index);

Q_SIGNALS:

    /// Signal is emitted whenever a different pipeline becomes the selected one.
    void selectedPipelineChanged(Pipeline* newPipeline);

protected:

    virtual void mouseReleaseEvent(QMouseEvent* event) override {
        if(event->button() == Qt::LeftButton && ViewportInputMode::getMousePosition(event).y() < _tabBar->height()) {
            toggle();
            event->accept();
        }
        QWidget::mouseReleaseEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override;

    /// Handles timer events for this object.
    virtual void timerEvent(QTimerEvent* event) override;

private:

    /// Updates the list of visible tabs.
    void updateTabsList();

    /// Evaluates the selected pipeline to obtains its output state.
    bool updatePipelineOutput();

private:

    /// The list of all installed data inspection applets.
    std::vector<OORef<DataInspectionApplet>> _applets;

    /// This maps applet indices to active tab indices.
    std::vector<int> _appletsToTabs;

    /// This maps tab indices to applets.
    std::vector<int> _tabsToApplets;

    /// The tab display.
    QTabBar* _tabBar;

    /// The container for the applet widgets.
    QStackedWidget* _appletContainer;

    /// The currently selected data pipeline.
    OORef<Pipeline> _selectedPipeline;

    /// The currently selected scene node.
    OORef<SceneNode> _selectedSceneNode;

    /// Helper object which asks the scene pipelines to compute their results.
    OORef<ScenePreparation> _scenePreparation;

    /// The most recent output data of the selected pipeline, which is displayed in the data inspector panel.
    PipelineFlowState _pipelineOutput;

    /// This timer is used to play the activity animation with some delay.
    QBasicTimer _activityDelayTimer;

    /// Animation shown in the title bar to indicate process.
    QMovie _waitingForSceneAnim;

    /// UI element indicating that we are waiting for computations to complete.
    QLabel* _waitingForSceneIndicator;

    /// UI element for opening/closing the inspector panel.
    QPushButton* _expandCollapseButton;

    // The icon for the expand button state.
    QIcon _expandIcon = QIcon::fromTheme("modify_modifier_move_up");

    // The icon for the collapse button state.
    QIcon _collapseIcon = QIcon::fromTheme("modify_modifier_move_down");

    /// The active page of the inspector.
    int _activeAppletIndex = -1;

    /// Indicates whether the inspector panel is currently open or collapsed.
    bool _inspectorActive = false;
};

}   // End of namespace
