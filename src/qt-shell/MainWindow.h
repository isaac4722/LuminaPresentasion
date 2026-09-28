// src/qt-shell/MainWindow.h — Carcasa Qt: vista en vivo del modo directo.
// Abre paquetes .pptx/.pptm y los muestra con el rasterizador D2D del
// núcleo (misma ruta que proyección y exportación).

#ifndef FUSION_QT_MAINWINDOW_H
#define FUSION_QT_MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

#include <memory>
#include <vector>

#include "fusion/core/PptxDirecto.h"
#include "fusion/core/RenderDirecto.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onAbrir();
    void onDiapositivaElegida(int fila);

private:
    // Rasteriza la diapositiva índice (0-based) al QLabel central.
    void MostrarDiapositiva(int fila);

    QPushButton*   btnAbrir_ = nullptr;
    QListWidget*   lstDiapos_ = nullptr;
    QLabel*        lblVista_ = nullptr;
    QLabel*        lblEstado_ = nullptr;

    // Paquete abierto y plan renderizable por diapositiva.
    bool ok_ = false;
    InfoPptx info_;
    std::vector<DiapositivaPptx> diapositivas_;
    std::unique_ptr<RasterizadorDirectoD2D> raster_;
};

#endif // FUSION_QT_MAINWINDOW_H
