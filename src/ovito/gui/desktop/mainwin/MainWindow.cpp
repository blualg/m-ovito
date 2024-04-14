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
#include <ovito/gui/desktop/app/GuiApplication.h>
#include <ovito/gui/desktop/app/GuiApplicationService.h>
#include <ovito/gui/desktop/widgets/animation/AnimationTimeSpinner.h>
#include <ovito/gui/desktop/widgets/animation/AnimationTimeSlider.h>
#include <ovito/gui/desktop/widgets/animation/AnimationTrackBar.h>
#include <ovito/gui/desktop/widgets/rendering/FrameBufferWindow.h>
#include <ovito/gui/desktop/widgets/display/CoordinateDisplayWidget.h>
#include <ovito/gui/desktop/widgets/general/StatusBar.h>
#include <ovito/gui/desktop/widgets/selection/SceneNodeSelectionBox.h>
#include <ovito/gui/desktop/dialogs/MessageDialog.h>
#include <ovito/gui/desktop/dialogs/ImportFileDialog.h>
#include <ovito/gui/desktop/dialogs/HistoryFileDialog.h>
#include <ovito/gui/desktop/actions/WidgetActionManager.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/app/StandaloneApplication.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include "MainWindow.h"
#include "ViewportsPanel.h"
#include "TaskDisplayWidget.h"
#include "cmdpanel/CommandPanel.h"
#include "data_inspector/DataInspectorPanel.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(MainWindow);

/******************************************************************************
* The constructor of the main window class.
******************************************************************************/
MainWindow::MainWindow()
{
    _baseWindowTitle = tr("%1 (Open Visualization Tool)").arg(Application::applicationName());
#if defined(OVITO_DEVELOPMENT_BUILD_DATE)
    _baseWindowTitle += tr(" - Development build %1 created on %2").arg(Application::applicationVersionString()).arg(QStringLiteral(OVITO_DEVELOPMENT_BUILD_DATE));
#endif
    setWindowTitle(_baseWindowTitle);

    // Set up the layout of docking widgets.
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // Disable context menus in toolbars.
    setContextMenuPolicy(Qt::NoContextMenu);

    // Create input manager.
    setViewportInputManager(new ViewportInputManager(this, *this));

    // Create an undo stack.
    setUndoStack(new UndoStack(*this, this));

    // Create actions.
    setActionManager(new WidgetActionManager(this, *this));

    // Reset undo stack whenever a new dataset is loaded.
    connect(&datasetContainer(), &DataSetContainer::dataSetChanged, undoStack(), &UndoStack::clear);

    // Let GUI application services register their actions.
    for(const auto& service : StandaloneApplication::instance()->applicationServices()) {
        if(auto gui_service = dynamic_object_cast<GuiApplicationService>(service))
            gui_service->registerActions(*actionManager(), *this);
    }

    // Create the main menu
    createMainMenu();

    // Create the main toolbar.
    createMainToolbar();

    // Store current state of ACTION_AUTO_KEY_MODE_TOGGLE in a member variable for quick access in isAutoGenerateAnimationKeysEnabled().
    connect(actionManager()->getAction(ACTION_AUTO_KEY_MODE_TOGGLE), &QAction::toggled, this, [&](bool checked) { _autoKeyModeOn = checked; });

    // Create the viewports panel and the data inspector panel.
    QSplitter* dataInspectorSplitter = new QSplitter();
    dataInspectorSplitter->setOrientation(Qt::Vertical);
    dataInspectorSplitter->setChildrenCollapsible(false);
    dataInspectorSplitter->setHandleWidth(0);
    _viewportsPanel = new ViewportsPanel(*this);
    dataInspectorSplitter->addWidget(_viewportsPanel);
    _dataInspector = new DataInspectorPanel(*this);
    dataInspectorSplitter->addWidget(_dataInspector);
    dataInspectorSplitter->setStretchFactor(0, 1);
    dataInspectorSplitter->setStretchFactor(1, 0);
    setCentralWidget(dataInspectorSplitter);
    _viewportsPanel->setFocus(Qt::OtherFocusReason);

    // Create the animation panel below the viewports.
    QWidget* animationPanel = new QWidget();
    QVBoxLayout* animationPanelLayout = new QVBoxLayout();
    animationPanelLayout->setSpacing(0);
    animationPanelLayout->setContentsMargins(0, 1, 0, 0);
    animationPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    animationPanel->setLayout(animationPanelLayout);

    // Create animation time slider
    AnimationTimeSlider* timeSlider = new AnimationTimeSlider(*this);
    animationPanelLayout->addWidget(timeSlider);
    AnimationTrackBar* trackBar = new AnimationTrackBar(*this, timeSlider);
    animationPanelLayout->addWidget(trackBar);

    // Create status bar.
    QWidget* statusBarContainer = new QWidget();
    statusBarContainer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    _statusBarLayout = new QHBoxLayout(statusBarContainer);
    _statusBarLayout->setContentsMargins(2,0,0,0);
    _statusBarLayout->setSpacing(2);
    animationPanelLayout->addWidget(statusBarContainer, 1);

    _statusBar = new StatusBar();
    _statusBar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    _statusBarLayout->addWidget(_statusBar);
    _statusBar->overflowWidget()->setParent(animationPanel);

    TaskDisplayWidget* taskDisplay = new TaskDisplayWidget(this);
    taskDisplay->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    _statusBarLayout->addWidget(taskDisplay, 1);

    _coordinateDisplay = new CoordinateDisplayWidget(*this, animationPanel);
    _coordinateDisplay->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    _statusBarLayout->addWidget(_coordinateDisplay);
    _statusBarLayout->addStrut(std::max(_coordinateDisplay->sizeHint().height(), taskDisplay->sizeHint().height()));

    // Create the animation control toolbar.
    QToolBar* animationControlBar1 = new QToolBar();
    animationControlBar1->addAction(actionManager()->getAction(ACTION_GOTO_START_OF_ANIMATION));
    animationControlBar1->addSeparator();
    animationControlBar1->addAction(actionManager()->getAction(ACTION_GOTO_PREVIOUS_FRAME));
    animationControlBar1->addAction(actionManager()->getAction(ACTION_TOGGLE_ANIMATION_PLAYBACK));
    animationControlBar1->addAction(actionManager()->getAction(ACTION_GOTO_NEXT_FRAME));
    animationControlBar1->addSeparator();
    animationControlBar1->addAction(actionManager()->getAction(ACTION_GOTO_END_OF_ANIMATION));
    QToolBar* animationControlBar2 = new QToolBar();
    animationControlBar2->addAction(actionManager()->getAction(ACTION_AUTO_KEY_MODE_TOGGLE));
    QWidget* animationTimeSpinnerContainer = new QWidget();
    QHBoxLayout* animationTimeSpinnerLayout = new QHBoxLayout(animationTimeSpinnerContainer);
    animationTimeSpinnerLayout->setContentsMargins(0,0,0,0);
    animationTimeSpinnerLayout->setSpacing(0);
    class TimeEditBox : public QLineEdit {
    public:
        virtual QSize sizeHint() const { return minimumSizeHint(); }
    };
    QLineEdit* timeEditBox = new TimeEditBox();
    timeEditBox->setToolTip(tr("Current Animation Time"));
    AnimationTimeSpinner* currentTimeSpinner = new AnimationTimeSpinner(*this);
    currentTimeSpinner->setTextBox(timeEditBox);
    animationTimeSpinnerLayout->addWidget(timeEditBox, 1);
    animationTimeSpinnerLayout->addWidget(currentTimeSpinner);
    animationControlBar2->addWidget(animationTimeSpinnerContainer);
    animationControlBar2->addAction(actionManager()->getAction(ACTION_ANIMATION_SETTINGS));
    animationControlBar2->addWidget(new QWidget());

    QWidget* animationControlPanel = new QWidget();
    QVBoxLayout* animationControlPanelLayout = new QVBoxLayout(animationControlPanel);
    animationControlPanelLayout->setSpacing(0);
    animationControlPanelLayout->setContentsMargins(0, 1, 0, 0);
    animationControlPanelLayout->addWidget(animationControlBar1);
    animationControlPanelLayout->addWidget(animationControlBar2);
    animationControlPanelLayout->addStretch(1);
    animationControlPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    animationTimeSpinnerContainer->setStyle(QApplication::style());

    // Create the viewport control toolbar.
    QToolBar* viewportControlBar1 = new QToolBar();
    viewportControlBar1->addAction(actionManager()->getAction(ACTION_VIEWPORT_ZOOM));
    viewportControlBar1->addAction(actionManager()->getAction(ACTION_VIEWPORT_PAN));
    viewportControlBar1->addAction(actionManager()->getAction(ACTION_VIEWPORT_ORBIT));
    QToolBar* viewportControlBar2 = new QToolBar();
    viewportControlBar2->addAction(actionManager()->getAction(ACTION_VIEWPORT_ZOOM_SCENE_EXTENTS));
    viewportControlBar2->addAction(actionManager()->getAction(ACTION_VIEWPORT_FOV));
    viewportControlBar2->addAction(actionManager()->getAction(ACTION_VIEWPORT_MAXIMIZE));
    QWidget* viewportControlPanel = new QWidget();
    QVBoxLayout* viewportControlPanelLayout = new QVBoxLayout(viewportControlPanel);
    viewportControlPanelLayout->setSpacing(0);
    viewportControlPanelLayout->setContentsMargins(0, 1, 0, 0);
    viewportControlPanelLayout->addWidget(viewportControlBar1);
    QHBoxLayout* sublayout = new QHBoxLayout();
    sublayout->addStretch(1);
    sublayout->addWidget(viewportControlBar2);
    viewportControlPanelLayout->addLayout(sublayout);
    viewportControlPanelLayout->addStretch(1);
    viewportControlPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    // Create the command panel.
    _commandPanel = new CommandPanel(*this, this);

    // Create the bottom docking widget.
    QWidget* bottomDockWidget = new QWidget();
    bottomDockWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QGridLayout* bottomDockLayout = new QGridLayout(bottomDockWidget);
    bottomDockLayout->setContentsMargins(0,0,0,0);
    bottomDockLayout->setSpacing(0);
    QFrame* separatorLine = new QFrame();
    QPalette pal = separatorLine->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::Mid));
    separatorLine->setFrameShape(QFrame::HLine);
    separatorLine->setFrameShadow(QFrame::Plain);
    separatorLine->setPalette(pal);
    bottomDockLayout->addWidget(separatorLine, 1, 0, 1, 5);
    bottomDockLayout->addWidget(animationPanel, 2, 0);
    separatorLine = new QFrame();
    separatorLine->setFrameShape(QFrame::VLine);
    separatorLine->setFrameShadow(QFrame::Plain);
    separatorLine->setPalette(pal);
    bottomDockLayout->addWidget(separatorLine, 2, 1);
    bottomDockLayout->addWidget(animationControlPanel, 2, 2);
    separatorLine = new QFrame();
    separatorLine->setFrameShape(QFrame::VLine);
    separatorLine->setFrameShadow(QFrame::Plain);
    separatorLine->setPalette(pal);
    bottomDockLayout->addWidget(separatorLine, 2, 3);
    bottomDockLayout->addWidget(viewportControlPanel, 2, 4);

    // Create docking widgets.
    createDockPanel(tr("Bottom panel"), "BottomPanel", Qt::BottomDockWidgetArea, Qt::BottomDockWidgetArea, bottomDockWidget);
    createDockPanel(tr("Command Panel"), "CommandPanel", Qt::RightDockWidgetArea, Qt::DockWidgetAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea), _commandPanel);

    // Update window title when document path changes.
    connect(&datasetContainer(), &DataSetContainer::filePathChanged, this, [this](const QString& filePath) { setWindowFilePath(filePath); });
    connect(undoStack(), &UndoStack::cleanChanged, this, [this](bool isClean) { setWindowModified(!isClean); });

    // Accept files via drag & drop.
    setAcceptDrops(true);
}

/******************************************************************************
* Destructor.
******************************************************************************/
MainWindow::~MainWindow()
{
    OVITO_ASSERT(isShuttingDown()); // Make sure this UserInterface was properly shutdown before being deleted.
    OVITO_ASSERT(datasetContainer().currentSet() == nullptr);
}

/******************************************************************************
* Creates a dock panel.
******************************************************************************/
QDockWidget* MainWindow::createDockPanel(const QString& caption, const QString& objectName, Qt::DockWidgetArea dockArea, Qt::DockWidgetAreas allowedAreas, QWidget* contents)
{
    QDockWidget* dockWidget = new QDockWidget(caption, this);
    dockWidget->setObjectName(objectName);
    dockWidget->setAllowedAreas(allowedAreas);
    dockWidget->setFeatures(QDockWidget::DockWidgetClosable);
    dockWidget->setWidget(contents);
    dockWidget->setTitleBarWidget(new QWidget());
    addDockWidget(dockArea, dockWidget);
    return dockWidget;
}

/******************************************************************************
* Restores a previously saved maximized/non-maximized state and shows the window.
******************************************************************************/
void MainWindow::restoreMainWindowGeometry()
{
    QSettings settings;
    settings.beginGroup("app/mainwindow");

    // TODO: For now we only restore the maximized/normal state of the main window, because
    // the QWidget::restoreGeometry() method is broken in Qt 6.4.2 under macOS. We'll activate the new code
    // once we've switched to Qt 6.5, which hopefully fixes the issue.
#if 1
    if(settings.value("maximized", true).toBool())
        showMaximized();
    else
        show();
#else
    restoreGeometry(settings.value("geometry").toByteArray());
    show();
#endif
}

/******************************************************************************
* Saves the maximized/non-maximized state of the window in the settings store.
******************************************************************************/
void MainWindow::saveMainWindowGeometry()
{
    QSettings settings;
    settings.beginGroup("app/mainwindow");
    settings.setValue("maximized", isMaximized());
    settings.setValue("geometry", saveGeometry());
}

/******************************************************************************
* Loads the layout of the docked widgets from the settings store.
******************************************************************************/
void MainWindow::restoreLayout()
{
    QSettings settings;
    settings.beginGroup("app/mainwindow");
    QVariant state = settings.value("state");
    if(state.canConvert<QByteArray>())
        restoreState(state.toByteArray());
    commandPanel()->restoreLayout();
}

/******************************************************************************
* Saves the layout of the docked widgets to the settings store.
******************************************************************************/
void MainWindow::saveLayout()
{
    QSettings settings;
    settings.beginGroup("app/mainwindow");
    settings.setValue("state", saveState());
    commandPanel()->saveLayout();
}

/******************************************************************************
* Creates the main menu.
******************************************************************************/
void MainWindow::createMainMenu()
{
    QMenuBar* menuBar = this->menuBar();

    // Build the file menu.
    QMenu* fileMenu = menuBar->addMenu(tr("&File"));
    fileMenu->setObjectName(QStringLiteral("FileMenu"));
    fileMenu->addAction(actionManager()->getAction(ACTION_FILE_IMPORT));
#ifdef OVITO_SSH_CLIENT
    fileMenu->addAction(actionManager()->getAction(ACTION_FILE_REMOTE_IMPORT));
#endif
    fileMenu->addAction(actionManager()->getAction(ACTION_FILE_EXPORT));
    fileMenu->addSeparator();
    fileMenu->addAction(actionManager()->getAction(ACTION_FILE_OPEN));
    fileMenu->addAction(actionManager()->getAction(ACTION_FILE_SAVE));
    fileMenu->addAction(actionManager()->getAction(ACTION_FILE_SAVEAS));
    fileMenu->addSeparator();
    if(QAction* runScriptFileAction = actionManager()->findAction(ACTION_SCRIPTING_RUN_FILE))
        fileMenu->addAction(runScriptFileAction);
    if(QAction* generateScriptFileAction = actionManager()->findAction(ACTION_SCRIPTING_GENERATE_CODE))
        fileMenu->addAction(generateScriptFileAction);
    fileMenu->addSeparator();
    if(QAction* remoteRenderingAction = actionManager()->findAction(ACTION_REMOTE_RENDERING)) fileMenu->addAction(remoteRenderingAction);
    fileMenu->addSeparator();
    fileMenu->addAction(actionManager()->getAction(ACTION_FILE_NEW_WINDOW));
    fileMenu->addSeparator();
    fileMenu->addAction(actionManager()->getAction(ACTION_QUIT));

    // Build the edit menu.
    QMenu* editMenu = menuBar->addMenu(tr("&Edit"));
    editMenu->setObjectName(QStringLiteral("EditMenu"));
    editMenu->addAction(actionManager()->getAction(ACTION_EDIT_UNDO));
    editMenu->addAction(actionManager()->getAction(ACTION_EDIT_REDO));
#ifdef OVITO_DEBUG
    editMenu->addAction(actionManager()->getAction(ACTION_EDIT_CLEAR_UNDO_STACK));
#endif
    editMenu->addSeparator();
    editMenu->addAction(actionManager()->getAction(ACTION_SETTINGS_DIALOG));

    // Build the help menu.
    QMenu* helpMenu = menuBar->addMenu(tr("&Help"));
    helpMenu->setObjectName(QStringLiteral("HelpMenu"));
    helpMenu->addAction(actionManager()->getAction(ACTION_HELP_SHOW_ONLINE_HELP));
    helpMenu->addAction(actionManager()->getAction(ACTION_HELP_SHOW_SCRIPTING_HELP));
    helpMenu->addSeparator();
    helpMenu->addAction(actionManager()->getAction(ACTION_HELP_GRAPHICS_SYSINFO));
#ifndef  Q_OS_MACOS
    helpMenu->addSeparator();
#endif
    helpMenu->addAction(actionManager()->getAction(ACTION_HELP_ABOUT));

    // Let GUI application services add their actions to the main menu.
    for(const auto& service : StandaloneApplication::instance()->applicationServices()) {
        if(auto gui_service = dynamic_object_cast<GuiApplicationService>(service))
            gui_service->addActionsToMenu(*actionManager(), menuBar);
    }
}

/******************************************************************************
* Creates the main toolbar.
******************************************************************************/
void MainWindow::createMainToolbar()
{
    _mainToolbar = addToolBar(tr("Main Toolbar"));
    _mainToolbar->setObjectName("MainToolbar");
    _mainToolbar->setMovable(false);

    _mainToolbar->addAction(actionManager()->getAction(ACTION_FILE_IMPORT));
    _mainToolbar->addAction(actionManager()->getAction(ACTION_FILE_REMOTE_IMPORT));

    _mainToolbar->addSeparator();

    _mainToolbar->addAction(actionManager()->getAction(ACTION_FILE_OPEN));
    _mainToolbar->addAction(actionManager()->getAction(ACTION_FILE_SAVE));

    _mainToolbar->addSeparator();

    _mainToolbar->addAction(actionManager()->getAction(ACTION_EDIT_UNDO));
    _mainToolbar->addAction(actionManager()->getAction(ACTION_EDIT_REDO));

    _mainToolbar->addSeparator();

    _mainToolbar->addAction(actionManager()->getAction(ACTION_SELECTION_MODE));
    _mainToolbar->addAction(actionManager()->getAction(ACTION_XFORM_MOVE_MODE));
    _mainToolbar->addAction(actionManager()->getAction(ACTION_XFORM_ROTATE_MODE));

    _mainToolbar->addSeparator();

    _mainToolbar->addAction(actionManager()->getAction(ACTION_RENDER_ACTIVE_VIEWPORT));

    _mainToolbar->addSeparator();

    _mainToolbar->addAction(actionManager()->getAction(ACTION_COMMAND_QUICKSEARCH));

    QLabel* pipelinesLabel = new QLabel(tr("  Pipelines: "));
    pipelinesLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pipelinesLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    _mainToolbar->addWidget(pipelinesLabel);
    _mainToolbar->addWidget(new SceneNodeSelectionBox(*this));
}

/******************************************************************************
* Is called when the window receives an event.
******************************************************************************/
bool MainWindow::event(QEvent* event)
{
    if(event->type() == QEvent::StatusTip) {
        showStatusBarMessage(static_cast<QStatusTipEvent*>(event)->tip());
        return true;
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    else if(event->type() == QEvent::ThemeChange) {
        // Switch icon theme to match with the current UI theme.
        QIcon::setThemeName(GuiApplication::instance()->usingDarkTheme() ? QStringLiteral("ovito-dark") : QStringLiteral("ovito-light"));
    }
#endif
    return QMainWindow::event(event);
}

/******************************************************************************
* Handles global key input.
******************************************************************************/
void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if(!static_cast<ViewportsPanel*>(_viewportsPanel)->onKeyShortcut(event))
        QMainWindow::keyPressEvent(event);
}

/******************************************************************************
* Is called when the user closes the window.
******************************************************************************/
void MainWindow::closeEvent(QCloseEvent* event)
{
    OVITO_ASSERT(!isShuttingDown());

    handleExceptions([&] {
        // Let the user save changes made to the current dataset.
        if(isVisible())
            askForSaveChanges();

        // Don't allow user to interact with the window anymore.
        setEnabled(false);

        // Stop all running tasks in this window and release program session objects.
        shutdown();

        // Save window geometry and layout in user settings file.
        if(isVisible()) {
            saveMainWindowGeometry();
            saveLayout();
        }

        // Closes the GUI window (but does not destroy this UserInterface C++ object yet, which is kept alive by a shared_ptr).
        event->accept();
    });

    // Swallow close event if the user chose to cancel the shutdown.
    if(!isShuttingDown()) {
        event->ignore();
    }
}

/******************************************************************************
* Closes the user interface and shuts down the entire application after displaying an error message.
******************************************************************************/
void MainWindow::exitWithFatalError(const Exception& ex)
{
    OVITO_ASSERT(ExecutionContext::isMainThread());

    // Avoid reentrance.
    if(_exitingWithFatalError)
        return;

    // Set flag.
    _exitingWithFatalError = true;

    // Disable all further viewport updates, because they may have beeen the reason for this fatal error.
    suspendViewportUpdates();

    // Display fatal error message to the user.
    reportError(ex, true);

    // The event loop may not be running yet. Use a timer to execute the following
    // once we enter the event loop - similar to a queued signal/slot connection.
    QTimer::singleShot(0, [weakPtr = OOWeakRef<MainWindow>(this)]() {
        if(auto self = weakPtr.lock()) {
            if(!self->close()) // Ask user if closing the window is ok. If not...
                self->shutdown(); // ... forcibly close the window anyway.
        }
        QCoreApplication::exit(1);
    });
}

/******************************************************************************
* Gives the active viewport the input focus.
******************************************************************************/
void MainWindow::setViewportInputFocus()
{
    viewportsPanel()->setFocus(Qt::OtherFocusReason);
}

/******************************************************************************
* Returns the page of the command panel that is currently visible.
******************************************************************************/
MainWindow::CommandPanelPage MainWindow::currentCommandPanelPage() const
{
    return _commandPanel->currentPage();
}

/******************************************************************************
* Sets the page of the command panel that is currently visible.
******************************************************************************/
void MainWindow::setCurrentCommandPanelPage(CommandPanelPage page)
{
    _commandPanel->setCurrentPage(page);
}

/******************************************************************************
* Sets the file path associated with this window and updates the window's title.
******************************************************************************/
void MainWindow::setWindowFilePath(const QString& filePath)
{
    if(filePath.isEmpty())
        setWindowTitle(_baseWindowTitle + QStringLiteral(" [*]"));
    else
        setWindowTitle(_baseWindowTitle + QStringLiteral(" - %1[*]").arg(QFileInfo(filePath).fileName()));
    QMainWindow::setWindowFilePath(filePath);
}

/******************************************************************************
* Called by the system when a drag is in progress and the mouse enters this
* window.
******************************************************************************/
void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

/******************************************************************************
* Called by the system when the drag is dropped on this window.
******************************************************************************/
void MainWindow::dropEvent(QDropEvent* event)
{
    event->acceptProposedAction();
    std::vector<QUrl> importUrls;
    bool success = handleExceptions([&] {
        for(const QUrl& url : event->mimeData()->urls()) {
            if(url.fileName().endsWith(".ovito", Qt::CaseInsensitive)) {
                if(url.isLocalFile()) {
                    askForSaveChanges();
                    OORef<DataSet> dataset = DataSet::createFromFile(url.toLocalFile());
                    if(checkLoadedDataset(dataset))
                        datasetContainer().setCurrentSet(std::move(dataset));
                    importUrls.clear();
                    return;
                }
            }
            else {
                importUrls.push_back(url);
            }
        }
    });
    if(success && !importUrls.empty()) {
        performTransaction(tr("Import data"), [&] {
            importFiles(std::move(importUrls));
        });
    }
}

/******************************************************************************
* Opens the data inspector panel and shows the data object generated by the
* given data pipeline node.
******************************************************************************/
bool MainWindow::openDataInspector(PipelineNode* createdByNode, const QString& objectNameHint, const QVariant& modeHint)
{
    if(_dataInspector->selectDataObject(createdByNode, objectNameHint, modeHint)) {
        _dataInspector->open();
        return true;
    }
    return false;
}

/******************************************************************************
* Displays a message string in the window's status bar.
******************************************************************************/
void MainWindow::showStatusBarMessage(const QString& message, int timeout)
{
    _statusBar->showMessage(message, timeout);
}

/******************************************************************************
* Hides any messages currently displayed in the window's status bar.
******************************************************************************/
void MainWindow::clearStatusBarMessage()
{
    // Conditional call to clearMessage() because clearMessage() always repaints the status bar, even it is not showing any message (as of Qt 6.3.2).
    if(!_statusBar->currentMessage().isEmpty())
        _statusBar->clearMessage();
}

/******************************************************************************
* Creates a frame buffer of the requested size and displays it as a window in the user interface.
******************************************************************************/
std::shared_ptr<FrameBuffer> MainWindow::createAndShowFrameBuffer(int width, int height)
{
    // This function must be called with an active task scope, because the frame buffer window
    // will display the progress of the rendering process.
    OVITO_ASSERT(this_task::get());

    // Create the frame buffer window.
    if(!_frameBufferWindow) {
        _frameBufferWindow = new FrameBufferWindow(*this, this);
        _frameBufferWindow->setWindowTitle(tr("Render output"));
    }

    std::shared_ptr<FrameBuffer> fb = _frameBufferWindow->createFrameBuffer(width, height);
    _frameBufferWindow->showAndActivateWindow();
    _frameBufferWindow->showRenderingProgress();

    return fb;
}

/******************************************************************************
* Handler function for exceptions.
******************************************************************************/
void MainWindow::reportError(const Exception& ex, bool blocking)
{
    OVITO_ASSERT(QThread::currentThread() == this->thread());

    // Always display errors in the terminal window too.
    UserInterface::reportError(ex, blocking);

    if(!blocking) {
        // Deferred display of the error message after execution returns to the main event loop.
        if(_errorList.empty())
            QMetaObject::invokeMethod(this, "showErrorMessages", Qt::QueuedConnection);

        // Queue error messages.
        _errorList.push_back(ex);
    }
    else {
        _errorList.push_back(ex);
        showErrorMessages();
    }
}

/******************************************************************************
* Displays an error message to the user that is associated with a particular child window or dialog.
******************************************************************************/
void MainWindow::reportError(const Exception& exception, QWidget* window)
{
    // Prepare a message box dialog.
    QPointer<MessageDialog> msgbox = new MessageDialog();
    msgbox->setWindowTitle(tr("Error - %1").arg(Application::applicationName()));
    msgbox->setStandardButtons(QMessageBox::Ok);
    msgbox->setText(exception.message());
    msgbox->setIcon(QMessageBox::Critical);
    msgbox->setTextInteractionFlags(Qt::TextBrowserInteraction);

    // Stop animation playback when an error occurred.
    QAction* playbackAction = actionManager()->getAction(ACTION_TOGGLE_ANIMATION_PLAYBACK);
    if(playbackAction && playbackAction->isChecked())
        playbackAction->trigger();

    if(window && window->isVisible()) {
        // If there currently is floating window being shown (e.g. the FrameBufferWindow),
        // make the error message dialog a child of this floating window to show it in front.
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
        for(QMainWindow* floatingChildWindow : window->findChildren<QMainWindow*>(Qt::FindDirectChildrenOnly)) {
#else
        for(QMainWindow* floatingChildWindow : window->findChildren<QMainWindow*>(QString{}, Qt::FindDirectChildrenOnly)) {
#endif
            if(floatingChildWindow->isVisible() && floatingChildWindow->isActiveWindow()) {
                window = floatingChildWindow;
                break;
            }
        }

        // If there currently is a modal dialog box being shown,
        // make the error message dialog a child of this dialog to prevent a UI dead-lock.
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
        for(QDialog* dialog : window->findChildren<QDialog*>(Qt::FindChildrenRecursively)) {
#else
        for(QDialog* dialog : window->findChildren<QDialog*>(QString{}, Qt::FindChildrenRecursively)) {
#endif
            if(dialog->isModal()) {
                window = dialog;
                dialog->show();
                break;
            }
        }
        msgbox->setParent(window);
        msgbox->setWindowModality(Qt::WindowModal);
    }

    // If the exception is associated with additional message strings,
    // show them in the Details section of the message box dialog.
    QString detailText;
    if(exception.messages().size() > 1) {
        for(int i = 1; i < exception.messages().size(); i++)
            detailText += exception.messages()[i] + QStringLiteral("\n");
    }
    // Also show traceback information.
    if(!exception.traceback().isEmpty()) {
        if(!detailText.isEmpty())
            detailText += QChar('\n');
        detailText += exception.traceback();
    }
    msgbox->setDetailedText(std::move(detailText));

    // Show message box.
    msgbox->exec();

    // Discard message box.
    delete msgbox.data();
}

/******************************************************************************
* Displays an error message box. This slot is called by reportError().
******************************************************************************/
void MainWindow::showErrorMessages()
{
    while(!_errorList.empty()) {
        // Show next exception from queue.
        reportError(_errorList.front(), this);
        _errorList.pop_front();
    }
}

/******************************************************************************
* Opens another main window (in addition to the existing windows) and
* optionally loads a file in the new window.
******************************************************************************/
void MainWindow::openNewWindow(const QStringList& arguments)
{
    // This is a workaround for a bug in Qt 6.4 on macOS platform. The displayed menu bar does not automatically follow
    // the main window that is currently active. That's why we simply start up another independent instance of the application.
#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    // Get the path to the ovito executable.
    QString execPath = Application::instance()->applicationFilePath();

    // If we are currently running ovitos in graphical mode, start ovito instead.
    if(execPath.endsWith("ovitos"))
        execPath.chop(1);

    // Start another instance of the program.
    if(!QProcess::startDetached(execPath, arguments))
        throw Exception(tr("Failed to start another instance of the program. Executable path: %1").arg(execPath));
#else
    OORef<MainWindow> mainWin = OORef<MainWindow>::create();
    mainWin->keepAliveUntilShutdown();
    mainWin->show();
    mainWin->restoreLayout();
    if(!mainWin->handleExceptions([&]() {
        GuiApplication::initializeUserInterface(*mainWin, arguments);
    })) {
        mainWin->shutdown();
    }
#endif
}

/******************************************************************************
* Checks (or even modifies) the contents of a DataSet after it has been loaded from a file.
* Returns false if loading the DataSet was rejected by the application.
******************************************************************************/
bool MainWindow::checkLoadedDataset(DataSet* dataset)
{
    if(!UserInterface::checkLoadedDataset(dataset))
        return false;

#ifndef OVITO_BUILD_PROFESSIONAL
    // Since version 3.8.0, OVITO Basic no longer supports multiple pipelines in the same scene.
    // Check if the state file contains more than one pipeline and inform user by displaying a dialog window.
    // Let the user pick one of the pipelines to be loaded and remove all others from the scene.
    if(ViewportConfiguration* viewportConfig = dataset->viewportConfig()) {
        if(Viewport* vp = viewportConfig->activeViewport()) {
            if(Scene* scene = vp->scene()) {
                std::vector<OORef<Pipeline>> fileSourcePipelines;
                QStringList itemsList;
                scene->visitPipelines([&](Pipeline* pipeline) {
                    if(dynamic_object_cast<FileSource>(pipeline->source())) {
                        fileSourcePipelines.emplace_back(pipeline);
                        itemsList.push_back(pipeline->objectTitle());
                    }
                    return true;
                });
                if(fileSourcePipelines.size() >= 2) {
                    QDialog dlg(this);
                    dlg.setWindowTitle(tr("Multiple pipelines found"));
                    QVBoxLayout* mainLayout = new QVBoxLayout(&dlg);
                    mainLayout->setSpacing(2);
                    QLabel* label = new QLabel(tr(
                        "<html><p>The OVITO session file contains %1 pipelines.</p>"
                        "<p><i>OVITO Pro</i> is required since version 3.8.0 to work with "
                        "multiple pipelines in the same scene. Please pick one of the pipelines "
                        "below to load only that pipeline in <i>OVITO Basic</i> now - or open the session "
                        "file in <i>OVITO Pro</i> to load all pipelines together.</p></html>"
                    ).arg(fileSourcePipelines.size()));
                    label->setWordWrap(true);
                    label->setMinimumWidth(440);
                    mainLayout->addWidget(label);
                    mainLayout->addSpacing(6);
                    mainLayout->addWidget(new QLabel(tr("Available pipelines:")));
                    QListWidget* listWidget = new QListWidget();
                    mainLayout->addWidget(listWidget);
                    listWidget->addItems(itemsList);
                    listWidget->setCurrentRow(0);
                    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dlg);
                    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
                    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
                    connect(listWidget, &QListWidget::itemSelectionChanged, &dlg, [&]() { buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!listWidget->selectedItems().empty()); });
                    mainLayout->addWidget(buttonBox);
                    if(dlg.exec() == QDialog::Accepted) {
                        QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
                        if(selectedItems.empty())
                            return false; // Abort loading of the DataSet.
                        int keepIndex = listWidget->row(selectedItems.front());
                        scene->selection()->setNode(fileSourcePipelines[keepIndex]);
                        for(const auto& pipeline : fileSourcePipelines) {
                            if(pipeline != fileSourcePipelines[keepIndex]) {
                                pipeline->deleteSceneNode();
                            }
                        }
                    }
                    else {
                        return false; // Abort loading of the DataSet.
                    }
                }
            }
        }
    }
#endif

    return true;
}

/******************************************************************************
* Save the current dataset.
******************************************************************************/
bool MainWindow::fileSave()
{
    OVITO_ASSERT(ExecutionContext::current().isValid());
    OORef<DataSet> dataset = datasetContainer().currentSet();

    if(!dataset)
        return false;

    // Ask the user for a filename if there is no one set.
    if(dataset->filePath().isEmpty())
        return fileSaveAs();

    // Save dataset to file.
    return handleExceptions([&] {
        dataset->saveToFile(dataset->filePath());
        undoStack()->setClean();
    });
}

/******************************************************************************
* This is the implementation of the "Save As" action.
* Returns true, if the scene has been saved.
******************************************************************************/
bool MainWindow::fileSaveAs(const QString& filename)
{
    OVITO_ASSERT(ExecutionContext::current().isValid());
    OORef<DataSet> dataset = datasetContainer().currentSet();

    if(!dataset)
        return false;

    if(filename.isEmpty()) {

        QFileDialog dialog(this, tr("Save Session State"));
        dialog.setNameFilter(tr("OVITO State Files (*.ovito);;All Files (*)"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setDefaultSuffix("ovito");

        QSettings settings;
        settings.beginGroup("file/scene");

        if(dataset->filePath().isEmpty()) {
            if(HistoryFileDialog::keepWorkingDirectoryHistoryEnabled()) {
                QString defaultPath = settings.value("last_directory").toString();
                if(!defaultPath.isEmpty())
                    dialog.setDirectory(defaultPath);
            }
        }
        else {
#ifndef Q_OS_LINUX
            dialog.selectFile(dataset->filePath());
#else
            // Workaround for bug in QFileDialog on Linux (Qt 6.2.4) crashing in exec() when selectFile() is called before (OVITO issue #216).
            dialog.setDirectory(QFileInfo(dataset->filePath()).dir());
#endif
        }

        TaskManager::setNativeDialogActive(true);
        auto dlgResult = dialog.exec();
        TaskManager::setNativeDialogActive(false);

        if(dlgResult != QDialog::Accepted)
            return false;

        QStringList files = dialog.selectedFiles();
        if(files.isEmpty())
            return false;
        QString newFilename = files.front();

        if(HistoryFileDialog::keepWorkingDirectoryHistoryEnabled()) {
            // Remember directory for the next time...
            settings.setValue("last_directory", dialog.directory().absolutePath());
        }

        dataset->setFilePath(newFilename);
    }
    else {
        dataset->setFilePath(filename);
    }
    return fileSave();
}

/******************************************************************************
* If the scene has been changed this will ask the user if he wants
* to save the changes.
******************************************************************************/
void MainWindow::askForSaveChanges()
{
    OVITO_ASSERT(ExecutionContext::current().isValid());
    OORef<DataSet> dataset = datasetContainer().currentSet();

    if(!dataset || dataset->filePath().isEmpty() || undoStack()->isClean())
        return;

    QString message;
    if(dataset->filePath().isEmpty() == false) {
        message = tr("The current session state has been modified. Do you want to save the changes?");
        message += QString("\n\nFile: %1").arg(dataset->filePath());
    }
    else {
        message = tr("The current program session has not been saved. Do you want to save it?");
    }

    QMessageBox::StandardButton result = MessageDialog::question(this, tr("Save changes"),
        message,
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);
    if(result == QMessageBox::Cancel) {
        this_task::cancelAndThrow();
    }
    else if(result != QMessageBox::No) {
        // Save scene first.
        fileSave();
    }
}

/******************************************************************************
* Imports a given file into the scene.
******************************************************************************/
void MainWindow::importFiles(const std::vector<QUrl>& urls, const FileImporterClass* importerType, const QString& importerFormat)
{
    OVITO_ASSERT(ExecutionContext::current().isValid());
    OVITO_ASSERT(!urls.empty());

    // Create a reference to the active scene to keep it alive during this long-running operation.
    OORef<DataSet> dataset = datasetContainer().currentSet();
    OORef<Scene> scene = datasetContainer().activeScene();
    if(!dataset || !scene)
        throw Exception(tr("Cannot import because there is no active scene."));

    std::vector<std::pair<QUrl, OORef<FileImporter>>> urlImporters;
    for(const QUrl& url : urls) {
        if(!url.isValid())
            throw Exception(tr("Failed to import file. URL is not valid: %1").arg(url.toString()));

        OORef<FileImporter> importer;
        if(!importerType) {

            // Detect file format.
            Future<OORef<FileImporter>> importerFuture = FileImporter::autodetectFileFormat(url);
            importer = importerFuture.result();
            if(!importer)
                throw Exception(tr("Could not auto-detect the format of the file %1. The file format might not be supported.").arg(url.fileName()));
        }
        else {
            importer = static_object_cast<FileImporter>(importerType->createInstance());
            if(!importer)
                throw Exception(tr("Failed to import file. Could not initialize import service."));
            importer->setSelectedFileFormat(importerFormat);
        }

        urlImporters.push_back(std::make_pair(url, std::move(importer)));
    }

    // Order URLs and their corresponding importers.
    std::stable_sort(urlImporters.begin(), urlImporters.end(), [](const auto& a, const auto& b) {
        int pa = a.second->importerPriority();
        int pb = b.second->importerPriority();
        if(pa > pb) return true;
        if(pa < pb) return false;
        return a.second->getOOClass().name() < b.second->getOOClass().name();
    });

    // Display the optional UI (which is provided by the corresponding FileImporterEditor class) for each importer.
    for(const auto& item : urlImporters) {
        const QUrl& url = item.first;
        const OORef<FileImporter>& importer = item.second;
        for(OvitoClassPtr clazz = &importer->getOOClass(); clazz != nullptr; clazz = clazz->superClass()) {
            OvitoClassPtr editorClass = PropertiesEditor::registry().getEditorClass(clazz);
            if(editorClass && editorClass->isDerivedFrom(FileImporterEditor::OOClass())) {
                OORef<FileImporterEditor> editor = dynamic_object_cast<FileImporterEditor>(editorClass->createInstance());
                if(editor) {
                    editor->inspectNewFile(importer, url, *this);
                    this_task::throwIfCanceled();
                }
            }
        }
    }

    // Determine how the file's data should be inserted into the current scene.
    FileImporter::ImportMode importMode = FileImporter::ResetScene;

    OORef<FileImporter> importer = urlImporters.front().second;
    if(importer->isReplaceExistingPossible(scene, urls)) {
        // Ask user if the existing pipeline should be preserved or reset.
        MessageDialog msgBox(QMessageBox::Question, tr("Import file"),
                tr("Do you want to reset the existing pipeline?"),
                QMessageBox::Yes | QMessageBox::Cancel, this);
#ifdef OVITO_BUILD_PROFESSIONAL
        msgBox.setInformativeText(tr(
            "<p>Select <b>Yes</b> to start over and discard the existing pipeline before importing the new file.</p>"
            "<p>Select <b>No</b> to keep modifiers in the current pipeline and replace the input data with the selected file.</p>"
            "<p>Select <b>Add to scene</b> to create an additional pipeline and visualize multiple datasets.</p>"));
#else
        msgBox.setInformativeText(tr(
            "<p>Select <b>Yes</b> to start over and discard the existing pipeline before importing the new file.</p>"
            "<p>Select <b>No</b> to keep modifiers in the current pipeline and replace the input data with the selected file.</p>"
            "<p>Select <b>Add to scene</b> to create an additional pipeline and visualize multiple datasets (requires <a href=\"https://www.ovito.org/about/ovito-pro/\">OVITO Pro</a>).</p>"));
#endif
        msgBox.addButton(tr("No"), QMessageBox::NoRole);
        QPushButton* addToSceneButton = msgBox.addButton(tr("Add to scene"), QMessageBox::NoRole);
#ifndef OVITO_BUILD_PROFESSIONAL
        addToSceneButton->setEnabled(false);
#else
        (void)addToSceneButton;
#endif
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.setEscapeButton(QMessageBox::Cancel);
        msgBox.exec();

        if(msgBox.clickedButton() == msgBox.button(QMessageBox::Cancel)) {
            this_task::cancelAndThrow(); // Operation canceled by user.
        }
        else if(msgBox.clickedButton() == msgBox.button(QMessageBox::Yes)) {
            importMode = FileImporter::ResetScene;
            // Ask user if current scene should be saved before it is replaced by the imported data.
            askForSaveChanges();
        }
        else if(msgBox.clickedButton() == addToSceneButton) {
            importMode = FileImporter::AddToScene;
        }
        else {
            // No button
            importMode = FileImporter::ReplaceSelected;
        }
    }
    else if(scene->children().empty() == false) {
        // Ask user if the current scene should be completely replaced by the imported data.
        QMessageBox::StandardButton result = MessageDialog::question(this, tr("Import file"),
            tr("Do you want to keep the existing objects in the current scene?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);

        if(result == QMessageBox::Cancel) {
            this_task::cancelAndThrow(); // Operation canceled by user.
        }
        else if(result == QMessageBox::No) {
            importMode = FileImporter::ResetScene;

            // Ask user if current scene should be saved before it is replaced by the imported data.
            askForSaveChanges();
        }
        else {
            importMode = FileImporter::AddToScene;
        }
    }

    // Do not create any animation keys during import.
    AnimationSuspender animSuspender(*this);

    if(OORef<Pipeline> pipeline = importer->importFileSet(scene, std::move(urlImporters), importMode, true, ImportFileDialog::multiFileImportMode())) {
        if(importMode == FileImporter::ResetScene) {
            undoStack()->clear();
            dataset->setFilePath({});
        }
    }
}

}   // End of namespace
