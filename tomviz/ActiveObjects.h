/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizActiveObjects_h
#define tomvizActiveObjects_h

#include <QObject>
#include <QPointer>

#include "DataSource.h"
#include "Enums.h"
#include "Module.h"
#include "MoleculeSource.h"
#include "Operator.h"
#include "OperatorResult.h"

class pqRenderView;
class pqView;
class vtkSMSessionProxyManager;
class vtkSMViewProxy;

namespace tomviz {

class Pipeline;

/// ActiveObjects keeps track of active objects in tomviz.
/// This is similar to pqActiveObjects in ParaView, however it tracks objects
/// relevant to tomviz.
class ActiveObjects : public QObject
{
  Q_OBJECT

public:
  /// Returns reference to the singleton instance.
  static ActiveObjects& instance();

  /// Returns the active view.
  vtkSMViewProxy* activeView() const;

  /// Returns the active view as a pqView object.
  pqView* activePqView() const;

  /// Returns the active pqRenderView object.
  pqRenderView* activePqRenderView() const;

  /// Returns the active data source.
  DataSource* activeDataSource() const { return m_activeDataSource; }

  /// Returns the selected data source, nullptr if no data source is selected.
  DataSource* selectedDataSource() const { return m_selectedDataSource; }

  /// Returns the active data source.
  MoleculeSource* activeMoleculeSource() const
  {
    return m_activeMoleculeSource;
  }

  /// Returns the active transformed data source.
  DataSource* activeTransformedDataSource() const
  {
    return m_activeTransformedDataSource;
  }

  /// Returns the active module.
  Module* activeModule() const { return m_activeModule; }

  /// Returns the active OperatorResult
  OperatorResult* activeOperatorResult() const
  {
    return m_activeOperatorResult;
  }

  /// Returns the vtkSMSessionProxyManager from the active server/session.
  /// Provided here for convenience, since we need to access the proxy manager
  /// often.
  vtkSMSessionProxyManager* proxyManager() const;

  TransformType moveObjectsMode() { return m_moveObjectsMode; }

  /// Returns the active pipelines.
  Pipeline* activePipeline() const;

  /// The "parent" data source is the data source that new operators will be
  /// appended to. i.e. The closes parent of the currently active data source
  /// that is not an "Output" data source.
  DataSource* activeParentDataSource();

public slots:
  /// Set the active view;
  void setActiveView(vtkSMViewProxy*);

  /// Set the active data source.
  void setActiveDataSource(DataSource* source);

  /// Set the selected data source.
  void setSelectedDataSource(DataSource* source);

  /// Set the active molecule source
  void setActiveMoleculeSource(MoleculeSource* source);

  /// Set the active transformed data source.
  void setActiveTransformedDataSource(DataSource* source);

  /// Set the active module.
  void setActiveModule(Module* module);

  /// Set the active operator result.
  void setActiveOperatorResult(OperatorResult* result);

  /// Set the active operator.
  void setActiveOperator(Operator* op);

  /// Create a render view if needed.
  void createRenderViewIfNeeded();

  /// Set first existing render view to be active.
  void setActiveViewToFirstRenderView();

  /// Renders all views.
  void renderAllViews();

  /// Set the active mode (true for the mode where objects can
  /// be moved via MoveActiveObject)
  void setMoveObjectsMode(TransformType transform);

signals:
  /// Fired whenever the active view changes.
  void viewChanged(vtkSMViewProxy*);

  /// Fired whenever the active data source changes (or changes type).
  void dataSourceChanged(DataSource*);

  /// Fired whenever the data source is activated, i.e. selected in the
  /// pipeline.
  void dataSourceActivated(DataSource*);

  /// Fired whenever the data source is activated, i.e. selected in the
  /// pipeline. This signal emits the transformed data source.
  void transformedDataSourceActivated(DataSource*);

  /// Fired whenever the active module changes.
  void moleculeSourceChanged(MoleculeSource*);

  /// Fired whenever a module is activated, i.e. selected in the pipeline.
  void moleculeSourceActivated(MoleculeSource*);

  /// Fired whenever the active module changes.
  void moduleChanged(Module*);

  /// Fired whenever a module is activated, i.e. selected in the pipeline.
  void moduleActivated(Module*);

  /// Fired whenever the active operator changes.
  void operatorChanged(Operator*);

  /// Fired whenever an operator is activated, i.e. selected in the pipeline.
  void operatorActivated(Operator*);

  /// Fired whenever the active operator changes.
  void resultChanged(OperatorResult*);

  /// Fired whenever an OperatorResult is activated.
  void resultActivated(OperatorResult*);

  /// Fired when the mode changes
  void moveObjectsModeChanged(TransformType transform);

  /// Fired whenever the color map has changed
  void colorMapChanged(DataSource*);

  /// Fired to set image viewer mode
  void setImageViewerMode(bool b);

private slots:
  void viewChanged(pqView*);
  void dataSourceRemoved(DataSource*);
  void moleculeSourceRemoved(MoleculeSource*);
  void moduleRemoved(Module*);
  void dataSourceChanged();

protected:
  ActiveObjects();
  ~ActiveObjects() override;

  QPointer<DataSource> m_activeDataSource = nullptr;
  QPointer<DataSource> m_activeTransformedDataSource = nullptr;
  QPointer<DataSource> m_selectedDataSource = nullptr;
  DataSource::DataSourceType m_activeDataSourceType = DataSource::Volume;
  QPointer<DataSource> m_activeParentDataSource = nullptr;
  QPointer<MoleculeSource> m_activeMoleculeSource = nullptr;
  QPointer<Module> m_activeModule = nullptr;
  QPointer<Operator> m_activeOperator = nullptr;
  QPointer<OperatorResult> m_activeOperatorResult = nullptr;
  TransformType m_moveObjectsMode = TransformType::None;

private:
  Q_DISABLE_COPY(ActiveObjects)
};
} // namespace tomviz

#endif
