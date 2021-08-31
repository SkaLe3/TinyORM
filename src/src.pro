QT *= core sql
QT -= gui

TEMPLATE = lib
TARGET = TinyOrm

# Common Configuration
# ---

include(../qmake/common.pri)

# TinyORM library specific configuration
# ---

CONFIG *= create_prl create_pc create_libtool

# TinyORM defines
# ---

DEFINES += PROJECT_TINYORM
# Log queries with a time measurement
DEFINES += TINYORM_DEBUG_SQL

# Build as the shared library
CONFIG(shared, dll|shared|static|staticlib) | CONFIG(dll, dll|shared|static|staticlib) {
    DEFINES += TINYORM_BUILDING_SHARED
}

# Enable code needed by tests, eg. connection overriding in the Model
build_tests {
    DEFINES += TINYORM_TESTS_CODE
}

# File version and Windows manifest
# ---

#TinyORM_VERSION_MAJOR = 0
#TinyORM_VERSION_MINOR = 1
#TinyORM_VERSION_PATCH = 0
#TinyORM_VERSION_TWEAK = 0
#QMAKE_SUBSTITUTES += ../include/orm/version.hpp.in

win32:VERSION = 0.1.0.0
else:VERSION = 0.1.0

win32-msvc* {
    QMAKE_TARGET_PRODUCT = TinyORM
    QMAKE_TARGET_DESCRIPTION = TinyORM user-friendly ORM
    QMAKE_TARGET_COMPANY = Crystal Studio
    QMAKE_TARGET_COPYRIGHT = Copyright (©) 2021 Crystal Studio
#    RC_ICONS = images/TinyOrm.ico
    RC_LANG = 1033
}

# User Configuration
# ---

exists(../conf.pri) {
    include(../conf.pri)
}
else:is_vcpkg_build {
    include(../qmake/vcpkgconf.pri)
}
else {
    error( "'conf.pri' for 'src' project does not exist. See an example configuration\
            in 'conf.pri.example' or call 'vcpkg install' in the project's root." )
}

# Use Precompiled headers (PCH)
# ---

precompile_header {
    include(../include/pch.pri)
}

# TinyORM library header and source files
# ---

include(../include/include.pri)
include(src.pri)

# Deployment
# ---

win32-msvc*:CONFIG(debug, debug|release) {
    win32-msvc*: target.path = C:/optx64/$${TARGET}
#    else: unix:!android: target.path = /opt/$${TARGET}/bin
    !isEmpty(target.path): INSTALLS += target
}

# Some info output
# ---

CONFIG(debug, debug|release): message( "Project is built in DEBUG mode." )
CONFIG(release, debug|release): message( "Project is built in RELEASE mode." )

# Disable debug output in release mode
CONFIG(release, debug|release) {
    message( "Disabling debug output." )
    DEFINES += QT_NO_DEBUG_OUTPUT
}
