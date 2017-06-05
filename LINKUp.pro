#-------------------------------------------------
#
# Project created by QtCreator 2016-04-01T09:11:07
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = LINKUp
TEMPLATE = app

CONFIG += c++11

SOURCES += main.cpp\
    logger.cpp \
    XmlRpcClient.cpp \
    XmlRpcMutex.cpp \
    XmlRpcSocket.cpp \
    XmlRpcSource.cpp \
    XmlRpcUtil.cpp \
    XmlRpcValue.cpp \
    linkuputils.cpp \
    XmlRpcDispatch.cpp \
    XmlRpcServer.cpp \
    XmlRpcServerConnection.cpp \
    XmlRpcServerMethod.cpp \
    linkup.cpp \
    arqcatcher.cpp \
    arqpitcher.cpp

HEADERS  += \
    logger.h \
    XmlRpc.h \
    XmlRpcBase64.h \
    XmlRpcClient.h \
    XmlRpcException.h \
    XmlRpcMutex.h \
    XmlRpcSocket.h \
    XmlRpcSource.h \
    XmlRpcUtil.h \
    XmlRpcValue.h \
    linkuputils.h \
    XmlRpcDispatch.h \
    XmlRpcServer.h \
    XmlRpcServerConnection.h \
    XmlRpcServerMethod.h \
    compat.h \
    mingw.h \
    linkup.h \
    arqcatcher.h \
    arqpitcher.h

FORMS    += \
    linkup.ui \
    arqcatcher.ui \
    arqpitcher.ui

RESOURCES +=

win32:LIBS += -lws2_32
