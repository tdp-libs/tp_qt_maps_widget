#include "tp_qt_maps_widget/EditMaterialWidget.h"
#include "tp_qt_maps_widget/SelectMaterialWidget.h"

#include "tp_qt_widgets/FileDialogLineEdit.h"

#include "tp_image_utils/LoadImages.h"

#include "tp_utils/JSONUtils.h"
#include "tp_utils/DebugUtils.h"

#include <QDialog>
#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QSlider>
#include <QPushButton>
#include <QPointer>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QImage>
#include <QPixmap>
#include <QLineEdit>
#include <QGuiApplication>
#include <QClipboard>
#include <QPainter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScrollArea>
#include <QScrollBar>

namespace tp_qt_maps_widget
{

namespace
{

//##################################################################################################
int colorScaleToInt(float scale, float scaleMax)
{
  auto s = std::sqrt(scale/scaleMax);
  return int(s*100000.0f);
}

//##################################################################################################
float colorScaleFromInt(int scale, float scaleMax)
{
  auto s = float(scale) / 100000.0f;
  return s*s*scaleMax;
}
}

//##################################################################################################
struct EditMaterialWidget::Private
{
  tp_math_utils::Material material;
  std::function<std::vector<tp_utils::StringID>()> getExistingTextures;
  std::function<tp_utils::StringID(const std::string&)> loadTexture;

  QLineEdit* nameEdit{nullptr};

  QScrollArea* scroll{nullptr};
  QWidget* scrollContents{nullptr};

  QPushButton* albedoColorButton  {nullptr};
  QPushButton* sssColorButton     {nullptr};
  QPushButton* emissionColorButton{nullptr};
  QPushButton* velvetColorButton  {nullptr};

  QSlider* albedoScaleSlider   {nullptr};
  QSlider* sssSlider           {nullptr};
  QSlider* emissionSlider      {nullptr};
  QSlider* velvetSlider        {nullptr};

  QSlider* alpha                {nullptr};
  QSlider* roughness            {nullptr};
  QSlider* metalness            {nullptr};
  QSlider* transmission         {nullptr};
  QSlider* transmissionRoughness{nullptr};
  QSlider* sheen                {nullptr};
  QSlider* sheenTint            {nullptr};
  QSlider* clearCoat            {nullptr};
  QSlider* clearCoatRoughness   {nullptr};
  QSlider* heightScale          {nullptr};
  QSlider* heightMidlevel       {nullptr};

  QDoubleSpinBox* ior{nullptr};

  QDoubleSpinBox* sssRadiusR{nullptr};
  QDoubleSpinBox* sssRadiusG{nullptr};
  QDoubleSpinBox* sssRadiusB{nullptr};

  QSlider* useAmbient    {nullptr};
  QSlider* useDiffuse    {nullptr};
  QSlider* useNdotL      {nullptr};
  QSlider* useAttenuation{nullptr};
  QSlider* useShadow     {nullptr};
  QSlider* useLightMask  {nullptr};
  QSlider* useReflection {nullptr};

  std::map<std::string, QLineEdit*> textureLineEdits;

  //################################################################################################
  void updateColors()
  {
    auto makeIcon = [](const glm::vec3& c)
    {
      QImage image(24, 24, QImage::Format_ARGB32);
      image.fill(QColor(0,0,0,0));
      {
        QPainter p(&image);
        p.setBrush(QColor::fromRgbF(c.x, c.y, c.z));
        p.setPen(Qt::black);
        p.drawRoundedRect(2,2,20,20,2.0, 2.0);
      }
      return QIcon(QPixmap::fromImage(image));
    };

    albedoColorButton  ->setIcon(makeIcon(material.albedo  ));
    sssColorButton     ->setIcon(makeIcon(material.sss     ));
    emissionColorButton->setIcon(makeIcon(material.emission));
    velvetColorButton  ->setIcon(makeIcon(material.velvet  ));
  }
};

//##################################################################################################
EditMaterialWidget::EditMaterialWidget(const std::function<void(QLayout*)>& addButtons, QWidget* parent):
  QWidget(parent),
  d(new Private())
{
  auto mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0,0,0,0);

  {
    auto ll = new QHBoxLayout();
    mainLayout->addLayout(ll);
    ll->setContentsMargins(0,0,0,0);
    ll->addWidget(new QLabel("Name"));
    d->nameEdit = new QLineEdit();
    ll->addWidget(d->nameEdit);
    connect(d->nameEdit, &QLineEdit::editingFinished, this, &EditMaterialWidget::materialEdited);
  }

  d->scroll = new QScrollArea();
  d->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  d->scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  d->scroll->setWidgetResizable(true);
  mainLayout->addWidget(d->scroll);

  d->scrollContents = new QWidget();
  d->scroll->setWidget(d->scrollContents);
  d->scrollContents->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
  d->scrollContents->installEventFilter(this);

  auto l = new QVBoxLayout(d->scrollContents);
  l->setContentsMargins(4,4,4,4);

  auto gridLayout = new QGridLayout();
  gridLayout->setContentsMargins(0,0,0,0);
  l->addLayout(gridLayout);

  {
    auto make = [&](const QString& text, const std::function<glm::vec3&()>& getColor, QSlider*& slider, float scaleMax)
    {
      int row = gridLayout->rowCount();

      auto button = new QPushButton(text);
      button->setStyleSheet("text-align:left; padding-left:2;");
      gridLayout->addWidget(button, row, 0);

      auto hLayout = new QHBoxLayout();
      hLayout->setContentsMargins(0,0,0,0);
      gridLayout->addLayout(hLayout, row, 1);

      auto spin = new QDoubleSpinBox();
      spin->setRange(0.0, double(scaleMax));
      spin->setDecimals(3);
      hLayout->addWidget(spin, 1);

      slider = new QSlider(Qt::Horizontal);
      slider->setRange(0, 100000);
      hLayout->addWidget(slider, 3);
      connect(slider, &QSlider::valueChanged, this, &EditMaterialWidget::materialEdited);

      connect(slider, &QSlider::valueChanged, this, [=]
      {
        double v = colorScaleFromInt(slider->value(), scaleMax);
        if(std::fabs(v-spin->value()) > 0.0001)
          spin->setValue(v);
      });

      connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=]
      {
        slider->setValue(colorScaleToInt(spin->value(), scaleMax));
      });

      connect(button, &QAbstractButton::clicked, this, [=]
      {
        glm::vec3& c = getColor();
        QColor color = QColorDialog::getColor(QColor::fromRgbF(c.x, c.y, c.z), this, "Select " + text + " color", QColorDialog::DontUseNativeDialog);
        if(color.isValid())
        {
          c.x = color.redF();
          c.y = color.greenF();
          c.z = color.blueF();
          d->updateColors();
          emit materialEdited();
        }
      });

      return button;
    };

    d->albedoColorButton   = make("Albedo"    , [&]()->glm::vec3&{return d->material.albedo;}  , d->albedoScaleSlider  ,   4.0f);
    d->sssColorButton      = make("Subsurface", [&]()->glm::vec3&{return d->material.sss;}     , d->sssSlider          ,   1.0f);
    d->emissionColorButton = make("Emission"  , [&]()->glm::vec3&{return d->material.emission;}, d->emissionSlider     , 100.0f);
    d->velvetColorButton   = make("Velvet"    , [&]()->glm::vec3&{return d->material.velvet;}  , d->velvetSlider       ,   1.0f);
  }

  {
    int row = gridLayout->rowCount();
    gridLayout->addWidget(new QLabel("Alpha"), row, 0, Qt::AlignLeft);
    d->alpha = new QSlider(Qt::Horizontal);
    gridLayout->addWidget(d->alpha, row, 1);
    d->alpha->setRange(0, 255);
    d->alpha->setSingleStep(1);
    connect(d->alpha, &QSlider::valueChanged, this, &EditMaterialWidget::materialEdited);
  }

  auto addSlider = [&](const QString& name)
  {
    int row = gridLayout->rowCount();
    gridLayout->addWidget(new QLabel(name), row, 0, Qt::AlignLeft);
    auto s = new QSlider(Qt::Horizontal);
    gridLayout->addWidget(s, row, 1);
    s->setRange(0, 255000);
    s->setSingleStep(1);
    connect(s, &QSlider::valueChanged, this, &EditMaterialWidget::materialEdited);
    return s;
  };

  auto addSpin = [&](const QString& name, double min, double max)
  {
    int row = gridLayout->rowCount();
    gridLayout->addWidget(new QLabel(name), row, 0, Qt::AlignLeft);
    auto s = new QDoubleSpinBox();
    gridLayout->addWidget(s, row, 1);
    s->setRange(min, max);
    s->setSingleStep(0.01);
    connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EditMaterialWidget::materialEdited);
    return s;
  };

  d->roughness             = addSlider("Roughness");
  d->metalness             = addSlider("Metalness");
  d->transmission          = addSlider("Transmission");
  d->transmissionRoughness = addSlider("Transmission roughness");
  d->ior                   = addSpin("IOR", 0.0, 6.0);
  d->sheen                 = addSlider("Sheen");
  d->sheenTint             = addSlider("Sheen tint");
  d->clearCoat             = addSlider("Clear coat");
  d->clearCoatRoughness    = addSlider("Clear coat roughness");

  {
    int row = gridLayout->rowCount();
    gridLayout->addWidget(new QLabel("Subsurface radius"), row, 0, Qt::AlignLeft);

    auto ll = new QHBoxLayout();
    gridLayout->addLayout(ll, row, 1);

    auto make = [&]
    {
      auto spin = new QDoubleSpinBox();
      ll->addWidget(spin);
      spin->setRange(0.0, 100.0);
      connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EditMaterialWidget::materialEdited);
      return spin;
    };

    d->sssRadiusR = make();
    d->sssRadiusG = make();
    d->sssRadiusB = make();
  }

  d->heightScale    = addSlider("Height scale");
  d->heightMidlevel = addSlider("Height midlevel");

  auto addTextureEdit = [&](const auto& name)
  {
    int row = gridLayout->rowCount();
    gridLayout->addWidget(new QLabel(name), row, 0, Qt::AlignLeft);

    auto ll = new QHBoxLayout();
    gridLayout->addLayout(ll, row, 1);

    auto edit = new QLineEdit();
    ll->addWidget(edit);
    connect(edit, &QLineEdit::editingFinished, this, &EditMaterialWidget::materialEdited);

    auto button = new QPushButton("Load");
    ll->addWidget(button);
    connect(button, &QPushButton::clicked, this, [=]
    {
      QPointer<QDialog> dialog = new QDialog(this);
      TP_CLEANUP([&]{delete dialog;});

      auto l = new QVBoxLayout(dialog);

      auto existingRadio = new QRadioButton("Existing");
      l->addWidget(existingRadio);
      auto existing = new QComboBox();
      l->addWidget(existing);
      existing->setEnabled(false);

      if(d->getExistingTextures)
      {
        for(const auto& name : d->getExistingTextures())
          existing->addItem(QString::fromStdString(name.keyString()));
      }

      auto loadRadio = new QRadioButton("Load");
      loadRadio->setChecked(true);
      l->addWidget(loadRadio);
      auto load = new tp_qt_widgets::FileDialogLineEdit();
      load->setQSettingsPath("EditMaterialWidget");
      load->setMode(tp_qt_widgets::FileDialogLineEdit::OpenFileMode);
      load->setFilter(QString::fromStdString(tp_image_utils::imageTypesFilter()));
      l->addWidget(load);

      connect(loadRadio, &QRadioButton::toggled, this, [=]
      {
        existing->setEnabled(existingRadio->isChecked());
        load->setEnabled(loadRadio->isChecked());
      });

      l->addStretch();
      auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
      l->addWidget(buttonBox);
      connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
      connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

      if(dialog->exec() == QDialog::Accepted)
      {
        if(existingRadio->isChecked() && !existing->currentText().isEmpty())
        {
          edit->setText(existing->currentText());
          emit materialEdited();
        }
        else if(loadRadio->isChecked() && d->loadTexture)
        {
          auto text = d->loadTexture(load->text().toStdString());
          if(text.isValid())
          {
            edit->setText(QString::fromStdString(text.keyString()));
            emit materialEdited();
          }
        }
      }
    });

    edit->installEventFilter(this);

    return edit;
  };

  d->material.viewTypedTextures([&](const auto& type, const auto&, const auto& pretty)
  {
    d->textureLineEdits[type] = addTextureEdit(pretty);
  });

  d->useAmbient     = addSlider("Use ambient");
  d->useDiffuse     = addSlider("Use diffuse");
  d->useNdotL       = addSlider("Use N dot L");
  d->useAttenuation = addSlider("Use attenuation");
  d->useShadow      = addSlider("Use shadow");
  d->useLightMask   = addSlider("Light mask");
  d->useReflection  = addSlider("Use reflection");

  {
    auto hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0,0,0,0);
    hLayout->addStretch();
    mainLayout->addLayout(hLayout);

    if(addButtons)
      addButtons(hLayout);

    {
      auto button = new QPushButton("Copy");
      hLayout->addWidget(button);
      connect(button, &QPushButton::clicked, this, [=]
      {
        QGuiApplication::clipboard()->setText(QString::fromStdString(material().saveState().dump(2)));
      });
    }
    {
      auto button = new QPushButton("Paste");
      hLayout->addWidget(button);
      connect(button, &QPushButton::clicked, this, [=]
      {
        tp_math_utils::Material material;
        material.loadState(tp_utils::jsonFromString(QGuiApplication::clipboard()->text().toStdString()));
        setMaterial(material);
        emit materialEdited();
      });
    }
  }

  d->scroll->setMinimumWidth(d->scrollContents->minimumSizeHint().width() + d->scroll->verticalScrollBar()->width());
}

//##################################################################################################
EditMaterialWidget::~EditMaterialWidget()
{
  delete d;
}

//##################################################################################################
void EditMaterialWidget::setMaterial(const tp_math_utils::Material& material)
{
  blockSignals(true);
  TP_CLEANUP([&]{blockSignals(false);});

  d->material = material;

  d->nameEdit->setText(QString::fromStdString(material.name.keyString()));

  d->updateColors();

  d->alpha    ->setValue(int(material.alpha    *255.0f));

  d->roughness            ->setValue(int(material.roughness             * 255000.0f));
  d->metalness            ->setValue(int(material.metalness             * 255000.0f));
  d->transmission         ->setValue(int(material.transmission          * 255000.0f));
  d->transmissionRoughness->setValue(int(material.transmissionRoughness * 255000.0f));
  d->heightScale          ->setValue(int(material.heightScale           * 255000.0f));
  d->heightMidlevel       ->setValue(int(material.heightMidlevel        * 255000.0f));

  d->ior                  ->setValue(double(material.ior));

  d->sheen                ->setValue(int(material.sheen              * 255000.0f));
  d->sheenTint            ->setValue(int(material.sheenTint          * 255000.0f));
  d->clearCoat            ->setValue(int(material.clearCoat          * 255000.0f));
  d->clearCoatRoughness   ->setValue(int(material.clearCoatRoughness * 255000.0f));

  d->sssRadiusR->setValue(material.sssRadius.x);
  d->sssRadiusG->setValue(material.sssRadius.y);
  d->sssRadiusB->setValue(material.sssRadius.z);

  d->useAmbient    ->setValue(int(material.useAmbient     * 255000.0f));
  d->useDiffuse    ->setValue(int(material.useDiffuse     * 255000.0f));
  d->useNdotL      ->setValue(int(material.useNdotL       * 255000.0f));
  d->useAttenuation->setValue(int(material.useAttenuation * 255000.0f));
  d->useShadow     ->setValue(int(material.useShadow      * 255000.0f));
  d->useLightMask  ->setValue(int(material.useLightMask   * 255000.0f));
  d->useReflection ->setValue(int(material.useReflection  * 255000.0f));

  d->albedoScaleSlider   ->setValue(colorScaleToInt(material.albedoScale   ,   4.0f));
  d->sssSlider           ->setValue(colorScaleToInt(material.sssScale      ,   1.0f));
  d->emissionSlider      ->setValue(colorScaleToInt(material.emissionScale , 100.0f));
  d->velvetSlider        ->setValue(colorScaleToInt(material.velvetScale   ,   1.0f));

  d->material.viewTypedTextures([&](const auto& type, const auto& value, const auto&)
  {
    d->textureLineEdits[type]->setText(QString::fromStdString(value.keyString()));
  });
}

//##################################################################################################
tp_math_utils::Material EditMaterialWidget::material() const
{
  d->material.name = d->nameEdit->text().toStdString();

  d->material.alpha        = float(d->alpha       ->value()) / 255.0f;

  d->material.roughness             = float(d->roughness            ->value()) / 255000.0f;
  d->material.metalness             = float(d->metalness            ->value()) / 255000.0f;
  d->material.transmission          = float(d->transmission         ->value()) / 255000.0f;
  d->material.transmissionRoughness = float(d->transmissionRoughness->value()) / 255000.0f;
  d->material.heightScale           = float(d->heightScale          ->value()) / 255000.0f;
  d->material.heightMidlevel        = float(d->heightMidlevel       ->value()) / 255000.0f;

  d->material.ior                   = float(d->ior                  ->value());

  d->material.sheen                 = float(d->sheen                ->value()) / 255000.0f;
  d->material.sheenTint             = float(d->sheenTint            ->value()) / 255000.0f;
  d->material.clearCoat             = float(d->clearCoat            ->value()) / 255000.0f;
  d->material.clearCoatRoughness    = float(d->clearCoatRoughness   ->value()) / 255000.0f;


  d->material.sssRadius.x = d->sssRadiusR->value();
  d->material.sssRadius.y = d->sssRadiusG->value();
  d->material.sssRadius.z = d->sssRadiusB->value();

  d->material.useAmbient     = float(d->useAmbient    ->value()) / 255000.0f;
  d->material.useDiffuse     = float(d->useDiffuse    ->value()) / 255000.0f;
  d->material.useNdotL       = float(d->useNdotL      ->value()) / 255000.0f;
  d->material.useAttenuation = float(d->useAttenuation->value()) / 255000.0f;
  d->material.useShadow      = float(d->useShadow     ->value()) / 255000.0f;
  d->material.useLightMask   = float(d->useLightMask  ->value()) / 255000.0f;
  d->material.useReflection  = float(d->useReflection ->value()) / 255000.0f;

  d->material.albedoScale    = colorScaleFromInt(d->albedoScaleSlider->value(),   4.0f);
  d->material.sssScale       = colorScaleFromInt(d->sssSlider        ->value(),   1.0f);
  d->material.emissionScale  = colorScaleFromInt(d->emissionSlider   ->value(), 100.0f);
  d->material.velvetScale    = colorScaleFromInt(d->velvetSlider     ->value(),   1.0f);

  d->material.updateTypedTextures([&](const auto& type, auto& value, const auto&)
  {
    value = d->textureLineEdits[type]->text().toStdString();
  });

  return d->material;
}

//##################################################################################################
void EditMaterialWidget::setGetExistingTextures(const std::function<std::vector<tp_utils::StringID>()>& getExistingTextures)
{
  d->getExistingTextures = getExistingTextures;
}

//##################################################################################################
void EditMaterialWidget::setLoadTexture(const std::function<tp_utils::StringID(const std::string&)>& loadTexture)
{
  d->loadTexture = loadTexture;
}

//##################################################################################################
bool EditMaterialWidget::editMaterialDialog(QWidget* parent,
                                            tp_math_utils::Material& material,
                                            const std::function<std::vector<tp_utils::StringID>()>& getExistingTextures,
                                            const std::function<tp_utils::StringID(const std::string&)>& loadTexture)
{
  QPointer<QDialog> dialog = new QDialog(parent);
  TP_CLEANUP([&]{delete dialog;});

  dialog->setWindowTitle("Edit Material");

  auto l = new QVBoxLayout(dialog);

  auto editMaterialWidget = new EditMaterialWidget();
  l->addWidget(editMaterialWidget);
  editMaterialWidget->setMaterial(material);
  editMaterialWidget->setGetExistingTextures(getExistingTextures);
  editMaterialWidget->setLoadTexture(loadTexture);

  auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  l->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

  if(dialog->exec() == QDialog::Accepted)
  {
    material = editMaterialWidget->material();
    return true;
  }

  return false;
}

//##################################################################################################
bool EditMaterialWidget::eventFilter(QObject* watched, QEvent* event)
{
  if(event->type() == QEvent::DragEnter)
  {
    QDragEnterEvent* e = static_cast<QDragEnterEvent*>(event);
    auto urls = e->mimeData()->urls();
    if(urls.size() == 1 && urls.front().isLocalFile())
    {
      e->acceptProposedAction();
      return true;
    }
  }

  else if(event->type() == QEvent::Drop)
  {
    QDropEvent* e = static_cast<QDropEvent*>(event);
    auto urls = e->mimeData()->urls();
    if(urls.size() == 1 && urls.front().isLocalFile())
    {
      std::string path = urls.front().toLocalFile().toStdString();
      auto text = d->loadTexture(path);
      if(text.isValid())
      {
        for(const auto& i: d->textureLineEdits)
        {
          if(watched == i.second)
          {
            i.second->setText(QString::fromStdString(text.keyString()));
            break;
          }
        }
        emit materialEdited();
      }
    }

    return true;
  }

  else if(event->type() == QEvent::Resize && watched == d->scrollContents)
  {
    d->scroll->setMinimumWidth(d->scrollContents->minimumSizeHint().width() + d->scroll->verticalScrollBar()->width());
  }

  return QWidget::eventFilter(watched, event);
}

}
