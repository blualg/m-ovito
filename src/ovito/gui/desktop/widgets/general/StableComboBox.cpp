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

#include <ovito/gui/desktop/GUI.h>
#include "StableComboBox.h"

namespace Ovito {

/******************************************************************************
* Returns the standard warning icon.
******************************************************************************/
const QIcon& StableComboBox::warningIcon()
{
    static QIcon warningIcon(QStringLiteral(":/guibase/mainwin/status/status_warning.png"));
    return warningIcon;
}

/******************************************************************************
* Replaces the list of items.
******************************************************************************/
void StableComboBox::setItems(const QList<std::pair<QString, QVariant>>& itemsWithData)
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(this->model());
    OVITO_ASSERT(model);

    // Overwrite existing items.
    int oldCount = model->rowCount();
    for(int i = 0; i < itemsWithData.size() && i < oldCount; ++i) {
        QStandardItem* item = model->item(i);
        item->setText(itemsWithData[i].first);
        item->setData(itemsWithData[i].second, Qt::UserRole);
    }

    // Add new items.
    for(int i = oldCount; i < itemsWithData.size(); ++i) {
        QStandardItem* item = new QStandardItem(itemsWithData[i].first);
        item->setData(itemsWithData[i].second, Qt::UserRole);
        model->insertRow(i, item);
    }

    // Remove excess items from model.
    for(int i = oldCount - 1; i >= itemsWithData.size(); --i)
        model->removeRow(i);
}

/******************************************************************************
* Replaces the list of items.
******************************************************************************/
void StableComboBox::setItems(std::vector<std::unique_ptr<QStandardItem>> items)
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(this->model());
    OVITO_ASSERT(model);

    // Overwrite existing items.
    int oldCount = model->rowCount();
    for(int i = 0; i < (int)items.size() && i < oldCount; ++i) {
        model->setItem(i, items[i].release());
    }

    // Add new items.
    for(int i = oldCount; i < (int)items.size(); ++i) {
        model->insertRow(i, items[i].release());
    }

    // Remove excess items from model.
    for(int i = oldCount - 1; i >= (int)items.size(); --i)
        model->removeRow(i);
}

}   // End of namespace
