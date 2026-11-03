/**************************************************************************
###########################################################################
 This file is part of LImA, a Library for Image Acquisition

 Copyright (C) : 2009-2011
 European Synchrotron Radiation Facility
 BP 220, Grenoble 38043
 FRANCE

 This is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

 This software is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, see <http://www.gnu.org/licenses/>.
###########################################################################
**************************************************************************/
#ifndef __linux__

#    include <cstdlib>

#    include <process.h>

#    include <sys/stat.h>
#    include <sys/timeb.h>
#    include <time.h>

#    include "lima/Exceptions.h"
#    include "lima/HwSyncCtrlObj.h"

#    include "PcoCamera.h"
#    include "PcoCameraSdk.h"
#    include "PcoSyncCtrlObj.h"

using namespace lima;
using namespace lima::Pco;

//=================================================================================================
//=================================================================================================
// SC2_SDK_FUNC int WINAPI PCO_SetImageParameters(HANDLE ph, WORD wxres, WORD
// wyres, DWORD dwflags, void* param, int ilen);
// Necessary while using a soft-roi enabled interface. It is recommended to use
// this function with all interface types of pco when soft-roi is enabled. This
// function can be used as a replacement for PCO_CamLinkSetImageParameters If
// there is a change in buffer size (ROI, binning) and/or ROI relocation this
// function must be called with the new x and y resolution. Additionally this
// function has to be called, if you switch to another camera internal memory
// segment with different x and y size or ROI and like to get images. In: HANDLE
// ph -> Handle to a previously opened camera.
//     WORD wxres -> X Resolution of the images to be transferred
//     WORD wyres -> Y Resolution of the images to be transferred
//     DWORD dwflags -> Flags to select the correct soft-ROI for interface
//     preparation
//                      Set IMAGEPARAMETERS_READ_FROM_SEGMENTS when the next
//                      image operation will read images from one of the camera
//                      internal memory segments (if available). Set
//                      IMAGEPARAMETERS_READ_WHILE_RECORDING when the next image
//                      operation is a recording
//     void* param -> Pointer to a structure for future use (set to NULL);
//     Currently not used. int ilen -> int to hold the length of the param
//     structure for future use (set to 0); Currently not used.
// Out: int -> Error message.

void Camera::_pco_SetImageParameters(int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    char *pcoFn;
    WORD wXres, wYres;

    void *param = NULL;
    int iLenParam = 0;
    DWORD dwFlags = 0;

    int forcedFifo = 0;
    getRecorderForcedFifo(forcedFifo);

    dwFlags = (_isCameraType(Edge) || forcedFifo || getRingBufferEnabled())
                  ? IMAGEPARAMETERS_READ_WHILE_RECORDING
                  : IMAGEPARAMETERS_READ_FROM_SEGMENTS;

    WORD _wMaxWidth, _wMaxHeight;
    _pco_GetSizes(&wXres, &wYres, &_wMaxWidth, &_wMaxHeight, error);

    DEB_TRACE() << "PCO_SetImageParameters: "
                << DEB_VAR3(wXres, wYres, dwFlags);

    // error = PCO_SetImageParameters(m_handle, wXres, wYres, dwFlags, param,
    // iLenParam);

    PCO_FN6(error, pcoFn, PCO_SetImageParameters, m_handle, wXres, wYres,
            dwFlags, param, iLenParam);

    if (_getDebug(DBG_TRACE_FIFO))
    {
        printf("---TRACE - PCO_SetImageParameters dwFlags[0x%lx]\n", dwFlags);
    }
    if (error)
    {
        DEB_ALWAYS() << "ERROR: \n" << DEB_VAR2(pcoFn, error);
        throw LIMA_HW_EXC(Error, pcoFn);
    }

    return;
}

#endif
