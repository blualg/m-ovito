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
#include <ovito/gui/desktop/app/GuiApplication.h>
#include <ovito/gui/desktop/app/GuiApplicationService.h>
#include <ovito/gui/desktop/widgets/animation/AnimationTimeSpinner.h>
#include <ovito/gui/desktop/widgets/animation/AnimationTimeSlider.h>
#include <ovito/gui/desktop/widgets/animation/AnimationTrackBar.h>
#include <ovito/gui/desktop/widgets/rendering/FrameBufferWindow.h>
#include <ovito/gui/desktop/widgets/display/CoordinateDisplayWidget.h>
#include <ovito/gui/desktop/widgets/general/StatusBar.h>
#include <ovito/gui/desktop/widgets/selection/SceneNodeSelectionBox.h>
#include <ovito/gui/desktop/actions/WidgetActionManager.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/viewport/ViewportWindowInterface.h>
#include <ovito/core/app/StandaloneApplication.h>
#include "MainWindow.h"
#include "ViewportsPanel.h"
#include "TaskDisplayWidget.h"
#include "cmdpanel/CommandPanel.h"
#include "data_inspector/DataInspectorPanel.h"

namespace Ovito {

/******************************************************************************
* The constructor of the main window class.
******************************************************************************/
MainWindow::MainWindow() : UserInterface(_datasetContainer, _taskManager), _datasetContainer(_taskManager, *this)
{
	_baseWindowTitle = tr("%1 (Open Visualization Tool)").arg(Application::applicationName());
#if defined(OVITO_EXPIRATION_DATE)
	_baseWindowTitle += tr(" - Preview build expiring on %1").arg(QDate::fromString(QStringLiteral(OVITO_EXPIRATION_DATE), Qt::ISODate).toString(Qt::SystemLocaleShortDate));
#elif defined(OVITO_DEVELOPMENT_BUILD_DATE)
	_baseWindowTitle += tr(" - Development build %1 created on %2").arg(Application::applicationVersionString()).arg(QStringLiteral(OVITO_DEVELOPMENT_BUILD_DATE));
#endif
	setWindowTitle(_baseWindowTitle);
	setAttribute(Qt::WA_DeleteOnClose);

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
	connect(&_datasetContainer, &DataSetContainer::filePathChanged, this, [this](const QString& filePath) { setWindowFilePath(filePath); });
	connect(undoStack(), &UndoStack::cleanChanged, this, [this](bool isClean) { setWindowModified(!isClean); });

	// Accept files via drag & drop.
	setAcceptDrops(true);
}

/******************************************************************************
* Destructor.
******************************************************************************/
MainWindow::~MainWindow()
{
	_datasetContainer.setCurrentSet(nullptr);
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
	else if(event->type() == QEvent::ThemeChange) {
		// Switch icon theme to match with the current UI theme.
		QIcon::setThemeName(GuiApplication::instance()->usingDarkTheme() ? QStringLiteral("ovito-dark") : QStringLiteral("ovito-light"));
	}
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
	if(!handleExceptions([&] {
		// Save changes made to the current dataset.
		if(!datasetContainer().askForSaveChanges()) {
			event->ignore();
			return;
		}

		// Close the dataset container.
		datasetContainer().setCurrentSet(nullptr);

		// Stop all running operations.
		taskManager().shutdown();

		// Save window layout.
		saveLayout();

		// Destroys the window.
		event->accept();
	})) {
		event->ignore();
	}
}

/******************************************************************************
* Closes the user interface and shuts down the entire application after displaying an error message.
******************************************************************************/
void MainWindow::exitWithFatalError(const Exception& ex) 
{ 
	suspendViewportUpdates();
	reportError(ex, true);
	QTimer::singleShot(0, this, [this]() {
		QMainWindow::close(); 
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
	handleExceptions([&] {
		for(const QUrl& url : event->mimeData()->urls()) {
			if(url.fileName().endsWith(".ovito", Qt::CaseInsensitive)) {
				if(url.isLocalFile()) {
					if(datasetContainer().askForSaveChanges()) {
						datasetContainer().loadDataset(url.toLocalFile());
					}
					importUrls.clear();
					return;
				}
			}
			else {
				importUrls.push_back(url);
			}
		}
	});
	if(!importUrls.empty()) {
		performTransaction(tr("Import data"), [&] {
			datasetContainer().importFiles(std::move(importUrls));
		});
	}
}

/******************************************************************************
* Opens the data inspector panel and shows the data object generated by the
* given data source.
******************************************************************************/
bool MainWindow::openDataInspector(PipelineObject* dataSource, const QString& objectNameHint, const QVariant& modeHint)
{
	if(_dataInspector->selectDataObject(dataSource, objectNameHint, modeHint)) {
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
std::shared_ptr<FrameBuffer> MainWindow::createAndShowFrameBuffer(int width, int height, bool showRenderingOperationProgress) 
{
	// Create the frame buffer window.
	if(!_frameBufferWindow) {
		_frameBufferWindow = new FrameBufferWindow(*this, this);
		_frameBufferWindow->setWindowTitle(tr("Render output"));
	}
	std::shared_ptr<FrameBuffer> fb = _frameBufferWindow->createFrameBuffer(width, height);
	_frameBufferWindow->showAndActivateWindow();
	if(showRenderingOperationProgress)
		_frameBufferWindow->showRenderingOperation();
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
	QPointer<QMessageBox> msgbox = new QMessageBox();
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
		for(QMainWindow* floatingChildWindow : window->findChildren<QMainWindow*>()) {
			if(floatingChildWindow->isVisible() && floatingChildWindow->isActiveWindow()) {
				window = floatingChildWindow;
				break;
			}
		}

		// If there currently is a modal dialog box being shown,
		// make the error message dialog a child of this dialog to prevent a UI dead-lock.
		for(QDialog* dialog : window->findChildren<QDialog*>()) {
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
	if(exception.messages().size() > 1) {
		QString detailText;
		for(int i = 1; i < exception.messages().size(); i++)
			detailText += exception.messages()[i] + QStringLiteral("\n");
		msgbox->setDetailedText(detailText);
	}

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

}	// End of namespace
