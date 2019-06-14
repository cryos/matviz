/* This source file is part of the Tomviz project, https://tomviz.org/.                                                                                                                                       It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPresetDialog_h
#define tomvizPresetDialog_h

#include <QDialog>

#include <QScopedPointer>

namespace Ui {
class PresetDialog;
}

namespace tomviz {

class PresetDialog : public QDialog
{
  Q_OBJECT

public:
  explicit PresetDialog(QWidget* parent);
  ~PresetDialog() override;

private:
  QScopedPointer<Ui::PresetDialog> m_ui;
};
} // namespace tomviz                                                                                                                                                                                       

#endif
