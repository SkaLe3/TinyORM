# The tom.pri file is available to simplify the integration of the TinyORM library
# into the Tom migrations application.
#
# It sets up and configures the CONFIG and DEFINES qmake variables, adds the TinyORM, Tom,
# and vcpkg header files on the system INCLUDEPATH (cross-platform using the -isystem or
# -imsvc), links against the TinyORM shared or static library using the LIBS.
#
# Requirements:
#
# You must build your tom application with the same CONFIG qmake variables that were used
# when building the TinyORM library.
#
# You must define the following variables before the tom.pri will be included:
#
# - TINYORM_BUILD_TREE - Path to the TinyORM build folder.
# - TINY_VCPKG_ROOT    - Path to the vcpkg installation folder.
#                        If not defined, then it tries to use the VCPKG_ROOT environment
#                        variable.
# - TINY_VCPKG_TRIPLET - The vcpkg triplet to use (vcpkg/installed/$$TINY_VCPKG_TRIPLET/).
#                        If not defined, then it tries to guess the vcpkg triplet based
#                        on the current compiler and OS (based on the QMAKESPEC), and
#                        as the last thing, it tries to use the VCPKG_DEFAULT_TRIPLET
#                        environment variable.
#
# These variables will be set after the configuration is done:
#
# - TINY_BUILD_SUBFOLDER        - Folder by release type if CONFIG+=debug_and_release is defined (/debug, /release, or an empty string).
# - TINY_CCACHE_BUILD           - To correctly link ccache build against a ccache build (_ccache or an empty string).
# - TINY_MSVC_VERSION           - Msvc compiler string (MSVC2022 or MSVC2019).
# - TINY_QT_VERSION_UNDERSCORED - Underscored Qt version (eg. 6_5_2).
# - TINY_RELEASE_TYPE_CAMEL     - Build type string (Debug, Profile, or Release).
# - TINY_VCPKG_INCLUDE          - Path to the vcpkg include folder (vcpkg/installed/<triplet>/include/).
#
# Partial guessing of the TINYORM_BUILD_TREE
#
# You don't have to manually define the TINYORM_BUILD_TREE in .env or .qmake.conf files.
# The TINYORM_BUILD_TREE absolute path can be put together for you (this is happening
# inside the variables.pri file) and TinyORM build folder name can be guessed for you too.
#
# You must define the following variables before the TinyOrm.pri will be included to make
# this real (set them in the .qmake.conf):
#
# - TINY_MAIN_DIR   - Path to the PARENT folder of the TinyORM source folder.
# - TINY_BUILD_TREE - Path to the current build tree - TINY_BUILD_TREE = $$shadowed($$PWD).
#
# The TINY_MAIN_DIR is required for another features anyway (so it should already be set)
# and all that's left is to set the TINY_BUILD_TREE.
#
# If you will follow this pattern or logic then you can switch QtCreator Kits and
# the TINYORM_BUILD_TREE will be auto-generated correctly and will always point
# to the correct TinyORM build tree.

# Path to the TinyORM source tree
TINYORM_SOURCE_TREE = $$clean_path($$quote($$PWD/..))
# Path to the Tom source tree
TINYTOM_SOURCE_TREE = $$quote($$TINYORM_SOURCE_TREE/tom)

# Variables to target a correct build folder
# ---

include($$TINYORM_SOURCE_TREE/qmake/support/variables.pri)

# Qt Common Configuration
# ---

QT *= core sql

CONFIG *= link_prl

include($$TINYORM_SOURCE_TREE/qmake/common.pri)

# Configure TinyORM library
# ---
# everything else is defined in the qmake/common.pri

# Link against the shared library
CONFIG(shared, dll|shared|static|staticlib) | \
CONFIG(dll, dll|shared|static|staticlib): \
    DEFINES *= TINYORM_LINKING_SHARED

# Disable the ORM-related source code
disable_orm: DEFINES *= TINYORM_DISABLE_ORM
# Forbid disabling the tom-related source code and header files
disable_tom: \
    error( "Disabling the tom is forbidden in the tom.pri, please remove\
            the 'disable_tom' configuration." )

# File version
# ---

# Find version numbers in the version header file and assign them to the
# <TARGET>_VERSION_<MAJOR,MINOR,PATCH,TWEAK> and also to the VERSION variable.
load(tiny_version_numbers)
tiny_version_numbers($$quote($$TINYTOM_SOURCE_TREE/include/tom/version.hpp))

# Windows resource and manifest files
# ---

# To find version.hpp
tinyRcIncludepath = $$quote($$TINYTOM_SOURCE_TREE/include/)
# To find Windows manifest
mingw: tinyRcIncludepath += $$quote($$TINYTOM_SOURCE_TREE/resources/)

load(tiny_resource_and_manifest)
tiny_resource_and_manifest(                                               \
    $$tinyRcIncludepath, $$TINYTOM_SOURCE_TREE/resources, tom, Tom        \
)

unset(tinyRcIncludepath)

# Link against TinyORM library
# ---

load(tiny_system_includepath)

tiny_add_system_includepath(                   \
    $$quote($$TINYORM_SOURCE_TREE/include/)    \
    $$quote($$TINYTOM_SOURCE_TREE/include/)    \
)

# Don't check if exists() because QtCreator depends on these LIBS paths it adds them
# on the PATH or LD_LIBRARY_PATH during Run Project
!isEmpty(TINYORM_BUILD_TREE) {
    LIBS += $$quote(-L$$clean_path($$TINYORM_BUILD_TREE)/src$${TINY_BUILD_SUBFOLDER}/)
    LIBS += -lTinyOrm
}

# vcpkg - range-v3 and tabulate
# ---
# Affected by (in the following order):
# TINY_VCPKG_ROOT and TINY_VCPKG_TRIPLET qmake variables
# TINY_VCPKG_ROOT and TINY_VCPKG_TRIPLET environment variables
# VCPKG_ROOT and VCPKG_DEFAULT_TRIPLET environment variables

load(tiny_find_packages)

tiny_find_vcpkg()

# Some info output
# ---

# The TINYORM_BUILD_TREE is super important, so print some warning if isn't defined,
# can be disabled using CONFIG*=no_tinyorm_link_warning
!build_pass: \
!no_tinyorm_link_warning: \
if(!defined(TINYORM_BUILD_TREE, var)|isEmpty(TINYORM_BUILD_TREE)): \
    warning( "The 'TINYORM_BUILD_TREE' qmake variable to the TinyORM build tree was not\
              defined, please define it or link against the TinyOrm library manually\
              using the 'LIBS' qmake variable." )
