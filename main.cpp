#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    a.setFont(QFont("Microsoft YaHei", 12));




    return a.exec();
}
