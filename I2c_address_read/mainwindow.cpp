#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <cstring>

#define I2C_DEV "/dev/i2c-1"
#define STM32_ADDR 0x04
#define MSG "Happy Diwali"
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    int file;
    char filename[20];
    snprintf(filename, sizeof(filename), "%s", I2C_DEV);

    // Open I2C device
    if ((file = open(filename, O_RDWR)) < 0) {
        qCritical() << "Failed to open the i2c bus";
        return;
    }

    qInfo() << "\nI2C Scanner + Communication Example";

    bool foundDevice = false;

    // --- I2C Scan ---
    for (int address = 1; address < 127; address++) {
        if (ioctl(file, I2C_SLAVE, address) < 0)
            continue;

        char buf[1] = {0};
        if (write(file, buf, 0) >= 0) {
            qInfo("I2C device found at 0x%02X", address);
            if (address == STM32_ADDR)
                foundDevice = true;
        }
    }

    if (!foundDevice) {
        qWarning() << "STM32 not found! Check wiring or address.";
        ::close(file);
        return;
    }

    // --- Communicate with STM32 ---
    if (ioctl(file, I2C_SLAVE, STM32_ADDR) < 0) {
        qCritical() << "Failed to set I2C address";
        ::close(file);
        return;
    }

    qInfo() << "STM32 found. Sending data...";

    // Send message
    const char *msg = MSG;
    int msgLen = strlen(msg);
    if (write(file, msg, msgLen) != msgLen) {
        qCritical() << "Failed to write to STM32";
    } else {
        qInfo() << "Data sent successfully";
    }

    // Wait for STM32 reply
    usleep(150000); // 150 ms

    // Read reply (2 bytes)
    char reply[3] = {0};
    int bytesRead = read(file, reply, 2);
    if (bytesRead == 2) {
        qInfo("STM32 reply (%d bytes): %s", bytesRead, reply);
    } else {
        qWarning() << "Failed to read reply or no data";
    }
    ::close(file);
}

