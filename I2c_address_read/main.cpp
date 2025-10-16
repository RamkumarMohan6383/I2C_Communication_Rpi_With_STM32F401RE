#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    const char *i2cDevice = "/dev/i2c-1";
       int fd = open(i2cDevice, O_RDWR);
       if (fd < 0) {
           qCritical() << "Failed to open I2C device";
           return -1;
       }

       int addr = 0x30; // STM32 I2C address (7-bit)
       if (ioctl(fd, I2C_SLAVE, addr) < 0) {
           qCritical() << "Failed to set I2C address";
           close(fd);
           return -1;
       }

       // Example: read 10 bytes from STM32
       char buffer[10];
       if (read(fd, buffer, sizeof(buffer)) != sizeof(buffer)) {
           qCritical() << "Failed to read from STM32";
       } else {
           qDebug() << "Data from STM32:";
           for (int i = 0; i < 10; i++)
               qDebug() << QString("0x%1").arg((unsigned char)buffer[i], 2, 16, QChar('0'));
       }

       close(fd);

    return a.exec();
}
