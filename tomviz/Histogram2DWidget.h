/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef tomvizHistogram2DWidget_h
#define tomvizHistogram2DWidget_h

#include <QWidget>

#include <vtkNew.h>
#include <vtkSmartPointer.h>

/**
 * \brief 
 */

class vtkChartTransfer2DEditor;
class vtkContextView;
class vtkEventQtSlotConnect;
class vtkImageData;
class vtkTransferFunctionBoxItem;
class QVTKOpenGLWidget;

namespace tomviz {

class Histogram2DWidget : public QWidget
{
  Q_OBJECT

public:
  explicit Histogram2DWidget(QWidget* parent_ = nullptr);
  ~Histogram2DWidget() override;

  void setInputData(vtkImageData* histogram);

  void addTransferFunction(vtkSmartPointer<vtkTransferFunctionBoxItem> item);

  vtkImageData* getTransfer2D();

signals:
  void mapUpdated();

protected:
  vtkNew<vtkChartTransfer2DEditor> m_chartHistogram2D;
  vtkNew<vtkContextView> m_histogramView;
  vtkNew<vtkEventQtSlotConnect> m_eventLink;

private:
  //void renderViews();

  QVTKOpenGLWidget* m_qvtk;
};
}
#endif // tomvizHistogram2DWidget_h