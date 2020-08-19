#ifndef tp_qt_maps_widget_EditMaterialWidget_h
#define tp_qt_maps_widget_EditMaterialWidget_h

#include "tp_qt_maps_widget/Globals.h"

#include "tp_maps/Globals.h"

#include <QWidget>

namespace tp_qt_maps_widget
{
class TP_QT_MAPS_WIDGET_SHARED_EXPORT EditMaterialWidget : public QWidget
{
  Q_OBJECT
public:
  //################################################################################################
  EditMaterialWidget(QWidget* parent = nullptr);

  //################################################################################################
  ~EditMaterialWidget() override;

  //################################################################################################
  void setMaterial(const tp_maps::Material& material);

  //################################################################################################
  tp_maps::Material material() const;

  //################################################################################################
  //! Shows a dialog to edit the material and returns true if accepted.
  static bool editMaterialDialog(QWidget* parent, tp_maps::Material& material);

signals:
  //################################################################################################
  //! Emitted when a material is edited by the user.
  void materialEdited();

private:
  struct Private;
  Private* d;
  friend struct Private;
};
}
#endif

