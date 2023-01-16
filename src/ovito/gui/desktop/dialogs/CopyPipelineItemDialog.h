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


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * This dialog box lets the user make a copy of a pipeline scene node.
 */
class CopyPipelineItemDialog : public QDialog
{
    Q_OBJECT

public:

    /// Constructor.
    CopyPipelineItemDialog(MainWindow& mainWindow, QWidget* parentWindow, PipelineSceneNode* sourcePipeline, QVector<OORef<PipelineObject>> pipelineObjects);

private Q_SLOTS:

    /// Is called when the user presses the 'Ok' button.
    void onAccept();

private:

    /// The parent window.
    MainWindow& _mainWindow; 

    /// The source pipeline.
    OORef<PipelineSceneNode> _sourcePipeline;

    /// The pipeline objects to be copied.
    QVector<OORef<PipelineObject>> _pipelineObjects;

    /// Target pipeline selector.
    QComboBox* _destinationPipelineList;

    /// Selects the insertion position.
    QRadioButton* _insertAtEndBtn;
    QRadioButton* _insertAtStartBtn;

    /// Controls the cloning mode.
    QCheckBox* _shareBetweenPipelinesBox;
};

}   // End of namespace
