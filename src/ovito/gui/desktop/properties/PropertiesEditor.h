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
#include <ovito/gui/desktop/widgets/general/RolloutContainer.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/utilities/DeferredMethodInvocation.h>
#include <ovito/gui/desktop/utilities/concurrent/ProgressDialog.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "PropertiesPanel.h"

namespace Ovito {

/**
 * \brief Base class for property editors for RefTarget derived objects.
 *
 * A properties editor for a RefTarget derived object can be created using the PropertiesEditor::create() function.
 */
class OVITO_GUI_EXPORT PropertiesEditor : public QObject, public RefMaker
{
    OVITO_CLASS(PropertiesEditor)
    Q_OBJECT

public:

    /// Registry for editor classes.
    /// Use the SET_OVITO_OBJECT_EDITOR macro to register an editor class for a RefTarget class at compile time.
    class Registry : private std::map<OvitoClassPtr, OvitoClassPtr>
    {
    public:
        void registerEditorClass(OvitoClassPtr refTargetClass, OvitoClassPtr editorClass) {
            insert(std::make_pair(refTargetClass, editorClass));
        }
        OvitoClassPtr getEditorClass(OvitoClassPtr refTargetClass) const {
            auto entry = find(refTargetClass);
            if(entry != end()) return entry->second;
            else return nullptr;
        }
    };

    /// Returns the global editor registry, which allows looking up the editor class for a RefTarget class.
    static Registry& registry();

public:

    /// \brief Creates a PropertiesEditor for an editable object.
    /// \param obj The object for which an editor should be created.
    /// \return The new editor component that allows the user to edit the properties of the RefTarget object.
    ///         It will be automatically destroyed by the system when the editor is closed.
    ///         Returns NULL if no editor component is registered for the RefTarget type.
    ///
    /// The returned editor object is not initialized yet. Call initialize() once to do so.
    static OORef<PropertiesEditor> create(MainWindow& mainWindow, RefTarget* obj);

    /// \brief This will bind the editor to the given container.
    /// \param container The properties panel that's the host of the editor.
    /// \param mainWindow The main window that hosts the editor.
    /// \param rolloutParams Specifies how the editor's rollouts should be created.
    /// \param parentEditor The editor that owns this editor if it is a sub-object editor; NULL otherwise.
    ///
    /// This method is called by the PropertiesPanel class to initialize the editor and to create the UI.
    void initialize(PropertiesPanel* container, const RolloutInsertionParameters& rolloutParams, PropertiesEditor* parentEditor);

    /// This method is called after the reference counter of this object has reached zero
    /// and before the object is being deleted.
    virtual void aboutToBeDeleted() override;

    /// \brief Returns the rollout container widget this editor is placed in.
    PropertiesPanel* container() const { return _container; }

    /// \brief Returns the main window that hosts the editor.
    MainWindow& mainWindow() const {
        OVITO_ASSERT(_mainWindow != nullptr);
        return *_mainWindow;
    }

    /// \brief Returns the top-level window hosting this editor panel.
    QWidget* parentWindow() const;

    /// \brief Returns a pointer to the parent editor which has opened this editor for one of its sub-components.
    PropertiesEditor* parentEditor() const { return _parentEditor; }

    /// \brief Creates a new rollout in the rollout container and returns
    ///        the empty widget that can then be filled with UI controls.
    /// \param title The title of the rollout.
    /// \param rolloutParams Specifies how the rollout should be created.
    /// \param helpPage The help page or topic in the user manual that describes this rollout.
    ///
    /// \note The rollout is automatically deleted when the editor is deleted.
    QWidget* createRollout(const QString& title, const RolloutInsertionParameters& rolloutParams, const char* helpPage = nullptr);

    /// \brief Completely disables the UI elements in the given rollout.
    /// \param rolloutWidget The rollout widget to be disabled, which has been created by createRollout().
    /// \param noticeText A text to displayed in the rollout panel to inform the user why the rollout has been disabled.
    void disableRollout(QWidget* rolloutWidget, const QString& noticeText);

    /// \brief Inserts a notice text at the top of the given rollout.
    /// \param rolloutWidget The rollout widget, which has been created by createRollout().
    /// \param noticeText A text to displayed in the rollout panel to inform the user why the rollout has been disabled.
    void showNotice(QWidget* rolloutWidget, const QString& noticeText);

    /// Returns the current input data from the upstream pipeline.
    PipelineFlowState getPipelineInput() const;

    /// Returns the current input data from the upstream pipeline.
    std::vector<PipelineFlowState> getPipelineInputs() const;

    /// Returns the current output data produced by the object being edited.
    PipelineFlowState getPipelineOutput() const;

    /// Returns the first ModificationNode of the modifier currently being edited.
    /// If this editor does not host a modifier, nullptr is returned.
    ModificationNode* modificationNode() const;

    /// Returns the list of all ModificationNode of the modifier currently being edited.
    /// If this editor does not host a modifier, an empty list is returned.
    QVector<ModificationNode*> modificationNodes() const;

    /// For an editor of a DataVis element, returns the DataObject which the DataVis element is attached to.
    ConstDataObjectRef getVisDataObject() const;

    /// For an editor of a DataVis element, returns the data collection path to the DataObject which the DataVis element is attached to.
    ConstDataObjectRefPath getVisDataObjectPath() const;

    /// Returns the current animation time of the scene the object being edited belongs to.
    AnimationTime currentAnimationTime() const;

    /// Returns the viewport that is currently selected.
    Viewport* activeViewport() const;

    /// Creates a ParameterUI component and registers it with the editor instance.
    /// The component will be freed when the editor is destroyed.
    template<typename T, typename... Args>
    T* createParamUI(Args&&... args) {
        _parameterUIs.push_back(OORef<T>::create(this, std::forward<Args>(args)...));
        return static_object_cast<T>(_parameterUIs.back().get());
    }

    /// Executes a functor and catches any exceptions thrown during its execution.
    /// If an exception is thrown by the functor, the error message is displayed to the user and this function returns false.
    template<typename Function>
    bool handleExceptions(Function&& func) const {
        return mainWindow().handleExceptions(std::forward<Function>(func));
    }

    /// Executes a functor provided by the caller that performs undoable actions in an interactive context.
    /// If an exception is thrown by the functor, the error message is displayed
    /// to the user, and this function returns false.
    template<typename Function>
    bool performActions(UndoableTransaction& transaction, Function&& func) {
        return mainWindow().performActions(transaction, std::forward<Function>(func));
    }

    /// \brief Executes the passed functor and catches any exceptions thrown during its execution.
    /// If an exception is thrown by the functor, all data changes performed by the functor
    /// so far will be undone and an error message is shown to the user.
    template<typename Function>
    bool performTransaction(const QString& undoOperationName, Function&& func) {
        return mainWindow().performTransaction(undoOperationName, std::forward<Function>(func));
    }

    /// \brief Recursively visits all pipeline scene nodes in the current scene
    ///        and invokes the given visitor function for every scene node with a pipeline.
    ///
    /// \param fn A function that takes an SceneNode pointer as argument and returns a boolean value.
    /// \return true if all pipelines have been visited; false if the loop has been
    ///         terminated early because the visitor function has returned false.
    ///
    /// The visitor function must return a boolean value to indicate whether
    /// it wants to continue visit more scene nodes. A return value of false
    /// leads to early termination and no further nodes are visited.
    template<typename Function>
    bool visitScenePipelines(Function&& fn) const {
        if(Scene* scene = mainWindow().datasetContainer().activeScene())
            return scene->visitPipelines(std::forward<Function>(fn));
        return true;
    }

    /// Returns the scene node that is currently selected in the scene.
    SceneNode* selectedSceneNode() const {
        if(SelectionSet* selection = mainWindow().datasetContainer().activeSelectionSet())
            return selection->firstNode();
        return nullptr;
    }

    /// Returns the pipeline that is currently selected in the scene.
    Pipeline* selectedPipeline() const {
        if(SceneNode* sceneNode = selectedSceneNode())
            return sceneNode->pipeline();
        return nullptr;
    }

    /// Shows a progress dialog while waiting for the given future to complete.
    /// Then runs the given continuation function in the GUI thread, which receives the results of the future.
    /// The continuation function is only executed if the future completes successfully, the editor
    /// is still open, and the original object is still being loaded in the editor.
    template<typename FutureType, typename Function>
    void scheduleOperationAfter(FutureType&& future, Function&& function) {
        if(!editObject())
            return;
        ProgressDialog* progressDialog = new ProgressDialog(future.task(), {}, mainWindow(), parentWindow());
        progressDialog->whenDone([self=QPointer<PropertiesEditor>(this), editObject=OOWeakRef<RefTarget>(editObject()), future=std::move(future), function=std::forward<Function>(function)]() mutable noexcept {
            if(!self.isNull() && self->editObject() == editObject.lock()) {
                self->mainWindow().handleExceptions([&]() {
                    std::invoke(std::move(function), std::move(future).result());
                });
            }
        });
    }

public Q_SLOTS:

    /// \brief Sets the object being edited in this editor.
    /// \param newObject The new object to load into the editor. This must be of the same class
    ///                  as the previous object.
    ///
    /// This method generates a contentsReplaced() and a contentsChanged() signal.
    void setEditObject(RefTarget* newObject);

Q_SIGNALS:

    /// \brief This signal is emitted by the editor when a new edit object
    ///        has been loaded into the editor via the setEditObject() method.
    /// \sa editObject The new object loaded into the editor.
    void contentsReplaced(RefTarget* editObject);

    /// \brief This signal is emitted by the editor when the current edit object has generated a TargetChanged
    ///        event or if a new object has been loaded into editor via the setEditObject() method.
    /// \sa editObject The object that has changed.
    void contentsChanged(RefTarget* editObject);

    /// \brief This signal is emitted whenever the edited object has produced new results as part of a pipeline evaluation
    ///        or when a new object has been loaded into the editor.
    void pipelineOutputChanged();

    /// \brief This signal is emitted whenever the edited object received new pipeline inputs due to an upstream pipeline change
    ///        or when a new object has been loaded into the editor.
    void pipelineInputChanged();

protected:

    /// Creates the user interface controls for the editor.
    /// This must be implemented by sub-classes.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) = 0;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this RefMaker changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private:

    /// The object being edited in this editor.
    DECLARE_REFERENCE_FIELD(OORef<RefTarget>, editObject);

    /// The container widget the editor is shown in.
    PropertiesPanel* _container = nullptr;

    /// The main window that hosts the editor.
    MainWindow* _mainWindow = nullptr;

    /// Pointer to the parent editor which opened this editor for a sub-component.
    PropertiesEditor* _parentEditor = nullptr;

    /// The list of ParameterUI components created by the editor.
    std::vector<OORef<ParameterUI>> _parameterUIs;

    /// The list of rollout widgets that have been created by editor.
    /// The cleanup handler is used to delete them when the editor is being deleted.
    QObjectCleanupHandler _rollouts;

    /// For emitting the pipelineInputChanged() signal with a short delay.
    DeferredMethodInvocation<PropertiesEditor, &PropertiesEditor::pipelineInputChanged> emitPipelineInputChangedSignal;

    /// For emitting the pipelineOutputChanged() signal with a short delay.
    DeferredMethodInvocation<PropertiesEditor, &PropertiesEditor::pipelineOutputChanged> emitPipelineOutputChangedSignal;
};

/// This macro is used to assign a PropertiesEditor-derived class to a RefTarget-derived class.
#define SET_OVITO_OBJECT_EDITOR(RefTargetClass, PropertiesEditorClass) \
    Q_DECL_UNUSED static const int __editorSetter##RefTargetClass = (Ovito::PropertiesEditor::registry().registerEditorClass(&RefTargetClass::OOClass(), &PropertiesEditorClass::OOClass()), 0);

}   // End of namespace
