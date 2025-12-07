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
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/gui/desktop/mainwin/MainWindowUI.h>
#include "AnimationTrackBar.h"

namespace Ovito {

/******************************************************************************
* The constructor of the AnimationTrackBar class.
******************************************************************************/
AnimationTrackBar::AnimationTrackBar(MainWindowUI& ui, AnimationTimeSlider* timeSlider, QWidget* parent) :
    QFrame(parent),
    UserInterfaceComponent<MainWindowUI>(ui),
    _timeSlider(timeSlider),
    _keyPen(Qt::black),
    _selectedKeyPen(QColor(255,255,255)),
    _selectionCursor(Qt::CrossCursor),
    _isDragging(false),
    _dragStartPos(-1)
{
    _keyBrushes[0] = QBrush(QColor(150,150,200));   // Color for float controller keys
    _keyBrushes[1] = QBrush(QColor(150,150,200));   // Color for integer controller keys
    _keyBrushes[2] = QBrush(QColor(150,200,150));   // Color for vector controller keys
    _keyBrushes[3] = QBrush(QColor(200,150,150));   // Color for position controller keys
    _keyBrushes[4] = QBrush(QColor(200,200,150));   // Color for rotation controller keys
    _keyBrushes[5] = QBrush(QColor(150,200,200));   // Color for scaling controller keys
    _keyBrushes[6] = QBrush(QColor(150,150,150));   // Color for transformation controller keys

    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(true);
    setMouseTracking(true);

    connect(&datasetContainer(), &DataSetContainer::animationIntervalChanged, this, qOverload<>(&AnimationTrackBar::update));
    connect(&datasetContainer(), &DataSetContainer::currentFrameChanged, this, qOverload<>(&AnimationTrackBar::update));
    connect(&datasetContainer(), &DataSetContainer::timeFormatChanged, this, qOverload<>(&AnimationTrackBar::update));
    connect(&datasetContainer(), &DataSetContainer::selectionChangeComplete, this, &AnimationTrackBar::onRebuildControllerList);
    _objects.connect(this, &AnimationTrackBar::onObjectNotificationEvent);
    _controllers.connect(this, &AnimationTrackBar::onControllerNotificationEvent);
}

/******************************************************************************
* Handles paint events.
******************************************************************************/
void AnimationTrackBar::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);

    // Paint track bar only if there is more than one animation frame.
    if(!animSettings())
        return;
    int numFrames = animSettings()->numberOfFrames();
    if(numFrames <= 1)
        return;

    QPainter painter(this);

    QRect clientRect = frameRect();
    clientRect.adjust(frameWidth(), frameWidth(), -frameWidth(), -frameWidth());

    // Compute minor tick positions.
    int startFrame, frameStep, endFrame;
    std::tie(startFrame, frameStep, endFrame) = _timeSlider->tickRange(10); // 10px minimum separation between minor ticks

    // Compute major tick positions.
    int startFrameMajor, frameStepMajor;
    std::tie(startFrameMajor, frameStepMajor, std::ignore) = _timeSlider->tickRange(_timeSlider->maxTickLabelWidth());

    painter.setPen(QPen(QColor(180,180,220)));
    for(int frame = startFrame; frame <= endFrame; frame += frameStep) {
        int pos = _timeSlider->frameToPos(frame);
        if((frame - startFrameMajor) % frameStepMajor == 0)
            painter.drawLine(pos, clientRect.top(), pos, clientRect.bottom());
        else
            painter.drawLine(pos, clientRect.top(), pos, clientRect.center().y());
    }

    // Draw the animation keys.
    for(KeyframeController* ctrl : _controllers.targets()) {
        // Draw keys only if there are more two or more of them.
        if(ctrl->keys().size() >= 2) {
            for(AnimationKey* key : ctrl->keys()) {
                paintKey(painter, key, ctrl);
            }
        }
    }

    // Draw the current time marker.
    int currentFramePos = _timeSlider->frameToPos(animSettings()->currentFrame());
    painter.setBrush(Qt::blue);
    painter.setPen(Qt::black);
    QPoint marker[3] = {{ currentFramePos - 3, clientRect.top() },
                        { currentFramePos + 3, clientRect.top() },
                        { currentFramePos    , clientRect.top() + 3 }
    };
    painter.drawConvexPolygon(marker, 3);
}

/******************************************************************************
* Computes the display rectangle of an animation key.
******************************************************************************/
QRect AnimationTrackBar::keyRect(AnimationKey* key, bool forDisplay) const
{
    OVITO_ASSERT(animSettings());

    // Don't draw keys that are not within the active animation interval.
    if(key->time().frame() < animSettings()->firstFrame() ||
        key->time().frame() > animSettings()->lastFrame()) return QRect();

    QRect clientRect = frameRect();
    clientRect.adjust(frameWidth(), frameWidth(), -frameWidth(), -frameWidth());

    int width = 6;
    int pos = _timeSlider->timeToPos(key->time());
    int offset = 0;

    bool done = false;
    for(KeyframeController* ctrl : _controllers.targets()) {
        // Draw keys only if there are more two or more of them.
        if(ctrl->keys().size() >= 2) {
            for(AnimationKey* key2 : ctrl->keys()) {
                if(key2 == key) done = true;
                else if(key->time() == key2->time()) offset++;
            }
        }
        if(done && forDisplay) break;
    }
    if(forDisplay)
        return QRect(pos - width / 2 + offset*2, clientRect.top() + 4 - offset*2, width, clientRect.height() - 5);
    else
        return QRect(pos - width / 2, clientRect.top() + 4 - offset*2, width + offset*2, clientRect.height() - 5 + offset*2);
}

/******************************************************************************
* Paints the symbol for a single animation key.
******************************************************************************/
void AnimationTrackBar::paintKey(QPainter& painter, AnimationKey* key, KeyframeController* ctrl) const
{
    QRect rect = keyRect(key, true);
    if(!rect.isValid())
        return;

    painter.setBrush(_keyBrushes[ctrl->controllerType() % _keyBrushes.size()]);
    painter.setPen(_selectedKeys.targets().contains(key) ? _selectedKeyPen : _keyPen);
    painter.drawRect(rect);
}

/******************************************************************************
* Returns the recommended size for the widget.
******************************************************************************/
QSize AnimationTrackBar::sizeHint() const
{
    return QSize(QFrame::sizeHint().width(), fontMetrics().height() * 1 + frameWidth() * 2);
}

/******************************************************************************
* This is called when the current scene node selection has changed.
******************************************************************************/
void AnimationTrackBar::onRebuildControllerList()
{
    // Rebuild the list of controllers shown in the track bar.
    _controllers.clear();
    _objects.clear();
    _selectedKeys.clear();
    _parameterNames.clear();

    // Traverse object graphs of selected scene nodes to find all animation controllers.
    if(SelectionSet* selection = datasetContainer().activeSelectionSet()) {
        for(SceneNode* node : selection->nodes()) {
            findControllers(node);
            if(Pipeline* pipeline = node->pipeline())
                findControllers(pipeline);
        }
    }

    _deferredUpdateScheduled = false;
    update();
}

/******************************************************************************
* Recursive function that finds all controllers in the object graph.
******************************************************************************/
void AnimationTrackBar::findControllers(RefTarget* target)
{
    OVITO_CHECK_OBJECT_POINTER(target);

    bool hasSubAnimatables = false;

    // Iterate over all reference fields of the current target.
    for(const PropertyFieldDescriptor* field : target->getOOMetaClass().propertyFields()) {
        if(field->isReferenceField() && !field->flags().testFlag(PROPERTY_FIELD_NO_SUB_ANIM)) {
            hasSubAnimatables = true;
            if(!field->isVector()) {
                if(RefTarget* subTarget = target->getReferenceFieldTarget(field)) {
                    findControllers(subTarget);
                    addController(subTarget, target, field);
                }
            }
            else {
                int count = target->getVectorReferenceFieldSize(field);
                for(int i = 0; i < count; i++) {
                    if(RefTarget* subTarget = target->getVectorReferenceFieldTarget(field, i)) {
                        findControllers(subTarget);
                        addController(subTarget, target, field);
                    }
                }
            }
        }
    }

    if(hasSubAnimatables)
        _objects.push_back(target);
}

/******************************************************************************
* Checks if the given ref target is a controller, and, if yes, add it to our
* list of controllers.
******************************************************************************/
void AnimationTrackBar::addController(RefTarget* target, RefTarget* owner, const PropertyFieldDescriptor* field)
{
    if(KeyframeController* ctrl = dynamic_object_cast<KeyframeController>(target)) {
        int ctrlIndex = _controllers.targets().indexOf(ctrl);
        QString pname = owner->objectTitle() + QStringLiteral(" - ") + field->displayName();
        if(ctrlIndex == -1) {
            _controllers.push_back(ctrl);
            _parameterNames.push_back(pname);
        }
        else if(_parameterNames[ctrlIndex].contains(pname) == false) {
            _parameterNames[ctrlIndex] += QStringLiteral(",") + pname;
        }
    }
}

/******************************************************************************
* Is called whenever one of the objects being monitored sends a notification signal.
******************************************************************************/
void AnimationTrackBar::onObjectNotificationEvent(RefTarget* source, const ReferenceEvent& event)
{
    // Rebuild the complete controller list whenever the reference object changes.
    if(event.type() == ReferenceEvent::ReferenceChanged || event.type() == ReferenceEvent::ReferenceAdded || event.type() == ReferenceEvent::ReferenceRemoved) {
        if(!_isDragging && !_objects.targets().empty() && !static_cast<const PropertyFieldEvent&>(event).field()->flags().testFlag(PROPERTY_FIELD_NO_SUB_ANIM)) {
            _objects.clear();
            if(!_deferredUpdateScheduled) {
                _deferredUpdateScheduled = true;
                QTimer::singleShot(100, this, &AnimationTrackBar::onRebuildControllerList);
            }
        }
    }
}

/******************************************************************************
* Is called whenever one of the controllers being monitored sends a notification signal.
******************************************************************************/
void AnimationTrackBar::onControllerNotificationEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged || event.type() == ReferenceEvent::ReferenceChanged || event.type() == ReferenceEvent::ReferenceAdded || event.type() == ReferenceEvent::ReferenceRemoved) {
        // Repaint track bar whenever a key has been created, deleted, or moved.
        update();
    }
    else if(event.type() == ReferenceEvent::TargetDeleted) {
        _parameterNames.removeAt(_controllers.targets().indexOf(static_cast<KeyframeController*>(source)));
    }
}

/******************************************************************************
* Finds all keys under the mouse cursor.
******************************************************************************/
QVector<AnimationKey*> AnimationTrackBar::hitTestKeys(QPoint pos) const
{
    QVector<AnimationKey*> result;
    for(KeyframeController* ctrl : _controllers.targets()) {
        if(ctrl->keys().size() >= 2) {
            for(int index = ctrl->keys().size() - 1; index >= 0; index--) {
                AnimationKey* key = ctrl->keys()[index];
                if((result.empty() && keyRect(key, false).contains(pos)) || (!result.empty() && result.front()->time() == key->time())) {
                    result.push_back(key);
                }
            }
        }
    }
    return result;
}

/******************************************************************************
* Returns the list index of the controller that owns the given key.
******************************************************************************/
int AnimationTrackBar::controllerIndexFromKey(AnimationKey* key) const
{
    for(int index = 0; index < _controllers.targets().size(); index++) {
        if(_controllers.targets()[index]->keys().contains(key))
            return index;
    }
    OVITO_ASSERT(false);
    return -1;
}

/******************************************************************************
* Handles mouse press events.
******************************************************************************/
void AnimationTrackBar::mousePressEvent(QMouseEvent* event)
{
    _dragStartPos = -1;
    if(event->button() == Qt::LeftButton) {
        QVector<AnimationKey*> clickedKeys = hitTestKeys(event->pos());
        if(!event->modifiers().testFlag(Qt::ControlModifier)) {
            if(clickedKeys.empty() ||
                    std::find_first_of(clickedKeys.begin(), clickedKeys.end(), _selectedKeys.targets().begin(), _selectedKeys.targets().end()) == clickedKeys.end())
                _selectedKeys.setTargets(clickedKeys);
        }
        else {
            for(AnimationKey* key : clickedKeys) {
                if(!_selectedKeys.targets().contains(key))
                    _selectedKeys.push_back(key);
                else
                    _selectedKeys.remove(key);
            }
        }
        if(!clickedKeys.empty()) {
            _dragStartPos = event->pos().x();
        }
        _isDragging = false;
        event->accept();
        update();
    }
    else if(event->button() == Qt::RightButton) {
        if(_isDragging) {
            _isDragging = false;
            _undoTransaction.cancel();
        }
        else {
            _isDragging = false;
            QVector<AnimationKey*> clickedKeys = hitTestKeys(event->pos());
            if(clickedKeys.empty() ||
                    std::find_first_of(clickedKeys.begin(), clickedKeys.end(), _selectedKeys.targets().begin(), _selectedKeys.targets().end()) == clickedKeys.end()) {
                _selectedKeys.setTargets(clickedKeys);
                update();
            }
            showKeyContextMenu(event->pos(), clickedKeys);
        }
        event->accept();
    }
}

/******************************************************************************
* Handles mouse move events.
******************************************************************************/
void AnimationTrackBar::mouseMoveEvent(QMouseEvent* event)
{
    if(event->buttons() == Qt::NoButton) {
        QVector<AnimationKey*> keys = hitTestKeys(event->pos());
        if(keys.empty()) {
            unsetCursor();
            QToolTip::hideText();
        }
        else if(animSettings()) {
            setCursor(_selectionCursor);
            QString tooltipText = tr("<p style='white-space:pre'>Time position %1:").arg(animSettings()->timeToString(keys.front()->time()));
            for(AnimationKey* key : keys) {
                tooltipText += QStringLiteral("<br>  %1: %2")
                        .arg(_parameterNames[controllerIndexFromKey(key)])
                        .arg(keyValueString(key));
            }
            tooltipText += QStringLiteral("</p>");
            QToolTip::showText(mapToGlobal(event->pos()), tooltipText, this);
        }
    }
    else if(_dragStartPos >= 0 && animSettings()) {
        if(!_isDragging && std::abs(_dragStartPos - event->pos().x()) > 4) {
            _undoTransaction.begin(ui(), tr("Move animation keys"));
            _isDragging = true;
        }
        if(_isDragging) {
            int delta = event->pos().x() - _dragStartPos;
            int frameDelta = _timeSlider->distanceToFrameDifference(delta);
            _undoTransaction.revert();
            performActions(_undoTransaction, [&] {
                // Clamp to animation interval.
                for(AnimationKey* key : _selectedKeys.targets()) {
                    int newFrame = key->time().frame() + frameDelta;
                    if(newFrame < animSettings()->firstFrame()) frameDelta += animSettings()->firstFrame() - newFrame;
                    if(newFrame > animSettings()->lastFrame()) frameDelta -= newFrame - animSettings()->lastFrame();
                }
                // Move keys.
                for(KeyframeController* ctrl : _controllers.targets()) {
                    ctrl->moveKeys(_selectedKeys.targets(), AnimationTime::TicksPerFrame * frameDelta);
                }
            });
        }
        event->accept();
    }
}

/******************************************************************************
* Handles mouse release events.
******************************************************************************/
void AnimationTrackBar::mouseReleaseEvent(QMouseEvent* event)
{
    if(_isDragging) {
        _isDragging = false;
        if(event->button() == Qt::LeftButton)
            _undoTransaction.commit();
        event->accept();
    }
}

/******************************************************************************
* Returns a text representation of a key's value.
******************************************************************************/
QString AnimationTrackBar::keyValueString(AnimationKey* key) const
{
    QVariant value = key->valueQVariant();
    if(value.userType() == qMetaTypeId<FloatType>())
        return QString::number(value.value<FloatType>());
    else if(value.userType() == qMetaTypeId<int>())
        return QString::number(value.value<int>());
    else if(value.userType() == qMetaTypeId<Vector3>()) {
        Vector3 vec = value.value<Vector3>();
        return QString("(%1, %2, %3)").arg(vec.x()).arg(vec.y()).arg(vec.z());
    }
    else if(value.userType() == qMetaTypeId<Rotation>()) {
        Rotation rot = value.value<Rotation>();
        return QString("axis (%1, %2, %3), angle: %4°").arg(rot.axis().x()).arg(rot.axis().y()).arg(rot.axis().z()).arg(qRadiansToDegrees(rot.angle()));
    }
    else if(value.userType() == qMetaTypeId<Scaling>()) {
        Scaling s = value.value<Scaling>();
        return QString("(%1, %2, %3)]").arg(s.S.x()).arg(s.S.y()).arg(s.S.z());
    }
    else
        return value.toString();
}

/******************************************************************************
* Displays the context menu.
******************************************************************************/
void AnimationTrackBar::showKeyContextMenu(const QPoint& pos, const QVector<AnimationKey*>& clickedKeys)
{
    QMenu contextMenu(this);

    // Action: Unselect key.
    QMenu* unselectKeyMenu = contextMenu.addMenu(tr("Unselect key"));
    unselectKeyMenu->setEnabled(!_selectedKeys.targets().empty());
    for(AnimationKey* key : _selectedKeys.targets()) {
        QString label = QStringLiteral("%1: %2")
                .arg(_parameterNames[controllerIndexFromKey(key)])
                .arg(keyValueString(key));
        QAction* unselectAction = unselectKeyMenu->addAction(label);
        connect(unselectAction, &QAction::triggered, [this, key]() {
            _selectedKeys.remove(key);
            update();
        });
    }

    // Action: Delete selected keys
    contextMenu.addSeparator();
    contextMenu.addAction(tr("Deleted selected keys"), this, SLOT(onDeleteSelectedKeys()))->setEnabled(_selectedKeys.targets().empty() == false);

    // Action: Jump to key
    contextMenu.addSeparator();
    QAction* jumpToTimeAction = contextMenu.addAction(tr("Jump to key"));
    if(clickedKeys.empty() == false) {
        int frame = clickedKeys.front()->time().frame();
        connect(jumpToTimeAction, &QAction::triggered, this, [this, frame]() {
            handleExceptions([&] {
                if(animSettings())
                    animSettings()->setCurrentFrame(frame);
            });
        });
    }
    else jumpToTimeAction->setEnabled(false);

    contextMenu.exec(mapToGlobal(pos));
}

/******************************************************************************
* Deletes the selected animation keys.
******************************************************************************/
void AnimationTrackBar::onDeleteSelectedKeys()
{
    performTransaction(tr("Delete animation keys"), [this]() {
        for(KeyframeController* ctrl : _controllers.targets()) {
            ctrl->deleteKeys(_selectedKeys.targets());
        }
    });
}

}   // End of namespace
