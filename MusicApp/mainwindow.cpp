#include "QDebug"
#include "QTimer"
#include "QComboBox"
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // connect signal and slot
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::HandlePause);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::HandleBackward);
    connect(ui->pushButton_3, &QPushButton::clicked, this, &MainWindow::HandleForward);
    connect(ui->pushButton_4, &QPushButton::clicked, this, &MainWindow::HandleNextSong);
    connect(ui->pushButton_5, &QPushButton::clicked, this, &MainWindow::HandleLastSong);
    connect(ui->verticalSlider, &QSlider::valueChanged, this, &MainWindow::HandleVolumeChanged);
    connect(&sliderTimer, &QTimer::timeout, this, &MainWindow::HandleVolumeChangedDelayed);
    connect(ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::HandleSpeed);
}

void MainWindow::HandlePause()
{
    qDebug() << "Click Pause Button";
}

void MainWindow::HandleForward()
{
    qDebug() << "Click Fast Forward Button";
}

void MainWindow::HandleBackward()
{
    qDebug() << "Click Fast Backward Button";
}

void MainWindow::HandleNextSong(){
    qDebug() << "Click Change Next Song Button";
}

void MainWindow::HandleLastSong(){
    qDebug() << "Click Change Last Song Button";
}

void MainWindow::HandleVolumeChanged(){
    // This function will be triggered every time when slider changed
    // In order to reduce the frequency of adjusting the volume,
    // we delay a period of time call the function HandleVOlumeChangedDelayed()

    int delay = 500;
    if (!sliderTimer.isActive()){
        sliderTimer.start(delay);
    }
}

void MainWindow::HandleVolumeChangedDelayed(){
    sliderTimer.stop();

    // Slider Range (0,99)
    QSlider* volumeSlider = ui->verticalSlider;
    qDebug() << "Volume Bar Changed: " << volumeSlider->value();

}
void MainWindow::HandleSpeed(){
    QComboBox* speedCombo = ui->comboBox;
    qDebug() << "Speed Combo Box Changed: " << speedCombo->currentIndex() << "-" << speedCombo->currentText();
}

MainWindow::~MainWindow()
{
    delete ui;
}
