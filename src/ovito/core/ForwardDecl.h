////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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

/**
 * \file
 * \brief Contains forward declarations of OVITO's core classes and namespaces.
 */

#pragma once

namespace Ovito {

	class Application;
	class FileHandle;
	class FileManager;
	class ObjectSaveStream;
	class ObjectLoadStream;
	class CompressedTextReader;
	class CompressedTextWriter;
	class VideoEncoder;
	class SftpJob;
	class PromiseBase;
	class FutureBase;
	class Task;
	class TaskManager;
	class TaskWatcher;
	template<typename... R> class Future;
	template<typename... R> class SharedFuture;
	template<typename... R> class Promise;
	template<class Tuple> class TaskWithStorage;
	using TaskPtr = std::shared_ptr<Task>;
	class MainThreadOperation;
	class UserInterface;
	class TriMeshObject;
	class TriMeshVis;
	class Controller;
	class AnimationSettings;
	class LookAtController;
	class KeyframeController;
	class PRSTransformationController;
	class Plugin;
	class PluginManager;
	class ApplicationService;
	class NativePlugin;
	class OvitoObject;
	class OvitoClass;
	using OvitoClassPtr = const OvitoClass*;
	template<class T> class OORef;
	class CloneHelper;
	class RefMaker;
	class RefMakerClass;
	class RefTarget;
	class PropertyFieldDescriptor;
	class PropertyFieldBase;
	template<typename property_data_type, int flags> class RuntimePropertyField;
	template<typename property_data_type, int flags> class PropertyField;
	template<typename T> class ReferenceField;
	template<typename T> class VectorReferenceField;
	class RefTargetExecutor;
	class DataSet;
	class DataSetContainer;
	class ParameterUnit;
	class UndoStack;
	class UndoableOperation;
	class SceneNode;
	class DataObject;
	class DataObjectReference;
	template<class T> class DataOORef;
	using ConstDataObjectRef = DataOORef<const DataObject>;
	template<typename DataObjectPtr> class DataObjectPathTemplate;
	using DataObjectPath = DataObjectPathTemplate<DataObject*>;
	using ConstDataObjectPath = DataObjectPathTemplate<const DataObject*>;
	using ConstDataObjectRefPath = DataObjectPathTemplate<ConstDataObjectRef>;
	class TransformedDataObject;
	class AttributeDataObject;
	class RootSceneNode;
	class DataBuffer;
	using DataBufferPtr = DataOORef<DataBuffer>;
	using ConstDataBufferPtr = DataOORef<const DataBuffer>;
	class SelectionSet;
	class Modifier;
	class ModifierClass;
	using ModifierClassPtr = const ModifierClass*;
	class ModifierGroup;
	class ModifierApplication;
	class PipelineSceneNode;
	class PipelineFlowState;
	class DataCollection;
	class PipelineObject;
	class PipelineCache;
	class PipelineEvaluationRequest;
	class PipelineEvaluationFuture;
	class CachingPipelineObject;
	class DataVis;
	class TransformingDataVis;
	class StaticSource;
	class ModifierEvaluationRequest;
	using ModifierInitializationRequest = ModifierEvaluationRequest;
	class ModifierDelegate;
	class DelegatingModifier;
	class MultiDelegatingModifier;
	class AsynchronousModifier;
	class AsynchronousDelegatingModifier;
	class AbstractCameraObject;
	class SceneRenderer;
	class ObjectPickInfo;
	class ViewportPickResult;
	class RenderSettings;
	class FrameBuffer;
	class CylinderPrimitive;
	class ImagePrimitive;
	class LinePrimitive;
	class MarkerPrimitive;
	class MeshPrimitive;
	class ParticlePrimitive;
	class TextPrimitive;
	class Viewport;
	class ViewportConfiguration;
	class ViewportLayoutCell;
	class ViewportSettings;
	struct ViewProjectionParameters;
	class ViewportOverlay;
	class ViewportGizmo;
	class ViewportWindowInterface;
	class FileImporter;
	class FileImporterClass;
	class FileExporter;
	class FileExporterClass;
	class FileSource;
	class FileSourceImporter;

	class ViewportInputManager;   // Note: This class is defined in another plugin module (GuiBase).
	class ActionManager;          // Note: This class is defined in another plugin module (GuiBase).
}
