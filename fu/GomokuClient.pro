QT       += core gui network

# ==================== 工具链路径（Qt 5.15.2 MinGW 64-bit）====================
# qmake 路径：F:\QT\5.15.2\mingw81_64\bin\qmake.exe
# 编译器路径：F:\QT\Tools\mingw810_64\bin\mingw32-make.exe

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# C++11 标准（Qt 5.x 最低要求）
CONFIG += c++11

# ==================== 目标配置 ====================
TARGET = GomokuClient
TEMPLATE = app

# 禁用 Qt 废弃 API 警告（Qt 5.15+）
DEFINES += QT_DEPRECATED_WARNINGS

# ==================== 源文件 ====================
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    networkmanager.cpp \
    gameboard.cpp \
    aiplayer.cpp

HEADERS += \
    mainwindow.h \
    networkmanager.h \
    gameboard.h \
    aiplayer.h

# ==================== 打包配置 ====================
# 1. Release 模式：优化构建，去掉调试符号
CONFIG(release, debug|release) {
    DEFINES += QT_NO_DEBUG_OUTPUT
    QMAKE_CXXFLAGS += -O2
}

# 2. Windows 特定配置
win32 {
    # 字符集：UTF-8（支持中文）
    DEFINES += UNICODE _UNICODE
    # 静态 CRT（可选，取消注释则静态链接 C 运行时，体积更大但更便携）
    # QMAKE_CXXFLAGS += /MT
}

# ==================== 部署配置 ====================
# 安装路径（默认）
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /tmp/$${TARGET}/install-prefix
!isEmpty(target.path): INSTALLS += target

# ==================== 资源文件（如果有）====================
# 如有 .qrc 文件，在下方添加：
# RESOURCES += resources.qrc
