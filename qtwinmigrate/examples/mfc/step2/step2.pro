TEMPLATE = app
TARGET	 = QtMfc2

DEFINES -= UNICODE
DEFINES += _AFXDLL WINVER=0x0501
QMAKE_LIBS_QT_ENTRY =

HEADERS = childview.h mainframe.h qtmfc.h stdafx.h
SOURCES = childview.cpp mainframe.cpp qtmfc.cpp stdafx.cpp
RC_FILE = qtmfc.rc

include(../../../qtwinmigrate.pri)
