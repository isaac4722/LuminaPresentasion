// src/qt-shell/MainWindow.cpp

#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("FUSION-HP — Qt Shell");
    resize(900, 500);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    lblTitulo_ = new QLabel("FUSION-HP", central);
    QFont f = lblTitulo_->font();
    f.setPointSize(24);
    f.setBold(true);
    lblTitulo_->setFont(f);

    btnEstudio_   = new QPushButton("Estudio", central);
    btnPresentar_ = new QPushButton("Presentar", central);
    btnBiblioteca_ = new QPushButton("Biblioteca", central);

    btnEstudio_->setMinimumHeight(80);
    btnPresentar_->setMinimumHeight(80);
    btnBiblioteca_->setMinimumHeight(80);

    lstRecientes_ = new QListWidget(central);
    lstRecientes_->addItem("Culto Domingo 2026-09-28");
    lstRecientes_->addItem("Culto Domingo 2026-09-21");

    auto* fila = new QHBoxLayout;
    fila->addWidget(btnEstudio_);
    fila->addWidget(btnPresentar_);
    fila->addWidget(btnBiblioteca_);

    auto* layout = new QVBoxLayout(central);
    layout->addWidget(lblTitulo_);
    layout->addLayout(fila);
    layout->addWidget(new QLabel("Recientes:"));
    layout->addWidget(lstRecientes_);

    connect(btnEstudio_,   &QPushButton::clicked, this, &MainWindow::onEstudio);
    connect(btnPresentar_, &QPushButton::clicked, this, &MainWindow::onPresentar);
    connect(btnBiblioteca_, &QPushButton::clicked, this, &MainWindow::onBiblioteca);
}

void MainWindow::onEstudio()    { /* TODO(P0): IPC programa.abrir */ }
void MainWindow::onPresentar()  { /* TODO(P0): IPC proyeccion.iniciar */ }
void MainWindow::onBiblioteca() { /* TODO(P0): IPC canto.listar */ }
