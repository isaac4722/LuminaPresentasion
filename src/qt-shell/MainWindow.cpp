// src/qt-shell/MainWindow.cpp — Vista en vivo del modo directo (9.2):
// paquete .pptx/.pptm → LectorPptx → ConstruirPlan (tema por defecto) →
// RasterizadorDirectoD2D (1280x720) → QImage. El shell NO repinta por su
// cuenta: usa el mismo motor del núcleo (una sola fuente de verdad).

#include "MainWindow.h"

#include <QApplication>
#include <QFileDialog>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QFile>

#include <string>

using namespace fusion;

namespace {

constexpr int kAnchoVista = 1280;
constexpr int kAltoVista  = 720;

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("FUSION-HP — Shell de escritorio (modo directo)"));
    resize(1360, 820);

    btnAbrir_ = new QPushButton(tr("Abrir PPTX…"), this);

    lstDiapos_ = new QListWidget(this);
    lstDiapos_->setFixedWidth(240);

    lblVista_ = new QLabel(this);
    lblVista_->setMinimumSize(kAnchoVista, kAltoVista);
    lblVista_->setAlignment(Qt::AlignCenter);
    lblVista_->setStyleSheet("background:#101010; color:#808080;");
    lblVista_->setText(tr("Abre un paquete .pptx para verlo con el motor del núcleo"));

    lblEstado_ = new QLabel(tr("Listo."), this);
    lblEstado_->setWordWrap(true);

    auto* izquierda = new QVBoxLayout();
    izquierda->addWidget(btnAbrir_);
    izquierda->addWidget(lstDiapos_, 1);

    auto* raiz = new QHBoxLayout();
    raiz->addLayout(izquierda);
    raiz->addWidget(lblVista_, 1);

    auto* central = new QWidget(this);
    central->setLayout(raiz);
    setCentralWidget(central);
    statusBar()->addWidget(lblEstado_, 1);

    connect(btnAbrir_, &QPushButton::clicked, this, &MainWindow::onAbrir);
    connect(lstDiapos_, &QListWidget::currentRowChanged, this,
            &MainWindow::onDiapositivaElegida);

    raster_ = std::make_unique<RasterizadorDirectoD2D>();
}

void MainWindow::onAbrir() {
    const QString ruta = QFileDialog::getOpenFileName(
        this, tr("Abrir paquete de presentación"), QString(),
        tr("Presentaciones (*.pptx *.pptm);;Todos los archivos (*)"));
    if (ruta.isEmpty()) return;

    // QFile maneja rutas Unicode sin la trampa del fopen ANSI.
    QFile archivo(ruta);
    if (!archivo.open(QIODevice::ReadOnly)) {
        lblEstado_->setText(tr("No se pudo abrir '%1'").arg(ruta));
        return;
    }
    const QByteArray bytes = archivo.readAll();
    archivo.close();

    lstDiapos_->clear();
    diapositivas_.clear();
    ok_ = false;

    info_ = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(bytes.constData()),
        static_cast<size_t>(bytes.size()), &diapositivas_);

    if (!info_.ok) {
        lblEstado_->setText(tr("Error al leer el paquete: %1")
                                .arg(QString::fromStdString(info_.msg_error)));
        return;
    }

    ok_ = true;
    for (const auto& d : diapositivas_) {
        QString texto = QString("Diapositiva %1").arg(d.indice);
        if (!d.titulo.empty())
            texto += QString(" — %1")
                         .arg(QString::fromStdString(d.titulo)
                                  .left(28));
        lstDiapos_->addItem(texto);
    }

    QString estado = tr("Diapositivas: %1").arg(info_.total_diapositivas);
    if (!info_.avisos.empty())
        estado += tr(" — %1 avisos (ver consola)").arg(info_.avisos.size());
    lblEstado_->setText(estado);

    if (!info_.avisos.empty()) {
        statusBar()->showMessage(
            tr("Aviso: %1")
                .arg(QString::fromStdString(info_.avisos.front())),
            8000);
    }

    if (!diapositivas_.empty()) lstDiapos_->setCurrentRow(0);
}

void MainWindow::onDiapositivaElegida(int fila) {
    if (!ok_ || fila < 0 || fila >= static_cast<int>(diapositivas_.size()))
        return;
    MostrarDiapositiva(fila);
}

void MainWindow::MostrarDiapositiva(int fila) {
    const DiapositivaPptx& d = diapositivas_[static_cast<size_t>(fila)];

    // Tema: sin runtime.json el shell usa el tema por defecto del núcleo
    // (ResolucionTema vacía → estilo por defecto). La herencia completa
    // vive en el núcleo/proyección; aquí es previsualización fiel del
    // contenido y del layout.
    ResolucionTema vacia;
    PlanRenderDirecto plan;
    OpcionesRenderDirecto ops;
    ops.ancho_emu = info_.ancho_emu;
    ops.alto_emu = info_.alto_emu;

    const float aspecto =
        info_.ancho_emu > 0 && info_.alto_emu > 0
            ? static_cast<float>(info_.ancho_emu) /
                  static_cast<float>(info_.alto_emu)
            : 16.0f / 9.0f;

    const ResultadoRenderDirecto res =
        RenderDirecto::ConstruirPlan(d, vacia, aspecto, &plan, ops);

    std::vector<unsigned char> rgba;
    std::string error;
    if (!raster_ || !raster_->Rasterizar(plan, kAnchoVista, kAltoVista,
                                         &rgba, &error)) {
        lblVista_->setText(tr("No se pudo rasterizar: %1")
                               .arg(QString::fromStdString(error)));
        return;
    }

    QImage imagen(rgba.data(), kAnchoVista, kAltoVista,
                  kAnchoVista * 4, QImage::Format_RGBA8888);
    lblVista_->setPixmap(
        QPixmap::fromImage(imagen.copy()).scaled(
            lblVista_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QString estado = tr("Diapositiva %1").arg(d.indice);
    if (!res.avisos.empty())
        estado += tr(" — %1 avisos: %2")
                      .arg(res.avisos.size())
                      .arg(QString::fromStdString(res.avisos.front()));
    lblEstado_->setText(estado);
}
