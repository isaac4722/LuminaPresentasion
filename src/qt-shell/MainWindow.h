// src/qt-shell/MainWindow.h

#ifndef FUSION_QT_MAINWINDOW_H
#define FUSION_QT_MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
private slots:
    void onEstudio();
    void onPresentar();
    void onBiblioteca();
private:
    QLabel*       lblTitulo_;
    QPushButton*  btnEstudio_;
    QPushButton*  btnPresentar_;
    QPushButton*  btnBiblioteca_;
    QListWidget*  lstRecientes_;
};

#endif // FUSION_QT_MAINWINDOW_H
