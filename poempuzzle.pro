QT += core gui widgets

TARGET = PoemPuzzle
TEMPLATE = app

SOURCES += main.cpp            mainwindow.cpp \
    poemrepository.cpp \
    solver_idastar.cpp \
    solver_util.cpp \
   solverworker.cpp

HEADERS += mainwindow.h \
    poemrepository.h \
    solver_idastar.h \
    solver_util.h \
    solverworker.h \
    solverworker.h \
    solverworker.h \
    solverworker.h \
    solverworker.h \
    solverworker.h

    solver_util.h \

    solverworker.h
FORM+=mainwindow.ui

FORMS += \
    mainwindow.ui
CONFIG+=c++17

DISTFILES += \
    data/poems.json

RESOURCES += \
    resources.qrc
