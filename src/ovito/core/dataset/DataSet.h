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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/oo/PropertyField.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/utilities/MixedKeyCache.h>

namespace Ovito {

/**
 * \brief Stores the current program state including the list of viewports, the scene, viewport configuration,
 *        render settings etc.
 *
 * A DataSet represents the state of the current user session.
 * It can be completely saved to a file (.ovito suffix) and loaded again at a later time.
 *
 * The DataSet class consists of various sub-objects that store different aspects. The
 * ViewportConfiguration object returned by viewportConfig(), for example, holds the list
 * of viewports.
 */
class OVITO_CORE_EXPORT DataSet final : public RefTarget
{
	OVITO_CLASS(DataSet)

#ifdef OVITO_QML_GUI
	Q_PROPERTY(Ovito::ViewportConfiguration* viewportConfiguration READ viewportConfig WRITE setViewportConfig NOTIFY viewportConfigReplaced)
#endif

public:

	/// \brief Constructs an empty dataset.
	Q_INVOKABLE DataSet(ObjectCreationParams params);

	/// \brief Destructor.
	virtual ~DataSet();

	/// \brief Returns the path where this dataset is stored on disk.
	/// \return The location where the dataset is stored or will be stored on disk.
	const QString& filePath() const { return _filePath; }

	/// \brief Sets the path where this dataset is stored.
	/// \param path The new path (should be absolute) where the dataset will be stored.
	void setFilePath(const QString& path) {
		if(path != _filePath) {
			_filePath = path;
			Q_EMIT filePathChanged(_filePath);
		}
	}

	/// \brief Returns the container this dataset belongs to.
	DataSetContainer* container() const;

	/// \brief Rescales the animation keys of all controllers in the scene.
	/// \param oldAnimationInterval The old animation interval, which will be mapped to the new animation interval.
	/// \param newAnimationInterval The new animation interval.
	///
	/// This method calls RefTarget::rescaleTime() for all objects (including animation controllers) in the scene.
	/// For keyed controllers this will rescale the key times of all keys from the
	/// old animation interval to the new interval using a linear mapping.
	///
	/// Keys that lie outside of the old active animation interval will also be rescaled
	/// according to a linear extrapolation.
	///
	/// \undoable
	virtual void rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval) override;

	/// \brief This is the high-level rendering function, which invokes the renderer to generate one or more
	///        output images of the scene. All rendering parameters are specified in the RenderSettings and ViewportConfiguration objects.
	/// \param renderSettings A RenderSettings object that specifies output image size, animation range to render etc.
	/// \param viewportConfiguration The viewport configuration to render.
	/// \param frameBuffer The frame buffer that will receive the rendered image. When rendering an animation
	///        sequence, the buffer will contain only the last rendered frame when the function returns.
	/// \return true on success; false if operation has been canceled by the user.
	/// \throw Exception on error.
	bool renderScene(const RenderSettings& renderSettings, const ViewportConfiguration& viewportConfiguration, FrameBuffer& frameBuffer, MainThreadOperation& operation);

	/// \brief This is the high-level rendering function, which invokes the renderer to generate one or more
	///        output images of the scene. All rendering parameters are specified in the RenderSettings object.
	/// \param renderSettings A RenderSettings object that specifies output image size, animation range to render etc.
	/// \param viewportLayout The viewport layout.
	/// \param frameBuffer The frame buffer that will receive the rendered image. 
	/// \return true on success; false if operation has been canceled by the user.
	/// \throw Exception on error.
	bool renderScene(const RenderSettings& renderSettings, const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, FrameBuffer& frameBuffer, MainThreadOperation& operation);

	/// \brief Saves the dataset to a session state file.
	/// \throw Exception on error.
	///
	/// Note that this method does NOT invoke setFilePath().
	void saveToFile(const QString& filePath, MainThreadOperation operation) const;

	/// \brief Loads the dataset contents from a session state file.
	/// \throw Exception on error.
	///
	/// Note that this method does NOT invoke setFilePath().
	void loadFromFile(const QString& filePath, MainThreadOperation operation);

	/// Provides access to the global data cache used by visualzation elements.
	MixedKeyCache& visCache() { return _visCache; }

Q_SIGNALS:

	/// \brief This signal is emitted whenever the current viewport configuration of this dataset
	///        has been replaced by a new one.
	/// \note This signal is NOT emitted when parameters of the current viewport configuration change.
    void viewportConfigReplaced(ViewportConfiguration* newViewportConfiguration);

	/// \brief This signal is emitted whenever the current render settings of this dataset
	///        have been replaced by new ones.
	/// \note This signal is NOT emitted when parameters of the current render settings object change.
    void renderSettingsReplaced(RenderSettings* newRenderSettings);

	/// \brief This signal is emitted whenever the dataset has been saved under a new file name.
    void filePathChanged(const QString& filePath);

protected:

	/// Is called when a RefTarget referenced by this object has generated an event.
	virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

	/// Is called when the value of a reference field of this RefMaker changes.
	virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private:

	/// Renders a single frame and saves the output file. This is part of the implementation of the renderScene() method.
	bool renderFrame(int frameNumber, const RenderSettings& settings, SceneRenderer& renderer,
			FrameBuffer& frameBuffer, const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, VideoEncoder* videoEncoder, MainThreadOperation& operation);

	/// Returns a viewport configuration that is used as template for new scenes.
	static OORef<ViewportConfiguration> createDefaultViewportConfiguration(ObjectCreationParams params);

private Q_SLOTS:

	/// Is called whenever a different viewport becomes the currently active one.
	void onActiveViewportChanged(Viewport* activeViewport);

private:

	/// The configuration of the interactive viewports in the OVITO desktop application.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<ViewportConfiguration>, viewportConfig, setViewportConfig, PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_ALWAYS_DEEP_COPY | PROPERTY_FIELD_MEMORIZE);

	/// The settings for rendering an output image of the scene.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<RenderSettings>, renderSettings, setRenderSettings, PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_ALWAYS_DEEP_COPY | PROPERTY_FIELD_MEMORIZE);

	/// The file path this DataSet has been saved to.
	QString _filePath;

	/// The DataSetContainer which currently hosts this DataSet.
	QPointer<DataSetContainer> _container;

	/// Data cache used by visualization elements to store rendering primitives.
	MixedKeyCache _visCache;

	friend class DataSetContainer;
};

}	// End of namespace
