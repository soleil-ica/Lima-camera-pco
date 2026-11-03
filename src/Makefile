############################################################################
# This file is part of LImA, a Library for Image Acquisition
#
# Copyright (C) : 2009-2011
# European Synchrotron Radiation Facility
# BP 220, Grenoble 38043
# FRANCE
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
############################################################################
.PHONY: src test clean all

PCO_DIR = ..
LIMA_DIR = $(PCO_DIR)/../..

PCO_SDK_DIR = $(PCO_DIR)/sdkPcoLin
PCO_SDK_WIN_DIR = $(PCO_DIR)/sdkPco
PCO_SDK_LIB_DIR  = $(PCO_SDK_DIR)/pco_common/pco_lib

SISO_DIR = $(SISODIR5)

#SISO_LIB_DIR = $(SISO_DIR)/lib64
SISO_LIB_DIR = $(SISO_DIR)/lib

SISO_INC = \
            -I$(SISO_DIR)/include

LIMA_INC =\
            -I$(LIMA_DIR)/hardware/include \
            -I$(LIMA_DIR)/common/include \
            -I$(LIMA_DIR)/third-party/Processlib/core/include \

PCO_INC = \
            -I$(PCO_DIR)/include \

PCO_SDK_INC = \
            -I$(PCO_SDK_DIR)/include \
            -I$(PCO_SDK_DIR) \
            -I$(PCO_SDK_DIR)/pco_common/pco_include \
            -I$(PCO_SDK_DIR)/pco_common/pco_classes \
            -I$(PCO_SDK_DIR)/pco_me4/pco_classes \
            -I$(PCO_SDK_WIN_DIR)/include \


LIBS_PCO_SDK = \
                $(PCO_SDK_LIB_DIR)/libpcocam_me4.a \
                $(PCO_SDK_LIB_DIR)/libpcofile.a  \
                $(PCO_SDK_LIB_DIR)/libpcolog.a  \
                $(PCO_SDK_LIB_DIR)/libreorderfunc.a

#                $(PCO_SDK_LIB_DIR)/libpcoclhs.a \
#                $(PCO_SDK_LIB_DIR)/libpcodisp.a  \

#                $(PCO_SDK_LIB_DIR)/libpcocnv.a  \




LDFLAGS += \
        -L$(SISO_LIB_DIR) \

LIBS = \
        $(LIBS_PCO_SDK)     


INCLUDES  = \
            $(LIMA_INC) \
            $(SISO_INC) \
            $(PCO_SDK_INC) \
            $(PCO_INC) \

#            -I../sdkPco/include \


CXXFLAGS += $(INCLUDES) -fPIC -Wall -pthread -g -D WITH_GIT_VERSION


pco-objs :=  \
            PcoBinCtrlObj.o  \
            PcoBufferCtrlObj.o  \
            PcoBufferCtrlObjRingBuffer.o  \
            PcoCamera.o  \
            PcoCameraRingBuffer.o  \
            PcoCameraSdk.o  \
            PcoCameraSdkLin.o  \
            PcoCameraSip.o  \
            PcoCameraUtils.o  \
            PcoDetInfoCtrlObj.o  \
            PcoHwEventCtrlObj.o  \
            PcoInterface.o  \
            PcoRoiCtrlObj.o  \
            PcoSyncCtrlObj.o  \
            
            
#            PcoCameraSdkAdd.o  \


#           PcoCameraLin.o  \
           
SRCS = $(pco-objs:.o=.cpp) 

#----------------------------------------------------------------
#----------------------------------------------------------------

src:	all

#all:	Pco.o pco_test

all:	Pco.o 


Pco.o:	$(pco-objs)
	@echo 
	@echo "+++ linking $@"
	$(LD) -d -o $@ -r $+ $(LDFLAGS) $(LIBS) -Map $(basename $@).map
	@echo "+++"


clean:
	rm -f *.o *.P $(pco-objs) *.map
	cd $(PCO_DIR) ; setId


%.o : %.cpp
	@echo 
	@echo "+++ compiling $<"
	$(COMPILE.cpp) -MD -o $@ $<
	@cp $*.d $*.P; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	-e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
	rm -f $*.d
	@echo "+++"
	

pco_%:		pco_%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(CFLAGS) $@.cpp $(LDFLAGS) $(LIBS)  -o $@


-include $(SRCS:.cpp=.P)
