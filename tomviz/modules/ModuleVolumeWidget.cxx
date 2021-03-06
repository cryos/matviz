/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleVolumeWidget.h"
#include "Module.h"
#include "ui_LightingParametersForm.h"
#include "ui_ModuleVolumeWidget.h"

#include "vtkVolumeMapper.h"

namespace tomviz {

// If we make this bigger, such as 1000, and we make the max too
// close to the data minimum or the min too close to the data maximum,
// we run into errors like these:
// ( 118.718s) [paraview        ]vtkOpenGLVolumeLookupTa:84    WARN|
// vtkOpenGLVolumeRGBTable (0x55ba0c5cc970): This OpenGL implementation does not
// support the required texture size of 65536, falling back to maximum allowed,
// 32768.This may cause an incorrect lookup table mapping.
static const double RANGE_INCREMENT = 500;

ModuleVolumeWidget::ModuleVolumeWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::ModuleVolumeWidget),
    m_uiLighting(new Ui::LightingParametersForm)
{
  m_ui->setupUi(this);

  QWidget* lightingWidget = new QWidget;
  m_uiLighting->setupUi(lightingWidget);
  QWidget::layout()->addWidget(lightingWidget);
  qobject_cast<QBoxLayout*>(QWidget::layout())->addStretch();

  const int leWidth = 50;
  m_uiLighting->sliAmbient->setLineEditWidth(leWidth);
  m_uiLighting->sliDiffuse->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecular->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecularPower->setLineEditWidth(leWidth);

  m_uiLighting->sliSpecularPower->setMaximum(150);
  m_uiLighting->sliSpecularPower->setMinimum(1);
  m_uiLighting->sliSpecularPower->setResolution(200);

  m_ui->soliditySlider->setLineEditWidth(leWidth);

  QStringList labelsBlending;
  labelsBlending << tr("Composite") << tr("Max") << tr("Min") << tr("Average")
                 << tr("Additive");
  m_ui->cbBlending->addItems(labelsBlending);

  QStringList labelsTransferMode;
  labelsTransferMode << tr("Scalar") << tr("Scalar-Gradient 1D")
                     << tr("Scalar-Gradient 2D");
  m_ui->cbTransferMode->addItems(labelsTransferMode);

  QStringList labelsInterp;
  labelsInterp << tr("Nearest Neighbor") << tr("Linear");
  m_ui->cbInterpolation->addItems(labelsInterp);

  connect(m_ui->cbJittering, SIGNAL(toggled(bool)), this,
          SIGNAL(jitteringToggled(const bool)));
  connect(m_ui->cbBlending, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onBlendingChanged(const int)));
  connect(m_ui->cbInterpolation, SIGNAL(currentIndexChanged(int)), this,
          SIGNAL(interpolationChanged(const int)));
  connect(m_ui->cbTransferMode, SIGNAL(currentIndexChanged(int)), this,
          SIGNAL(transferModeChanged(const int)));
  connect(m_ui->cbMultiVolume, SIGNAL(toggled(bool)), this,
          SIGNAL(allowMultiVolumeToggled(const bool)));
  connect(m_ui->cbMultiVolume, &QCheckBox::toggled, this,
          &ModuleVolumeWidget::setAllowMultiVolume);

  connect(m_ui->useRgbaMapping, &QCheckBox::toggled, this,
          &ModuleVolumeWidget::useRgbaMappingToggled);

  connect(m_ui->rgbaMappingCombineComponents, &QCheckBox::toggled, this,
          &ModuleVolumeWidget::rgbaMappingCombineComponentsToggled);
  connect(m_ui->rgbaMappingComponent, &QComboBox::currentTextChanged, this,
          &ModuleVolumeWidget::rgbaMappingComponentChanged);

  // Using QueuedConnections here to circumvent DoubleSliderWidget->BlockUpdate
  connect(m_ui->sliRgbaMappingMin, &DoubleSliderWidget::valueEdited, this,
          &ModuleVolumeWidget::onRgbaMappingMinChanged, Qt::QueuedConnection);
  connect(m_ui->sliRgbaMappingMax, &DoubleSliderWidget::valueEdited, this,
          &ModuleVolumeWidget::onRgbaMappingMaxChanged, Qt::QueuedConnection);

  connect(m_uiLighting->gbLighting, SIGNAL(toggled(bool)), this,
          SIGNAL(lightingToggled(const bool)));
  connect(m_uiLighting->sliAmbient, SIGNAL(valueEdited(double)), this,
          SIGNAL(ambientChanged(const double)));
  connect(m_uiLighting->sliDiffuse, SIGNAL(valueEdited(double)), this,
          SIGNAL(diffuseChanged(const double)));
  connect(m_uiLighting->sliSpecular, SIGNAL(valueEdited(double)), this,
          SIGNAL(specularChanged(const double)));
  connect(m_uiLighting->sliSpecularPower, SIGNAL(valueEdited(double)), this,
          SIGNAL(specularPowerChanged(const double)));
  connect(m_ui->soliditySlider, SIGNAL(valueEdited(double)), this,
          SIGNAL(solidityChanged(const double)));

  m_ui->groupRgbaMappingRange->setVisible(false);
  m_ui->rgbaMappingComponentLabel->setVisible(false);
  m_ui->rgbaMappingComponent->setVisible(false);
}

ModuleVolumeWidget::~ModuleVolumeWidget() = default;

void ModuleVolumeWidget::setJittering(const bool enable)
{
  m_ui->cbJittering->setChecked(enable);
}

void ModuleVolumeWidget::setBlendingMode(const int mode)
{
  m_uiLighting->gbLighting->setEnabled(usesLighting(mode));
  m_ui->cbBlending->setCurrentIndex(static_cast<int>(mode));
}

void ModuleVolumeWidget::setInterpolationType(const int type)
{
  m_ui->cbInterpolation->setCurrentIndex(type);
}

void ModuleVolumeWidget::setLighting(const bool enable)
{
  m_uiLighting->gbLighting->setChecked(enable);
}

void ModuleVolumeWidget::setAmbient(const double value)
{
  m_uiLighting->sliAmbient->setValue(value);
}

void ModuleVolumeWidget::setDiffuse(const double value)
{
  m_uiLighting->sliDiffuse->setValue(value);
}

void ModuleVolumeWidget::setSpecular(const double value)
{
  m_uiLighting->sliSpecular->setValue(value);
}

void ModuleVolumeWidget::setSpecularPower(const double value)
{
  m_uiLighting->sliSpecularPower->setValue(value);
}

void ModuleVolumeWidget::onBlendingChanged(const int mode)
{
  m_uiLighting->gbLighting->setEnabled(usesLighting(mode));
  emit blendingChanged(mode);
}

bool ModuleVolumeWidget::usesLighting(const int mode) const
{
  if (mode == vtkVolumeMapper::COMPOSITE_BLEND) {
    return true;
  }

  return false;
}

void ModuleVolumeWidget::setTransferMode(const int transferMode)
{
  m_ui->cbTransferMode->setCurrentIndex(transferMode);
}

void ModuleVolumeWidget::setSolidity(const double value)
{
  m_ui->soliditySlider->setValue(value);
}

void ModuleVolumeWidget::setRgbaMappingAllowed(const bool b)
{
  m_ui->useRgbaMapping->setVisible(b);

  if (!b) {
    setUseRgbaMapping(false);
  }
}

void ModuleVolumeWidget::setUseRgbaMapping(const bool b)
{
  m_ui->useRgbaMapping->setChecked(b);
}

void ModuleVolumeWidget::setRgbaMappingMin(const double v)
{
  m_ui->sliRgbaMappingMin->setValue(v);
}

void ModuleVolumeWidget::setRgbaMappingMax(const double v)
{
  m_ui->sliRgbaMappingMax->setValue(v);
}

void ModuleVolumeWidget::setRgbaMappingSliderRange(const double range[2])
{
  double min = range[0];
  double max = range[1];
  m_ui->sliRgbaMappingMin->setMinimum(min);
  m_ui->sliRgbaMappingMin->setMaximum(max);
  m_ui->sliRgbaMappingMax->setMinimum(min);
  m_ui->sliRgbaMappingMax->setMaximum(max);
}

void ModuleVolumeWidget::setRgbaMappingCombineComponents(const bool b)
{
  m_ui->rgbaMappingCombineComponents->setChecked(b);
  m_ui->rgbaMappingComponent->setVisible(!b);
  m_ui->rgbaMappingComponentLabel->setVisible(!b);
}

void ModuleVolumeWidget::setRgbaMappingComponentOptions(
  const QStringList& components)
{
  m_ui->rgbaMappingComponent->clear();
  m_ui->rgbaMappingComponent->addItems(components);
}

void ModuleVolumeWidget::setRgbaMappingComponent(const QString& component)
{
  m_ui->rgbaMappingComponent->setCurrentText(component);
}

void ModuleVolumeWidget::setAllowMultiVolume(const bool checked)
{
  if (checked != m_ui->cbMultiVolume->isChecked()) {
    m_ui->cbMultiVolume->setChecked(checked);
  }

  m_uiLighting->gbLighting->setEnabled(!checked ||
                                       !m_ui->cbMultiVolume->isEnabled());
}

void ModuleVolumeWidget::setEnableAllowMultiVolume(const bool enable)
{
  if (enable != m_ui->cbMultiVolume->isEnabled()) {
    m_ui->cbMultiVolume->setEnabled(enable);
  }

  m_uiLighting->gbLighting->setEnabled(!enable ||
                                       !m_ui->cbMultiVolume->isChecked());
}

void ModuleVolumeWidget::onRgbaMappingMinChanged(double v)
{
  // Compute an increment. Don't let the min value get closer
  // than this to the maximum.
  double fullRange[2] = { m_ui->sliRgbaMappingMax->minimum(),
                          m_ui->sliRgbaMappingMax->maximum() };
  double increment = (fullRange[1] - fullRange[0]) / RANGE_INCREMENT;
  double trueMaximum = fullRange[1] - increment;
  if (v > trueMaximum) {
    setRgbaMappingMin(trueMaximum);
    v = trueMaximum;
  }

  double currentMax = m_ui->sliRgbaMappingMax->value();
  if (v > currentMax) {
    // Set the maximum to be an increment above...
    setRgbaMappingMax(v + increment);
  }

  emit rgbaMappingMinChanged(v);
}

void ModuleVolumeWidget::onRgbaMappingMaxChanged(double v)
{
  // Compute an increment. Don't let the max value get closer
  // than this to the minimum.
  double fullRange[2] = { m_ui->sliRgbaMappingMin->minimum(),
                          m_ui->sliRgbaMappingMin->maximum() };
  double increment = (fullRange[1] - fullRange[0]) / RANGE_INCREMENT;
  double trueMinimum = fullRange[0] + increment;
  if (v < trueMinimum) {
    setRgbaMappingMax(trueMinimum);
    v = trueMinimum;
  }

  double currentMin = m_ui->sliRgbaMappingMin->value();
  if (v < currentMin) {
    // Set the minimum to be an increment below...
    setRgbaMappingMin(v - increment);
  }

  emit rgbaMappingMaxChanged(v);
}

QFormLayout* ModuleVolumeWidget::formLayout()
{
  return m_ui->formLayout;
}
} // namespace tomviz
