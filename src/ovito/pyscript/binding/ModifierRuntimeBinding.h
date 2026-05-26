#pragma once

#include <ovito/pyscript/PyScript.h>
#include <ovito/core/dataset/data/DataCollection.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>

namespace PyScript {

namespace py = pybind11;
using namespace Ovito;

void defineModifierRuntimeBindings(py::module m);

quint64 currentPythonModifierCancelEpoch() noexcept;
void advancePythonModifierCancelEpoch() noexcept;

py::object createModifierDataWrapper(DataOORef<const DataCollection> data, bool writable, OOWeakRef<const PipelineNode> createdByNode = {});
py::object createModifierUpstreamWrapper(OOWeakRef<const PipelineNode> upstreamNode, int currentFrame, bool interactiveMode);
py::object createModifierProgressWrapper(std::shared_ptr<TaskProgress> progress, std::function<void(const QString&)> statusCallback = {}, std::function<bool()> cancelRequestedCallback = {});
py::dict getModifierCache(quintptr modifierKey, quint64 generation);

} // namespace PyScript
