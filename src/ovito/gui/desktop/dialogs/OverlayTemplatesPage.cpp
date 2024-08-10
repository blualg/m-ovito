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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/CommandPanel.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/OverlayCommandPage.h>
#include <ovito/gui/base/mainwin/OverlayListModel.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include "OverlayTemplatesPage.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OverlayTemplatesPage);

/******************************************************************************
* When the user is creating a new template, this method populates the list of available objects,
* which the the user can select to be included in the template.
******************************************************************************/
QVector<QTreeWidgetItem*> OverlayTemplatesPage::populateAvailableObjectsList(QTreeWidget* objectListWidget, QComboBox* nameBox)
{
    OverlayListModel* overlayModel = mainWindow().commandPanel()->overlayPage()->overlayListModel();
    ViewportOverlay* selectedOverlay = overlayModel->selectedLayer();
    QVector<QTreeWidgetItem*> itemList;

    // Iterate over the overlays of the selected viewport.
    if(Viewport* viewport = overlayModel->selectedViewport()) {
        QVector<OORef<ViewportOverlay>> layers;
        layers.append(viewport->underlays());
        layers.append(viewport->overlays());
        for(auto layer = layers.crbegin(); layer != layers.crend(); ++layer) {
            QTreeWidgetItem* listItem = new QTreeWidgetItem(objectListWidget, { (*layer)->objectTitle() });
            listItem->setFlags(Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren));
                listItem->setCheckState(0, (*layer == selectedOverlay) ? Qt::Checked : Qt::Unchecked);
            listItem->setData(0, Qt::UserRole, QVariant::fromValue(OORef<OvitoObject>(*layer)));
            itemList.push_back(listItem);
        }
    }
    if(itemList.empty())
        throw Exception(tr("A viewport layer template must always be created on the basis of an existing layer, but the selected viewport does not have any layers attached. "
                            "Please close this dialog, add some layer to the viewport first, configure its settings and then come back here to create a template from it."));
    objectListWidget->setMaximumHeight(objectListWidget->sizeHintForRow(0) * qBound(3, itemList.size(), 10) + 2 * objectListWidget->frameWidth());

    if(selectedOverlay) {
        if(selectedOverlay->title().isEmpty())
            nameBox->setCurrentText(tr("Custom %1").arg(selectedOverlay->objectTitle()));
        else
            nameBox->setCurrentText(selectedOverlay->title());
    }
    else {
        nameBox->setCurrentText(tr("Custom viewport layer template 1"));
    }

    return itemList;
}

}   // End of namespace
