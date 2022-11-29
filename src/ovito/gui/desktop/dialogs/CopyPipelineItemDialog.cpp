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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/oo/CloneHelper.h>
#include "CopyPipelineItemDialog.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
CopyPipelineItemDialog::CopyPipelineItemDialog(MainWindow& mainWindow, QWidget* parent, PipelineSceneNode* sourcePipeline, QVector<OORef<PipelineObject>> pipelineObjects) :
	QDialog(parent), _mainWindow(mainWindow), _sourcePipeline(sourcePipeline), _pipelineObjects(std::move(pipelineObjects))
{
	setWindowTitle(tr("Copy Pipeline Items"));

	QVBoxLayout* mainLayout = new QVBoxLayout(this);

	QGridLayout* gridLayout = new QGridLayout();
	mainLayout->addLayout(gridLayout, 1);
	gridLayout->setColumnStretch(1, 1);

	_destinationPipelineList = new QComboBox(this);
	gridLayout->addWidget(new QLabel(tr("Copy to pipeline:")), 0, 0);
	gridLayout->addWidget(_destinationPipelineList, 0, 1);

	// Populate list of scene pipelines.
	if(Scene* scene = _mainWindow.activeScene()) {
		scene->visitChildren([&](SceneNode* node) -> bool {
			if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(node)) {
				QString itemLabel = pipeline->objectTitle();
				if(pipeline == sourcePipeline)
					itemLabel += tr(" (source pipeline)");
				_destinationPipelineList->addItem(std::move(itemLabel), QVariant::fromValue(OORef<OvitoObject>(pipeline)));
				if(pipeline == sourcePipeline)
					_destinationPipelineList->setCurrentIndex(_destinationPipelineList->count() - 1);
				else {
	#ifndef OVITO_BUILD_PROFESSIONAL
					QStandardItem* item = static_cast<QStandardItemModel*>(_destinationPipelineList->model())->item(_destinationPipelineList->count() - 1);
					item->setEnabled(false);
					item->setText(item->text() + " (requires OVITO Pro)");
	#endif
				}
			}
			return true;
		});
	}

	gridLayout->addWidget(new QLabel(tr("Insert at:")), 1, 0);
	QButtonGroup* insertionPositionGroup = new QButtonGroup(this);
	_insertAtEndBtn = new QRadioButton(tr("End of pipeline (top)"));
	_insertAtStartBtn = new QRadioButton(tr("Beginning of pipeline (bottom)"));
	insertionPositionGroup->addButton(_insertAtEndBtn);
	insertionPositionGroup->addButton(_insertAtStartBtn);
	gridLayout->addWidget(_insertAtEndBtn, 1, 1);
	gridLayout->addWidget(_insertAtStartBtn, 2, 1);
	_insertAtEndBtn->setChecked(true);
	
	// Only allow insertion at beginning of pipeline if it's a pipeline source
	// that is being cloned.
	if(!boost::algorithm::all_of(_pipelineObjects, [](const auto& item) {
		return ModifierApplication::OOClass().isMember(item);
	})) {
		_insertAtStartBtn->setChecked(true);
		_insertAtEndBtn->setEnabled(false);
	}
	
	_shareBetweenPipelinesBox = new QCheckBox(tr("Share with source pipeline (do not duplicate)"));
	gridLayout->addWidget(_shareBetweenPipelinesBox, 3, 0, 1, 2);

	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, Qt::Horizontal, this);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &CopyPipelineItemDialog::onAccept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &CopyPipelineItemDialog::reject);
	connect(buttonBox, &QDialogButtonBox::helpRequested, []() {
		ActionManager::openHelpTopic("manual:clone_pipeline.copy_pipeline_items_dialog");
	});
	mainLayout->addWidget(buttonBox);
}

/******************************************************************************
* Is called when the user presses the 'Ok' button
******************************************************************************/
void CopyPipelineItemDialog::onAccept()
{
	UndoableTransaction::handleExceptions(_mainWindow, tr("Copy pipeline item"), [this]() {
		OORef<PipelineSceneNode> destinationPipeline = static_object_cast<PipelineSceneNode>(_destinationPipelineList->currentData().value<OORef<OvitoObject>>());
		CloneHelper cloneHelper;

		// Do not create any animation keys during cloning.
		AnimationSuspender animSuspender(_mainWindow);

		OORef<PipelineObject> precedingObj;
		for(auto item = _pipelineObjects.crbegin(); item != _pipelineObjects.crend(); ++item) {
			if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(*item)) {
				// Copy modifier application.
				OORef<ModifierApplication> clonedModApp = cloneHelper.cloneObject(modApp, false);
				clonedModApp->setInput(nullptr); // To avoid cyclic reference errors.
				if(!_shareBetweenPipelinesBox->isChecked()) {
					clonedModApp->setModifier(cloneHelper.cloneObject(clonedModApp->modifier(), true));
				}
				if(!precedingObj) {
					if(_insertAtEndBtn->isChecked()) {
						// Append copied modifiers at end of current pipeline.
						precedingObj = destinationPipeline->dataProvider();
					}
					else {
						// Prepend copied modifiers at beginning of current pipeline, right after the existing pipeline source.
						precedingObj = destinationPipeline->pipelineSource();
					}
				}
				clonedModApp->setInput(precedingObj);
				precedingObj = clonedModApp;
			}
			else {
				// Copy or clone pipeline source.
				precedingObj = _shareBetweenPipelinesBox->isChecked() ? *item : cloneHelper.cloneObject(*item, false);
			}
		}

		if(_insertAtEndBtn->isChecked()) {
			// Append copied modifiers at end of current pipeline.
			destinationPipeline->setDataProvider(precedingObj);
		}
		else {
			// Prepend copied modifiers at beginning of current pipeline, right after the existing pipeline source.
			destinationPipeline->setPipelineSource(precedingObj);
		}
	});
	accept();
}

}	// End of namespace
