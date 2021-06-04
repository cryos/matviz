/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorPythonWrapper.h"
#include "PybindVTKTypeCaster.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "core/DataSourceBase.h"

#include "PipelineStateManager.h"
#include "vtkImageData.h"

namespace py = pybind11;

using tomviz::DataSourceBase;

PYBIND11_VTK_TYPECASTER(vtkImageData)

PYBIND11_PLUGIN(_wrapping)
{
  py::module m("_wrapping", "tomviz wrapped classes");

  py::class_<OperatorPythonWrapper>(m, "OperatorPythonWrapper")
    .def(py::init([](void* op) { return new OperatorPythonWrapper(op); }))
    .def_property_readonly("canceled", &OperatorPythonWrapper::canceled)
    .def_property_readonly("done", &OperatorPythonWrapper::done)
    .def_property("progress_maximum",
                  &OperatorPythonWrapper::totalProgressSteps,
                  &OperatorPythonWrapper::setTotalProgressSteps)
    .def_property("progress_value", &OperatorPythonWrapper::progressStep,
                  &OperatorPythonWrapper::setProgressStep)
    .def_property("progress_message", &OperatorPythonWrapper::progressMessage,
                  &OperatorPythonWrapper::setProgressMessage)
    .def_property("progress_data", &OperatorPythonWrapper::progressData,
                  &OperatorPythonWrapper::setProgressData);

  py::class_<tomviz::DataSourceBase>(m, "DataSource")
    .def_property_readonly("dark_data", &DataSourceBase::darkData,
                           "Get the dark image data")
    .def_property_readonly("white_data", &DataSourceBase::whiteData,
                           "Get the white image data");

  py::class_<PipelineStateManager>(m, "PipelineStateManagerBase")
    .def(py::init())
    .def("serialize", &PipelineStateManager::serialize)
    .def("load", &PipelineStateManager::load)
    .def("module_json", &PipelineStateManager::modulesJson)
    .def("operator_json", &PipelineStateManager::operatorsJson)
    .def("serialize_op", &PipelineStateManager::serializeOperator)
    .def("serialize_module", &PipelineStateManager::serializeModule)
    .def("serialize_datasource", &PipelineStateManager::serializeDataSource)
    .def("update_op", &PipelineStateManager::updateOperator)
    .def("update_module", &PipelineStateManager::updateModule)
    .def("update_datasource", &PipelineStateManager::updateDataSource)
    .def("modified", &PipelineStateManager::modified)
    .def("add_module", &PipelineStateManager::addModule)
    .def("add_operator", &PipelineStateManager::addOperator)
    .def("add_datasource", &PipelineStateManager::addDataSource)
    .def("remove_operator", &PipelineStateManager::removeOperator)
    .def("remove_module", &PipelineStateManager::removeModule)
    .def("remove_datasource", &PipelineStateManager::removeDataSource)
    .def("enable_sync_to_python", &PipelineStateManager::enableSyncToPython)
    .def("disable_sync_to_python", &PipelineStateManager::disableSyncToPython)
    .def("pause_pipeline", &PipelineStateManager::pausePipeline)
    .def("resume_pipeline", &PipelineStateManager::resumePipeline)
    .def("execute_pipeline", &PipelineStateManager::executePipeline)
    .def("pipeline_paused", &PipelineStateManager::pipelinePaused);

  return m.ptr();
}
