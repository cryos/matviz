/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "HistogramWidget.h"

#include "ActiveObjects.h"
#include "BrightnessContrastWidget.h"
#include "ColorMap.h"
#include "ColorMapSettingsWidget.h"
#include "DataSource.h"
#include "DoubleSliderWidget.h"
#include "ModuleContour.h"
#include "ModuleManager.h"
#include "PresetDialog.h"
#include "QVTKGLWidget.h"
#include "Utilities.h"

#include "vtkChartHistogramColorOpacityEditor.h"

#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkControlPointsItem.h>
#include <vtkDataArray.h>
#include <vtkDiscretizableColorTransferFunction.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderWindow.h>
#include <vtkTable.h>
#include <vtkVector.h>

#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <pqView.h>

#include <vtkSMPropertyHelper.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkType.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <QDebug>

namespace tomviz {

HistogramWidget::HistogramWidget(QWidget* parent)
  : QWidget(parent), m_qvtk(new QVTKGLWidget(this))
{
  // Set up our little chart.
  m_histogramView->SetRenderWindow(m_qvtk->renderWindow());
  m_histogramView->SetInteractor(m_qvtk->interactor());
  m_histogramView->GetScene()->AddItem(m_histogramColorOpacityEditor);

  // Connect events from the histogram color/opacity editor.
  m_eventLink->Connect(m_histogramColorOpacityEditor,
                       vtkCommand::CursorChangedEvent, this,
                       SLOT(histogramClicked(vtkObject*)));
  m_eventLink->Connect(m_histogramColorOpacityEditor,
                       vtkCommand::EndEvent, this,
                       SLOT(onScalarOpacityFunctionChanged()));
  m_eventLink->Connect(m_histogramColorOpacityEditor,
                       vtkControlPointsItem::CurrentPointEditEvent, this,
                       SLOT(onCurrentPointEditEvent()));

  auto hLayout = new QHBoxLayout(this);
  hLayout->addWidget(m_qvtk);
  auto vLayout = new QVBoxLayout;
  hLayout->addLayout(vLayout);
  hLayout->setContentsMargins(0, 0, 5, 0);

  vLayout->setContentsMargins(0, 0, 0, 0);
  vLayout->addStretch(1);

  auto button = new QToolButton;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqResetRange.svg"));
  button->setToolTip("Reset data range");
  connect(button, SIGNAL(clicked()), this, SLOT(onResetRangeClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/icons/pqResetRangeCustom.png"));
  button->setToolTip("Specify data range");
  connect(button, SIGNAL(clicked()), this, SLOT(onCustomRangeClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqInvert.svg"));
  button->setToolTip("Invert color map");
  connect(button, SIGNAL(clicked()), this, SLOT(onInvertClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  m_colorMapSettingsButton = button;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqAdvanced.svg"));
  button->setToolTip("Edit color map settings");
  connect(button, &QToolButton::clicked, this,
          &HistogramWidget::onColorMapSettingsClicked);
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqFavorites.svg"));
  button->setToolTip("Choose preset color map");
  connect(button, SIGNAL(clicked()), this, SLOT(onPresetClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  m_savePresetButton = button;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqSave.svg"));
  button->setToolTip("Save current color map as a preset");
  button->setEnabled(false);
  connect(button, SIGNAL(clicked()), this, SLOT(onSaveToPresetClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  m_colorLegendToolButton = button;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqScalarBar.svg"));
  button->setToolTip("Show color legend in the 3D window");
  button->setEnabled(false);
  button->setCheckable(true);
  connect(button, SIGNAL(toggled(bool)), this,
          SIGNAL(colorLegendToggled(bool)));
  button->setChecked(false);
  vLayout->addWidget(button);

  button = new QToolButton;
  m_brightnessAndContrastButton = button;
  button->setIcon(QIcon(":/icons/greybar.png"));
  button->setToolTip("Brightness and Contrast");
  button->setEnabled(false);
  connect(button, &QToolButton::clicked, this,
          &HistogramWidget::onBrightnessAndContrastClicked);
  vLayout->addWidget(button);

  vLayout->addStretch(1);

  connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
          this, SLOT(updateUI()));
  connect(&ActiveObjects::instance(),
          QOverload<DataSource*>::of(&ActiveObjects::dataSourceChanged), this,
          &HistogramWidget::updateColorMapDialogs);
  connect(&ModuleManager::instance(), SIGNAL(dataSourceRemoved(DataSource*)),
	  this, SLOT(updateUI()));
  connect(this, SIGNAL(colorMapUpdated()), this, SLOT(updateUI()));

  setLayout(hLayout);
}

HistogramWidget::~HistogramWidget() = default;

void HistogramWidget::setLUT(vtkDiscretizableColorTransferFunction* lut)
{
  if (m_LUT != lut) {
    if (m_LUT) {
      m_eventLink->Disconnect(m_LUT, vtkCommand::ModifiedEvent, this,
                              SLOT(onColorFunctionChanged()));
    }
    if (m_scalarOpacityFunction) {
      m_eventLink->Disconnect(m_scalarOpacityFunction,
                              vtkCommand::ModifiedEvent, this,
                              SLOT(onScalarOpacityFunctionChanged()));
    }
    m_LUT = lut;
    m_scalarOpacityFunction = m_LUT->GetScalarOpacityFunction();

    m_eventLink->Connect(m_LUT, vtkCommand::ModifiedEvent, this,
                         SLOT(onColorFunctionChanged()));
    m_eventLink->Connect(m_scalarOpacityFunction, vtkCommand::ModifiedEvent,
                         this, SLOT(onScalarOpacityFunctionChanged()));

    onColorFunctionChanged();
    resetAutoContrastState();
    emit colorMapUpdated();
  }

  updateColorMapDialogs();
}

void HistogramWidget::setLUTProxy(vtkSMProxy* proxy)
{
  if (proxy && m_LUTProxy != proxy) {
    m_LUTProxy = proxy;
    auto lut =
      vtkDiscretizableColorTransferFunction::SafeDownCast(
        proxy->GetClientSideObject());
    setLUT(lut);

    auto view = ActiveObjects::instance().activeView();

    // Update widget to reflect scalar bar visibility.
    if (m_LUTProxy) {
      auto sbProxy = getScalarBarRepresentation(view);
      if (sbProxy) {
        bool visible =
          vtkSMPropertyHelper(sbProxy, "Visibility").GetAsInt() == 1;
        m_colorLegendToolButton->setChecked(visible);
      }
    }
  }
}

void HistogramWidget::updateLUTProxy()
{
  // Update the LUT proxy from the LUT object
  auto* lutProxy = m_LUTProxy.Get();
  auto* lut = m_LUT.Get();

  if (!lutProxy || !lut) {
    return;
  }

  auto numNodes = lut->GetSize();
  auto* dataArray = lut->GetDataPointer();
  int nodeStride = 4;

  auto colorSpace = lut->GetColorSpace();

  auto* controlPointsProperty = lutProxy->GetProperty("RGBPoints");
  vtkSMPropertyHelper(controlPointsProperty)
    .Set(dataArray, numNodes * nodeStride);

  auto* colorSpaceProperty = lutProxy->GetProperty("ColorSpace");
  vtkSMPropertyHelper(colorSpaceProperty).Set(colorSpace);
}

void HistogramWidget::updateColorMapDialogs()
{
  auto* ds = ActiveObjects::instance().activeDataSource();
  auto* lut = m_LUT.Get();

  if (m_colorMapSettingsWidget) {
    m_colorMapSettingsWidget->setLut(lut);
    m_colorMapSettingsWidget->updateGui();
  }

  if (m_brightnessContrastWidget) {
    m_brightnessContrastWidget->setDataSource(ds);
    m_brightnessContrastWidget->setLut(lut);
    m_brightnessContrastWidget->updateGui();
  }
}

void HistogramWidget::setInputData(vtkTable* table, const char* x_,
                                   const char* y_)
{
  m_inputData = table;
  m_histogramColorOpacityEditor->SetHistogramInputData(table, x_, y_);
  m_histogramColorOpacityEditor->SetOpacityFunction(m_scalarOpacityFunction);
  if (m_LUT && table) {
    m_histogramColorOpacityEditor->SetScalarVisibility(true);
    m_histogramColorOpacityEditor->SetColorTransferFunction(m_LUT);
    m_histogramColorOpacityEditor->SelectColorArray("image_extents");
  }
  m_histogramView->Render();
}

vtkSMProxy* HistogramWidget::getScalarBarRepresentation(vtkSMProxy* view)
{
  if (!view) {
    return nullptr;
  }

  auto tferProxy = vtkSMTransferFunctionProxy::SafeDownCast(m_LUTProxy);
  if (!tferProxy) {
    return nullptr;
  }

  auto sbProxy = tferProxy->FindScalarBarRepresentation(view);
  if (!sbProxy) {
    // No scalar bar representation exists yet, create it and initialize it
    // with some default settings.
    vtkNew<vtkSMTransferFunctionManager> tferManager;
    sbProxy = tferManager->GetScalarBarRepresentation(m_LUTProxy, view);
    vtkSMPropertyHelper(sbProxy, "Visibility").Set(0);
    vtkSMPropertyHelper(sbProxy, "Enabled").Set(0);
    vtkSMPropertyHelper(sbProxy, "Title").Set("");
    vtkSMPropertyHelper(sbProxy, "ComponentTitle").Set("");
    vtkSMPropertyHelper(sbProxy, "RangeLabelFormat").Set("%g");
    sbProxy->UpdateVTKObjects();
  }

  return sbProxy;
}

void HistogramWidget::onColorFunctionChanged()
{
  if (m_updatingColorFunction) {
    // Avoid infinite recursion
    return;
  }
  m_updatingColorFunction = true;

  updateLUTProxy();
  if (m_LUT) {
    m_LUT->Build();
    renderViews();
    emit colorMapUpdated();
  }

  m_updatingColorFunction = false;
}

void HistogramWidget::onScalarOpacityFunctionChanged()
{
  // Update rendered views of the data.
  ActiveObjects::instance().renderAllViews();

  // Update the histogram
  m_histogramView->GetRenderWindow()->Render();

  // Update the scalar opacity function proxy as it does not update its
  // internal state when the VTK object changes.
  if (!m_LUTProxy) {
    return;
  }

  auto opacityMapProxy =
    vtkSMPropertyHelper(m_LUTProxy, "ScalarOpacityFunction", true).GetAsProxy();
  if (!opacityMapProxy) {
    return;
  }

  vtkSMPropertyHelper pointsHelper(opacityMapProxy, "Points");
  auto opacityMapObject = opacityMapProxy->GetClientSideObject();
  auto pwf = vtkPiecewiseFunction::SafeDownCast(opacityMapObject);
  if (pwf) {
    pointsHelper.SetNumberOfElements(4 * pwf->GetSize());
    for (int i = 0; i < pwf->GetSize(); ++i) {
      double value[4];
      pwf->GetNodeValue(i, value);
      pointsHelper.Set(4 * i + 0, value[0]);
      pointsHelper.Set(4 * i + 1, value[1]);
      pointsHelper.Set(4 * i + 2, value[2]);
      pointsHelper.Set(4 * i + 3, value[3]);
    }
  }

  emit opacityChanged();
}

void HistogramWidget::onCurrentPointEditEvent()
{
  double rgb[3];
  if (m_histogramColorOpacityEditor->GetCurrentControlPointColor(rgb)) {
    QColor color =
      QColorDialog::getColor(QColor::fromRgbF(rgb[0], rgb[1], rgb[2]), this,
                             "Select Color for Control Point");
    if (color.isValid()) {
      rgb[0] = color.redF();
      rgb[1] = color.greenF();
      rgb[2] = color.blueF();
      m_histogramColorOpacityEditor->SetCurrentControlPointColor(rgb);
    }
  }
  ActiveObjects::instance().renderAllViews();
}

void HistogramWidget::histogramClicked(vtkObject*)
{
  auto activeDataSource = ActiveObjects::instance().activeDataSource();
  Q_ASSERT(activeDataSource);

  auto view = ActiveObjects::instance().activeView();
  if (!view) {
    return;
  }

  // Use active ModuleContour is possible. Otherwise, find the first existing
  // ModuleContour instance or just create a new one, if none exists.
  typedef ModuleContour ModuleContourType;

  auto isoValue = m_histogramColorOpacityEditor->GetContourValue();
  auto contour =
    qobject_cast<ModuleContourType*>(ActiveObjects::instance().activeModule());
  if (!contour) {
    QList<ModuleContourType*> contours =
      ModuleManager::instance().findModules<ModuleContourType*>(
        activeDataSource, view);
    if (contours.size() == 0) {
      auto res = createContourDialog(isoValue);
      if (!res) {
        return;
      }
      contour = qobject_cast<ModuleContourType*>(
        ModuleManager::instance().createAndAddModule("Contour",
                                                     activeDataSource, view));
    } else {
      contour = contours[0];
    }
    ActiveObjects::instance().setActiveModule(contour);
  }
  Q_ASSERT(contour);
  contour->setIsoValue(isoValue);
  tomviz::convert<pqView*>(view)->render();
}

bool HistogramWidget::createContourDialog(double& isoValue)
{
  QSettings* settings = pqApplicationCore::instance()->settings();
  bool autoAccept =
    settings->value("ContourSettings.AutoAccept", false).toBool();
  if (autoAccept) {
    return true;
  }

  auto ds = ActiveObjects::instance().activeDataSource();
  if (!ds) {
    return false;
  }

  QDialog dialog;
  dialog.setFixedWidth(300);
  dialog.setMaximumHeight(50);
  dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  QVBoxLayout vLayout;
  dialog.setLayout(&vLayout);
  dialog.setWindowTitle(tr("New Iso Contour"));

  QFormLayout formLayout;
  vLayout.addLayout(&formLayout);

  // Get the range of the dataset
  double range[2];
  ds->getRange(range);

  DoubleSliderWidget w(true);
  w.setMinimum(range[0]);
  w.setMaximum(range[1]);

  // We want to round this to two decimal places
  isoValue = QString::number(isoValue, 'f', 2).toDouble();
  w.setValue(isoValue);

  w.setLineEditWidth(50);

  formLayout.addRow("Iso value", &w);

  QCheckBox dontAskAgain("Don't ask again");
  formLayout.addRow(&dontAskAgain);

  QDialogButtonBox buttons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  vLayout.addWidget(&buttons);

  connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto r = dialog.exec();

  if (r == QDialog::Accepted) {
    if (dontAskAgain.isChecked()) {
      settings->setValue("ContourSettings.AutoAccept", true);
    }
    isoValue = w.value();
    return true;
  } else {
    return false;
  }
}

void HistogramWidget::onResetRangeClicked()
{
  resetRange();
}

void HistogramWidget::resetRange()
{
  auto activeDataSource = ActiveObjects::instance().activeDataSource();
  if (!activeDataSource)
    return;

  double range[2];
  activeDataSource->getRange(range);
  resetRange(range);
}

void HistogramWidget::resetRange(double range[2])
{
  resetAutoContrastState();
  rescaleTransferFunction(m_LUTProxy, range[0], range[1]);
  renderViews();
}

void HistogramWidget::onCustomRangeClicked()
{
  // Get the max allowable range
  auto activeDataSource = ActiveObjects::instance().activeDataSource();
  if (!activeDataSource)
    return;

  double maxRange[2];
  activeDataSource->getRange(maxRange);

  // Get the type of the active scalar
  auto scalar = activeDataSource->activeScalars();
  auto array = activeDataSource->getScalarsArray(scalar);
  auto dataType = array->GetDataType();
  int precision = 0;
  if (dataType == VTK_FLOAT || dataType == VTK_DOUBLE) {
    precision = 6;
  }

  // Get the current range
  vtkVector2d currentRange;
  vtkDiscretizableColorTransferFunction* discFunc =
    vtkDiscretizableColorTransferFunction::SafeDownCast(
      m_LUTProxy->GetClientSideObject());
  if (!discFunc) {
    return;
  }
  discFunc->GetRange(currentRange.GetData());

  QDialog dialog;
  QVBoxLayout vLayout;
  QHBoxLayout hLayout;
  vLayout.addLayout(&hLayout);

  // Fix the size of this window
  vLayout.setSizeConstraint(QLayout::SetFixedSize);
  hLayout.setSizeConstraint(QLayout::SetFixedSize);

  dialog.setLayout(&vLayout);
  dialog.setWindowTitle(tr("Specify Data Range"));

  QDoubleSpinBox bottom;
  bottom.setRange(maxRange[0], maxRange[1]);
  bottom.setValue(currentRange[0]);
  bottom.setDecimals(precision);
  bottom.setFixedSize(bottom.sizeHint());
  bottom.setToolTip("Min: " + QString::number(maxRange[0]));
  hLayout.addWidget(&bottom);

  QLabel dash("-");
  dash.setAlignment(Qt::AlignHCenter);
  dash.setAlignment(Qt::AlignVCenter);
  hLayout.addWidget(&dash);

  QDoubleSpinBox top;
  top.setRange(maxRange[0], maxRange[1]);
  top.setValue(currentRange[1]);
  top.setDecimals(precision);
  top.setFixedSize(top.sizeHint());
  top.setToolTip("Max: " + QString::number(maxRange[1]));
  hLayout.addWidget(&top);

  // Make sure the bottom isn't higher than the top, and the
  // top isn't higher than the bottom.
  connect(&bottom,
          static_cast<void (QDoubleSpinBox::*)(double)>(
            &QDoubleSpinBox::valueChanged),
          &top, &QDoubleSpinBox::setMinimum);
  connect(&top,
          static_cast<void (QDoubleSpinBox::*)(double)>(
            &QDoubleSpinBox::valueChanged),
          &bottom, &QDoubleSpinBox::setMaximum);

  QDialogButtonBox buttonBox;
  buttonBox.addButton(QDialogButtonBox::Ok);
  buttonBox.addButton(QDialogButtonBox::Cancel);
  connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  vLayout.addWidget(&buttonBox);

  if (dialog.exec() == QDialog::Accepted) {
    resetAutoContrastState();
    rescaleTransferFunction(m_LUTProxy, bottom.value(), top.value());
    renderViews();
  }

  // vLayout should not call 'delete' on hLayout...
  hLayout.setParent(nullptr);
}

void HistogramWidget::onInvertClicked()
{
  removePlaceholderNodes();
  vtkSMTransferFunctionProxy::InvertTransferFunction(m_LUTProxy);
  addPlaceholderNodes();
  resetAutoContrastState();
  renderViews();
  emit colorMapUpdated();
}

void HistogramWidget::onColorMapSettingsClicked()
{
  if (m_colorMapSettingsDialog) {
    // It's already visible
    return;
  }

  m_colorMapSettingsDialog = new QDialog(this);
  auto& dialog = *m_colorMapSettingsDialog;
  dialog.setLayout(new QVBoxLayout);
  dialog.setWindowTitle("Color map settings");

  m_colorMapSettingsWidget = new ColorMapSettingsWidget(m_LUT, this);
  dialog.layout()->addWidget(m_colorMapSettingsWidget);

  dialog.show();

  // Delete the dialog when it is closed
  connect(&dialog, &QDialog::finished, &dialog, &QDialog::deleteLater);
}

void HistogramWidget::showPresetDialog(const QJsonObject& newPreset)
{
  if (m_presetDialog == nullptr) {
    m_presetDialog = new PresetDialog(this);
    QObject::connect(m_presetDialog, &PresetDialog::applyPreset, this,
                     &HistogramWidget::applyCurrentPreset);
  }

  if (!newPreset.isEmpty()) {
    m_presetDialog->addNewPreset(newPreset);
  }

  m_presetDialog->show();
}

void HistogramWidget::onSaveToPresetClicked()
{
  QDialog dialog;
  QVBoxLayout vLayout;
  QHBoxLayout hLayout;

  vLayout.addLayout(&hLayout);
  dialog.setLayout(&vLayout);
  dialog.setWindowTitle(tr("Create Preset"));

  QLineEdit name;
  name.setPlaceholderText("Enter name of new preset");
  hLayout.addWidget(&name);

  QDialogButtonBox buttonBox;
  buttonBox.addButton(QDialogButtonBox::Ok);
  buttonBox.addButton(QDialogButtonBox::Cancel);
  connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  vLayout.addWidget(&buttonBox);

  if (dialog.exec() == QDialog::Accepted) {
    auto newName = name.text();
    vtkSMProxy* lut = m_LUTProxy;
    auto presetInfo = tomviz::serialize(lut);
    auto presetColors = presetInfo["colors"];
    auto colorSpace = presetInfo["colorSpace"];
    QJsonObject newPreset{ { "name", newName },
                           { "colorSpace", colorSpace },
                           { "colors", presetColors },
			   { "default", QJsonValue(false) }
    };
    showPresetDialog(newPreset);
  }
}

void HistogramWidget::onPresetClicked()
{
  showPresetDialog(QJsonObject());
}

void HistogramWidget::resetAutoContrastState()
{
  m_currentAutoContrastThreshold = m_defaultAutoContrastThreshold;
}

void HistogramWidget::onBrightnessAndContrastClicked()
{
  if (m_brightnessContrastDialog) {
    // It's already visible
    return;
  }

  m_brightnessContrastDialog = new QDialog(this);
  auto& dialog = *m_brightnessContrastDialog;
  dialog.setLayout(new QVBoxLayout);
  dialog.setWindowTitle("Brightness and Contrast");

  auto* ds = ActiveObjects::instance().activeDataSource();
  m_brightnessContrastWidget = new BrightnessContrastWidget(ds, m_LUT, this);
  dialog.layout()->addWidget(m_brightnessContrastWidget);

  auto* widget = m_brightnessContrastWidget.data();
  connect(widget, &BrightnessContrastWidget::autoPressed, this,
          QOverload<>::of(&HistogramWidget::autoAdjustContrast));
  connect(widget, &BrightnessContrastWidget::resetPressed, this,
          QOverload<>::of(&HistogramWidget::resetRange));

  dialog.show();

  // Delete the dialog when it is closed
  connect(&dialog, &QDialog::finished, &dialog, &QDialog::deleteLater);
}

void HistogramWidget::autoAdjustContrast()
{
  auto* table = m_inputData.Get();

  // For now, auto adjust contrast for the whole data source. We can
  // also do it for individual slices in the future (in which case
  // we should generate a histogram for an individual slice).
  auto* ds = ActiveObjects::instance().activeDataSource();

  if (!table || !ds || !m_LUT) {
    return;
  }

  auto* imageData = ds->imageData();
  auto* histogram =
    vtkDataArray::SafeDownCast(table->GetColumnByName("image_pops"));
  auto* extents =
    vtkDataArray::SafeDownCast(table->GetColumnByName("image_extents"));

  if (!imageData || !histogram || !extents ||
      extents->GetNumberOfTuples() < 2) {
    return;
  }

  autoAdjustContrast(histogram, extents, imageData);
}

void HistogramWidget::autoAdjustContrast(vtkDataArray* histogram,
                                         vtkDataArray* extents,
                                         vtkImageData* imageData)
{
  // Gather some information
  double range[2];
  DataSource::getRange(imageData, range);

  int dims[3];
  imageData->GetDimensions(dims);

  auto voxelCount = static_cast<size_t>(dims[0]) * dims[1] * dims[2];
  auto numBins = histogram->GetNumberOfTuples();
  auto histMin = extents->GetTuple1(0);
  auto binSize = extents->GetTuple1(1) - histMin;
  auto& autoThreshold = m_currentAutoContrastThreshold;

  // Perform the operation as ImageJ does it
  auto limit = voxelCount / 10;
  if (autoThreshold < 10) {
    autoThreshold = m_defaultAutoContrastThreshold;
  } else {
    autoThreshold /= 2;
  }
  auto threshold = voxelCount / autoThreshold;

  int i;
  for (i = 0; i < numBins; ++i) {
    double count = histogram->GetTuple1(i);
    count = count > limit ? 0 : count;
    if (count > threshold) {
      break;
    }
  }
  int hmin = i;

  for (i = 255; i >= 0; --i) {
    double count = histogram->GetTuple1(i);
    count = count > limit ? 0 : count;
    if (count > threshold) {
      break;
    }
  }
  int hmax = i;

  if (hmax < hmin) {
    resetRange(range);
    return;
  }

  double min = histMin + hmin * binSize;
  double max = histMin + hmax * binSize;
  if (min == max) {
    min = range[0];
    max = range[1];
  }
  rescaleTransferFunction(m_LUTProxy, min, max);
}

void HistogramWidget::applyCurrentPreset()
{
  vtkSMProxy* lut = m_LUTProxy;

  if (!lut) {
    return;
  }

  auto current = m_presetDialog->presetName();
  ColorMap::instance().applyPreset(current, lut);

  renderViews();
  resetAutoContrastState();
  emit colorMapUpdated();

  updateColorMapDialogs();
}

void HistogramWidget::updateUI()
{
  auto view = ActiveObjects::instance().activeView();

  // Update widget to reflect scalar bar visibility.
  if (m_LUTProxy) {
    auto sbProxy = getScalarBarRepresentation(view);
    if (view && sbProxy) {
      QSignalBlocker blocker1(m_colorLegendToolButton);
      QSignalBlocker blocker2(m_colorMapSettingsButton);
      QSignalBlocker blocker3(m_savePresetButton);
      QSignalBlocker blocker4(m_brightnessAndContrastButton);
      m_colorLegendToolButton->setEnabled(true);
      m_colorMapSettingsButton->setEnabled(true);
      m_colorLegendToolButton->setChecked(
        vtkSMPropertyHelper(sbProxy, "Visibility").GetAsInt() == 1);
      m_savePresetButton->setEnabled(true);
      m_brightnessAndContrastButton->setEnabled(true);
    }
  }

  auto dataSource = ActiveObjects::instance().activeDataSource();
  if (!dataSource) {
    QSignalBlocker blocker1(m_colorLegendToolButton);
    QSignalBlocker blocker2(m_colorMapSettingsButton);
    QSignalBlocker blocker3(m_savePresetButton);
    QSignalBlocker blocker4(m_brightnessAndContrastButton);
    m_colorLegendToolButton->setEnabled(false);
    m_colorMapSettingsButton->setEnabled(false);
    m_savePresetButton->setEnabled(false);
    m_brightnessAndContrastButton->setEnabled(false);
  }
}

void HistogramWidget::renderViews()
{
  pqView* view =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}

void HistogramWidget::rescaleTransferFunction(vtkSMProxy* lutProxy, double min,
                                              double max)
{
  auto opacityMap =
    vtkSMPropertyHelper(m_LUTProxy, "ScalarOpacityFunction").GetAsProxy();

  removePlaceholderNodes();
  vtkSMTransferFunctionProxy::RescaleTransferFunction(lutProxy, min, max);
  vtkSMTransferFunctionProxy::RescaleTransferFunction(opacityMap, min, max);
  addPlaceholderNodes();

  emit colorMapUpdated();
}

void HistogramWidget::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  this->renderViews();
}

void HistogramWidget::addPlaceholderNodes()
{
  auto* lut = m_LUT.Get();
  auto* opacity = m_scalarOpacityFunction.Get();
  auto* ds = ActiveObjects::instance().activeDataSource();

  if (lut && ds) {
    tomviz::addPlaceholderNodes(lut, ds);
  }

  if (opacity && ds) {
    tomviz::addPlaceholderNodes(opacity, ds);
  }
}

void HistogramWidget::removePlaceholderNodes()
{
  auto* lut = m_LUT.Get();
  auto* opacity = m_scalarOpacityFunction.Get();

  if (lut) {
    tomviz::removePlaceholderNodes(lut);
  }

  if (opacity) {
    tomviz::removePlaceholderNodes(opacity);
  }
}

} // namespace tomviz
