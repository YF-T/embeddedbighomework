#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "QTimer"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void HandleSpeed();
    void HandlePause();
    void HandleForward();
    void HandleBackward();
    void HandleVolumeChanged();
    void HandleVolumeChangedDelayed();
    void HandleNextSong();
    void HandleLastSong();

private:
    Ui::MainWindow *ui;
    QTimer sliderTimer;

};
#endif // MAINWINDOW_H
