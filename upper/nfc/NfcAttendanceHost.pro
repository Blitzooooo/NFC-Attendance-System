QT += core gui widgets serialport sql

CONFIG += c++17

TARGET = NfcAttendanceHost
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    databasemanager.cpp

HEADERS += \
    mainwindow.h \
    databasemanager.h
