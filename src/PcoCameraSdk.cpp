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
#include <cstdlib>

#ifndef __linux__
#    include <process.h>
#endif

#include <sys/stat.h>
#include <sys/timeb.h>
#include <time.h>

#include "lima/Exceptions.h"
#include "lima/HwSyncCtrlObj.h"

#include "PcoCamera.h"
#include "PcoCameraSdk.h"
#include "PcoSyncCtrlObj.h"

using namespace lima;
using namespace lima::Pco;

//=================================================================================================
//=================================================================================================

// 4294967295.0 = double(DWORD(0xFFFFFFFF))
#define DWORD_MAX_FLOAT 4294967295.0

#define MAX_DWORD_MS (double(4294967295.0e-3))
#define MAX_DWORD_US (double(4294967295.0e-6))
#define MAX_DWORD_NS (double(4294967295.0e-9))

void _pco_time2dwbase(double exp_time, DWORD &dwExp, WORD &wBase)
{
    // conversion time(s) to PCO standard DWORD + UNIT(ms, us, ns)
    // exp & lat time is saved in seconds (LIMA).
    // PCO requires them expressed in DWORD as ms(base=2), us(base=1) or
    // ns(base=0) max DWORD 0xFFFFFFFF = 4294967295.0 find the lowest unit (ns
    // -> us -> ms) which does not overflow DWORD

    if (exp_time <= MAX_DWORD_NS)
    {
        dwExp = DWORD(exp_time * 1.0e9);
        wBase = 0; // ns
    }
    else if (exp_time <= MAX_DWORD_US)
    {
        dwExp = DWORD(exp_time * 1.0e6);
        wBase = 1; // us
    }
    else
    {
        dwExp = DWORD(exp_time * 1.0e3);
        wBase = 2; // ms
    }

    DWORD mask = 0x7;
    DWORD min = 0x1000;

    if (dwExp > min)
    {
        dwExp |= mask;
        dwExp ^= mask;
    }

    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetAcqEnblSignalStatus(WORD &wAcquEnableState, int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    WORD _wAcquEnableState;

#ifdef __linux__
    error = camera->PCO_GetAcqEnblSignalStatus(&_wAcquEnableState);
    PCO_CHECK_ERROR(error, "PCO_GetAcqEnblSignalStatus");

#else
    error =
        PcoCheckError(__LINE__, __FILE__,
                      PCO_GetAcqEnblSignalStatus(m_handle, &_wAcquEnableState));

#endif

    if (error)
        return;

    wAcquEnableState = _wAcquEnableState;
}

//=================================================================================================
//=================================================================================================

double Camera::pcoGetCocRunTime()
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    return m_pcoData->cocRunTime;
}

double Camera::pcoGetFrameRate()
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    return m_pcoData->frameRate;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetCOCRuntime(int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    const char *msg;

    //====================================== get the coc runtime
    //---- only valid if it was used PCO_SetDelayExposureTime
    //---- and AFTER armed the cam

    // Get and split the 'camera operation code' runtime into two DWORD. One
    // will hold the longer part, in seconds, and the other will hold the
    // shorter part, in nanoseconds. This function can be used to calculate the
    // FPS. The sum of dwTime_s and dwTime_ns covers the delay, exposure and
    // readout time. If external exposure is active, it returns only the readout
    // time.

    DWORD dwTime_s, dwTime_ns;
    double runTime;

#ifndef __linux__
    PCO_FN3(error, msg, PCO_GetCOCRuntime, m_handle, &dwTime_s, &dwTime_ns);
    PCO_PRINT_ERR(error, msg);
    if (error)
        return;
#else

    error = camera->PCO_GetCOCRuntime(&dwTime_s, &dwTime_ns);
    msg = "PCO_GetCOCRuntime";
    PCO_CHECK_ERROR(error, msg);
    if (error)
        return;
#endif

    m_pcoData->cocRunTime = runTime =
        ((double)dwTime_ns * NANO) + (double)dwTime_s;
    m_pcoData->frameRate = (dwTime_ns | dwTime_s) ? 1.0 / runTime : 0.0;

    DEB_TRACE() << DEB_VAR2(m_pcoData->frameRate, m_pcoData->cocRunTime);

    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetTimestampMode(WORD mode, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    WORD modeNew, modeOld;

#ifndef __linux__
    char *msg;
    int error;
    err = error = 0;

    WORD modeMax;

    DWORD capsDesc1;
    _pco_GetGeneralCapsDESC(capsDesc1, error);

    if (capsDesc1 & BIT8)
    {
        DEB_ALWAYS() << "\n   timestampmode not allowed";
        err = -1;
        return;
    }
    modeMax = (capsDesc1 & BIT3) ? 3 : 2;
    if (mode > modeMax)
    {
        DEB_ALWAYS() << "\n  invalid value" << DEB_VAR2(mode, modeMax);
        err = -1;
        return;
    }

    _pco_GetTimestampMode(modeOld, err);
    if (err)
        return;

    if (mode == modeOld)
    {
        DEB_TRACE() << "\n   no change " << DEB_VAR2(mode, modeOld);
        return;
    }

    PCO_FN2(error, msg, PCO_SetTimestampMode, m_handle, mode);
    PCO_PRINT_ERR(error, msg);
    err |= error;
    if (err)
        return;

    _pco_GetTimestampMode(modeNew, err);
    if (err)
        return;

#else

    err = camera->PCO_GetTimestampMode(&modeOld);
    PCO_CHECK_ERROR(err, "PCO_GetTimestampMode");

    err = camera->PCO_SetTimestampMode(mode);
    PCO_CHECK_ERROR(err, "PCO_SetTimestampMode");

    err = camera->PCO_GetTimestampMode(&modeNew);
    PCO_CHECK_ERROR(err, "PCO_GetTimestampMode");

#endif

    DEB_TRACE() << "\n   " << DEB_VAR3(mode, modeOld, modeNew);

    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetTimestampMode(WORD &mode, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

#ifndef __linux__
    char *msg;
    int error;
    err = error = 0;

    PCO_FN2(error, msg, PCO_GetTimestampMode, m_handle, &mode);
    PCO_PRINT_ERR(error, msg);
    err |= error;
    if (err)
        return;
#else

    err = camera->PCO_GetTimestampMode(&mode);
    PCO_CHECK_ERROR(err, "PCO_GetTimestampMode");

#endif

    DEB_TRACE() << "\n   " << DEB_VAR1(mode);
    return;
}
//=================================================================================================
//=================================================================================================
WORD Camera::_pco_GetRecordingState(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    WORD wRecState_actual;

#ifndef __linux__
    char *msg;
    int error;
    msg = _PcoCheckError(__LINE__, __FILE__,
                         PCO_GetRecordingState(m_handle, &wRecState_actual),
                         error);

    err = error;

    if (error)
    {
        printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
        throw LIMA_HW_EXC(Error, "PCO_GetRecordingState");
    }
#else
    err = camera->PCO_GetRecordingState(&wRecState_actual);
    PCO_CHECK_ERROR(err, "PCO_GetRecordingState");
#endif

    return wRecState_actual;
}
//=================================================================================================
//=================================================================================================

/**************************************************************************************************
        If a set recording status = [stop] command is sent and the current
status is already [stop]’ped, nothing will happen (only warning, error message).

        If the camera is in
        [run]’ing state, it will last some time (system delay + last image
readout), until the camera is stopped. The system delay depends on the PC and
the image readout depends on the image size transferred. The SetRecordingState =
[stop] checks for a stable stop state by calling GetRecordingState.  --- 165 ms

        Please call PCO_CancelImages to remove pending buffers from the driver.
--- 1.5 s
**************************************************************************************************/

const char *Camera::_pco_SetRecordingState(int state, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    long long usStart;

    WORD wRecState_new, wRecState_actual;

    wRecState_new = state ? 0x0001 : 0x0000; // 0x0001 => START acquisition

    usElapsedTimeSet(usStart);

    wRecState_actual = _pco_GetRecordingState(err);

#ifndef __linux__
    char *msg;

    _setCameraState(CAMSTATE_RECORD_STATE, !!(wRecState_actual));

    m_pcoData->traceAcq.usTicks[8].value = usElapsedTime(usStart);
    m_pcoData->traceAcq.usTicks[8].desc = "PCO_GetRecordingState execTime";
    usElapsedTimeSet(usStart);
#else
    //_setCameraState(CAMSTATE_RECORD_STATE, !!(wRecState_actual));

    m_pcoData->traceAcq.usTicks[traceAcq_GetRecordingState].value =
        usElapsedTime(usStart);
    m_pcoData->traceAcq.usTicks[traceAcq_GetRecordingState].desc =
        "PCO_GetRecordingState execTime";
    usElapsedTimeSet(usStart);
#endif

    if (wRecState_new == wRecState_actual)
    {
        DEB_TRACE() << "  SetRecordingState - BYPASSED "
                    << DEB_VAR2(wRecState_new, wRecState_actual);
        err = 0;
        return fnId;
    }

    // ------------------------------------------ cancel images
    if (wRecState_new == 0)
    {
        int count = 1;

        _setCameraState(CAMSTATE_RECORD_STATE, false);

#if 0
		PCO_FN2(error, msg,PCO_GetPendingBuffer, m_handle, &count);
		PCO_PRINT_ERR(error, msg); 	if(error) return msg;
#endif

#ifndef __linux__
        if (count)
        {
            DEB_TRACE() << ":  PCO_CancelImages() - CALLING";
            PCO_FN1(err, msg, PCO_CancelImages, m_handle);
            PCO_PRINT_ERR(err, msg);
            if (err)
                return msg;
        }
        else
        {
            DEB_TRACE() << ":  PCO_CancelImages() - BYPASSED";
        }
    }

    if (wRecState_new != wRecState_actual)
    {
        DEB_TRACE() << fnId << ": PCO_SetRecordingState "
                    << DEB_VAR1(wRecState_new);
        PCO_FN2(err, msg, PCO_SetRecordingState, m_handle, wRecState_new);
        PCO_PRINT_ERR(err, msg);
        if (err)
            return msg;

        if (_getDebug(DBG_TRACE_FIFO))
        {
            printf("---TRACE - PCO_SetRecordingState[%d]\n", wRecState_new);
        }
    }

    if (wRecState_new)
        m_sync->setExposing(pcoAcqRecordStart);

#else
        if (count && !_isCameraType(Edge))
        {
            DEB_TRACE() << fnId << ": PCO_CancelImages";

            err = camera->PCO_CancelImage();
            PCO_CHECK_ERROR(err, "PCO_CancelImage");

            err = camera->PCO_CancelImageTransfer();
            PCO_CHECK_ERROR(err, "PCO_CancelImageTransfer");
        }
    }

    err = camera->PCO_SetRecordingState(wRecState_new);
    PCO_CHECK_ERROR(err, "PCO_SetRecordingState");
#endif

    wRecState_actual = _pco_GetRecordingState(err);
    _setCameraState(CAMSTATE_RECORD_STATE, !!(wRecState_actual));

#ifndef __linux__
    m_pcoData->traceAcq.usTicks[9].value = usElapsedTime(usStart);
    m_pcoData->traceAcq.usTicks[9].desc = "PCO_SetRecordingState execTime";
    usElapsedTimeSet(usStart);
#else
    m_pcoData->traceAcq.usTicks[traceAcq_SetRecordingState].value =
        usElapsedTime(usStart);
    m_pcoData->traceAcq.usTicks[traceAcq_SetRecordingState].desc =
        "PCO_SetRecordingState execTime";
    usElapsedTimeSet(usStart);
#endif

    _armRequired(true);

#ifndef __linux__
    m_pcoData->traceAcq.usTicks[10].value = usElapsedTime(usStart);
    m_pcoData->traceAcq.usTicks[10].desc = "PCO_CancelImages execTime";
    usElapsedTimeSet(usStart);
#else
    m_pcoData->traceAcq.usTicks[traceAcq_CancelImages].value =
        usElapsedTime(usStart);
    m_pcoData->traceAcq.usTicks[traceAcq_CancelImages].desc =
        "PCO_CancelImages execTime";
    usElapsedTimeSet(usStart);
#endif

    // DEB_TRACE() << fnId << ": " << DEB_VAR4(error, state, wRecState_actual,
    // wRecState_new);
    return fnId;
}
//=================================================================================================
//=================================================================================================

//=================================================================================================
//=================================================================================================
int Camera::_pco_GetBitAlignment(int &alignment)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    int error = 0;
    WORD wBitAlignment;

#ifndef __linux__
    error = PCO_GetBitAlignment(m_handle, &wBitAlignment);
    error = PCO_CHECK_ERROR(error, "PCO_GetBitAlignment");
#else
    error = camera->PCO_GetBitAlignment(&wBitAlignment);
    PCO_CHECK_ERROR(error, "PCO_GetBitAlignment");
    PCO_THROW_OR_TRACE(error, "PCO_GetBitAlignment");
#endif

    if (error)
        return error;

    alignment = m_pcoData->wBitAlignment = wBitAlignment;

    return error;
}

//=================================================================================================
//=================================================================================================

// wBitAlignment:
// - 0x0000 = [MSB aligned]; all raw image data will be aligned to the MSB. This
// is thedefault setting.
// - 0x0001 = [LSB aligned]; all raw image data will be aligned to the LSB.

int Camera::_pco_SetBitAlignment(int alignment)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    int error = 0;
    WORD wBitAlignment;

    if ((alignment < 0) || (alignment > 1))
    {
        DEB_ALWAYS() << "ERROR - invalid value " << DEB_VAR1(alignment);
        return -1;
    }

    wBitAlignment = int(alignment);

#ifndef __linux__
    error = PCO_SetBitAlignment(m_handle, wBitAlignment);
    error = PCO_CHECK_ERROR(error, "PCO_SetBitAlignment");
#else
    error = camera->PCO_SetBitAlignment(wBitAlignment);
    const char *msg = "PCO_SetBitAlignment";
    PCO_CHECK_ERROR(error, msg);
    PCO_THROW_OR_TRACE(error, msg);
#endif

    return error;
}

//=================================================================================================
//=================================================================================================
//-------------------------------------------------------------------------------------------------
// PCO_SetADCOperation
// Set analog-digital-converter (ADC) operation for reading the image sensor
// data. Pixel data can be read out using one ADC (better linearity) or in
// parallel using two ADCs (faster). This option is only available for some
// camera models. If the user sets 2ADCs he must center and adapt the ROI to
// symmetrical values, e.g. pco.1600: x1,y1,x2,y2=701,1,900,500 (100,1,200,500
// is not possible).
//
// DIMAX -> 1 adc
//-------------------------------------------------------------------------------------------------
int Camera::_pco_GetADCOperation(int &adc_working, int &adc_max)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    int error;
    WORD wADCOperation;

    adc_max =
        m_pcoData->stcPcoDescription.wNumADCsDESC; // nr of ADC in the system

    if (adc_max == 2)
    {
#ifndef __linux__
        const char *msg;
        PCO_FN2(error, msg, PCO_GetADCOperation, m_handle, &wADCOperation);
#else
        error = camera->PCO_GetADCOperation(&wADCOperation);
        PCO_CHECK_ERROR(error, "PCO_GetADCOperation");
#endif

        if (error)
            wADCOperation = (WORD)1;
    }
    else
    {
        adc_max = 1;
        wADCOperation = (WORD)1;
    }

    adc_working = wADCOperation;
    m_pcoData->wNowADC = wADCOperation;

    return error;
}

//=================================================================================================
//=================================================================================================

int Camera::_pco_SetADCOperation(int adc_new, int &adc_working)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    int error, adc_max;

    error = _pco_GetADCOperation(adc_working, adc_max);

    DEB_TRACE() << fnId << ": " DEB_VAR2(adc_max, adc_working);

    if (error)
        return error;

    if ((adc_new >= 1) && (adc_new <= adc_max) && (adc_new != adc_working))
    {
#ifndef __linux__
        const char *msg;
        PCO_FN2(error, msg, PCO_SetADCOperation, m_handle, (WORD)adc_new);
#else
        error = camera->PCO_SetADCOperation((WORD)adc_new);
        PCO_CHECK_ERROR(error, "PCO_SetADCOperation");
#endif
        _pco_GetADCOperation(adc_working, adc_max);
    }
    m_pcoData->wNowADC = adc_working;
    return error;
}
//=================================================================================================
//=================================================================================================

bool Camera::_isCapsDesc(int caps)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    DWORD _dwCaps1;

#ifndef __linux__
    int error;
    _pco_GetGeneralCapsDESC(_dwCaps1, error);
    if (error)
    {
        DEB_ALWAYS() << "ERROR - _pco_GetGeneralCapsDESC";
        return false;
    }
#else
    _dwCaps1 = m_pcoData->stcPcoDescription.dwGeneralCaps1;
#endif

    switch (caps)
    {
        case capsCDI:
            return !!(_dwCaps1 & GENERALCAPS1_CDI_MODE);

        case capsDoubleImage:
            return !!(m_pcoData->stcPcoDescription.wDoubleImageDESC);

        case capsRollingShutter:
            return _isCameraType(Edge);

        case capsGlobalShutter:
            //#define GENERALCAPS1_NO_GLOBAL_SHUTTER                 0x00080000
            //// Camera does not support global shutter
            return _isCameraType(Edge) &&
                   !(_dwCaps1 & GENERALCAPS1_NO_GLOBAL_SHUTTER);

        case capsGlobalResetShutter:
            //#define GENERALCAPS1_GLOBAL_RESET_MODE                 0x00100000
            //// Camera supports global reset rolling readout
            return _isCameraType(Edge) &&
                   !(_dwCaps1 & GENERALCAPS1_NO_GLOBAL_SHUTTER) &&
                   !!(_dwCaps1 & GENERALCAPS1_GLOBAL_RESET_MODE);

        case capsHWIO:
            return !!(_dwCaps1 & GENERALCAPS1_HW_IO_SIGNAL_DESCRIPTOR);

        case capsCamRam:
            return ((_dwCaps1 & GENERALCAPS1_NO_RECORDER) == 0);

        case capsMetadata:
            return !!(_dwCaps1 & GENERALCAPS1_METADATA);

            //----------------------------------------------------------------------------------------------------------
            // dwGeneralCapsDESC1;      // General capabilities:
            //		Bit 3: Timestamp ASCII only available (Timestamp mode 3
            // enabled) 		Bit 8: Timestamp not available
            // m_pcoData->stcPcoDescription.dwGeneralCapsDESC1 & BIT3 / BIT8

        case capsTimestamp3:
            return (!(_dwCaps1 & BIT8)) && (_dwCaps1 & BIT3);

        case capsTimestamp:
            return !(_dwCaps1 & BIT8);

        default:
            return false;
    }
}

//=================================================================================================
//=================================================================================================
// SC2_SDK_FUNC int WINAPI PCO_GetCDIMode(HANDLE ph, WORD *wCDIMode);
// Gets the correlated double image mode of the camera, if available.
// Only available with a dimax
// In: HANDLE ph -> Handle to a previously opened camera.
//     WORD *wCDIMode -> Pointer to a WORD variable to receive the correlated
//     double image mode.
// Out: int -> Error message.

// SC2_SDK_FUNC int WINAPI PCO_SetCDIMode(HANDLE ph,  WORD wCDIMode);
// Sets the correlated double image mode of the camera, if available.
// Only available with a dimax
// In: HANDLE ph -> Handle to a previously opened camera.
//     WORD wCDIMode -> WORD variable to set the correlated double image mode.
// Out: int -> Error message.
// CDI mode is available if in the camera descriptor the flag

// Mode parameter is simple: 0 is off (default), 1 is on.
// CDI mode is available if in the camera descriptor the flag
// #define GENERALCAPS1_CDI_MODE                          0x00010000 // Camera
// has Correlated Double Image Mode in GENERALCAPS1 is set.

void Camera::_pco_GetCDIMode(WORD &wCDIMode, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    // char *pcoFn;
    int pcoErr;
    err = 0;

    if (!_isCapsDesc(capsCDI))
    {
        wCDIMode = 0;
        err = 1;
        DEB_TRACE() << "WARNING / CDI mode is NOT ALLOWED!";
        return;
    }

    err = 0;

#ifndef __linux__
    pcoErr = PCO_GetCDIMode(m_handle, &wCDIMode);
#else
    wCDIMode = 0;
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    pcoErr = -1;
#endif

    err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetCDIMode");

    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetCDIMode(WORD wCDIMode, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    // char *pcoFn;
    int pcoErr;
    err = 0;

    if (!_isCapsDesc(capsCDI))
    {
        err = 1;
        DEB_TRACE() << "WARNING / CDI mode is NOT ALLOWED!";
        return;
    }

    if (wCDIMode)
    {
        wCDIMode = 1;
        _pco_SetDoubleImageMode(0, err); // cdi and double image are exclusive
    }

    err = 0;

#ifndef __linux__
    pcoErr = PCO_SetCDIMode(m_handle, wCDIMode);
#else
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    pcoErr = -1;
#endif

    err |= PCO_CHECK_ERROR(pcoErr, "PCO_SetCDIMode");

    return;
}
//=================================================================================================
//=================================================================================================
void Camera::_pco_GetDoubleImageMode(WORD &wDoubleImage, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    // char *pcoFn;
    int pcoErr;
    err = 0;

    if (!_isCapsDesc(capsDoubleImage))
    {
        wDoubleImage = 0;
        err = 1;
        DEB_TRACE() << "WARNING / DoubleImage mode is NOT ALLOWED!";
        return;
    }

    err = 0;

#ifndef __linux__
    pcoErr = PCO_GetDoubleImageMode(m_handle, &wDoubleImage);
#else
    wDoubleImage = 0;
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    pcoErr = -1;
#endif

    err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetDoubleImageMode");

    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetDoubleImageMode(WORD wDoubleImage, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    // char *pcoFn;
    int pcoErr;
    err = 0;

    if (!_isCapsDesc(capsDoubleImage))
    {
        err = 1;
        DEB_TRACE() << "WARNING / DoubleImage mode is NOT ALLOWED!";
        return;
    }

    if (wDoubleImage)
    {
        wDoubleImage = 1;
        _pco_SetCDIMode(0, err); // cdi and double image are exclusive
    }

    err = 0;

#ifndef __linux__
    pcoErr = PCO_SetDoubleImageMode(m_handle, wDoubleImage);
#else
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    pcoErr = -1;
#endif

    err |= PCO_CHECK_ERROR(pcoErr, "PCO_SetDoubleImageMode");

    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetGeneralCapsDESC(DWORD &capsDesc1, int &err)
{
    err = 0;
#ifndef __linux__
    capsDesc1 = m_pcoData->stcPcoDescription.dwGeneralCapsDESC1;
#else
    capsDesc1 = m_pcoData->stcPcoDescription.dwGeneralCaps1;
#endif

    return;
}

//=================================================================================================
//=================================================================================================

// SC2_SDK_FUNC int WINAPI PCO_GetSegmentImageSettings(HANDLE ph, WORD wSegment,
//                                                    WORD* wXRes,
//                                                    WORD* wYRes,
//                                                    WORD* wBinHorz,
//                                                    WORD* wBinVert,
//                                                    WORD* wRoiX0,
//                                                    WORD* wRoiY0,
//                                                    WORD* wRoiX1,
//                                                    WORD* wRoiY1);
// Gets the sizes information for one segment. X0, Y0 start at 1. X1, Y1 end
// with max. sensor size. In: HANDLE ph -> Handle to a previously opened camera.
//     WORD *wXRes -> Pointer to a WORD variable to receive the x resolution of
//     the image in segment WORD *wYRes -> Pointer to a WORD variable to receive
//     the y resolution of the image in segment WORD *wBinHorz -> Pointer to a
//     WORD variable to receive the horizontal binning of the image in segment
//     WORD *wBinVert -> Pointer to a WORD variable to receive the vertical
//     binning of the image in segment WORD *wRoiX0 -> Pointer to a WORD
//     variable to receive the left x offset of the image in segment WORD
//     *wRoiY0 -> Pointer to a WORD variable to receive the upper y offset of
//     the image in segment WORD *wRoiX1 -> Pointer to a WORD variable to
//     receive the right x offset of the image in segment WORD *wRoiY1 ->
//     Pointer to a WORD variable to receive the lower y offset of the image in
//     segment
//      x0,y0----------|
//      |     ROI      |
//      ---------------x1,y1
// Out: int -> Error message.
// SC2_SDK_FUNC int WINAPI PCO_GetNumberOfImagesInSegment(HANDLE ph,
//                                             WORD wSegment,
//                                             DWORD* dwValidImageCnt,
//                                             DWORD* dwMaxImageCnt);
// Gets the number of images in the addressed segment.
// In: HANDLE ph -> Handle to a previously opened camera.
//     DWORD *dwValidImageCnt -> Pointer to a DWORD varibale to receive the
//     valid image count. DWORD *dwMaxImageCnt -> Pointer to a DWORD varibale to
//     receive the max image count.
// Out: int -> Error message.

void Camera::_pco_GetSegmentInfo(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    // char *pcoFn;
    int pcoErr;
    err = 0;

    err = 0;
    if (!_isCameraType(Dimax | Pco2k | Pco4k))
    {
        err = 1;
        return;
    }

#ifndef __linux__
    struct stcSegmentInfo *_stc;

    for (int iseg = 0; iseg < PCO_MAXSEGMENTS; iseg++)
    {
        _stc = &m_pcoData->m_stcSegmentInfo[iseg];

        _stc->iSegId = iseg + 1;
        _stc->wXRes = _stc->wYRes = _stc->wBinHorz = _stc->wBinVert =
            _stc->wRoiX0 = _stc->wRoiY0 = _stc->wRoiX1 = _stc->wRoiY1 = 0;
        _stc->dwValidImageCnt = _stc->dwMaxImageCnt = 0;
        _stc->iErr = 0;

        pcoErr = PCO_GetSegmentImageSettings(
            m_handle, iseg, &_stc->wXRes, &_stc->wYRes, &_stc->wBinHorz,
            &_stc->wBinVert, &_stc->wRoiX0, &_stc->wRoiY0, &_stc->wRoiX1,
            &_stc->wRoiY1);

        _stc->iErr |= pcoErr;
        err |= pcoErr;

        pcoErr = PCO_GetNumberOfImagesInSegment(
            m_handle, iseg, &_stc->dwValidImageCnt, &_stc->dwMaxImageCnt);

        _stc->iErr |= pcoErr;
        err |= pcoErr;
    }
#else
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    pcoErr = -1;
    err |= pcoErr;
#endif
    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_CloseCamera(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

#ifndef __linux__
    int pcoErr;

    // PCO_PRINT_ERR(error, msg);

    err = 0;
    pcoErr = PCO_CloseCamera(m_handle);
    err |= PCO_CHECK_ERROR(pcoErr, "PCO_CloseCamera");

    m_handle = NULL;
#else
    err = 0;

    if (grabber)
    {
        delete grabber;
        grabber = NULL;
    }

    if (camera)
    {
        err = camera->Close_Cam();
        PCO_CHECK_ERROR(err, "Close_Cam");

        delete camera;
        camera = NULL;
    }
#endif
    return;
}
//=================================================================================================
//=================================================================================================

bool Camera::_isCooledCamera()
{
    return !((m_pcoData->stcPcoDescription.sMinCoolSetDESC == 0) &&
             (m_pcoData->stcPcoDescription.sMaxCoolSetDESC == 0));
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetCoolingSetpointTemperature(int &val, int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

#ifndef __linux__
    char *pcoFn;
    SHORT _sSetpoint;

    if (!_isCooledCamera())
    {
        error = -1;
        val = 0;
        return;
    }

    PCO_FN2(error, pcoFn, PCO_GetCoolingSetpointTemperature, m_handle,
            &_sSetpoint);
    if (error)
    {
        val = 0;
        return;
    }

    val = _sSetpoint;

#else
    error = -1;

    DEB_ALWAYS() << NOT_IMPLEMENTED;

    val = 0;
    return;

#endif
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetCoolingSetpointTemperature(int val, int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

#ifndef __linux__
    char *pcoFn;
    SHORT _sSetpoint = val;

    if (!_isCooledCamera() ||
        (_sSetpoint < m_pcoData->stcPcoDescription.sMinCoolSetDESC) ||
        (_sSetpoint > m_pcoData->stcPcoDescription.sMaxCoolSetDESC))
    {
        error = -1;
        return;
    }

    PCO_FN2(error, pcoFn, PCO_SetCoolingSetpointTemperature, m_handle,
            _sSetpoint);
    if (error)
    {
        return;
    }

#else
    error = -1;

    DEB_ALWAYS() << NOT_IMPLEMENTED;

    return;

#endif
}

//=================================================================================================
//=================================================================================================
// SC2_SDK_FUNC int WINAPI PCO_GetInfoString(HANDLE ph, DWORD dwinfotype, char
// *buf_in, WORD size_in);
// Gets the name of the camera.
// In: HANDLE ph -> Handle to a previously opened camera.
//     DWORD dwinfotype -> 0: Camera and interface name
//                         1: Camera name only
//                         2: Sensor name
//     char *buf_in -> Pointer to a string, to receive the info string.
//     WORD size_in -> WORD variable which holds the maximum length of the
//     string.
// Out: int -> Error message.

void Camera::_pco_GetInfoString(int infotype, char *buf_in, int size_in,
                                int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

#ifndef __linux__
    DWORD dwinfotype = (DWORD)infotype;
    WORD wsize_in = (WORD)size_in;

    error = PCO_GetInfoString(m_handle, dwinfotype, buf_in, wsize_in);
    PCO_CHECK_ERROR(error, "PCO_GetInfoString");
    if (error)
    {
        *buf_in = 0;
    }

#else
    error = -1;
    *buf_in = 0;
    DEB_ALWAYS() << NOT_IMPLEMENTED;

#endif
}

//=================================================================================================
// only in linux - to merge
//=================================================================================================
void Camera::_pco_SetCameraToCurrentTime(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

#ifdef __linux__
    // Sets the camera time to current system time.
    //
    // The date and time is updated automatically, as long as the camera is
    // supplied with power. Camera time is used for the timestamp and metadata.
    // When powering up the camera, then this command or PCO_SetDateTime should
    // be done once. return: Error code

    err = camera->PCO_SetCameraToCurrentTime();

#else
    {
        // set date/time to PCO
        struct tm tmNow;
        time_t now = time(NULL);
        int day, mon, year, hour, min, sec;

        localtime_s(&tmNow, &now);

        BYTE ucDay = day = tmNow.tm_mday;
        BYTE ucMonth = mon = tmNow.tm_mon + 1;
        WORD wYear = year = tmNow.tm_year + 1900;
        WORD wHour = hour = tmNow.tm_hour;
        BYTE ucMin = min = tmNow.tm_min;
        BYTE ucSec = sec = tmNow.tm_sec;

        err = PCO_SetDateTime(m_handle, ucDay, ucMonth, wYear, wHour, ucMin,
                              ucSec);

        // DEB_TRACE() << DEB_VAR6(day, mon, year, hour, min, sec);
    }
#endif

    PCO_CHECK_ERROR(err, "PCO_SetCameraToCurrentTime");

    return;
}

//=================================================================================================
//=================================================================================================
// SC2_SDK_FUNC int WINAPI PCO_SetBinning(HANDLE ph, WORD wBinHorz, WORD
// wBinVert)

void Camera::_pco_SetBinning(Bin binNew, Bin &binActual, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    WORD wBinHorz, wBinVert;
    Bin binOld;
    int err0;

    _pco_GetBinning(binOld, err);

    if (binOld == binNew)
    {
        binActual = binOld;
        err = 0;
        return;
    }
    wBinHorz = binNew.getX();
    wBinVert = binNew.getY();

#ifndef __linux__
    err = PCO_SetBinning(m_handle, wBinHorz, wBinVert);

    if (PCO_CHECK_ERROR(err, "PCO_SetBinning"))
    {
        DEB_ALWAYS() << "ERROR - PCO_SetBinning";
    }

#else
    err = camera->PCO_SetBinning(wBinHorz, wBinVert);
    PCO_CHECK_ERROR(err, "PCO_SetBinning");
    PCO_THROW_OR_TRACE(err, "PCO_SetBinning");

#endif
    _armRequired(true);

    _pco_GetBinning(binActual, err0);
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetCamLinkSetImageParameters(int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

#ifndef __linux__

    // camLink -> imgPar
    // GigE    -> imgPar
    switch (_getInterfaceType())
    {
        case INTERFACE_CAMERALINK:
        case INTERFACE_ETHERNET:
            WORD wXres, wYres;

            WORD _wMaxWidth, _wMaxHeight;
            _pco_GetSizes(&wXres, &wYres, &_wMaxWidth, &_wMaxHeight, error);

            DEB_ALWAYS() << "ERROR PCO_CamLinkSetImageParameters: "
                         << DEB_VAR2(wXres, wYres);

            error = PCO_CamLinkSetImageParameters(m_handle, wXres, wYres);
            if (error)
            {
                throw LIMA_HW_EXC(Error, "PCO_CamLinkSetImageParameters");
            }

        default:
            break;
    } // switch

#else
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    error = -1;
#endif
    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetHWIOSignalAll(int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    if (!_isCapsDesc(capsHWIO))
    {
        error = -1;
        return;
    }

#ifndef __linux__
    const char *msg;

    int i, imax;
    error = 0;

    PCO_FN2(error, msg, PCO_GetHWIOSignalCount, m_handle,
            &m_pcoData->wNrPcoHWIOSignal0);
    if (error)
        return;

    imax = m_pcoData->wNrPcoHWIOSignal =
        (m_pcoData->wNrPcoHWIOSignal0 <= SIZEARR_stcPcoHWIOSignal)
            ? m_pcoData->wNrPcoHWIOSignal0
            : SIZEARR_stcPcoHWIOSignal;

    // DEB_TRACE()  << "--- size" << DEB_VAR3(imax, m_pcoData->wNrPcoHWIOSignal0
    // , m_pcoData->wNrPcoHWIOSignal ) ;

    for (i = 0; i < imax; i++)
    {
        // DEB_TRACE()  << "---  descriptor" << DEB_VAR2(i,
        // m_pcoData->stcPcoHWIOSignalDesc[i].wSize) ;
        PCO_FN3(error, msg, PCO_GetHWIOSignalDescriptor, m_handle, i,
                &m_pcoData->stcPcoHWIOSignalDesc[i]);
        if (error)
            return;

        // DEB_TRACE()  << "---  signal" << DEB_VAR2(i,
        // m_pcoData->stcPcoHWIOSignal[i].wSize) ;
        PCO_FN3(error, msg, PCO_GetHWIOSignal, m_handle, i,
                &m_pcoData->stcPcoHWIOSignal[i]);
        if (error)
            return;
    }
    return;

#else
    int errorTot;
    errorTot = 0;
    const char *msg __attribute__((unused));

    int iSignal, iSignalMax;

    if (!(_isCameraType(Dimax | Edge | Pco2k | Pco4k)))
    {
        errorTot = -1;
        return;
    }

    error = camera->PCO_GetHWIOSignalCount(&m_pcoData->wNrPcoHWIOSignal0);
    msg = "PCO_GetHWIOSignalCount";
    PCO_CHECK_ERROR(error, msg);
    errorTot |= error;

    iSignalMax = m_pcoData->wNrPcoHWIOSignal =
        (m_pcoData->wNrPcoHWIOSignal0 <= SIZEARR_stcPcoHWIOSignal)
            ? m_pcoData->wNrPcoHWIOSignal0
            : SIZEARR_stcPcoHWIOSignal;

    DEB_ALWAYS() << "--- size"
                 << DEB_VAR3(iSignalMax, m_pcoData->wNrPcoHWIOSignal0,
                             m_pcoData->wNrPcoHWIOSignal);

    WORD wEnabled, wType, wPolarity, wFilterSetting, wSelected;

    for (iSignal = 0; iSignal < iSignalMax; iSignal++)
    {
        int sizeName = SIZESTR_PcoHWIOSignal;
        char *ptrName = &(m_pcoData->sPcoHWIOSignalDesc[iSignal][0]);

        DEB_ALWAYS() << "---  descriptor"
                     << DEB_VAR2(
                            iSignal,
                            m_pcoData->stcPcoHWIOSignalDesc[iSignal].wSize);

        // telegram structure 4 signals * 24 char
        // memset(&m_pcoData->stcPcoHWIOSignalDesc[iSignal].szSignalName[0][0],0,24*4);

        error = camera->PCO_GetHWIOSignalDescriptor(
            iSignal, (SC2_Get_HW_IO_Signal_Descriptor_Response *)&m_pcoData
                         ->stcPcoHWIOSignalDesc[iSignal]);
        msg = "PCO_GetHWIOSignalDescriptor (struct)";
        PCO_CHECK_ERROR(error, msg);
        errorTot |= error;

        // DEB_ALWAYS()  << "---  signal" << DEB_VAR2(iSignal,
        // m_pcoData->stcPcoHWIOSignal[iSignal].wSize) ;

        // DWORD PCO_GetHWIOSignalDescriptor ( WORD SignalNum, char &outbuf, int
        // &size )

        // Gets the signal descriptor of the requested signal number as a string
        // for console output.
        *ptrName = 0;
        error =
            camera->PCO_GetHWIOSignalDescriptor(iSignal, ptrName, &sizeName);
        msg = "PCO_GetHWIOSignalDescriptor (name)";
        PCO_CHECK_ERROR(error, msg);
        errorTot |= error;

        DEB_ALWAYS() << "---  signal name "
                     << DEB_VAR3(iSignal, sizeName, ptrName);

        // m_pcoData->stcPcoHWIOSignal[iSignal].wSignalNum = iSignal;

        if (_isCameraType(Dimax | Edge))
        {
            error = camera->PCO_GetHWIOSignal(iSignal, &wEnabled, &wType,
                                              &wPolarity, &wFilterSetting,
                                              &wSelected);
            msg = "PCO_GetHWIOSignal";
            PCO_CHECK_ERROR(error, msg);

            // PCO3(error, msg,PCO_GetHWIOSignal, m_handle, iSignal,
            // &m_pcoData->stcPcoHWIOSignal[i]);
            errorTot |= error;

            m_pcoData->stcPcoHWIOSignal[iSignal].wEnabled = wEnabled;
            m_pcoData->stcPcoHWIOSignal[iSignal].wType = wType;
            m_pcoData->stcPcoHWIOSignal[iSignal].wPolarity = wPolarity;
            m_pcoData->stcPcoHWIOSignal[iSignal].wFilterSetting =
                wFilterSetting;
            m_pcoData->stcPcoHWIOSignal[iSignal].wSelected = wSelected;

            DEB_ALWAYS() << "---  "
                         << DEB_VAR6(iSignal, wEnabled, wType, wPolarity,
                                     wFilterSetting, wSelected);
        }
    }

    error = errorTot;

#endif
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetNumberOfImagesInSegment(WORD wSegment,
                                             DWORD &dwValidImageCnt,
                                             DWORD &dwMaxImageCnt, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    err = 0;

    if ((!_isCameraType(Dimax | Pco2k | Pco4k)) ||
        ((wSegment > PCO_MAXSEGMENTS) || (wSegment < 1)))
    {
        err = -1;
        dwValidImageCnt = dwMaxImageCnt = 0;
        return;
    }

#ifndef __linux__

    // char *pcoFn;

    err = PCO_GetNumberOfImagesInSegment(m_handle, wSegment, &dwValidImageCnt,
                                         &dwMaxImageCnt);

#else
    // DWORD PCO_GetNumberOfImagesInSegment ( WORD wSegment, DWORD *dwValid,
    // DWORD *dwMax )

    err = camera->PCO_GetNumberOfImagesInSegment(wSegment, &dwValidImageCnt,
                                                 &dwMaxImageCnt);
    PCO_CHECK_ERROR(err, "PCO_GetNumberOfImagesInSegment");

#endif

    if (err)
    {
        dwValidImageCnt = dwMaxImageCnt = 0;
    }

    return;
}
//=========================================================================================================
//=========================================================================================================
unsigned long Camera::_pco_GetNumberOfImagesInSegment_MaxCalc(int segmentPco)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    unsigned long framesMax;

    // if(_isCameraType(Edge)) {return LONG_MAX;}

    if (!_isCameraType(Dimax | Pco2k | Pco4k))
    {
        // DEB_WARNING() << "ERROR camera not valid for this function: " <<
        // _getCameraType();
        return 0;
    }

    if ((segmentPco < 1) || (segmentPco > PCO_MAXSEGMENTS))
    {
        DEB_ALWAYS() << "ERROR invalid segment: " << DEB_VAR1(segmentPco);
        return 0;
    }

#ifdef __linux__

    int error;
    DWORD dwValid, dwMax;

    _pco_GetNumberOfImagesInSegment((WORD)segmentPco, dwValid, dwMax, error);

    if (error)
    {
        framesMax = 0;
        DEB_ALWAYS() << "ERROR _pco_GetNumberOfImagesInSegment()";
    }
    else
    {
        framesMax = dwMax;
    }

#else
    int error;
    int segmentArr = segmentPco - 1;
    unsigned long xroisize, yroisize;
    unsigned long long pixPerFrame, pagesPerFrame;
    WORD _pixPerPage;

    WORD wDoubleImage;
    WORD wCDIMode;

    Roi limaRoi;
    _pco_GetROI(limaRoi, error);

    xroisize = limaRoi.getSize().getWidth();
    yroisize = limaRoi.getSize().getHeight();

    // xroisize = m_roi.x[1] - m_roi.x[0] + 1;
    // yroisize = m_roi.y[1] - m_roi.y[0] + 1;

    // Get CDIMode and DoubleImageMode values
    _pco_GetDoubleImageMode(wDoubleImage, error);
    _pco_GetCDIMode(wCDIMode, error);

    if(wDoubleImage == 1 || wCDIMode == 1) // If CDIMode or DoubleImageMode are enabled, each image use twice as more pixels
    {
        pixPerFrame = (unsigned long long)xroisize * (unsigned long long)yroisize * 2;
    }
    else
    {
        pixPerFrame = (unsigned long long)xroisize * (unsigned long long)yroisize;
    }

    if (pixPerFrame < 0)
    {
        printf("=== %s> ERROR pixPerFrame[%lld]\n", fnId, pixPerFrame);
        return 0;
    }

    if (m_pcoData->wPixPerPage < 1)
    {
        printf("=== %s> ERROR m_pcoData->wPixPerPage[%d]\n", fnId,
               m_pcoData->wPixPerPage);
        return 0;
    }

    if (_isCameraType(Dimax))
    {
        _pixPerPage = m_pcoData->wPixPerPage*1.9; // Taking into account compression (PCO use proprietary compression algorithm to store twice as much images)
    }
    else
    {
        _pixPerPage = m_pcoData->wPixPerPage;
    }

    pagesPerFrame = (pixPerFrame / _pixPerPage) + 1;
    if (pixPerFrame % _pixPerPage)
        pagesPerFrame++;

    framesMax = m_pcoData->dwMaxFramesInSegment[segmentArr] =
        (unsigned long)(((long long)m_pcoData->dwSegmentSize[segmentArr]) /
                        pagesPerFrame);
#endif

    return framesMax;
}
//=================================================================================================
//=================================================================================================
void Camera::_pco_GetTemperatureInfo(int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

#ifdef __linux__
    error = camera->PCO_GetTemperature(&sTempCcd, &sTempCam, &sTempPS);
    PCO_CHECK_ERROR(error, "PCO_GetTemperature");
    if (error)
    {
        sTempCcd = sTempCam = sTempPS = 0;
    }

    error = camera->PCO_GetCoolingSetpointTemperature(&sCoolingSetpoint);
    PCO_CHECK_ERROR(error, "PCO_GetCoolingSetpointTemperature");
    if (error)
    {
        sCoolingSetpoint = 0;
    }

    DEB_ALWAYS() << "\n   " << DEB_VAR1(sTempCcd) << "\n   "
                 << DEB_VAR1(sTempCam) << "\n   " << DEB_VAR1(sTempPS)
                 << "\n   " << DEB_VAR1(sCoolingSetpoint);

    m_pcoData->temperature.sCcd = sTempCcd;
    m_pcoData->temperature.sCam = sTempCam;
    m_pcoData->temperature.sPower = sTempPS;

#else

    char msg[MSG_SIZE + 1];

    // -- Print out current temperatures
    error = PCO_GetTemperature(m_handle, &m_pcoData->temperature.sCcd,
                               &m_pcoData->temperature.sCam,
                               &m_pcoData->temperature.sPower);
    if (error)
        return;
    // PCO_THROW_OR_TRACE(error, "PCO_GetTemperature") ;

    __sprintfSExt(msg, MSG_SIZE, "* temperature: CCD[%.1f]  CAM[%d]  PS[%d]\n",
                  m_pcoData->temperature.sCcd / 10.,
                  m_pcoData->temperature.sCam, m_pcoData->temperature.sPower);
    // DEB_TRACE() <<   msg;
    m_log.append(msg);

    m_pcoData->temperature.sMinCoolSet =
        m_pcoData->stcPcoDescription.sMinCoolSetDESC;
    m_pcoData->temperature.sMaxCoolSet =
        m_pcoData->stcPcoDescription.sMaxCoolSetDESC;
    m_pcoData->temperature.sDefaultCoolSet =
        m_pcoData->stcPcoDescription.sDefaultCoolSetDESC;

    // SC2_SDK_FUNC int WINAPI PCO_GetCoolingSetpointTemperature(HANDLE ph,
    // SHORT* sCoolSet)

    if ((m_pcoData->temperature.sMinCoolSet == 0) &&
        (m_pcoData->temperature.sMaxCoolSet == 0)) // no cooled camera
    {
        m_pcoData->temperature.sSetpoint = 0;
        __sprintfSExt(msg, MSG_SIZE, "*     cooling: NO cooled camera");
        m_log.append(msg);
        return;
    }

    error = PCO_GetCoolingSetpointTemperature(
        m_handle, &m_pcoData->temperature.sSetpoint);

    __sprintfSExt(msg, MSG_SIZE,
                  "*     cooling: min[%d] max[%d] default[%d] setpoint[%d]\n",
                  m_pcoData->temperature.sMinCoolSet,
                  m_pcoData->temperature.sMaxCoolSet,
                  m_pcoData->temperature.sDefaultCoolSet,
                  m_pcoData->temperature.sSetpoint);
    m_log.append(msg);

#endif
    return;
}
//=================================================================================================
//=================================================================================================
void Camera::_pco_GetTemperatureInfo(char *ptr, char *ptrMax, int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

#ifndef __linux__

    // -- Print out current temperatures
    error = PCO_GetTemperature(m_handle, &m_pcoData->temperature.sCcd,
                               &m_pcoData->temperature.sCam,
                               &m_pcoData->temperature.sPower);
    if (error)
    {
        ptr += __sprintfSExt(ptr, ptrMax - ptr, "[SDK error - getTemperature]");
        return;
    }
    ptr += __sprintfSExt(ptr, ptrMax - ptr, "CCD[%.1f] CAM[%d] PS[%d]",
                         m_pcoData->temperature.sCcd / 10.,
                         m_pcoData->temperature.sCam,
                         m_pcoData->temperature.sPower);

    m_pcoData->temperature.sMinCoolSet =
        m_pcoData->stcPcoDescription.sMinCoolSetDESC;
    m_pcoData->temperature.sMaxCoolSet =
        m_pcoData->stcPcoDescription.sMaxCoolSetDESC;
    m_pcoData->temperature.sDefaultCoolSet =
        m_pcoData->stcPcoDescription.sDefaultCoolSetDESC;

    // SC2_SDK_FUNC int WINAPI PCO_GetCoolingSetpointTemperature(HANDLE ph,
    // SHORT* sCoolSet)

    if ((m_pcoData->temperature.sMinCoolSet == 0) &&
        (m_pcoData->temperature.sMaxCoolSet == 0)) // no cooled camera
    {
        m_pcoData->temperature.sSetpoint = 0;
        ptr += __sprintfSExt(ptr, ptrMax - ptr, " cooling: [NO cooled camera]");
        return;
    }

    error = PCO_GetCoolingSetpointTemperature(
        m_handle, &m_pcoData->temperature.sSetpoint);
    if (error)
    {
        ptr += __sprintfSExt(ptr, ptrMax - ptr, " [SDK error - getSetpoint]");
        return;
    }

    ptr += __sprintfSExt(
        ptr, ptrMax - ptr, " cooling: min[%d] max[%d] default[%d] setpoint[%d]",
        m_pcoData->temperature.sMinCoolSet, m_pcoData->temperature.sMaxCoolSet,
        m_pcoData->temperature.sDefaultCoolSet,
        m_pcoData->temperature.sSetpoint);

    return;

#else
    error = -1;
    ptr += __sprintfSExt(ptr, ptrMax - ptr, NOT_IMPLEMENTED);

    return;
#endif
}
//=================================================================================================
//=================================================================================================
void Camera::_pco_GetCameraType(int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

#ifndef __linux__
    char *msg;

    int errTot = 0;
    bool errTotPcoSdk = false;

    m_pcoData->frames_per_buffer = 1; // for PCO DIMAX

    // --- Get camera type
    {
        const char *ptr;

        // m_pcoData->stcPcoCamType.wSize= sizeof(m_pcoData->stcPcoCamType);

        // OBSOLETE function after sdk 120
        // error = PCO_GetGigEIPAddress(m_handle, &m_pcoData->ipField[0],
        // &m_pcoData->ipField[1], &m_pcoData->ipField[2],
        // &m_pcoData->ipField[3]);  if(error) {m_pcoData->ipField[0] =
        // m_pcoData->ipField[1] = m_pcoData->ipField[2] =m_pcoData->ipField[3]=
        // 0;}
        m_pcoData->ipField[0] = m_pcoData->ipField[1] = m_pcoData->ipField[2] =
            m_pcoData->ipField[3] = 0;

        _pco_FillStructures(errTot);

        // PCO_GETGENERAL(hCam, &strGeneral)
        // -- Get General
        // PCO_GETCAMERATYPE(hCam, &strCamType)
        // PCO_GETSENSORSTRUCT(hCam, &strSensor)
        // -- Get Sensor struct
        // PCO_GETCAMERADESCRIPTION(hCam, &strDescription)
        // -- Get camera description
        // PCO_GETTIMINGSTRUCT(hCam, &strTiming)
        // -- Get timing struct
        // PCO_GETRECORDINGSTRUCT(hCam, &strRecording)
        // -- Get recording struct
        // -- Get storage struct

        ptr = _xlatPcoCode2Str(_getCameraType(), ModelType, error);
        errTot |= error;
        strcpy_s(m_pcoData->model, MODEL_TYPE_SIZE, ptr);

        ptr = _xlatPcoCode2Str(_getCameraSubType(), ModelSubType, error);
        strcpy_s(m_pcoData->modelSubType, MODEL_SUBTYPE_SIZE, ptr);
        errTot |= error;

        ptr = _xlatPcoCode2Str(_getInterfaceType(), InterfaceType, error);
        strcpy_s(m_pcoData->iface, INTERFACE_TYPE_SIZE, ptr);
        errTot |= error;

        __sprintfSExt(m_pcoData->camera_name, CAMERA_NAME_SIZE,
                      "%s (%s) (I/F %s) (SN %d)", _getCameraTypeStr(),
                      _getCameraSubTypeStr(), _getInterfaceTypeStr(),
                      _getCameraSerialNumber());
        DEB_TRACE()
            << DEB_VAR2(_getCameraTypeStr(), _getInterfaceTypeStr()) << "\n"
            << "\n====================== CAMERA FOUND ======================"
            << "\n* " << _getCameraIdn()
            << "\n=========================================================="
            << "\n";

        // if(errTot) return m_pcoData->camera_name;
    }

    // -- Reset to default settings

    PCO_FN2(error, msg, PCO_SetRecordingState, m_handle, 0);
    errTotPcoSdk = errTotPcoSdk || error; // if(error) return msg;

    PCO_FN1(error, msg, PCO_ResetSettingsToDefault, m_handle);
    PCO_PRINT_ERR(error, msg);
    errTotPcoSdk = errTotPcoSdk || error; // if(error) return msg;

    // callback to update in lima the valid_ranges from the last
    // stcPcoDescription read
    if (m_sync)
    {
        HwSyncCtrlObj::ValidRangesType valid_ranges;
        m_sync->getValidRanges(valid_ranges);     // from stcPcoDescription
        m_sync->validRangesChanged(valid_ranges); // callback
        DEB_TRACE() << fnId << ": callback - new valid_ranges: "
                    << DEB_VAR1(valid_ranges);
    }

    // get the max CAMERA pixel rate (Hz) from the description structure
    m_pcoData->dwPixelRateMax = 0;
    for (int i = 0; i < 4; i++)
    {
        if (m_pcoData->dwPixelRateMax <
            m_pcoData->stcPcoDescription.dwPixelRateDESC[i])
            m_pcoData->dwPixelRateMax =
                m_pcoData->stcPcoDescription.dwPixelRateDESC[i];
    }

    m_pcoData->bMetaDataAllowed = _isCapsDesc(capsMetadata);

    if (errTotPcoSdk)
    {
        DEB_ALWAYS() << "ERRORs in SDK functions!!!";
    }

    if (errTot)
    {
        DEB_ALWAYS() << "ERRORs in types!!!";
    }

    return;

#else
    const char *ptr;
    int errTot = 0;

    //------------------ GigE
    //------------------ PCO_GetCameraType ---> camtype, serialnumber, iftype
    //------------------ PCO_GetInfo idem

    WORD camtype;
    WORD iftype = 0;
    DWORD serialnumber;

    m_pcoData->frames_per_buffer = 1; // for PCO DIMAX

    DEB_ALWAYS() << fnId;

    if (1)
    {
        m_pcoData->ipField[0] = m_pcoData->ipField[1] = m_pcoData->ipField[2] =
            m_pcoData->ipField[3] = 0;
    }

    error = camera->PCO_GetCameraType(&camtype, &serialnumber, &iftype);
    PCO_CHECK_ERROR(error, "PCO_GetCameraType");
    if (error)
        return;

    m_pcoData->stcPcoCamType.wCamType = m_pcoData->wCamType = camtype;
    m_pcoData->stcPcoCamType.wCamSubType = 0;
    m_pcoData->stcPcoCamType.dwSerialNumber = m_pcoData->dwSerialNumber =
        serialnumber;
    m_pcoData->stcPcoCamType.wInterfaceType = m_pcoData->wIfType = iftype;

    ptr = _xlatPcoCode2Str(camtype, ModelType, error);
    strcpy_s(m_pcoData->model, MODEL_TYPE_SIZE, ptr);
    errTot |= error;

    ptr = _xlatPcoCode2Str(iftype, InterfaceType, error);
    strcpy_s(m_pcoData->iface, INTERFACE_TYPE_SIZE, ptr);
    errTot |= error;

    __sprintfSExt(m_pcoData->camera_name, CAMERA_NAME_SIZE,
                  "%s (IF %s) (SN %u)", m_pcoData->model, m_pcoData->iface,
                  m_pcoData->dwSerialNumber);
    DEB_ALWAYS() << "\n   " << DEB_VAR1(m_pcoData->model) << "\n   "
                 << DEB_VAR1(m_pcoData->iface) << "\n   "
                 << DEB_VAR1(m_pcoData->camera_name);

    error = camera->PCO_GetInfo(0, &m_pcoData->nameCamIf,
                                sizeof(m_pcoData->nameCamIf) - 1);
    error = camera->PCO_GetInfo(1, &m_pcoData->nameCam,
                                sizeof(m_pcoData->nameCam) - 1);
    error = camera->PCO_GetInfo(2, &m_pcoData->nameSensor,
                                sizeof(m_pcoData->nameSensor) - 1);

    DEB_ALWAYS() << "\n   " << DEB_VAR1(m_pcoData->nameCamIf) << "\n   "
                 << DEB_VAR1(m_pcoData->nameCam) << "\n   "
                 << DEB_VAR1(m_pcoData->nameSensor);

#endif

    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetMetaDataMode(WORD wMetaDataMode, int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    if (!_isCameraType(Dimax))
    {
        error = 0;
        return;
    }
#ifndef __linux__
    char *msg;
    WORD _wMetaDataMode, _wMetaDataSize, _wMetaDataVersion;

    error = 0;

    m_pcoData->wMetaDataSize = m_pcoData->wMetaDataVersion =
        m_pcoData->wMetaDataMode = 0;

    if (!m_pcoData->bMetaDataAllowed)
    {
        error = -1;
        return;
    }

    PCO_FN4(error, msg, PCO_GetMetaDataMode, m_handle, &_wMetaDataMode,
            &_wMetaDataSize, &_wMetaDataVersion);
    PCO_PRINT_ERR(error, msg);
    if (error)
        return;

    m_pcoData->wMetaDataSize = _wMetaDataSize;
    m_pcoData->wMetaDataVersion = _wMetaDataVersion;
    m_pcoData->wMetaDataMode = _wMetaDataMode;

    // now pco edge also allows metatada (not only dimax)
    // if(_isCameraType(Dimax)) {

    PCO_FN4(error, msg, PCO_SetMetaDataMode, m_handle, wMetaDataMode,
            &_wMetaDataSize, &_wMetaDataVersion);
    PCO_PRINT_ERR(error, msg);
    if (error)
        return;

    m_pcoData->wMetaDataSize = _wMetaDataSize;
    m_pcoData->wMetaDataVersion = _wMetaDataVersion;
    m_pcoData->wMetaDataMode = wMetaDataMode;

#else
    const char *msg;

    m_pcoData->wMetaDataSize = m_pcoData->wMetaDataVersion = 0;
    if (_isCameraType(Dimax))
    {
        m_pcoData->wMetaDataMode = wMetaDataMode;

        error = camera->PCO_SetMetadataMode(wMetaDataMode,
                                            &m_pcoData->wMetaDataSize,
                                            &m_pcoData->wMetaDataVersion);
        msg = "PCO_SetMetadataMode";
        PCO_CHECK_ERROR(error, msg);
        if (error)
            return;
    }

#endif
    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetPixelRate(int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    error = 0;
    const char *msg;
    DWORD _dwPixelRate, _dwPixelRateOld, _dwPixelRateReq;
    DWORD _dwPixelRateMax;

#ifndef __linux__

    if (_isCameraType(Edge))
    {
        PCO_FN2(error, msg, PCO_GetPixelRate, m_handle,
                &m_pcoData->dwPixelRate);
        PCO_THROW_OR_TRACE(error, msg);

        _dwPixelRateOld = m_pcoData->dwPixelRate;
        _dwPixelRateReq = m_pcoData->dwPixelRateRequested;
        DEB_TRACE() << "PIXEL rate (actual/req): "
                    << DEB_VAR2(_dwPixelRateOld, _dwPixelRateReq);

        if (_isValid_pixelRate(_dwPixelRateReq) &&
            (_dwPixelRateOld != _dwPixelRateReq))
        {
            PCO_FN2(error, msg, PCO_SetPixelRate, m_handle, _dwPixelRateReq);
            PCO_THROW_OR_TRACE(error, msg);

            PCO_FN2(error, msg, PCO_GetPixelRate, m_handle,
                    &m_pcoData->dwPixelRate);
            PCO_THROW_OR_TRACE(error, msg);

            _dwPixelRate = m_pcoData->dwPixelRate;
            DEB_TRACE() << "PIXEL rate SET (old/new): "
                        << DEB_VAR2(_dwPixelRateOld, _dwPixelRate);

            _armRequired(true);
        }
        m_pcoData->dwPixelRateRequested = 0;
        return;
    }

    if (_isCameraType(Pco2k | Pco4k))
    {
        PCO_FN2(error, msg, PCO_GetPixelRate, m_handle,
                &m_pcoData->dwPixelRate);
        PCO_THROW_OR_TRACE(error, msg);

        _dwPixelRateOld = m_pcoData->dwPixelRate;
        _dwPixelRateMax = m_pcoData->dwPixelRateMax;
        _dwPixelRateReq = m_pcoData->dwPixelRateRequested;

        DEB_TRACE() << "PIXEL rate (requested/actual/max): "
                    << DEB_VAR3(_dwPixelRateReq, _dwPixelRateOld,
                                _dwPixelRateMax);

        if (_isValid_pixelRate(_dwPixelRateReq) &&
            (_dwPixelRateOld != _dwPixelRateReq))
        {
            // if(_dwPixelRateMax > _dwPixelRateOld) {

            PCO_FN2(error, msg, PCO_SetPixelRate, m_handle, _dwPixelRateReq);
            PCO_THROW_OR_TRACE(error, msg);

            PCO_FN2(error, msg, PCO_GetPixelRate, m_handle,
                    &m_pcoData->dwPixelRate);
            PCO_THROW_OR_TRACE(error, msg);

            _dwPixelRate = m_pcoData->dwPixelRate;
            DEB_TRACE() << "PIXEL rate SET (old/new): "
                        << DEB_VAR2(_dwPixelRateOld, _dwPixelRate);

            _armRequired(true);
        }
        return;
    }
#else
    DWORD _dwPixelRateNext;

    if (_isCameraType(Edge))
    {
        _pco_GetPixelRate(m_pcoData->dwPixelRate, _dwPixelRateNext, error);
        PCO_THROW_OR_TRACE(error, "_pco_GetPixelRate");

        _dwPixelRateOld = m_pcoData->dwPixelRate;
        _dwPixelRateReq = m_pcoData->dwPixelRateRequested;
        // DEB_ALWAYS() << "PIXEL rate (actual/req): " <<
        // DEB_VAR2(_dwPixelRateOld, _dwPixelRateReq) ;

        if (_isValid_pixelRate(_dwPixelRateReq) &&
            (_dwPixelRateOld != _dwPixelRateReq))
        {
            error = camera->PCO_SetPixelRate(_dwPixelRateReq);
            msg = "PCO_SetPixelRate";
            PCO_CHECK_ERROR(error, msg);
            PCO_THROW_OR_TRACE(error, msg);

            _pco_GetPixelRate(m_pcoData->dwPixelRate, _dwPixelRateNext, error);
            PCO_THROW_OR_TRACE(error, "_pco_GetPixelRate");

            _dwPixelRate = m_pcoData->dwPixelRate;
            // DEB_ALWAYS() << "PIXEL rate SET (old/new): "  <<
            // DEB_VAR2(_dwPixelRateOld, _dwPixelRate) ;

            _armRequired(true);
        }
        m_pcoData->dwPixelRateRequested = 0;
        return;
    }

    if (_isCameraType(Pco2k | Pco4k))
    {
        _pco_GetPixelRate(m_pcoData->dwPixelRate, _dwPixelRateNext, error);
        PCO_THROW_OR_TRACE(error, "_pco_GetPixelRate");

        _dwPixelRateOld = m_pcoData->dwPixelRate;
        _dwPixelRateMax = m_pcoData->dwPixelRateMax;
        _dwPixelRateReq = m_pcoData->dwPixelRateRequested;

        DEB_TRACE() << "PIXEL rate (requested/actual/max): "
                    << DEB_VAR3(_dwPixelRateReq, _dwPixelRateOld,
                                _dwPixelRateMax);

        if (_isValid_pixelRate(_dwPixelRateReq) &&
            (_dwPixelRateOld != _dwPixelRateReq))
        {
            error = camera->PCO_SetPixelRate(_dwPixelRateReq);
            msg = "PCO_SetPixelRate";
            PCO_CHECK_ERROR(error, msg);
            PCO_THROW_OR_TRACE(error, msg);

            _pco_GetPixelRate(m_pcoData->dwPixelRate, _dwPixelRateNext, error);
            PCO_THROW_OR_TRACE(error, "_pco_GetPixelRate");

            _dwPixelRate = m_pcoData->dwPixelRate;
            DEB_TRACE() << "PIXEL rate SET (old/new): "
                        << DEB_VAR2(_dwPixelRateOld, _dwPixelRate);

            _armRequired(true);
        }
        return;
    }
#endif
    return;
}
//=================================================================================================
//=================================================================================================
void Camera::_pco_GetPixelRate(DWORD &pixRateActual, DWORD &pixRateNext,
                               int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    const char *msg;

#ifndef __linux__

    PCO_FN2(err, msg, PCO_GetPixelRate, m_handle, &m_pcoData->dwPixelRate);
    PCO_THROW_OR_TRACE(err, msg);

    pixRateActual = m_pcoData->dwPixelRate;

    pixRateNext = ((m_pcoData->dwPixelRateRequested != 0) &&
                   (pixRateActual != m_pcoData->dwPixelRateRequested))
                      ? m_pcoData->dwPixelRateRequested
                      : pixRateActual;

#else
    DWORD _dwPixRate;

    err = camera->PCO_GetPixelRate(&_dwPixRate);
    msg = "PCO_GetPixelRate";
    PCO_CHECK_ERROR(err, msg);
    if (err)
        _dwPixRate = 0;

    m_pcoData->dwPixelRate = pixRateActual = _dwPixRate;

    pixRateNext = ((m_pcoData->dwPixelRateRequested != 0) &&
                   (pixRateActual != m_pcoData->dwPixelRateRequested))
                      ? m_pcoData->dwPixelRateRequested
                      : pixRateActual;

    DEB_ALWAYS() << "\n   " << DEB_VAR1(pixRateActual) << "\n   "
                 << DEB_VAR1(pixRateNext);

#endif
}
//=================================================================================================
// ----------------------------------------- storage mode (recorder + sequence)
// current storage mode
//
// case RecSeq
// case RecRing
// - 0x0000 = [recorder] mode
//		. images are recorded and stored within the internal camera
// memory (camRAM)
//      . Live View transfers the most recent image to the PC (for viewing /
//      monitoring) . indexed or total image readout after the recording has
//      been stopped
//
// case Fifo
// - 0x0001 = [FIFO buffer] mode
//      . all images taken are transferred to the PC in chronological order
//      . camera memory (camRAM) is used as huge FIFO buffer to bypass short
//      bottlenecks in data transmission . if buffer overflows, the oldest
//      images are overwritten . if Set Recorder = [stop] is sent, recording is
//      stopped and the transfer of the current image to the PC is finished. .
//      Images not read are stored within the segment and can be read with the
//      Read Image From Segment command.
//
// current recorder submode:
//
// case RecSeq
// - 0x0000 = [sequence]
//      . recording is stopped when the allocated buffer is full
//
// case RecRing
// - 0x0001 = [ring buffer].
//      . camera records continuously into ring buffer
//      . if the allocated buffer overflows, the oldest images are overwritten
//      . recording is stopped by software or disabling acquire signal (<acq
//      enbl>)
//
// for the case of ExtTrigSingle (dimax) we use RecRing
//    case RecRing
//       StorageMode 0 - record mode
//       RecorderSubmode 1 - ring buffer
//  Triggermode 0 - auto
//  Acquiremode 0 - auto / ignored
//=================================================================================================
//=================================================================================================
// sets storage mode and subrecord mode
//    PCO_SetStorageMode
//    PCO_SetRecorderSubmode
//=================================================================================================
void Camera::_pco_SetStorageMode_SetRecorderSubmode(enumPcoStorageMode mode,
                                                    int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    const char *msg, *sMode;

    sMode = "invalid";
    switch (mode)
    {
        case RecSeq:
            m_pcoData->storage_mode = 0;
            m_pcoData->recorder_submode = 0;
            sMode = "RecSeq";
            break;
        case RecRing:
            m_pcoData->storage_mode = 0;
            m_pcoData->recorder_submode = 1;
            sMode = "RecRing";
            break;
        case Fifo:
            m_pcoData->storage_mode = 1;
            m_pcoData->recorder_submode = 0;
            sMode = "Fifo";
            break;
        default:
            throw LIMA_HW_EXC(Error, "FATAL - invalid storage mode!");
    }
    DEB_TRACE() << "\n>>> storage/recorder mode: " << DEB_VAR2(sMode, mode);

    m_pcoData->traceAcq.sPcoStorageRecorderMode = sMode;
    m_pcoData->traceAcq.iPcoStorageMode = m_pcoData->storage_mode;
    m_pcoData->traceAcq.iPcoRecorderSubmode = m_pcoData->recorder_submode;

#ifndef __linux__
    PCO_FN2(error, msg, PCO_SetStorageMode, m_handle, m_pcoData->storage_mode);
    if (error)
        return;
    // PCO_THROW_OR_TRACE(error, msg) ;

    PCO_FN2(error, msg, PCO_SetRecorderSubmode, m_handle,
            m_pcoData->recorder_submode);
    if (error)
        return;
        // PCO_THROW_OR_TRACE(error, msg) ;
#else
    error = camera->PCO_SetStorageMode(m_pcoData->storage_mode);
    msg = "PCO_SetStorageMode";
    PCO_CHECK_ERROR(error, msg);
    // PCO_THROW_OR_TRACE(error, msg) ;

    error = camera->PCO_SetRecorderSubmode(m_pcoData->recorder_submode);
    msg = "PCO_SetRecorderSubmode";
    PCO_CHECK_ERROR(error, msg);
    // PCO_THROW_OR_TRACE(error, msg) ;
#endif
    return;
}

//=================================================================================================
//=================================================================================================
int Camera::_pco_GetStorageMode_GetRecorderSubmode()
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    const char *msg;

    WORD wStorageMode, wRecSubmode;
    int error;

#ifndef __linux__
    PCO_FN2(error, msg, PCO_GetStorageMode, m_handle, &wStorageMode);
    if (error)
    {
        PCO_THROW_OR_TRACE(error, msg);
    }
    PCO_FN2(error, msg, PCO_GetRecorderSubmode, m_handle, &wRecSubmode);
    if (error)
    {
        PCO_THROW_OR_TRACE(error, msg);
    }
#else
    error = camera->PCO_GetStorageMode(&wStorageMode);
    msg = "PCO_GetStorageMode";
    PCO_CHECK_ERROR(error, msg);
    if (error)
    {
        PCO_THROW_OR_TRACE(error, msg);
    }
    error = camera->PCO_GetRecorderSubmode(&wRecSubmode);
    msg = "PCO_getRecorderSubmode";
    PCO_CHECK_ERROR(error, msg);
    if (error)
    {
        PCO_THROW_OR_TRACE(error, msg);
    }
#endif
    m_pcoData->storage_mode = wStorageMode;
    m_pcoData->recorder_submode = wRecSubmode;

    if ((wStorageMode == 0) && (wRecSubmode == 0))
    {
        m_pcoData->storage_str = "RecSeq";
        return RecSeq;
    }
    if ((wStorageMode == 0) && (wRecSubmode == 1))
    {
        m_pcoData->storage_str = "RecRing";
        return RecRing;
    }
    if ((wStorageMode == 1) && (wRecSubmode == 0))
    {
        m_pcoData->storage_str = "Fifo";
        return Fifo;
    }

    m_pcoData->storage_str = "INVALID";
    return RecInvalid;
}
//=================================================================================================
//=================================================================================================

/******************************************************************************************
typedef struct
{
  DWORD  FrameTime_ns;                 // Frametime replaces COC_Runtime
  DWORD  FrameTime_s;

  DWORD  ExposureTime_ns;
  DWORD  ExposureTime_s;               // 5

  DWORD  TriggerSystemDelay_ns;        // System internal min. trigger delay

  DWORD  TriggerSystemJitter_ns;       // Max. possible trigger jitter -0/+ ...
ns

  DWORD  TriggerDelay_ns;              // Resulting trigger delay = system delay
  DWORD  TriggerDelay_s;               // + delay of SetDelayExposureTime ... //
9

} PCO_ImageTiming;
******************************************************************************************/

int Camera::_pco_GetImageTiming(double &frameTime, double &expTime,
                                double &sysDelay, double &sysJitter,
                                double &trigDelay)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    int error;
    const char *msg;

#ifndef __linux__
    PCO_ImageTiming pstrImageTiming;
    PCO_FN2(error, msg, PCO_GetImageTiming, m_handle, &pstrImageTiming);
#else
    SC2_Get_Image_Timing_Response pstrImageTiming;
    error = camera->PCO_GetImageTiming(&pstrImageTiming);
    msg = "PCO_GetImageTiming";
    PCO_CHECK_ERROR(error, msg);
#endif

    frameTime =
        (pstrImageTiming.FrameTime_ns * NANO) + pstrImageTiming.FrameTime_s;
    expTime = (pstrImageTiming.ExposureTime_ns * NANO) +
              pstrImageTiming.ExposureTime_s;
    sysDelay = (pstrImageTiming.TriggerSystemDelay_ns * NANO);
    sysJitter = (pstrImageTiming.TriggerSystemJitter_ns * NANO);
    trigDelay = (pstrImageTiming.TriggerDelay_ns * NANO) +
                pstrImageTiming.TriggerDelay_s;

    return error;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetTriggerMode_SetAcquireMode(int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    const char *msg;

    WORD trigmode, acqmode;

    lima::TrigMode limaTrigMode;
    m_sync->getTrigMode(limaTrigMode);
    m_sync->xlatLimaTrigMode2Pco(limaTrigMode, trigmode, acqmode,
                                 m_pcoData->bExtTrigEnabled, error);

    //------------------------------------------------- triggering mode
#ifndef __linux__
    PCO_FN2(error, msg, PCO_SetTriggerMode, m_handle, trigmode);
#else
    error = camera->PCO_SetTriggerMode(trigmode);
    msg = "PCO_SetTriggerMode";
    PCO_CHECK_ERROR(error, msg);
#endif
    if (error)
    {
        DEB_ALWAYS() << "ERROR PCO_SetTriggerMode" << DEB_VAR1(trigmode);
        return;
    }

    //------------------------------------- acquire mode : ignore or not
    // ext. signal

#ifndef __linux__
    PCO_FN2(error, msg, PCO_SetAcquireMode, m_handle, acqmode);
#else
    error = camera->PCO_SetAcquireMode(acqmode);
    msg = "PCO_SetAcquireMode";
    PCO_CHECK_ERROR(error, msg);
#endif
    if (error)
    {
        DEB_ALWAYS() << "ERROR PCO_SetAcquireMode" << DEB_VAR1(acqmode);
        return;
    }

    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetHWIOSignal(int sigNum, int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    const char *msg;
    error = 0;

    if (!_isCapsDesc(capsHWIO) || (sigNum < 0) ||
        (sigNum >= m_pcoData->wNrPcoHWIOSignal))
    {
        error = -1;
        return;
    }

#ifndef __linux__
    PCO_FN3(error, msg, PCO_SetHWIOSignal, m_handle, sigNum,
            &m_pcoData->stcPcoHWIOSignal[sigNum]);
#else
    // error=camera->PCO_SetHWIOSignal(wSignalNum, wEnabled, wType, wPolarity,
    // wFilterSetting, wSelected);
    error = camera->PCO_SetHWIOSignal(
        sigNum, m_pcoData->stcPcoHWIOSignal[sigNum].wEnabled,
        m_pcoData->stcPcoHWIOSignal[sigNum].wType,
        m_pcoData->stcPcoHWIOSignal[sigNum].wPolarity,
        m_pcoData->stcPcoHWIOSignal[sigNum].wFilterSetting,
        m_pcoData->stcPcoHWIOSignal[sigNum].wSelected);

    msg = "PCO_SetHWIOSignal";
    PCO_CHECK_ERROR(error, msg);
#endif
}
//=================================================================================================
//=================================================================================================
// PCO_CL_DATAFORMAT_5x12   0x07     //extract data to 12bit
// PCO_CL_DATAFORMAT_5x12L  0x09     //extract data to 16Bit
// PCO_CL_DATAFORMAT_5x12R  0x0A     //without extract

// transfer dataformat must be changed depending on pixelrate and horizontal
// resolution

// SC2_SDKAddendum.h:#define PCO_CL_DATAFORMAT_5x12   0x07     //extract data to
// 12bit SC2_SDKAddendum.h:#define PCO_CL_DATAFORMAT_5x12L  0x09     //extract
// data to 16Bit SC2_SDKAddendum.h:#define PCO_CL_DATAFORMAT_5x12R  0x0A
// //without extract

// DWORD   baudrate;         // serial baudrate: 9600, 19200, 38400, 56400,
// 115200 DWORD   ClockFrequency;   // Pixelclock in Hz:
// 40000000,66000000,80000000 DWORD   CCline;           // Usage of CameraLink
// CC1-CC4 lines, use value returned by Get DWORD   DataFormat;       // see
// defines below, use value returned by Get DWORD   Transmit;         // single
// or continuous transmitting images, 0-single, 1-continuous

void Camera::_pco_SetTransferParameter_SetActiveLookupTable(int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    if (!_isInterfaceType(ifCameralinkAll))
    {
        DEB_TRACE() << "PCO_SetTransferParameter (clTransferParam) NOT DONE!";
        return;
    }

    const char *info = "[none]";
    bool doLut = false;

    WORD width, height, wXResMax, wYResMax;
    _pco_GetSizes(&width, &height, &wXResMax, &wYResMax, error);
    WORD wXResActual = width;

#ifndef __linux__ // linux prep

    PCO_SC2_CL_TRANSFER_PARAM clTransferParam;
    char msg[ERRMSG_SIZE + 1];
    char *pcoFn;

    // sizes are only updated AFTER arm, so i will use the last roi settings
    // to get the size. after arm, the size is updated with this value

    //================================================================================================
    // PCO_SetTransferParameter
    //================================================================================================

    _pco_GetTransferParameter(error);
    PCO_THROW_OR_TRACE(error, "_pco_GetTransferParameter(");

    memcpy(&clTransferParam, &m_pcoData->clTransferParam,
           sizeof(PCO_SC2_CL_TRANSFER_PARAM));

    m_pcoData->clTransferParam.baudrate = PCO_CL_BAUDRATE_115K2;

#else // linux prep

#    define USERSET
    struct stcPcoData _pcoData;
    char *pbla = mybla;
    const char *msg;
    DWORD pixelrate, pixRateNext;
    WORD actlut;
    // WORD lutparam;
    int pcoBuffNr = 10;

#    ifdef USERSET // USERSET
    WORD lut;

    _pco_GetPixelRate(pixelrate, pixRateNext, error);
    error = camera->PCO_GetPixelRate(&pixelrate);
    msg = "PCO_GetPixelRate";
    PCO_CHECK_ERROR(error, msg);

    error = camera->PCO_GetTransferParameter(&clpar, sizeof(clpar));
    if (error != PCO_NOERROR)
    {
        DEB_ALWAYS() << "ERROR - PCO_GetTransferParameter " << DEB_VAR1(error);
    }

    m_pcoData->clTransferParam.baudrate = clpar.baudrate;
    m_pcoData->clTransferParam.ClockFrequency = clpar.ClockFrequency;
    m_pcoData->clTransferParam.CCline = clpar.CCline;
    m_pcoData->clTransferParam.DataFormat = clpar.DataFormat;
    m_pcoData->clTransferParam.Transmit = clpar.Transmit;

#    endif // USERSET

#endif // linux prep

    //---------------------------------------------------------------------------
    // set of parameters
    //---------------------------------------------------------------------------
    if (_isCameraType(Dimax))
    {
        // m_pcoData->clTransferParam.Transmit = 1;
        //_pcoData.clTransferParam.Transmit =
        // m_pcoData->clTransferParam.Transmit;
        m_pcoData->clTransferParam.DataFormat = PCO_CL_DATAFORMAT_2x12; //=2
        info = "DIMAX / 2x12 / LUT notValid";
        doLut = false;
    }
    else if (_isCameraType(EdgeGL))
    {
        m_pcoData->clTransferParam.Transmit = 1;
        m_pcoData->clTransferParam.DataFormat =
            PCO_CL_DATAFORMAT_5x12 | SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER;
        // SCCMOS_FORMAT_TOP_BOTTOM;
        m_pcoData->wLUT_Identifier = 0; // Switch LUT->off
        doLut = true;
        info = "EDGE GL / 5x12 topCenter bottomCenter / LUT off";
    }
    else if (_isCameraType(EdgeHS))
    {
        m_pcoData->clTransferParam.Transmit = 1;
        m_pcoData->clTransferParam.DataFormat =
            PCO_CL_DATAFORMAT_5x16 | SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER;
        m_pcoData->wLUT_Identifier = PCO_EDGE_LUT_NONE; // Switch LUT->off
        doLut = true;
        info = "EDGE HS / 5x16 topCenter bottomCenter / LUT off";
    }
    else if (_isCameraType(EdgeRolling))
    {
        m_pcoData->clTransferParam.Transmit = 1;

        if (m_pcoData->dwPixelRate <= PCO_EDGE_PIXEL_RATE_LOW)
        {
            m_pcoData->clTransferParam.DataFormat =
                PCO_CL_DATAFORMAT_5x16 | SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER;
            m_pcoData->wLUT_Identifier = PCO_EDGE_LUT_NONE; // Switch LUT->off
            info = "EDGE Rolling / 5x16 topCenter bottomCenter / LUT off";
        }
        else if (((m_pcoData->dwPixelRate >= PCO_EDGE_PIXEL_RATE_HIGH) &
                  (wXResActual > PCO_EDGE_WIDTH_HIGH)))
        {
            m_pcoData->clTransferParam.DataFormat =
                PCO_CL_DATAFORMAT_5x12L |
                SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER;
            m_pcoData->wLUT_Identifier = PCO_EDGE_LUT_SQRT; // Switch LUT->sqrt
            info = "EDGE Rolling / 5x12L topCenter bottomCenter / LUT SQRT";
        }
        else
        {
            m_pcoData->clTransferParam.DataFormat =
                PCO_CL_DATAFORMAT_5x16 | SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER;
            m_pcoData->wLUT_Identifier = PCO_EDGE_LUT_NONE; // Switch LUT->off
            info = "EDGE Rolling / 5x16 topCenter bottomCenter / LUT off";
        }
        doLut = true;
    }

    //---------------------------------------------------------------------------

#ifndef __linux__ // linux pcoSet

    DEB_TRACE() << "PCO_SetTransferParameter (clTransferParam) " << info;
    PCO_FN3(error, pcoFn, PCO_SetTransferParameter, m_handle,
            &m_pcoData->clTransferParam, sizeof(m_pcoData->clTransferParam));
    if (error)
    {
        __sprintfSExt(
            msg, ERRMSG_SIZE,
            "PCO_SetTransferParameter - baudrate[%d][%d] "
            "dataFormat[x%08x][x%08x] trasmit[%d][%d]",
            clTransferParam.baudrate, m_pcoData->clTransferParam.baudrate,
            clTransferParam.DataFormat, m_pcoData->clTransferParam.DataFormat,
            clTransferParam.Transmit, m_pcoData->clTransferParam.Transmit);
        throw LIMA_HW_EXC(Error, msg);
    }

    _armRequired(true);

    //================================================================================================
    // PCO_SetActiveLookupTable
    //================================================================================================

    if (doLut && !_isInterfaceType(ifCameralinkHS))
    {
        WORD _wLUT_Identifier, _wLUT_Parameter;

        error = PCO_GetActiveLookupTable(m_handle, &_wLUT_Identifier,
                                         &_wLUT_Parameter);
        PCO_CHECK_ERROR(error, "PCO_GetActiveLookupTable");
        if (error)
        {
            DEB_ALWAYS() << "ERROR - PCO_GetActiveLookupTable";
        }

        if (_wLUT_Identifier != m_pcoData->wLUT_Identifier)
        {
            error =
                PCO_SetActiveLookupTable(m_handle, &m_pcoData->wLUT_Identifier,
                                         &m_pcoData->wLUT_Parameter);
            PCO_CHECK_ERROR(error, "PCO_SetActiveLookupTable");
            if (error)
            {
                DEB_ALWAYS() << "ERROR - PCO_SetActiveLookupTable";
            }

            _armRequired(true);

            error =
                PCO_GetActiveLookupTable(m_handle, &m_pcoData->wLUT_Identifier,
                                         &m_pcoData->wLUT_Parameter);
            PCO_CHECK_ERROR(error, "PCO_GetActiveLookupTable");
            if (error)
            {
                DEB_ALWAYS() << "ERROR - PCO_GetActiveLookupTable";
            }
        }
    }

#else // linux pcoSet

#    ifdef USERSET // USERSET

    // m_pcoData->clTransferParam.baudrate = PCO_CL_BAUDRATE_115K2;

    clpar.DataFormat = m_pcoData->clTransferParam.DataFormat;
    m_pcoData->sClTransferParameterSettings = info;
    lut = m_pcoData->wLUT_Identifier;

    pbla +=
        __sprintfSExt(pbla, myblamax - pbla, " / width[%d][%d] height[%d][%d]",
                      width, wXResMax, height, wYResMax);

    DEB_ALWAYS() << mybla;
    // mylog->writelog(INFO_M, "%s", bla);
    mylog->writelog(INFO_M, mybla);

    if (doLut)
    {
        actlut = lut;
        error = camera->PCO_SetLut(actlut, 0);
        msg = "PCO_SetLut";
        PCO_CHECK_ERROR(error, msg);
    }

    error = camera->PCO_SetTransferParameter(&clpar, sizeof(clpar));
    if (error != PCO_NOERROR)
    {
        DEB_ALWAYS() << "ERROR - PCO_SetTransferParameter " << DEB_VAR1(error);
    }

    error = camera->PCO_ArmCamera();
    msg = "PCO_ArmCamera()";
    PCO_CHECK_ERROR(error, msg);
    if (error != PCO_NOERROR)
    {
        DEB_ALWAYS() << "ERROR - PCO_ArmCamera() " << DEB_VAR1(error);
    }

    error = grabber->Set_DataFormat(clpar.DataFormat);
    msg = "Set_DataFormat";
    PCO_CHECK_ERROR(error, msg);
    if (error != PCO_NOERROR)
    {
        DEB_ALWAYS() << "ERROR - Set_DataFormat " << DEB_VAR1(error);
    }

    error = grabber->Set_Grabber_Size(width, height);
    msg = "Set_Grabber_Size";
    PCO_CHECK_ERROR(error, msg);

    error = grabber->PostArm(1);
    msg = "PostArm(1)";
    PCO_CHECK_ERROR(error, msg);

#    else  // USERSET
    error = grabber->PostArm();
    msg = "PostArm(0)";
    PCO_CHECK_ERROR(error, msg);
#    endif // USERSET

    error = grabber->Allocate_Framebuffer(pcoBuffNr);
    msg = "Allocate_Framebuffer";
    PCO_CHECK_ERROR(error, msg);
    error = 0;

#endif // linux pcoSet

    return;
}
//=================================================================================================
//=================================================================================================
#define LEN_ERRSTR 127

void Camera::_pco_GetActiveRamSegment(WORD &wActSeg, int &err)
{
    DEB_MEMBER_FUNCT();

    char errstr[LEN_ERRSTR + 1];

    // if((m_pcoData->stcPcoDescription.dwGeneralCaps1&GENERALCAPS1_NO_RECORDER)==0)
    if (_isCapsDesc(capsCamRam))
    {
        // DWORD PCO_GetActiveRamSegment ( WORD & wActSeg )
#ifdef __linux
        err = camera->PCO_GetActiveRamSegment(&wActSeg);
#else
        err = PCO_GetActiveRamSegment(m_handle, &wActSeg);
#endif

        if (err != PCO_NOERROR)
        {
            PCO_GetErrorText(err, errstr, LEN_ERRSTR);
            DEB_ALWAYS() << "ERROR: " << DEB_VAR2(err, errstr);
            wActSeg = 1;
        }
    }
    else
        wActSeg = 1;

    m_pcoData->wActiveRamSegment = wActSeg;

    return;
}
//=================================================================================================
//=================================================================================================

/**************************************************************************************************
        name[Acquire Enable] idx[0] num[0]
        -def:     def[0x1] type[0xf] pol[0x3] filt[0x7]
        -sig:    enab[0x1] type[0x1] pol[0x1] filt[0x1] sel[0x0]

        name[Exposure Trigger] idx[1] num[1]
        -def:     def[0x1] type[0xf] pol[0xc] filt[0x7]
        -sig:    enab[0x1] type[0x1] pol[0x4] filt[0x1] sel[0x0]

        name[Status Expos] idx[2] num[2]
        -def:     def[0x3] type[0x1] pol[0x3] filt[0x0]
        -sig:    enab[0x1] type[0x1] pol[0x1] filt[0x0] sel[0x0]

        name[Ready Status] idx[3] num[3]
        -def:     def[0x3] type[0x1] pol[0x3] filt[0x0]
        -sig:    enab[0x1] type[0x1] pol[0x1] filt[0x0] sel[0x0]

        name[Set Ready] idx[4] num[4]
        -def:     def[0x1] type[0xf] pol[0x3] filt[0x7]
        -sig:    enab[0x1] type[0x1] pol[0x1] filt[0x1] sel[0x0]
**************************************************************************************************/

void Camera::_pco_initHWIOSignal(int mode, WORD wVal, int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    int _err, sigNum;
    error = 0;
    char *name;
    WORD wSelected;

    if (!_isCapsDesc(capsHWIO))
    {
        DEB_WARNING()
            << "WARNING - camera does not supoort HWIO signals - IGNORED";
        error = -1;
        return;
    }

    _pco_GetHWIOSignalAll(_err);
    error |= _err;

    if (mode == 0)
    {
        const char *sSignalPolarity;
        switch (wVal)
        {
            case 0x01:
                sSignalPolarity = "Low level active";
                break;
            case 0x02:
                sSignalPolarity = "High Level active";
                break;
            case 0x04:
                sSignalPolarity = "Rising edge active";
                break;
            case 0x08:
                sSignalPolarity = "Falling edge active";
                break;
            default:
                sSignalPolarity = "UNKNOWN";
        }

        /***************************************************************
        wSignalPolarity: Flags showing which signal polarity can be selected:
        - 0x01: Low level active
        - 0x02: High Level active
        - 0x04: Rising edge active
        - 0x08: Falling edge active
        ***************************************************************/

        //	name[Acquire Enable] idx[0] num[0]
        sigNum = 0; // descriptor
        wSelected = 0;
        WORD wPolarity = wVal;

#ifndef __linux__
        name = m_pcoData->stcPcoHWIOSignalDesc[sigNum].strSignalName[wSelected];
#else
        name = m_pcoData->stcPcoHWIOSignalDesc[sigNum].szSignalName[wSelected];
#endif
        m_pcoData->stcPcoHWIOSignal[sigNum].wPolarity = wVal;
        m_pcoData->stcPcoHWIOSignal[sigNum].wSelected = wSelected;

        _pco_SetHWIOSignal(sigNum, _err);
        error |= _err;

        DEB_ALWAYS() << "set PcoHWIOSignal polarity "
                     << DEB_VAR5(name, sigNum, wSelected, wPolarity,
                                 sSignalPolarity);
    }
}
//=================================================================================================
//=================================================================================================

void Camera::_pco_GetROI(Roi &roi, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    WORD wX0, wY0, wX1, wY1;

#ifndef __linux__
    err = PCO_GetROI(m_handle, &wX0, &wY0, &wX1, &wY1);
#else
    // DWORD PCO_GetROI ( WORD *RoiX0, WORD * RoiY0, WORD *RoiX1, WORD *RoiY1 )
    err = camera->PCO_GetROI(&wX0, &wY0, &wX1, &wY1);
#endif
    PCO_CHECK_ERROR(err, "PCO_GetROI");
    if (err)
    {
        DEB_ALWAYS() << "ERROR - PCO_GetROI";
        wX0 = wY0 = wX1 = wY1 = 0;
    }

    _xlatRoi_pco2lima(roi, wX0, wX1, wY0, wY1);
}
//=================================================================================================
//=================================================================================================
void Camera::_pco_SetROI(Roi roi, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    unsigned int uiX0, uiY0, uiX1, uiY1;

    _xlatRoi_lima2pco(roi, uiX0, uiX1, uiY0, uiY1);

#ifndef __linux__
    err = PCO_SetROI(m_handle, uiX0, uiY0, uiX1, uiY1);

#else
    err = camera->PCO_SetROI(uiX0, uiY0, uiX1, uiY1);

#endif
    PCO_CHECK_ERROR(err, "PCO_SetROI");
    if (err)
    {
        DEB_ALWAYS() << "ERROR - PCO_SetROI";
    }

    _armRequired(true);
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetRoiInfo(char *buf_in, int size_in, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    char *ptr = buf_in;
    char *ptrMax = ptr + size_in;

    Roi roi;

    unsigned int x0, x1, y0, y1;
    _pco_GetROI(roi, err);

    _xlatRoi_lima2pco(roi, x0, x1, y0, y1);

    ptr += __sprintfSExt(ptr, ptrMax - ptr,
                         "pco[x<%d,%d> y<%d,%d>] lima[<%d,%d>-<%dx%d>]", x0, x1,
                         y0, y1, roi.getTopLeft().x, roi.getTopLeft().y,
                         roi.getSize().getWidth(), roi.getSize().getHeight());
}

//=================================================================================================
//=================================================================================================
int Camera::_binning_fit(int binRequested, int binMax, int binMode)
{
    int binLast, bin;

    if (binRequested < 1)
        return 1;
    if (binRequested >= binMax)
        return binMax;

    binLast = bin = 1;

    while (true)
    {
        if (bin == binRequested)
            return bin;
        if (bin > binRequested)
            return binLast;
        binLast = bin;
        bin = binMode ? bin + 1 : bin * 2;
        if (bin > binMax)
            return binLast;
    }
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetBinningInfo(char *buf_in, int size_in, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    char *ptr = buf_in;
    char *ptrMax = ptr + size_in;

    Bin aBin;

    int binX, binY, binMaxX, binModeX, binMaxY, binModeY;
    _pco_GetBinning(aBin, err);

    binX = aBin.getX();
    binY = aBin.getY();

#ifndef __linux__
    binMaxX = m_pcoData->stcPcoDescription.wMaxBinHorzDESC;
    binModeX = m_pcoData->stcPcoDescription.wBinHorzSteppingDESC;

    binMaxY = m_pcoData->stcPcoDescription.wMaxBinVertDESC;
    binModeY = m_pcoData->stcPcoDescription.wBinVertSteppingDESC;
#else
    // TOCHECK - the same?
    binMaxX = m_pcoData->stcPcoDescription.wMaxBinHorzDESC;
    binModeX = m_pcoData->stcPcoDescription.wBinHorzSteppingDESC;

    binMaxY = m_pcoData->stcPcoDescription.wMaxBinVertDESC;
    binModeY = m_pcoData->stcPcoDescription.wBinVertSteppingDESC;
#endif

    ptr += __sprintfSExt(ptr, ptrMax - ptr,
                         "bin[%d,%d] binMax[%d,%d] binStepMode[%d,%d][%s,%s]",
                         binX, binY, binMaxX, binMaxY, binModeX, binModeY,
                         binModeX ? "lin" : "bin", binModeY ? "lin" : "bin");
}

//=================================================================================================
//=================================================================================================
// SC2_SDK_FUNC int WINAPI PCO_GetBinning(HANDLE ph, WORD* wBinHorz, WORD*
// wBinVert)

void Camera::_pco_GetBinning(Bin &bin, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    WORD wBinHorz, wBinVert;

#ifndef __linux__
    err = PCO_GetBinning(m_handle, &wBinHorz, &wBinVert);

    if (PCO_CHECK_ERROR(err, "PCO_GetBinning"))
    {
        wBinHorz, wBinVert = 1;
        DEB_ALWAYS() << "ERROR - PCO_GetBinning";
    }

#else
    err = camera->PCO_GetBinning(&wBinHorz, &wBinVert);
    PCO_CHECK_ERROR(err, "PCO_GetBinning");
    if (err)
    {
        wBinHorz = wBinVert = 1;
        DEB_ALWAYS() << "ERROR - PCO_GetBinning";
    }

#endif

    bin = Bin(wBinHorz, wBinVert);
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetFirmwareInfo(char *buf_in, int size_in, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    char *ptr = buf_in;
    char *ptrMax = ptr + size_in;

#ifndef __linux__
    ptr += __sprintfSExt(ptr, ptrMax - ptr, "* firmware \n");
    ptr += __sprintfSExt(
        ptr, ptrMax - ptr,
        "* ... firmware dwHWVersion[%x]  dwFWVersion[%x] <- not used\n",
        m_pcoData->stcPcoCamType.dwHWVersion,
        m_pcoData->stcPcoCamType.dwFWVersion);

    int nrDev, iDev;

    nrDev = m_pcoData->stcPcoCamType.strHardwareVersion.BoardNum;
    ptr += __sprintfSExt(ptr, ptrMax - ptr,
                         "* Hardware_DESC device[%d]  szName          "
                         "wBatchNo/wRevision   wVariant\n",
                         nrDev);
    for (iDev = 0; iDev < nrDev; iDev++)
    {
        PCO_SC2_Hardware_DESC *ptrhw;
        ptrhw = &m_pcoData->stcPcoCamType.strHardwareVersion.Board[iDev];
        ptr += __sprintfSExt(
            ptr, ptrMax - ptr, "* %20d      %-18s   %4d.%-4d    %4d\n", iDev,
            ptrhw->szName, ptrhw->wBatchNo, ptrhw->wRevision, ptrhw->wVariant);
    }

    nrDev = m_pcoData->stcPcoCamType.strFirmwareVersion.DeviceNum;
    ptr += __sprintfSExt(ptr, ptrMax - ptr,
                         "* Firmware_DESC device[%d]  szName          "
                         "bMajorRev/Minor   wVariant\n",
                         nrDev);

    for (iDev = 0; iDev < nrDev; iDev++)
    {
        PCO_SC2_Firmware_DESC *ptrfw;
        ptrfw = &m_pcoData->stcPcoCamType.strFirmwareVersion.Device[iDev];
        ptr += __sprintfSExt(
            ptr, ptrMax - ptr, "* %20d      %-18s   %4d.%-4d    %4d\n", iDev,
            ptrfw->szName, ptrfw->bMajorRev, ptrfw->bMinorRev, ptrfw->wVariant);
    }

    PCO_FW_Vers strFirmwareVersion;
    WORD wblock = 0;
    int iCnt;
    //		err =  PCO_GetFirmwareInfo(m_handle, wblock++,
    //&strFirmwareVersion);
    err = PCO_GetFirmwareInfo(m_handle, wblock++, &strFirmwareVersion);
    nrDev = (err == PCO_NOERROR) ? strFirmwareVersion.DeviceNum : 0;

    if (nrDev > 0)
    {
        ptr +=
            __sprintfSExt(ptr, ptrMax - ptr,
                          "* Firmware_DESC device[%d]  szName          "
                          "bMajorRev/Minor   wVariant (PCO_GetFirmwareInfo)\n",
                          nrDev);

        for (iDev = 0, iCnt = 0; iDev < nrDev; iDev++, iCnt++)
        {
            PCO_SC2_Firmware_DESC *ptrfw;
            if (iCnt >= 10)
            {
                iCnt = 0;
                err = PCO_GetFirmwareInfo(m_handle, wblock++,
                                          &strFirmwareVersion);
                if (err != PCO_NOERROR)
                    break;
            } // iCnt

            ptrfw = &strFirmwareVersion.Device[iCnt];
            ptr += __sprintfSExt(ptr, ptrMax - ptr,
                                 "* %20d      %-18s   %4d.%-4d    %4d\n", iDev,
                                 ptrfw->szName, ptrfw->bMajorRev,
                                 ptrfw->bMinorRev, ptrfw->wVariant);
        } // for
    }     // if nrDev
#else
    int lg = ptrMax - ptr;

    err = camera->PCO_GetHardwareVersion(ptr, &lg);
    PCO_CHECK_ERROR(err, "PCO_GetROI");
    if (err)
        return;

    lg = strlen(buf_in);
    ptr = buf_in + lg;
    lg = ptrMax - ptr;

    if (lg)
    {
        err = camera->PCO_GetFirmwareVersion(ptr, &lg);
        PCO_CHECK_ERROR(err, "PCO_GetROI");
        if (err)
            return;
    }

    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::getXYdescription(unsigned int &xSteps, unsigned int &ySteps,
                              unsigned int &xMax, unsigned int &yMax,
                              unsigned int &xMinSize, unsigned int &yMinSize)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    xSteps = m_pcoData->stcPcoDescription.wRoiHorStepsDESC;
    ySteps = m_pcoData->stcPcoDescription.wRoiVertStepsDESC;

    xMax = m_pcoData->stcPcoDescription.wMaxHorzResStdDESC;
    yMax = m_pcoData->stcPcoDescription.wMaxVertResStdDESC;

#ifndef __linux__
    unsigned int xMinSize0;
    xMinSize = xMinSize0 = m_pcoData->stcPcoDescription.wMinSizeHorzDESC;
    yMinSize = m_pcoData->stcPcoDescription.wMinSizeVertDESC;

    { // patch meanwhile firmware 1.19 is fixed
        if (m_pcoData->params_xMinSize)
        {
            xMinSize += xSteps;
            DEB_TRACE() << "PATCH APPLIED: " << DEB_VAR2(xMinSize0, xMinSize);
        }
    }

#else
    // xMinSize = xMinSize0 = m_pcoData->stcPcoDescription.wRoiHorStepsDESC;
    // yMinSize = m_pcoData->stcPcoDescription.wRoiVertStepsDESC;
    WORD wXMinSize, wYMinSize;
    _pco_GetCameraMinSizeCalc(wXMinSize, wYMinSize);
    xMinSize = wXMinSize;
    yMinSize = wYMinSize;
#endif
}

void Camera::getXYsteps(unsigned int &xSteps, unsigned int &ySteps)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    xSteps = m_pcoData->stcPcoDescription.wRoiHorStepsDESC;
    ySteps = m_pcoData->stcPcoDescription.wRoiVertStepsDESC;
}

void Camera::getMaxWidthHeight(unsigned int &xMax, unsigned int &yMax)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    xMax = m_pcoData->stcPcoDescription.wMaxHorzResStdDESC;
    yMax = m_pcoData->stcPcoDescription.wMaxVertResStdDESC;
}

#if 0
void Camera::getMaxWidthHeight(DWORD &xMax, DWORD &yMax){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	xMax = m_pcoData->stcPcoDescription.wMaxHorzResStdDESC;
	yMax = m_pcoData->stcPcoDescription.wMaxVertResStdDESC;
}
#endif

void Camera::getBytesPerPixel(unsigned int &pixbytes)
{
    pixbytes = (m_pcoData->stcPcoDescription.wDynResDESC <= 8) ? 1 : 2;
}

void Camera::getBitsPerPixel(WORD &pixbits)
{
    pixbits = m_pcoData->stcPcoDescription.wDynResDESC;
    m_pcoData->m_wBitPerPixel = pixbits;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetSizes(WORD *wXResActual, WORD *wYResActual, WORD *wXResMax,
                           WORD *wYResMax, int &error)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    error = 0;

#ifdef __linux__
    const char *msg;
    char *bla = mybla;
    DWORD dwXResActual, dwYResActual;

    error = camera->PCO_GetActualSize(&dwXResActual, &dwYResActual);
    msg = "PCO_GetActualSize";
    PCO_CHECK_ERROR(error, msg);

    if (error)
        return;

    *wXResActual = (WORD)dwXResActual;
    *wYResActual = (WORD)dwYResActual;
    *wXResMax = m_pcoData->stcPcoDescription.wMaxHorzResStdDESC;
    *wYResMax = m_pcoData->stcPcoDescription.wMaxVertResStdDESC;

    bla +=
        __sprintfSExt(bla, myblamax - bla,
                      "%s> resAct[%d][%d] resStdMax[%d][%d] resExtMax[%d][%d]",
                      fnId, *wXResActual, *wYResActual, *wXResMax, *wYResMax,
                      m_pcoData->stcPcoDescription.wMaxHorzResExtDESC,
                      m_pcoData->stcPcoDescription.wMaxVertResExtDESC);
    mylog->writelog(INFO_M, mybla);

#else
    // PCOFN5(error, msg,PCO_GetSizes , m_handle, &dwXResActual, &dwYResActual,
    // &dwXResMax, &dwYResMax) ;
    error =
        PCO_GetSizes(m_handle, wXResActual, wYResActual, wXResMax, wYResMax);
    PCO_CHECK_ERROR(error, "PCO_GetSizes");
    if (error)
    {
        DEB_ALWAYS() << "ERROR - PCO_GetSizes";
        //*wXResActual = *wYResActual = 0;
        *wXResActual = m_pcoData->m_wArmWidth;
        *wYResActual = m_pcoData->m_wArmHeight;
        *wXResMax = m_pcoData->m_wMaxWidth;
        *wYResMax = m_pcoData->m_wMaxHeight;
    }
#endif

    m_pcoData->m_wArmWidth = *wXResActual;
    m_pcoData->m_wArmHeight = *wYResActual;
    m_pcoData->m_wMaxWidth = *wXResMax;
    m_pcoData->m_wMaxHeight = *wYResMax;


    return;
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_SetRecordStopEvent(WORD wRecordStopEventMode,
                                     DWORD dwRecordStopDelayImages, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    err = camera->PCO_SetRecordStopEvent(wRecordStopEventMode,
                                         dwRecordStopDelayImages);
#else
    err = PCO_SetRecordStopEvent(m_handle, wRecordStopEventMode,
                                 dwRecordStopDelayImages);
#endif

    PCO_CHECK_ERROR(err, "PCO_SetRecordStopEvent");
    if (err)
    {
        PCO_THROW_OR_TRACE(err, "PCO_SetRecordStopEvent");
    }
    return;
}
//=================================================================================================
//=================================================================================================
void Camera::_pco_SetDelayExposureTime(int &error, int ph)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    bool doIt;
    // const char *msg;

    DWORD dwExposure, dwDelay;
    WORD wExposure_base, wDelay_base;
    double _exposure, _delay;
    m_sync->getExpTime(_exposure);
    m_sync->getLatTime(_delay);
    double _delay0 = _delay;

    doIt = true;

    ph = 0;

    if (ph != 0)
    {
        doIt = false;

        WORD _wArmWidth, _wArmHeight, _wMaxWidth, _wMaxHeight;
        _pco_GetSizes(&_wArmWidth, &_wArmHeight, &_wMaxWidth, &_wMaxHeight,
                      error);

        if ((_isCameraType(Edge)) &&
            (m_pcoData->dwPixelRate >= PCO_EDGE_PIXEL_RATE_HIGH))
        {
            double pixels = ((double)_wArmWidth) * ((double)_wArmHeight);
            double bytes =
                (m_pcoData->wLUT_Identifier == PCO_EDGE_LUT_SQRT) ? 1.5 : 2.0;
            double period =
                bytes * pixels / (m_pcoData->fTransferRateMHzMax * 1000000.);

            printf("--- %s>period[%g] -> cocRunTime[%g]\n", fnId, period,
                   m_pcoData->cocRunTime);
            if (period > m_pcoData->cocRunTime)
            {
                _delay += period - m_pcoData->cocRunTime;
                doIt = true;
                printf("--- %s> delay forced [%g] -> [%g]\n", fnId, _delay0,
                       _delay);
            }
        }
    }

    if (!doIt)
        return;

    error = 0;

    _pco_time2dwbase(_exposure, dwExposure, wExposure_base);
    _pco_time2dwbase(_delay, dwDelay, wDelay_base);

#ifndef __linux__
    // PCO_FN5(error, msg,PCO_SetDelayExposureTime, m_handle, dwDelay,
    // dwExposure, wDelay_base, wExposure_base);
    error = PCO_SetDelayExposureTime(m_handle, dwDelay, dwExposure, wDelay_base,
                                     wExposure_base);
    PCO_CHECK_ERROR(error, "PCO_SetDelayExposureTime");
#else
    int err;

    err = camera->PCO_SetTimebase(wDelay_base, wExposure_base);
    PCO_CHECK_ERROR(err, "PCO_SetTimebase");
    error |= err;

    err = camera->PCO_SetDelayExposure(dwDelay, dwExposure);
    PCO_CHECK_ERROR(err, "PCO_SetDelayExposure");
    error |= err;
#endif

    m_pcoData->traceAcq.dLimaExposure = _exposure;
    m_pcoData->traceAcq.dLimaDelay = _delay;

    m_pcoData->traceAcq.iPcoExposure = dwExposure;
    m_pcoData->traceAcq.iPcoDelay = dwDelay;
    m_pcoData->traceAcq.iPcoExposureBase = wExposure_base;
    m_pcoData->traceAcq.iPcoDelayBase = wDelay_base;

    if (error || _getDebug(DBG_EXP))
    {
        DEB_ALWAYS() << DEB_VAR3(_exposure, dwExposure, wExposure_base);
        DEB_ALWAYS() << DEB_VAR3(_delay, dwDelay, wDelay_base);
    }

    return;
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_FreeBuffer(int bufIdx, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else
    err = PCO_FreeBuffer(m_handle, bufIdx);

    err = PCO_CHECK_ERROR(err, "PCO_FreeBuffer");
    if (err)
    {
        DEB_ALWAYS() << "SDK ERROR - PCO_FreeBuffer: "
                     << DEB_VAR3(err, m_handle, bufIdx);
        // PCO_THROW_OR_TRACE(err, "PCO_FreeBuffer") ;
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_AllocateBuffer(SHORT *sBufNr, DWORD dwSize, WORD **wBuf,
                                 void **hEvent, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else
    err = PCO_AllocateBuffer(m_handle, sBufNr, dwSize, wBuf, hEvent);

    PCO_CHECK_ERROR(err, "PCO_AllocateBuffer");
    if (err)
    {
        DEB_ALWAYS() << "SDK ERROR - pCO_AllocateBuffer: "
                     << DEB_VAR4(DEB_HEX(err), m_handle, sBufNr, dwSize);
        PCO_THROW_OR_TRACE(err, "PCO_AllocateBuffer");
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_GetImageEx(WORD wSegment, DWORD dw1stImage, DWORD dwLastImage,
                             SHORT sBufNr, WORD wXRes, WORD wYRes,
                             WORD wBitPerPixel, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else
    // SC2_SDK_FUNC int WINAPI PCO_GetImageEx(HANDLE ph, WORD wSegment, DWORD
    // dw1stImage, DWORD dwLastImage, SHORT sBufNr,
    //                                        WORD wXRes, WORD wYRes, WORD
    //                                        wBitPerPixel);
    err = PCO_GetImageEx(m_handle, wSegment, dw1stImage, dwLastImage, sBufNr,
                         wXRes, wYRes, wBitPerPixel);

    PCO_CHECK_ERROR(err, "PCO_GetImageEx");
    if (err)
    {
        _pco_GetStorageMode_GetRecorderSubmode();

        DEB_ALWAYS() << "ERROR:  "
                     << DEB_VAR3(dw1stImage, dwLastImage,
                                 m_pcoData->storage_str)
                     << "\n   "
                     << DEB_VAR5(wSegment, sBufNr, wXRes, wYRes, wBitPerPixel);
        PCO_THROW_OR_TRACE(err, "PCO_GetImageEx");
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_GetBufferStatus(SHORT sBufNr, DWORD *dwStatusDll,
                                  DWORD *dwStatusDrv, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else
    err = PCO_GetBufferStatus(m_handle, sBufNr, dwStatusDll, dwStatusDrv);

    PCO_CHECK_ERROR(err, "PCO_GetBufferStatus");
    if (err)
    {
        PCO_THROW_OR_TRACE(err, "PCO_GetBufferStatus");
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_AddBufferExtern(HANDLE hEvent, WORD wActSeg, DWORD dw1stImage,
                                  DWORD dwLastImage, DWORD dwSynch, void *pBuf,
                                  DWORD dwLen, DWORD *dwStatus, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else
    err = PCO_AddBufferExtern(m_handle, hEvent, wActSeg, dw1stImage,
                              dwLastImage, dwSynch, pBuf, dwLen, dwStatus);

    PCO_CHECK_ERROR(err, "PCO_AddBufferExtern");
    if (err)
    {
        PCO_THROW_OR_TRACE(err, "PCO_AddBufferExtern");
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_AddBufferEx(DWORD dw1stImage, DWORD dwLastImage, SHORT sBufNr,
                              WORD wXRes, WORD wYRes, WORD wBitPerPixel,
                              int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else
    err = PCO_AddBufferEx(m_handle, dw1stImage, dwLastImage, sBufNr, wXRes,
                          wYRes, wBitPerPixel);

    PCO_CHECK_ERROR(err, "PCO_AddBufferEx");
    if (err)
    {
        PCO_THROW_OR_TRACE(err, "PCO_AddBufferEx");
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_RebootCamera(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__

    err = camera->PCO_RebootCamera();
    PCO_CHECK_ERROR(err, "PCO_RebootCamera");

    // DEB_ALWAYS() <<  NOT_IMPLEMENTED ;
    return;

#else
    // PCO_FN1(error, msg,PCO_RebootCamera, m_handle);

    err = PCO_RebootCamera(m_handle);

    PCO_CHECK_ERROR(err, "PCO_RebootCamera");
    if (err)
    {
        PCO_THROW_OR_TRACE(err, "PCO_RebootCamera");
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_OpenCamera(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else
    //==========================================================================================
    // SC2_SDK_FUNC int WINAPI PCO_OpenCamera(HANDLE* ph, WORD wCamNum)
    //   b.) Input parameter:
    //       HANDLE* ph: HANDLE pointer to receive the opened camera device.
    //       This parameter has
    //                   to be stored for later use with all other function
    //                   calls.
    //       WORD wCamNum: Don’t care.
    //   The input data should be filled with the following parameter:
    //       HANDLE* ph:
    //                  - 0 = open new camera.
    //                  - xyz = Handle to a previously opened camera.
    //       WORD wCamNum:
    //                  - don’t care, set to zero.
    //==========================================================================================

    err = PCO_OpenCamera(&m_handle, 0);

    PCO_CHECK_ERROR(err, "PCO_OpenCamera");
    if (err)
    {
        PCO_THROW_OR_TRACE(err, "PCO_OpenCamera");
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_GetCameraRamSize(DWORD &dwRamSize, WORD &wPageSizeint,
                                   int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else
    // PCO_GetCameraRamSize, m_handle, &ramSize, &pageSize);

    err = PCO_GetCameraRamSize(m_handle, &dwRamSize, &wPageSizeint);

    PCO_CHECK_ERROR(err, "PCO_GetCameraRamSize");
    if (err)
    {
        PCO_THROW_OR_TRACE(err, "PCO_GetCameraRamSize");
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_ResetLib(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << "INFO: function does NOT exists in linux [" << fnId << "]";
    return;
#else
    err = PCO_ResetLib();

    PCO_CHECK_ERROR(err, fnId);
    if (err)
    {
        PCO_THROW_OR_TRACE(err, fnId);
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_GetCameraSetup(WORD &wType, DWORD &dwSetup, WORD &wLen,
                                 int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__

    /// \brief Request the current camera setup
    /// \anchor PCO_GetCameraSetup
    ///
    /// This function is used to query the current operation mode of the camera.
    /// Some cameras can work at different operation modes with different
    /// descriptor settings.\n pco.edge:\n To get the current shutter mode input
    /// index setup_id must be set to 0.\n current shutter mode is returned in
    /// setup_flag[0]
    ///  - 0x00000001 = Rolling Shutter
    ///  - 0x00000002 = Global Shutter
    ///  - 0x00000004 = Global Reset
    ///
    /// \param setup_id Identification code for selected setup type.
    /// \param setup_flag Pointer to a DWORD array to get the current setup
    /// flags. If set to NULL in input only the array length is returned.
    /// - On input this variable can be set to NULL, then only array length is
    /// filled with correct value.
    /// - On output the array is filled with the available information for the
    /// selected setup_id \param length Pointer to a WORD variable
    /// - On input to indicate the length of the Setup_flag array in DWORDs.
    /// - On output the length of the setup_flag array in DWORDS
    /// \return Error code or PCO_NOERROR on success
    ///
    DWORD PCO_GetCameraSetup(WORD setup_id, DWORD * setup_flag, WORD * length);

    // DWORD PCO_GetCameraSetup(WORD *setup_id, DWORD *setup_flag, WORD
    // *length); DWORD PCO_SetCameraSetup(WORD setup_id, DWORD *setup_flag,WORD
    // length);
    //  setup_flag[0]=PCO_EDGE_SETUP_ROLLING_SHUTTER;
    //    setup_flag[1]=0;
    //    setup_flag[2]=0;
    //    setup_flag[3]=0;

    // WORD setup_id=0;
    // int err = 0;
    // DWORD setup_flag[4];
    // WORD  length=sizeof(setup_flag);

    DWORD dwErr = PCO_NOERROR;
    wType = 0;

    // dwErr = camera->PCO_SetCameraSetup(setup_id, setup_flag, length);    WORD
    // setup_id=0;
    dwErr = camera->PCO_GetCameraSetup(wType, &dwSetup, &wLen);
    err = dwErr;
    PCO_CHECK_ERROR(err, "PCO_GetCameraSetup");

    DEB_ALWAYS() << fnId << " " << DEB_VAR2(wType, wLen);
    ;

    return;

#else

    /***********************************************************************************
    SC2_SDK_FUNC int WINAPI PCO_GetCameraSetup(HANDLE ph, WORD* wType, DWORD*
    dwSetup, WORD* wLen) b.) Input parameter: HANDLE ph: Handle to a previously
    opened camera device. WORD* wType: Pointer to a WORD to receive the actual
    setup type. Init to zero. DWORD* dwSetup: Pointer to a DWORD array to get
    the actual setup. WORD* wLen: Pointer to a WORD to indicate the number of
    valid DWORDs in dwSetup. The input pointers will be filled with the
    following parameters: wType: Actual setup type. dwSetup: Actual setup. wLen:
    Length in number of DWORDs.
    ***********************************************************************************/

    err = PCO_GetCameraSetup(m_handle, &wType, &dwSetup, &wLen);

    PCO_CHECK_ERROR(err, fnId);
    if (err)
    {
        PCO_THROW_OR_TRACE(err, fnId);
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_SetCameraSetup(WORD wType, DWORD &dwSetup, WORD wLen,
                                 int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    /// \brief Sets the camera setup structure (see camera specific structures)
    /// \anchor PCO_SetCameraSetup
    ///
    /// pco.edge:\n
    /// To get the current shutter mode input index setup_id must be set to 0.\n
    /// current shutter mode is returned in setup_flag[0]
    ///  - 0x00000001 = Rolling Shutter
    ///  - 0x00000002 = Global Shutter
    ///  - 0x00000004 = Global Reset
    /// When camera is set to a new shuttermode uit must be reinitialized by
    /// calling one of the reboot functions. After rebooting, camera description
    /// must be read again see \ref  PCO_GetCameraDescription.
    ///
    /// \param setup_id Identification code for selected setup type.
    /// \param setup_flag Flags to be set for the selected setup type.
    /// \param length Number of valid DWORDs in setup_flag array.
    /// \return Error code or PCO_NOERROR on success
    ///
    // DWORD PCO_SetCameraSetup(WORD setup_id, DWORD *setup_flag,WORD length);

    DWORD dwErr = PCO_NOERROR;
    wType = 0;

    dwErr = camera->PCO_SetCameraSetup(wType, &dwSetup, wLen);
    err = dwErr;
    PCO_CHECK_ERROR(err, "PCO_SetCameraSetup");

    DEB_ALWAYS() << fnId << " " << DEB_VAR2(wType, wLen);
    ;
    return;

#else

    /***********************************************************************************
    SC2_SDK_FUNC int WINAPI PCO_SetCameraSetup(HANDLE ph, WORD wType, DWORD*
    dwSetup, WORD wLen) b.) Input parameter: HANDLE ph: Handle to a previously
    opened camera device. WORD wType: WORD to set the actual setup type. Do not
    change this value. DWORD* dwSetup: Pointer to a DWORD array to set the
    actual setup. WORD wLen: WORD to indicate the number of valid DWORDs in
    dwSetup. The input pointers should be filled with the following parameters:
     wType: Actual setup type got by a previous call to PCO_GetCameraSetup.
     dwSetup: dwSetup[0] = 1 (Rolling Shutter), 2 (Global Shutter)
     wLen: Length in number of DWORDs, usually 1.
     ***********************************************************************************/

    err = PCO_SetCameraSetup(m_handle, wType, &dwSetup, wLen);

    PCO_CHECK_ERROR(err, fnId);
    if (err)
    {
        PCO_THROW_OR_TRACE(err, fnId);
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_SetTimeouts(void *buf_in, unsigned int size_in, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else

    /***********************************************************************************
    4.7.18 PCO_SetTimeouts
    This call sets the driver time outs to new values. Usually there is no need
    to change these values. In case there are timeout errors while calling e.g.
    PCO_ArmCamera, the user might increase the command timeout. a.) Prototype:
    SC2_SDK_FUNC int WINAPI PCO_SetTimeouts(HANDLE ph, void *buf_in, unsigned
    int size_in) b.) Input parameter: HANDLE ph: Handle to a previously opened
    camera device. void* buf: Pointer to hold the address of the first element
    of an unsigned int array. unsigned int size_in: Variable which sets the
    number of valid values accessible by the pointer. The input data should be
    filled with the following parameter: buf_in: Array of unsigned int values,
    whereas:
    - buf_in[0]: Command timeout – A command will be aborted after x ms if there
    is no response from the camera.
    - buf_in[1]: Image timeout – An image request will be aborted after x ms if
    there is no response from the camera. This is valid for the PCO_GetImage
    command.
    - buf_in[2]: Transfer timeout – The 1394 driver will close the allocated
    isochronous channel after x ms if there is no image transfer from the
    camera. The camera link interface will remove all occupied resources after x
    ms.

     size_in: Number of valid values, usually three. Set this to two for the
    camera link interfaces
    ***********************************************************************************/

    err = PCO_SetTimeouts(m_handle, buf_in, size_in);

    PCO_CHECK_ERROR(err, fnId);
    if (err)
    {
        PCO_THROW_OR_TRACE(err, fnId);
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_GetCameraRamSegmentSize(DWORD *dwRamSegSize, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else

    /***********************************************************************************
    4.4.4 PCO_GetCameraRamSegmentSize
    Get camera RAM (camRAM) segment size.
    a.) Prototype:
    SC2_SDK_FUNC int WINAPI PCO_GetCameraRamSegmentSize(HANDLE ph, DWORD*
    dwRamSegSize) b.) Input parameter: HANDLE ph: Handle to a previously opened
    camera device. DWORD* dwRamSegSize: Address of a DWORD array to get the
    segment sizes. The input pointer will be filled with the following
    parameter: Size of segment 1 .. segment 4 as multiples of RAM pages Note:
     the sum of all segment sizes must not be larger than the total size of the
    RAM (as multiples of pages) size = [0] indicates that the segment will not
    be used using only one segment is possible by assigning the total RAM size
    to segment 1 and 0x0000 to all other segments. The segment number is 1
    based, while the array dwRamSegSize is zero based, e.g. ram size of segment
    1 is stored in dwRamSegSize[0]! c.) Return value: int: Error message, 0 in
    case of success else less than 0: see Error / Warning Codes
     ***********************************************************************************/

    err = PCO_GetCameraRamSegmentSize(m_handle, dwRamSegSize);

    PCO_CHECK_ERROR(err, fnId);
    if (err)
    {
        PCO_THROW_OR_TRACE(err, fnId);
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_SetCameraRamSegmentSize(DWORD *dwRamSegSize, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else

    /***********************************************************************************
    4.4.5 PCO_SetCameraRamSegmentSize
    Set Camera RAM Segment Size. The segment size has to be big enough to hold
    at least two images. a.) Prototype: SC2_SDK_FUNC int WINAPI
    PCO_SetCameraRamSegmentSize(HANDLE ph, DWORD* dwRamSegSize) b.) Input
    parameter: HANDLE ph: Handle to a previously opened camera device. DWORD*
    dwRamSegSize: Address of a DWORD array to set the segment sizes. The input
    pointer should be filled with the following parameters: Size of segment 1 ..
    segment 4 as multiples of RAM pages Note: the sum of all segment sizes must
    not be larger than the total size of the RAM (as multiples of pages) a
    single segment size can have the value 0x0000, but the sum of all four
    segments must be bigger than the size of two images. the command will be
    rejected, if Recording State is [run] The segment number is 1 based, while
    the array dwRamSegSize is zero based, e.g. ram size of segment 1 is stored
    in dwRamSegSize[0]! This function will result in all segments being cleared.
    All previously recorded images will be lost!
     ***********************************************************************************/

    err = PCO_SetCameraRamSegmentSize(m_handle, dwRamSegSize);

    PCO_CHECK_ERROR(err, fnId);
    if (err)
    {
        PCO_THROW_OR_TRACE(err, fnId);
    }
    return;
#endif
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_ArmCamera(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifndef __linux__
    err = PCO_ArmCamera(m_handle);
#else
    err = camera->PCO_ArmCamera();
#endif

    PCO_CHECK_ERROR(err, "PCO_ArmCamera()");

    if (err != PCO_NOERROR)
    {
        DEB_ALWAYS() << "ERROR - PCO_ArmCamera() " << DEB_VAR1(err);
    }

    PCO_THROW_OR_TRACE(err, "PCO_ArmCamera");
    return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetTransferParameter(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__

    err = camera->PCO_GetTransferParameter(&clpar, sizeof(clpar));
    PCO_CHECK_ERROR(err, "PCO_GetTransferParameter");
    if (err)
        return;

    DEB_ALWAYS() << "\n   " << DEB_VAR1(clpar.baudrate) << "\n   "
                 << DEB_VAR1(clpar.ClockFrequency) << "\n   "
                 << DEB_VAR1(clpar.DataFormat) << "\n   "
                 << DEB_VAR1(clpar.Transmit);

#else

    err = PCO_GetTransferParameter(m_handle, &m_pcoData->clTransferParam,
                                   sizeof(PCO_SC2_CL_TRANSFER_PARAM));
    PCO_CHECK_ERROR(err, "PCO_GetTransferParameter");

#endif

    return;
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_FillStructures(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    // char *pcoFn;
    err = 0;

#ifndef __linux__
    int pcoErr;
    // PCO_GETGENERAL(hCam, &strGeneral)

    // -- Get General
    m_pcoData->stcPcoGeneral.wSize = sizeof(m_pcoData->stcPcoGeneral);
    m_pcoData->stcPcoGeneral.strCamType.wSize =
        sizeof(m_pcoData->stcPcoGeneral.strCamType);

    pcoErr = PCO_GetGeneral(m_handle, &m_pcoData->stcPcoGeneral);
    err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetGeneral");

    // PCO_GETCAMERATYPE(hCam, &strCamType)

    m_pcoData->stcPcoCamType.wSize = sizeof(m_pcoData->stcPcoCamType);
    pcoErr = PCO_GetCameraType(m_handle, &m_pcoData->stcPcoCamType);
    err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetCameraType");

    // PCO_GETSENSORSTRUCT(hCam, &strSensor)

    // -- Get Sensor struct
    m_pcoData->stcPcoSensor.wSize = sizeof(m_pcoData->stcPcoSensor);
    m_pcoData->stcPcoSensor.strDescription.wSize =
        sizeof(m_pcoData->stcPcoSensor.strDescription);
    m_pcoData->stcPcoSensor.strDescription2.wSize =
        sizeof(m_pcoData->stcPcoSensor.strDescription2);

    pcoErr = PCO_GetSensorStruct(m_handle, &m_pcoData->stcPcoSensor);
    err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetSensorStruct");

    // PCO_GETCAMERADESCRIPTION(hCam, &strDescription)

    // -- Get camera description
    m_pcoData->stcPcoDescription.wSize = sizeof(m_pcoData->stcPcoDescription);

    pcoErr = PCO_GetCameraDescription(m_handle, &m_pcoData->stcPcoDescription);
    err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetCameraDescription");

    // PCO_GETTIMINGSTRUCT(hCam, &strTiming)

    // -- Get timing struct
    m_pcoData->stcPcoTiming.wSize = sizeof(m_pcoData->stcPcoTiming);

    pcoErr = PCO_GetTimingStruct(m_handle, &m_pcoData->stcPcoTiming);
    err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetTimingStruct");

    // PCO_GETRECORDINGSTRUCT(hCam, &strRecording)

    // -- Get recording struct
    m_pcoData->stcPcoRecording.wSize = sizeof(m_pcoData->stcPcoRecording);

    pcoErr = PCO_GetRecordingStruct(m_handle, &m_pcoData->stcPcoRecording);
    err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetRecordingStruct");

    // -- Get storage struct
    m_pcoData->stcPcoStorage.wSize = sizeof(m_pcoData->stcPcoStorage);

    pcoErr = PCO_GetStorageStruct(m_handle, &m_pcoData->stcPcoStorage);
    err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetStorageStruct");

#else
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    err = -1;
#endif

    return;
}

//=================================================================================================
//=================================================================================================

#define HANDLE_LIST_DIM 30

void Camera::_pco_OpenCameraSn(DWORD snRequested, int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else
    HANDLE handleList[HANDLE_LIST_DIM];
    DWORD snList[HANDLE_LIST_DIM];
    char *ptr, *ptr0;
    ptr = ptr0 = m_pcoData->camerasFound;
    char *ptrMax = m_pcoData->camerasFound + sizeof(m_pcoData->camerasFound);
    *ptr = 0;

    int iHandle, iHandleLast;
    for (iHandle = 0; iHandle < HANDLE_LIST_DIM; iHandle++)
    {
        handleList[iHandle] = NULL;
    }

    iHandle = 0;
    iHandleLast = -1;
    while (true)
    {
        if (iHandle >= HANDLE_LIST_DIM)
        {
            DEB_ALWAYS() << "WARNING!!!! - many opened cameras! "
                         << DEB_VAR1(iHandle);
            break;
        }

        err = PCO_OpenCamera(&handleList[iHandle], 0);
        PCO_CHECK_ERROR(err, "PCO_OpenCamera");
        if (err != PCO_NOERROR)
            break;
        iHandleLast = iHandle++;
    }

    if (iHandleLast < 0)
    {
        DEB_ALWAYS() << "ERROR!!!! - no cam found!";
        err = -1;
        return;
    }

    for (iHandle = 0; iHandle <= iHandleLast; iHandle++)
    {
        m_handle = handleList[iHandle];
        _pco_GetCameraType(err);

        snList[iHandle] = _getCameraSerialNumber();

        ptr += __sprintfSExt(ptr, ptrMax - ptr, "%s\n", _getCameraIdn());
    }

    DEB_ALWAYS() << "\n* CAMERA SEARCH:\n" << ptr0;

    HANDLE handleOK = NULL;

    // ---- if sn == 0, it opens the first camera!
    for (iHandle = 0; iHandle <= iHandleLast; iHandle++)
    {
        m_handle = handleList[iHandle];
        DWORD snCam = snList[iHandle];
        if (((snRequested == 0) || (snCam == snRequested)) &&
            (handleOK == NULL))
        {
            handleOK = m_handle;
            DEB_ALWAYS() << "\n* CAMERA FOUND & OPENED: "
                         << DEB_VAR4(snRequested, snCam, iHandle, iHandleLast);
        }
        else
        {
            _pco_CloseCamera(err);
        }
    }

    if (handleOK != NULL)
    {
        m_handle = handleOK;
        err = 0;
    }
    else
    {
        m_handle = NULL;
        DEB_ALWAYS() << "CAMERA NOT FOUND: "
                     << DEB_VAR2(snRequested, iHandleLast);
        err = -1;
    }

#endif
}
//=================================================================================================
//=================================================================================================

void Camera::_pco_GetCameraTypeOnly(int &err)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    err = 0;

#ifdef __linux__
    DEB_ALWAYS() << NOT_IMPLEMENTED;
    return;

#else

    m_pcoData->stcPcoCamType.wSize = sizeof(m_pcoData->stcPcoCamType);
    err = PCO_GetCameraType(m_handle, &m_pcoData->stcPcoCamType);

    PCO_CHECK_ERROR(err, "PCO_GetCameraType");
#endif
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetCameraMinSizeCalc(WORD &wMinSizeHorz, WORD &wMinSizeVert)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    unsigned int uiHorzSteps, uiVertSteps;
    getXYsteps(uiHorzSteps, uiVertSteps);
    WORD wHorzSteps = uiHorzSteps;
    WORD wVertSteps = uiVertSteps;

    WORD wh = _getCameraType();
    if ((wh == CAMERATYPE_PCO_EDGE) || (wh == CAMERATYPE_PCO_EDGE_GL) ||
        (wh == CAMERATYPE_PCO_EDGE_42) || (wh == CAMERATYPE_PCO_EDGE_USB3) ||
        (wh == CAMERATYPE_PCO_EDGE_HS))
    {
        wMinSizeHorz = wHorzSteps;
        wMinSizeVert = wVertSteps;

        if (wMinSizeHorz < 160)
            wMinSizeHorz = 160; // due to camera limitation
        if (wMinSizeVert < 16)
            wMinSizeVert = 16;
        if (wh == CAMERATYPE_PCO_EDGE_42)
        {
            wMinSizeHorz = 40; // due to camera limitation
            wMinSizeVert = 16;
        }
        if ((wh == CAMERATYPE_PCO_EDGE_USB3) || (wh == CAMERATYPE_PCO_EDGE_HS))
        {
            wMinSizeVert = 16;
            wMinSizeHorz = 64; // due to camera limitation
        }
    }
    else
    {
        wMinSizeHorz = wHorzSteps * 2;
        wMinSizeVert = wVertSteps * 2;

        if (wMinSizeHorz < 8)
            wMinSizeHorz = 8;
        if (wMinSizeVert < 8)
            wMinSizeVert = 8;
    }
    if (wHorzSteps > 1)
    {
        int k = (wMinSizeHorz) % wHorzSteps;
        if (k > 0)
        {
            k++;
            wMinSizeHorz = wHorzSteps * k;
        }
    }

    if (wVertSteps > 1)
    {
        int k = (wMinSizeVert) % wVertSteps;
        if (k > 0)
        {
            k++;
            wMinSizeVert = wVertSteps * k;
        }
    }

    DEB_ALWAYS() << DEB_VAR4(wMinSizeHorz, wMinSizeVert, wHorzSteps,
                             wVertSteps);
}
