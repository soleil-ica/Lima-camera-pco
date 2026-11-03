###########################################################################
# This file is part of LImA, a Library for Image Acquisition
#
#  Copyright (C) : 2009-2017
#  European Synchrotron Radiation Facility
#  CS40220 38043 Grenoble Cedex 9 
#  FRANCE
# 
#  Contact: lima@esrf.fr
# 
#  This is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
# 
#  This software is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, see <http://www.gnu.org/licenses/>.
############################################################################

set(PCO_INCLUDE_DIRS)
set(PCO_LIBRARIES)
set(PCO_DEFINITIONS)

if(WIN32)

    set(PCO_SDKWIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdkPco" CACHE PATH "location of PCO Windows SDK")

    find_path(PCO_INCLUDE_DIRS "PcoSdkVersion.h" ${PCO_SDKWIN_DIR})
    list(APPEND PCO_INCLUDE_DIRS
        ${PCO_INCLUDE_DIRS}/include
    )
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PCO_SDK_LIBRARY_DIR "${PCO_SDKWIN_DIR}/lib64")
    else()
        set(PCO_SDK_LIBRARY_DIR "${PCO_SDKWIN_DIR}/lib")
    endif()
    find_library(PCO_LIBRARIES NAMES SC2_Cam.lib HINTS "${PCO_SDK_LIBRARY_DIR}")

else()

    set(PCO_SDKLIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdkPcoLin" CACHE PATH "location of PCO Linux SDK")
    set(PCO_SDK_LIB_DIR "${PCO_SDKLIN_DIR}/pco_common/pco_lib" CACHE PATH "location of PCO Linux SDK LIBS")

    find_path(SISO_INCLUDE sisoboards.h)

    list(APPEND PCO_INCLUDE_DIRS
        ${SISO_INCLUDE}

        ${PCO_SDKLIN_DIR}
        ${PCO_SDKLIN_DIR}/include
        ${PCO_SDKLIN_DIR}/pco_common/pco_include
        ${PCO_SDKLIN_DIR}/pco_common/pco_classes
        ${PCO_SDKLIN_DIR}/pco_me4/pco_classes

        ${PCO_SDKWIN_DIR}/include
    )

    set(PCOLIB1)
    set(PCOLIB2)
    set(PCOLIB3)
    set(PCOLIB4)
    find_library(PCOLIB1 pcocam_me4 HINTS ${PCO_SDK_LIB_DIR})
    find_library(PCOLIB2 pcofile HINTS ${PCO_SDK_LIB_DIR})
    find_library(PCOLIB3 pcolog HINTS ${PCO_SDK_LIB_DIR})
    find_library(PCOLIB4 reorderfunc HINTS ${PCO_SDK_LIB_DIR})

    set(SISOLIB1)
    set(SISOLIB2)
    set(SISOLIB3)
    find_library(SISOLIB1 fglib5)
    find_library(SISOLIB2 clsersis)
    find_library(SISOLIB3 haprt)

    list(APPEND PCO_LIBRARIES 
        ${PCOLIB1} ${PCOLIB2} ${PCOLIB3} ${PCOLIB4}
        ${SISOLIB1} ${SISOLIB2} ${SISOLIB3}
    )

endif()

message("==========================================================")
message("PCO_INCLUDE_DIRS: [${PCO_INCLUDE_DIRS}]")
message("PCO_LIBRARIES: [${PCO_LIBRARIES}]")
message("PCO_DEFINITIONS: [${PCO_DEFINITIONS}]")
message("==========================================================")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCO DEFAULT_MSG
  PCO_LIBRARIES
  PCO_INCLUDE_DIRS
)

list(APPEND PCO_DEFINITIONS WITH_GIT_VERSION)


