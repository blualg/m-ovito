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


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/oo/RefMaker.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

/**
 * An item managed by the PipelineListModel representing a data source, data object, modifier application or vis element.
 */
class OVITO_GUIBASE_EXPORT PipelineListItem : public QObject, public RefMaker
{
    OVITO_CLASS(PipelineListItem)
    Q_OBJECT

public:

    enum PipelineItemType {
        DeletedObject,
        DeletedVisualElement,
        VisualElement,
        Modifier,
        DataSource,
        ModifierGroup,
        VisualElementsHeader,
        ModificationsHeader,
        DataSourceHeader,
        PipelineBranch
    };
    Q_ENUM(PipelineItemType);

public:

    /// Constructor.
    void initializeObject(RefTarget* object, PipelineItemType itemType, PipelineListItem* parent = nullptr);

    /// Returns true if this is a sub-object entry.
    bool isSubObject() const { return _parent != nullptr; }

    /// Returns the parent entry if this item represents a sub-object.
    PipelineListItem* parent() const { return _parent; }

    /// Returns the status of the object represented by the list item.
    const PipelineStatus& status() const;

    /// Returns a short piece of information (typically a string or color) to be displayed next to the object's title in the pipeline editor.
    QVariant shortInfo(SceneNode* selectedSceneNode) const;

    /// Returns whether an active computation is in progress for this object.
    bool isObjectActive() const { return _isObjectActive; }

    /// Returns the title text for this list item.
    const QString& title() const { return _title; }

    /// Returns the type of this list item.
    PipelineItemType itemType() const { return _itemType; }

    /// Returns whether this list item represents an OVITO object.
    bool isObjectItem() const { return _itemType < VisualElementsHeader; }

Q_SIGNALS:

    /// This signal is emitted when this item has changed.
    void itemChanged(PipelineListItem* item);

    /// This signal is emitted when the list of sub-items of this item has changed.
    void subitemsChanged(PipelineListItem* parent);

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Updates the stored title string of the item.
    void updateTitle();

    /// Handles timer events for this object.
    virtual void timerEvent(QTimerEvent* event) override;

private:

    /// The object represented by this item in the list box.
    DECLARE_REFERENCE_FIELD(OORef<RefTarget>, object);

    /// The type of this list item.
    PipelineItemType _itemType;

    /// If this is a sub-object entry then this points to the parent.
    PipelineListItem* _parent;

    /// The display title of the list item.
    QString _title;

    /// Timer used to throttle UI updates due to status and activity state changes.
    QBasicTimer _statusAndActivityTimer;

    /// Indicates whether the object is currently performing a computation.
    bool _isObjectActive = false;

    /// Indicates that the UI of the item needs to be updated.
    bool _statusUpdatePending = false;

    /// Indicates that the UI of the item needs to be updated.
    bool _activityUpdatePending = false;
};

}   // End of namespace
