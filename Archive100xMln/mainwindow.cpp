#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QRandomGenerator>
#include <QVector>

#define gHIGH 100
#define gLOW 1

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_bGenerate_clicked()
{
    QVector<quint32> arr;
    arr.resize(1000000);

    QRandomGenerator::global()->fillRange(arr.data(), arr.size());

    for (int i=0; i<1000000; i++)
    {
        arr[i]=(arr[i]%100)+1;
        //( QRandomGenerator::global()->bounded(gLOW, gHIGH));
    }
}
