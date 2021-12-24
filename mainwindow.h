#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFuture>
#include <QtNetwork>
#include <QTemporaryDir>
#include "lib/mapdescriptor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    QNetworkAccessManager *manager;

    QTemporaryDir tempGameDir;

    void openFile();
    void openIsoWbfs();
    void exportToFolder();
    void exportIsoWbfs();
    void loadDescriptors(const QVector<MapDescriptor> &descriptors);
    QFuture<void> checkForRequiredFiles(bool alwaysAsk = false);
    void validateMaps();
    void saveCleanItastCsmmBrsar();
    void saveMapList();
    void loadMapList();

    QString getSaveId();
};
#endif // MAINWINDOW_H
