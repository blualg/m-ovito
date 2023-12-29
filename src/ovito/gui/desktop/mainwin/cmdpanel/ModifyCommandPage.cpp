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
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModifierTemplates.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/oo/CloneHelper.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/mainwin/PipelineListModel.h>
#include <ovito/gui/desktop/app/GuiApplication.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/dialogs/ModifierTemplatesPage.h>
#include <ovito/gui/desktop/dialogs/CopyPipelineItemDialog.h>
#include "CommandPanel.h"
#include "ModifyCommandPage.h"

#include <QtNetwork>

namespace Ovito {

/******************************************************************************
* Initializes the modify tab.
******************************************************************************/
ModifyCommandPage::ModifyCommandPage(MainWindow& mainWindow, QWidget* parent) : QWidget(parent), _mainWindow(mainWindow)
{
    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(2,2,2,2);
    layout->setSpacing(4);
    layout->setColumnStretch(0,1);

    _pipelineListModel = new PipelineListModel(mainWindow, this);
    class ModifierListBox : public QComboBox {
    public:
        using QComboBox::QComboBox;
        virtual void showPopup() override {
            static_cast<ModifierListModel*>(model())->updateActionState();
            QComboBox::showPopup();
        }
    };
    _modifierSelector = new ModifierListBox(this);
    layout->addWidget(_modifierSelector, 1, 0, 1, 1);
    _modifierSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    _modifierSelector->setModel(new ModifierListModel(this, mainWindow, _pipelineListModel));
    _modifierSelector->setMaxVisibleItems(0xFFFF);
    connect(_modifierSelector, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        QComboBox* selector = static_cast<QComboBox*>(sender());
        static_cast<ModifierListModel*>(selector->model())->insertModifierByIndex(index);
        selector->setCurrentIndex(0);
    });

    class PipelineListView : public QListView {
    public:
        PipelineListView(QWidget* parent) : QListView(parent) {}
        virtual QSize sizeHint() const override { return QSize(256, 260); }
    protected:
        virtual bool edit(const QModelIndex& index, QAbstractItemView::EditTrigger trigger, QEvent* event) override {
            if(trigger == QAbstractItemView::SelectedClicked && event->type() == QEvent::MouseButtonRelease) {
                // Avoid triggering edit mode when user clicks the check box next to a list item.
                QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
                int origin = visualRect(index).left();
#ifndef Q_OS_MACOS
                if(mouseEvent->pos().x() < origin + 50)
#else
                if(mouseEvent->pos().x() < origin + 60)
#endif
                    trigger = QAbstractItemView::NoEditTriggers;
            }
            if((trigger == QAbstractItemView::SelectedClicked || trigger == QAbstractItemView::NoEditTriggers) && event->type() == QEvent::MouseButtonRelease) {
                // Detect when user clicks on the collapsable part of a group item.
                if(index.data(PipelineListModel::ItemTypeRole) == PipelineListItem::ModifierGroup) {
                    QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
                    int origin = visualRect(index).left();
#ifndef Q_OS_MACOS
                    if(mouseEvent->pos().x() >= origin + 25 && mouseEvent->pos().x() < origin + 50) {
#else
                    if(mouseEvent->pos().x() >= origin + 30 && mouseEvent->pos().x() < origin + 60) {
#endif
                        trigger = QAbstractItemView::NoEditTriggers;
                        // Toggle the collapsed state of the group.
                        bool isCollapsed = index.data(PipelineListModel::IsCollapsedRole).toBool();
                        const_cast<QAbstractItemModel*>(index.model())->setData(index, !isCollapsed, PipelineListModel::IsCollapsedRole);
                    }
                }
            }
            return QListView::edit(index, trigger, event);
        }
    };

    _splitter = new QSplitter(Qt::Vertical);
    _splitter->setChildrenCollapsible(false);

    QWidget* upperContainer = new QWidget();
    _splitter->addWidget(upperContainer);
    QHBoxLayout* subLayout = new QHBoxLayout(upperContainer);
    subLayout->setContentsMargins(0,0,0,0);
    subLayout->setSpacing(2);

    _pipelineWidget = new PipelineListView(upperContainer);
    _pipelineWidget->setDragDropMode(QAbstractItemView::InternalMove);
    _pipelineWidget->setDragEnabled(true);
    _pipelineWidget->setAcceptDrops(true);
    _pipelineWidget->setDragDropOverwriteMode(false);
    _pipelineWidget->setDropIndicatorShown(true);
    _pipelineWidget->setEditTriggers(QAbstractItemView::SelectedClicked);
    _pipelineWidget->setModel(_pipelineListModel);
    _pipelineWidget->setSelectionModel(_pipelineListModel->selectionModel());
    _pipelineWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _pipelineWidget->setIconSize(_pipelineListModel->iconSize());
    _pipelineWidget->setItemDelegate(new ExtendedListItemDelegate(_pipelineWidget, PipelineListModel::StatusInfoRole));
    subLayout->addWidget(_pipelineWidget);

    // Set up context menu.
    ActionManager* actionManager = mainWindow.actionManager();
    _pipelineWidget->addAction(actionManager->getAction(ACTION_PIPELINE_TOGGLE_MODIFIER_GROUP));
    QAction* separator = new QAction(_pipelineWidget);
    separator->setSeparator(true);
    _pipelineWidget->addAction(separator);
    _pipelineWidget->addAction(actionManager->getAction(ACTION_PIPELINE_RENAME_ITEM));
    separator = new QAction(_pipelineWidget);
    separator->setSeparator(true);
    _pipelineWidget->addAction(separator);
    _pipelineWidget->addAction(actionManager->getAction(ACTION_PIPELINE_COPY_ITEM));
    _pipelineWidget->addAction(actionManager->getAction(ACTION_PIPELINE_MAKE_INDEPENDENT));
    separator = new QAction(_pipelineWidget);
    separator->setSeparator(true);
    _pipelineWidget->addAction(separator);
    _pipelineWidget->addAction(actionManager->getAction(ACTION_MODIFIER_DELETE));
    _pipelineWidget->setContextMenuPolicy(Qt::ActionsContextMenu);

    // Listen to selection changes in the pipeline editor list widget.
    connect(_pipelineListModel, &PipelineListModel::selectedItemChanged, this, &ModifyCommandPage::onSelectedItemChanged);

    // Double-click on a modifier or visual element toggles the enabled state of the element.
    connect(_pipelineWidget, &PipelineListView::doubleClicked, this, &ModifyCommandPage::onModifierStackDoubleClicked);

    QToolBar* editToolbar = new QToolBar(this);
    editToolbar->setOrientation(Qt::Vertical);
    subLayout->addWidget(editToolbar);

    // Create pipeline editor toolbar.
    editToolbar->addAction(actionManager->getAction(ACTION_MODIFIER_DELETE));
    editToolbar->addSeparator();
    editToolbar->addAction(actionManager->getAction(ACTION_MODIFIER_MOVE_UP));
    editToolbar->addAction(actionManager->getAction(ACTION_MODIFIER_MOVE_DOWN));
    editToolbar->addSeparator();
    editToolbar->addAction(actionManager->getAction(ACTION_PIPELINE_TOGGLE_MODIFIER_GROUP));

    QAction* manageModifierTemplatesAction = actionManager->createCommandAction(ACTION_MODIFIER_MANAGE_TEMPLATES, tr("Manage Modifier Templates..."), "modify_modifier_save_preset", tr("Open the dialog that lets you manage the saved modifier templates."));
    connect(manageModifierTemplatesAction, &QAction::triggered, [&mainWindow]() {
        ApplicationSettingsDialog dlg(mainWindow, &ModifierTemplatesPage::OOClass());
        dlg.exec();
    });
    editToolbar->addAction(manageModifierTemplatesAction);

    connect(actionManager->getAction(ACTION_PIPELINE_RENAME_ITEM), &QAction::triggered, this, [this]() {
        _pipelineWidget->edit(_pipelineWidget->currentIndex());
    });

    connect(actionManager->getAction(ACTION_PIPELINE_COPY_ITEM), &QAction::triggered, [&]() {
        // Collect all currently selected pipeline nodes.
        QVector<OORef<PipelineNode>> nodes;
        for(RefTarget* obj : _pipelineListModel->selectedObjects()) {
            if(PipelineNode* pnode = dynamic_object_cast<PipelineNode>(obj)) {
                if(!nodes.contains(pnode))
                    nodes.push_back(pnode);
            }
            else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(obj)) {
                for(ModificationNode* modNode : group->nodes()) {
                    if(!nodes.contains(modNode))
                        nodes.push_back(modNode);
                }
            }
        }
        if(!nodes.empty()) {
            CopyPipelineItemDialog dlg(_mainWindow, &_mainWindow, _pipelineListModel->selectedPipeline(), std::move(nodes));
            dlg.exec();
        }
    });

    layout->addWidget(_splitter, 2, 0, 1, 2);
    layout->setRowStretch(2, 1);

    // Create the properties panel.
    _propertiesPanel = new PropertiesPanel(mainWindow);
    _propertiesPanel->setFrameStyle(QFrame::NoFrame | QFrame::Plain);
    _splitter->addWidget(_propertiesPanel);
    _splitter->setStretchFactor(1,1);

    // Create About panel.
    createAboutPanel();

    // Initialize state.
    onSelectedItemChanged();
}

/******************************************************************************
* Loads the layout of the widgets from the settings store.
******************************************************************************/
void ModifyCommandPage::restoreLayout()
{
    QSettings settings;
    settings.beginGroup("app/mainwindow/modify");
    QVariant state = settings.value("splitter");
    if(state.canConvert<QByteArray>())
        _splitter->restoreState(state.toByteArray());
}

/******************************************************************************
* Saves the layout of the widgets to the settings store.
******************************************************************************/
void ModifyCommandPage::saveLayout()
{
    QSettings settings;
    settings.beginGroup("app/mainwindow/modify");
    settings.setValue("splitter", _splitter->saveState());
}

/******************************************************************************
* Is called when a new modification list item has been selected, or if the currently
* selected item has changed.
******************************************************************************/
void ModifyCommandPage::onSelectedItemChanged()
{
    RefTarget* selectedObject = pipelineListModel()->selectedObject();

    _modifierSelector->setEnabled(selectedObject != nullptr);

    if(selectedObject != _propertiesPanel->editObject()) {
        _propertiesPanel->setEditObject(selectedObject);

        // Request a viewport update whenever a new item in the pipeline editor is selected,
        // because the currently selected modifier may render gizmos in the viewports.
        _mainWindow.updateViewports();
    }

    // Whenever no object is selected, show information about the program.
    if(pipelineListModel()->selectedItems().empty())
        _aboutRollout->show();
    else
        _aboutRollout->hide();
}

/******************************************************************************
* This called when the user double clicks on an item in the modifier stack.
******************************************************************************/
void ModifyCommandPage::onModifierStackDoubleClicked(const QModelIndex& index)
{
    PipelineListItem* item = pipelineListModel()->item(index.row());
    OVITO_CHECK_OBJECT_POINTER(item);

    if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(item->object())) {
        // Toggle enabled state of modifier.
        _mainWindow.performTransaction(tr("Toggle modifier state"), [modNode]() {
            modNode->modifier()->setEnabled(!modNode->modifier()->isEnabled());
        });
    }

    if(DataVis* vis = dynamic_object_cast<DataVis>(item->object())) {
        // Toggle enabled state of vis element.
        _mainWindow.performTransaction(tr("Toggle visual element"), [vis]() {
            vis->setEnabled(!vis->isEnabled());
        });
    }
}

/******************************************************************************
* Creates the rollout panel that shows information about the application
* whenever no object is selected.
******************************************************************************/
void ModifyCommandPage::createAboutPanel()
{
    QWidget* rollout = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(8,8,8,8);

    QTextBrowser* aboutLabel = new QTextBrowser(rollout);
    aboutLabel->setObjectName("AboutLabel");
    aboutLabel->setOpenExternalLinks(true);
    aboutLabel->setMinimumHeight(600);
    aboutLabel->setFrameStyle(QFrame::NoFrame | QFrame::Plain);
    aboutLabel->viewport()->setAutoFillBackground(false);
    layout->addWidget(aboutLabel);

    QByteArray newsPage;
#if !defined(OVITO_BUILD_APPSTORE_VERSION)
    QSettings settings;
    if(settings.value("updates/check_for_updates", true).toBool()) {
        // Retrieve cached news page from settings store.
        newsPage = settings.value("news/cached_webpage").toByteArray();
    }
    if(newsPage.isEmpty()) {
        QResource res("/gui/mainwin/command_panel/about_panel.html");
        newsPage = QByteArray((const char *)res.data(), (int)res.size());
    }
#else
    QResource res("/gui/mainwin/command_panel/about_panel_no_updates.html");
    newsPage = QByteArray((const char *)res.data(), (int)res.size());
#endif

    _aboutRollout = _propertiesPanel->addRollout(rollout, Application::applicationName());
    showProgramNotice(QString::fromUtf8(newsPage.constData()));

#if !defined(OVITO_BUILD_APPSTORE_VERSION)
    if(settings.value("updates/check_for_updates", true).toBool()) {
        QString operatingSystemString;
#if defined(Q_OS_MACOS)
        operatingSystemString = QStringLiteral("macosx");
#elif defined(Q_OS_WIN) || defined(Q_OS_CYGWIN)
        operatingSystemString = QStringLiteral("win");
#elif defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
        operatingSystemString = QStringLiteral("linux");
#else
        operatingSystemString = QStringLiteral("other");
#endif

        QString programEdition;
#if defined(OVITO_BUILD_BASIC)
        programEdition = QStringLiteral("basic/");
#elif defined(OVITO_BUILD_PRO)
        programEdition = QStringLiteral("pro/");
#endif

        // Fetch newest web page from web server.
        QString urlString = QString("https://www.ovito.org/appnews/v%1.%2.%3/%4?ovito=000000000000000000&OS=%5%6")
                .arg(Application::applicationVersionMajor())
                .arg(Application::applicationVersionMinor())
                .arg(Application::applicationVersionRevision())
                .arg(programEdition)
                .arg(operatingSystemString)
                .arg(QT_POINTER_SIZE*8);
        QNetworkAccessManager* networkAccessManager = Application::instance()->networkAccessManager();
        QNetworkReply* networkReply = networkAccessManager->get(QNetworkRequest(QUrl(urlString)));
        connect(networkReply, &QNetworkReply::finished, this, &ModifyCommandPage::onWebRequestFinished);
    }
#endif
}

/******************************************************************************
* Is called by the system when fetching the news web page from the server is
* completed.
******************************************************************************/
void ModifyCommandPage::onWebRequestFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(QObject::sender());
    if(reply->error() == QNetworkReply::NoError) {
        QByteArray page = reply->readAll();
        reply->close();
        if(page.startsWith("<html><!--OVITO-->")) {

            showProgramNotice(QString::fromUtf8(page.constData()));

            QSettings settings;
            settings.setValue("news/cached_webpage", page);
        }
    }
    reply->deleteLater();
}

/******************************************************************************
* Displays the given HTML page content in the About pane.
******************************************************************************/
void ModifyCommandPage::showProgramNotice(const QString& htmlPage)
{
    QString finalText = htmlPage;

#if defined(OVITO_DEVELOPMENT_BUILD_DATE)
    QString previewNotice = tr("<h4>Preview version notice</h4><p style=\"background-color: rgb(230,180,180); color: black;\">You are using an early development build of %1, which was created on %2.</p> "
            "<p style=\"background-color: rgb(230,180,180); color: black;\">Remember to install the final release of %1 as soon as it becomes available on our website <a href=\"https://www.ovito.org/\">www.ovito.org</a>.</p>")
        .arg(Application::applicationName())
        .arg(QStringLiteral(OVITO_DEVELOPMENT_BUILD_DATE));
    finalText.replace(QStringLiteral("<p>&nbsp;</p>"), previewNotice);
#endif

    QTextBrowser* aboutLabel = _aboutRollout->findChild<QTextBrowser*>("AboutLabel");
    OVITO_CHECK_POINTER(aboutLabel);
    aboutLabel->setHtml(finalText);
}

}   // End of namespace
