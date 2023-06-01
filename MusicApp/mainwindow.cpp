#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "QDebug"
#include "QTimer"
#include "QComboBox"
#include "QFileSystemModel"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "musicplayer.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // get music name
    QDir directory("./MusicLists");
    QFileInfoList fileList = directory.entryInfoList();

    QString fileNames;
    for (const QFileInfo &fileInfo : fileList){
        if(fileInfo.isFile()){
            fileNames+= fileInfo.fileName() + "\n";
            mp.musicList.append(fileInfo.fileName());
        }
    }
    mp.musicIndex = 0;
    mp.fileName = mp.musicDir + mp.musicList.at(0);
    qDebug() << "first music name: " << mp.fileName;
    // play the music
    mp.play_music();

    // set text for music list label
    ui->label_4->setText(fileNames);
    QFont font("Arial", 12);
    ui->label_4->setFont(font);
    ui->label_4->setStyleSheet("QLabel { line-height: 40px; }");

    // set text for music name label
    ui->label_5->setText(mp.musicList.at(0));

    // set volume bar
    ui->verticalSlider->setValue(45);
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
    mp.m_pauseFlag = !mp.m_pauseFlag;
    qDebug() << "now m_pauseFlag = " << mp.m_pauseFlag;
}

void MainWindow::HandleForward()
{
    qDebug() << "Click Fast Forward Button";
    pthread_t pthread;
    pthread_create(&pthread, NULL, play_forward, &mp);
}

void MainWindow::HandleBackward()
{
    qDebug() << "Click Fast Backward Button";
    pthread_t pthread;
    pthread_create(&pthread, NULL, play_backward, &mp);
}

void MainWindow::HandleNextSong(){
    qDebug() << "Click Change Next Song Button";
    if(mp.musicIndex<mp.musicList.size()-1){
        ui->label_5->setText(mp.musicList.at(mp.musicIndex+1));
    }
    play_next(&mp);
}

void MainWindow::HandleLastSong(){
    qDebug() << "Click Change Last Song Button";
    if(mp.musicIndex>0){
        ui->label_5->setText(mp.musicList.at(mp.musicIndex-1));
    }
    play_before(&mp);
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
    int value = volumeSlider->value() * 5 / 9 + 200;
    mp.audio_rec_mixer_set_volume(value);
}
void MainWindow::HandleSpeed(){
    QComboBox* speedCombo = ui->comboBox;
    qDebug() << "Speed Combo Box Changed: " << speedCombo->currentIndex() << "-" << speedCombo->currentText();
}

MainWindow::~MainWindow()
{
    delete ui;
}
