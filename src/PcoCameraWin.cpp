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

//=========================================================================================================
//=========================================================================================================

#include <exception>

void _pco_acq_thread_dimax(void *argin);

void _pco_acq_thread_dimax_live(void *argin);

void _pco_acq_thread_edge(void *argin);
void _pco_shutter_thread_edge(void *argin);


//=========================================================================================================
//=========================================================================================================

Camera::Camera(const std::string &camPar)
    : m_cam_connected(false), m_acq_frame_nb(1), m_sync(NULL), m_buffer(NULL),
      m_handle(NULL), m_Roi_lastFixed_time(0),
      m_pco_buffer_nrevents(PCO_BUFFER_NREVENTS), m_iPcoAcqMode(0),
      m_bRecorderForcedFifo(false)
{
    DEF_FNID;
    DEB_CONSTRUCTOR();

    int error = 0;
    m_config = true;
    DebParams::checkInit();

    m_msgLog = new ringLog(300);
    m_tmpLog = new ringLog(300);
    if (m_msgLog == NULL)
        throw LIMA_HW_EXC(Error, "m_msgLog > creation error");
    if (m_tmpLog == NULL)
        throw LIMA_HW_EXC(Error, "m_tmpLog > creation error");

    m_pcoData = new stcPcoData();
    if (m_pcoData == NULL)
        throw LIMA_HW_EXC(Error, "m_pcoData > creation error");

    m_checkImgNr = new CheckImgNr(this);

    // properties: params
    paramsInit(camPar.c_str());

    char *value;
    const char *key;
    bool ret;
    int iValue;

    /***
    key = "test";
    key = "withConfig";
    key = "testMode";
    key = "debugPco";
    ***/
    key = "testMode";
    ret = paramsGet(key, value);
    if (ret)
    {
        m_pcoData->testCmdMode = _atoi64(value);
    }

    key = "acqTimeoutRetry";
    ret = paramsGet(key, value);
    iValue = ret ? atoi(value) : 3;
    m_pcoData->acqTimeoutRetry = (iValue < 0) ? 0 : iValue;

    // patch in xMinSize meanwhile firmware for CLHS 1.19 is fixed
    key = "xMinSize";
    m_pcoData->params_xMinSize = !!paramsGet(key, value);

    // ignore the nbMaxImages calculated for dimax HS
    key = "ignoreMaxImages";
    m_pcoData->params_ignoreMaxImages = !!paramsGet(key, value);

    DEB_ALWAYS() << ALWAYS_NL << DEB_VAR1(m_pcoData->version) << ALWAYS_NL
                 << _checkLogFiles(true);

    // m_bin.changed = Invalid;

    _init();
    m_config = false;
    _setActionTimestamp(tsConstructor);
}

//=========================================================================================================
//=========================================================================================================
Camera::~Camera()
{
    DEB_DESTRUCTOR();
    DEB_TRACE() << "DESTRUCTOR ....................";
    if (m_ringBuffer.control)
    {
        m_ringBuffer.control->cameraDestroyed();
        try
        {
            m_sync->stopAcq();
        }
        catch (const Exception& error)
        {
            DEB_ERROR() << "Stopping ring buffer during camera destruction: " << error.getErrMsg();
        }
        catch (const std::exception& error)
        {
            DEB_ERROR() << "Stopping ring buffer during camera destruction: " << error.what();
        }
        m_ringBuffer.control = 0;
    }

    m_cam_connected = false;

    reset(RESET_CLOSE_INTERFACE);
}

//==========================================================================================================
//==========================================================================================================
/** @brief Run the dedicated ring worker from the Windows CRT thread entry point. */
static void pco_ring_buffer_thread(void* camera)
{
    static_cast<Camera*>(camera)->runRingBuffer();
    _endthread();
}

void _pco_acq_thread_dimax(void *argin)
{
    DEF_FNID;

    static char msgErr[LEN_ERROR_MSG + 1];

    int error;
    int _nrStop;
    DWORD _dwPcoValidImageCnt, _dwPcoMaxImageCnt;

    Camera *m_cam = (Camera *)argin;
    SyncCtrlObj *m_sync = m_cam->_getSyncCtrlObj();
    // BufferCtrlObj* m_buffer = m_sync->_getBufferCtrlObj();
    BufferCtrlObj *m_buffer = m_cam->_getBufferCtrlObj();

    char _msg[LEN_MSG + 1];
    __sprintfSExt(_msg, LEN_MSG, "%s> [ENTRY]", fnId);
    m_cam->_traceMsg(_msg);

    m_cam->_sprintComment(true, fnId, "[ENTRY]");

    struct stcPcoData *m_pcoData = m_cam->_getPcoData();
    m_pcoData->traceAcq.fnId = fnId;

    const char *msg;
    TIME_USEC tStart, tStart0;
    msElapsedTimeSet(tStart);
    tStart0 = tStart;

    long timeout, timeout0, msNowRecordLoop, msRecord, msXfer, msTotal;
    DWORD dwPcoAcquiredFrames;
    int iLimaAcquiredFrames;

    int requestStop = stopNone;

    HANDLE m_handle = m_cam->getHandle();

    WORD wSegment;
    m_cam->_pco_GetActiveRamSegment(wSegment, error);
    double msPerFrame = (m_cam->pcoGetCocRunTime() * 1000.);
    m_pcoData->traceAcq.msImgCoc = msPerFrame;

    // DWORD dwMsSleepOneFrame = (DWORD) (msPerFrame + 0.5);	// 4/5 rounding
    DWORD dwMsSleepOneFrame = (DWORD)(msPerFrame / 5.0); // 4/5 rounding
    if (dwMsSleepOneFrame == 0)
        dwMsSleepOneFrame = 1; // min sleep

    WORD wDoubleImage;
    int err;
    m_cam->_pco_GetDoubleImageMode(wDoubleImage, err);

    bool nb_frames_fixed = false;

    int iLimaRequestedFrames;
    m_sync->getNbFrames(iLimaRequestedFrames);
    m_pcoData->traceAcq.nrImgRequested0 = iLimaRequestedFrames;
    WORD dwPcoRequestedFrames =
        wDoubleImage ? iLimaRequestedFrames / 2 : iLimaRequestedFrames;

    m_sync->setAcqFrames(0);

    timeout = timeout0 =
        (long)(msPerFrame * (iLimaRequestedFrames * 1.3)); // 30% guard
    if (timeout < TOUT_MIN_DIMAX)
        timeout = TOUT_MIN_DIMAX;

    m_pcoData->traceAcq.msTout = m_pcoData->msAcqTout = timeout;
    _dwPcoValidImageCnt = 0;

    m_sync->getExpTime(m_pcoData->traceAcq.sExposure);
    m_sync->getLatTime(m_pcoData->traceAcq.sDelay);

    m_sync->setExposing(pcoAcqRecordStart);

    while (true)
    {
        m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwPcoValidImageCnt,
                                               _dwPcoMaxImageCnt, error);
        if (error)
        {
            printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__,
                   "_pco_GetNumberOfImagesInSegment");
            throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
        }

        m_pcoData->dwValidImageCnt[wSegment - 1] =
            m_pcoData->traceAcq.nrImgRecorded = _dwPcoValidImageCnt;
        m_pcoData->dwMaxImageCnt[wSegment - 1] =
            m_pcoData->traceAcq.maxImgCount = _dwPcoMaxImageCnt;

        m_pcoData->msAcqTnow = msNowRecordLoop = msElapsedTime(tStart);
        m_pcoData->traceAcq.msRecordLoop = msNowRecordLoop;

        if ((dwPcoRequestedFrames > _dwPcoMaxImageCnt))
        {
            nb_frames_fixed = true;

            __sprintfSExt(msgErr, LEN_ERROR_MSG,
                          "=== %s [%d]> ERROR INVALID NR FRAMES fixed "
                          "nb_frames[%d] _dwPcoMaxImageCnt[%d]",
                          fnId, __LINE__, dwPcoRequestedFrames,
                          _dwPcoMaxImageCnt);
            printf("%s\n", msgErr);

            m_sync->setExposing(pcoAcqError);
            break;
        }

        if ((_dwPcoValidImageCnt >= dwPcoRequestedFrames))
            break;

        if ((timeout < msNowRecordLoop) && !m_pcoData->bExtTrigEnabled)
        {
            // m_sync->setExposing(pcoAcqRecordTimeout);
            // m_sync->stopAcq();
            m_sync->setExposing(pcoAcqStop);
            printf("=== %s [%d]> TIMEOUT!!! tout[(%ld) 0(%ld)] recLoopTime[%ld "
                   "ms] lastImgRecorded[%ld] dwPcoRequestedFrames[%d]\n",
                   fnId, __LINE__, timeout, timeout0, msNowRecordLoop,
                   _dwPcoValidImageCnt, dwPcoRequestedFrames);
            break;
        }

        if ((requestStop = m_sync->_getRequestStop(_nrStop)) == stopRequest)
        {
            m_sync->_setRequestStop(stopNone);

            char msg[LEN_TRACEACQ_MSG + 1];
            // m_buffer->_setRequestStop(stopProcessing);
            // m_sync->setExposing(pcoAcqStop);

            snprintf(msg, LEN_TRACEACQ_MSG,
                     "=== %s> STOP REQ (recording). lastImgRec[%d]\n", fnId,
                     _dwPcoValidImageCnt);
            printf(msg);
            m_pcoData->traceMsg(msg);
            break;
        }
        Sleep(dwMsSleepOneFrame); // sleep 1 frame
    }                             // while(true)

    m_pcoData->msAcqTnow = msNowRecordLoop = msElapsedTime(tStart);
    m_pcoData->traceAcq.msRecordLoop = msNowRecordLoop;

    msg = m_cam->_pco_SetRecordingState(0, error);
    if (error)
    {
        printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
        throw LIMA_HW_EXC(Error, "_pco_SetRecordingState");
    }

    if ((requestStop != stopRequest) && (!nb_frames_fixed))
    {
        if (m_sync->getExposing() == pcoAcqRecordStart)
            m_sync->setExposing(pcoAcqRecordEnd);

        m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwPcoValidImageCnt,
                                               _dwPcoMaxImageCnt, error);
        if (error)
        {
            printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
            throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
        }

        m_pcoData->dwValidImageCnt[wSegment - 1] =
            m_pcoData->traceAcq.nrImgRecorded = _dwPcoValidImageCnt;

        dwPcoAcquiredFrames = (_dwPcoValidImageCnt < dwPcoRequestedFrames)
                                  ? _dwPcoValidImageCnt
                                  : dwPcoRequestedFrames;
        iLimaAcquiredFrames =
            wDoubleImage ? dwPcoAcquiredFrames * 2 : dwPcoAcquiredFrames;
        // m_sync->setAcqFrames(nb_acq_frames);

        // dimax recording time
        m_pcoData->msAcqRec = msRecord = msElapsedTime(tStart);
        m_pcoData->traceAcq.msRecord = msRecord; // loop & stop record

        m_pcoData->traceAcq.endRecordTimestamp = m_pcoData->msAcqRecTimestamp =
            getTimestamp();

        m_pcoData->traceAcq.nrImgAcquired = iLimaAcquiredFrames;
        m_pcoData->traceAcq.nrImgRequested = iLimaRequestedFrames;

        msElapsedTimeSet(tStart); // reset for xfer

        if (iLimaAcquiredFrames < iLimaRequestedFrames)
            m_sync->setNbFrames(iLimaAcquiredFrames);

        //		if(m_buffer->_getRequestStop()) {
        //			m_sync->setExposing(pcoAcqStop);
        //		} else

        // --- in case of stop request during the record phase, the transfer
        // --- is made to avoid lose the image recorded
        {
            pcoAcqStatus status;

            if (m_cam->_isCameraType(Pco2k | Pco4k))
            {
                if (m_pcoData->testCmdMode & TESTCMDMODE_DIMAX_XFERMULTI)
                {
                    status = (pcoAcqStatus)m_buffer->_xferImag();
                }
                else
                {
                    if (wDoubleImage)
                    {
                        status =
                            (pcoAcqStatus)m_buffer
                                ->_xferImagMultDoubleImage(); //  <-------------
                                                              //  default
                                                              //  pco2k/4k NO
                                                              //  waitobj
                    }
                    else
                    {
                        status =
                            (pcoAcqStatus)
                                m_buffer->_xferImagMult(); //  <-------------
                                                           //  default pco2k/4k
                                                           //  NO waitobj
                    }
                }
            }
            else
            {
                if (m_pcoData->testCmdMode & TESTCMDMODE_DIMAX_XFERMULTI)
                {
                    status = (pcoAcqStatus)m_buffer->_xferImagMult();
                }
                else
                {
                    if (wDoubleImage)
                    {
                        status =
                            (pcoAcqStatus)m_buffer
                                ->_xferImagDoubleImage(); //  <-------------
                                                          //  default dimax YES
                                                          //  waitobj
                    }
                    else
                    {
                        status = (pcoAcqStatus)
                                     m_buffer->_xferImag(); //  <-------------
                                                            //  default dimax
                                                            //  YES waitobj
                    }
                }
            }

            if (nb_frames_fixed)
                status = pcoAcqError;
            m_sync->setExposing(status);
        }

    } // if nb_frames_fixed && no stopRequested

    // m_sync->setExposing(status);
    m_pcoData->dwMaxImageCnt[wSegment - 1] = m_pcoData->traceAcq.maxImgCount =
        _dwPcoMaxImageCnt;

    // traceAcq info - dimax xfer time
    m_pcoData->msAcqXfer = msXfer = msElapsedTime(tStart);
    m_pcoData->traceAcq.msXfer = msXfer;

    m_pcoData->msAcqAll = msTotal = msElapsedTime(tStart0);
    m_pcoData->traceAcq.msTotal = msTotal;

    m_pcoData->traceAcq.endXferTimestamp = m_pcoData->msAcqXferTimestamp =
        getTimestamp();

    __sprintfSExt(_msg, LEN_MSG,
                  "%s [%d]> [EXIT] imgRecorded[%d] coc[%g] recLoopTime[%ld] "
                  "tout[(%ld) 0(%ld)] rec[%ld] xfer[%ld] all[%ld](ms)\n",
                  fnId, __LINE__, _dwPcoValidImageCnt, msPerFrame,
                  msNowRecordLoop, timeout, timeout0, msRecord, msXfer,
                  msTotal);
    m_cam->_traceMsg(_msg);

    // included in 34a8fb6723594919f08cf66759fe5dbd6dc4287e only for dimax (to
    // check for others)
    m_sync->setStarted(false);

#if 0
    if (requestStop == stopRequest)
    {
        Event* ev = new Event(Hardware, Event::Error, Event::Camera, Event::CamFault, errMsg);
        m_cam->_getPcoHwEventCtrlObj()->reportEvent(ev);
    }

#endif

    _endthread();
}

//=====================================================================
//=====================================================================

void _pco_acq_thread_edge(void *argin)
{
    DEF_FNID;

    Camera *m_cam = (Camera *)argin;
    SyncCtrlObj *m_sync = m_cam->_getSyncCtrlObj();
    // BufferCtrlObj* m_buffer = m_sync->_getBufferCtrlObj();
    BufferCtrlObj *m_buffer = m_cam->_getBufferCtrlObj();

    char _msg[LEN_MSG + 1];
    __sprintfSExt(_msg, LEN_MSG, "%s> [ENTRY]", fnId);
    m_cam->_traceMsg(_msg);

    m_cam->_sprintComment(true, fnId, "[ENTRY]");

    struct stcPcoData *m_pcoData = m_cam->_getPcoData();

    TIME_USEC tStart;
    msElapsedTimeSet(tStart);
    int error;
    long msXfer;
    int requestStop = stopNone;
    pcoAcqStatus status;

    HANDLE m_handle = m_cam->getHandle();

    m_sync->setAcqFrames(0);

    if (m_pcoData->testCmdMode & TESTCMDMODE_EDGE_XFER)
    {
        status = (pcoAcqStatus)m_buffer->_xferImag_getImage_edge();
    }
    else
    {
        status = (pcoAcqStatus)m_buffer->_xferImag(); // original
    }

    m_sync->setExposing(status);
    // m_sync->stopAcq();

    printf("=== %s [%d]> _pco_SetRecordingState(0)\n", fnId, __LINE__);

    const char *msg = m_cam->_pco_SetRecordingState(0, error);
    if (error)
    {
        printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
        // throw LIMA_HW_EXC(Error, "_pco_SetRecordingState");
    }

    m_pcoData->traceAcq.fnId = fnId;

    m_sync->getExpTime(m_pcoData->traceAcq.sExposure);
    m_sync->getLatTime(m_pcoData->traceAcq.sDelay);

    m_pcoData->msAcqXfer = msXfer = msElapsedTime(tStart);
    __sprintfSExt(_msg, LEN_MSG, "%s> [EXIT] xfer[%ld] (ms) status[%s]\n", fnId,
                  msXfer, sPcoAcqStatus[status]);
    m_cam->_traceMsg(_msg);

    m_sync->setStarted(false); // updated

    char *errMsg = NULL;
    switch (status)
    {
        case pcoAcqRecordTimeout:
            errMsg = "pcoAcqRecordTimeout";
            break;
        case pcoAcqWaitTimeout:
            errMsg = "pcoAcqWaitTimeout";
            break;
        case pcoAcqWaitError:
            errMsg = "pcoAcqWaitError";
            break;
        case pcoAcqError:
            errMsg = "pcoAcqError";
            break;
        case pcoAcqPcoError:
            errMsg = "pcoAcqPcoError";
            break;
        default:
            errMsg = NULL;
    }

    if (errMsg != NULL)
    {
        Event *ev = new Event(Hardware, Event::Error, Event::Camera,
                              Event::CamFault, errMsg);
        //m_cam->_getPcoHwEventCtrlObj()->reportEvent(ev);
    }

    printf("=== %s [%d]> TRACE %s\n", fnId, __LINE__, "exit thread");

    _endthread();
}

//=====================================================================
//=====================================================================

void _pco_acq_thread_dimax_live(void *argin)
{
    DEF_FNID;

    Camera *m_cam = (Camera *)argin;
    SyncCtrlObj *m_sync = m_cam->_getSyncCtrlObj();
    // BufferCtrlObj* m_buffer = m_sync->_getBufferCtrlObj();
    BufferCtrlObj *m_buffer = m_cam->_getBufferCtrlObj();

    char _msg[LEN_MSG + 1];
    __sprintfSExt(_msg, LEN_MSG, "%s> [ENTRY]", fnId);
    m_cam->_traceMsg(_msg);

    struct stcPcoData *m_pcoData = m_cam->_getPcoData();

    TIME_USEC tStart;
    msElapsedTimeSet(tStart);
    int error;
    long msXfer;
    int requestStop = stopNone;

    pcoAcqStatus status;

    m_cam->_sprintComment(true, fnId, "[ENTRY]");

    HANDLE m_handle = m_cam->getHandle();

    m_sync->setAcqFrames(0);

    // dimax recording time -> live NO record
    m_pcoData->msAcqRec = 0;
    m_pcoData->msAcqRecTimestamp = getTimestamp();

    //status = (pcoAcqStatus)m_buffer->_xferImag();
    
    if ( !(m_pcoData->testCmdMode & TESTCMDMODE_DIMAX_XFERMULTI))
    {
        // _xferImagMult [PCO_GetImageEx]
        status = (pcoAcqStatus)m_buffer->_xferImagMult();
    }
    else
    {
        // TODO wDoubleImage
        
        // _xferImag [WaitForMultipleObjects]
        status = (pcoAcqStatus) m_buffer->_xferImag();
    }
    
    m_sync->setExposing(status);
    // m_sync->stopAcq();
    const char *msg = m_cam->_pco_SetRecordingState(0, error);
    if (error)
    {
        printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
        // throw LIMA_HW_EXC(Error, "_pco_SetRecordingState");
    }

    // dimax xfer time
    m_pcoData->msAcqXfer = msXfer = msElapsedTime(tStart);
    m_pcoData->msAcqXferTimestamp = getTimestamp();
    __sprintfSExt(_msg, LEN_MSG, "%s> [EXIT] xfer[%ld] (ms) status[%s]\n", fnId,
                  msXfer, sPcoAcqStatus[status]);
    m_cam->_traceMsg(_msg);

    m_sync->setStarted(false); // to test

    _endthread();
}

//=================================================================================================
//=================================================================================================
void Camera::_set_shutter_rolling_edge(DWORD dwRolling, int &error)
{
    DEB_MEMBER_FUNCT();
    error = 0;

    if (!_isValid_rollingShutter(dwRolling))
    {
        DEB_ALWAYS() << "ERROR requested Rolling Shutter not allowed "
                     << DEB_VAR1(dwRolling);
        error = -1;
        return;
    }

    m_pcoData->dwRollingShutter = dwRolling;

    DEB_TRACE() << "requested Rolling Shutter OK " << DEB_VAR1(dwRolling);

    _beginthread(_pco_shutter_thread_edge, 0, (void *)this);

    return;
}
//=====================================================================
//=====================================================================
void _pco_shutter_thread_edge(void *argin)
{
    DEF_FNID;
    int error;

    Camera *m_cam = (Camera *)argin;
    // SyncCtrlObj* m_sync = m_cam->_getSyncCtrlObj();

    char _msg[LEN_MSG + 1];
    __sprintfSExt(_msg, LEN_MSG, "%s> [ENTRY]", fnId);
    m_cam->_traceMsg(_msg);

    m_cam->_sprintComment(true, fnId, "[ENTRY]");

    m_cam->_pco_set_shutter_rolling_edge(error);

    __sprintfSExt(_msg, LEN_MSG, "%s> [EXIT]", fnId);
    m_cam->_traceMsg(_msg);

    // m_sync->setStarted(false); // to test

    _endthread();
} //=================================================================================================
//=================================================================================================

bool Camera::_isRunAfterAssign()
{
    return (_isCameraType(Edge) && (_isInterfaceType(ifCameralinkAll)));
}

//=================================================================================================
//=================================================================================================
void Camera::startAcq()
{
    DEB_MEMBER_FUNCT();
    TrigMode trigger;
    m_sync->getTrigMode(trigger);
    if (!m_sync->checkTrigMode(trigger))
        THROW_HW_ERROR(NotSupported) << "Trigger type not supported for this camera";
    if (_isCameraType(Dimax) && trigger == ExtTrigSingle && !getRingBufferEnabled())
        THROW_HW_ERROR(NotSupported)
            << "EXTERNAL_SINGLE on Dimax requires ringBufferEnabled=true";

    m_acq_frame_nb = -1;
    m_pcoData->pcoError = 0;
    m_pcoData->pcoErrorMsg[0] = 0;

    m_pcoData->traceAcqClean();

    TIME_USEC tStart;
    msElapsedTimeSet(tStart);

    //=====================================================================
    DEF_FNID;
    HANDLE hEvent = NULL;

    DEB_ALWAYS() << _sprintComment(false, fnId, "[ENTRY]") << _checkLogFiles();

    int error;

    //------------------------------------------------- start acquisition

    m_pcoData->traceAcq.msStartAcqStart = msElapsedTime(tStart);

    m_sync->setStarted(true);
    // m_sync->setExposing(pcoAcqRecordStart);
    m_sync->setExposing(pcoAcqStart);

    int iRequestedFrames;
    m_sync->getNbFrames(iRequestedFrames);

    int forced = 0;
    getRecorderForcedFifo(forced);

    int iPending = 0;
    error = PCO_GetPendingBuffer(m_handle, &iPending);
    PCO_THROW_OR_TRACE(error, "PCO_GetPendingBuffer");
    if (iPending < m_pco_buffer_nrevents)
    {
        error = PCO_CancelImages(m_handle);
        PCO_THROW_OR_TRACE(error, "PCO_CancelImages");
        DEB_ALWAYS() << "PCO_CancelImages " << DEB_VAR1(iPending);
    }

    unsigned long ulFramesMaxInSegment =
        _pco_GetNumberOfImagesInSegment_MaxCalc(m_pcoData->wActiveRamSegment);
    unsigned long ulRequestedFrames = (unsigned long)iRequestedFrames;

    if (ulRequestedFrames > 0)
    {
        WORD wDoubleImage;
        int err;

        // Double Image -> requested images will be the total nr of images
        // (basic + primary)
        //      must be even and twice of the nr of images for pco
        _pco_GetDoubleImageMode(wDoubleImage, err);
        PCO_THROW_OR_TRACE(err, "PCO_GetDoubleImageMode");

        bool bOutOfRange = false;

        if ((wDoubleImage) && ((ulRequestedFrames % 2) != 0))
        {
            DEB_ALWAYS() << "\nERROR odd nr of frames in DoubleImage";
            bOutOfRange = true;
        }

        const unsigned long ramFrames =
            getRingBufferEnabled() && wDoubleImage ? ulRequestedFrames / 2 : ulRequestedFrames;
        if ((ulFramesMaxInSegment > 0) &&
            (ramFrames > ulFramesMaxInSegment) && (!forced))
        {
            DEB_ALWAYS() << "\nERROR many frames in record mode";
            bOutOfRange = true;
        }

        if (bOutOfRange)
        {
            char msg[MSG1K];
            __sprintfSExt(
                msg, sizeof(msg) - 1,
                "ERROR frames OUT OF RANGE ulRequestedFrames[%ld], "
                "ulFramesMaxInSegment[%ld], wDoubleImage[%d], forced[%d]",
                ulRequestedFrames, ulFramesMaxInSegment, wDoubleImage, forced);

            DEB_ALWAYS() << msg;
            m_sync->setStarted(false);
            m_sync->setExposing(pcoAcqError);

            throw LIMA_EXC(CameraPlugin, InvalidValue, msg);
            return;
        }
    }

    if (getRingBufferEnabled())
    {
        if (forced)
            THROW_HW_ERROR(NotSupported) << "Ring buffer is incompatible with forced FIFO";
        RingBufferControl* control;
        {
            AutoMutex lock(m_ringBuffer.cond.mutex());
            if (!m_ringBuffer.prepared || !m_ringBuffer.control)
                THROW_HW_ERROR(Error) << "Ring buffer was not prepared";
            control = m_ringBuffer.control;
        }
        try
        {
            control->start();
        }
        catch (const std::exception& error)
        {
            THROW_HW_ERROR(Error) << error.what();
        }
        _pco_SetRecordingState(1, error);
        PCO_THROW_OR_TRACE(error, "PCO_SetRecordingState");
        {
            AutoMutex lock(m_ringBuffer.cond.mutex());
            m_ringBuffer.worker = true;
        }
        if (_beginthread(pco_ring_buffer_thread, 0, this) == static_cast<uintptr_t>(-1L))
        {
            {
                AutoMutex lock(m_ringBuffer.cond.mutex());
                m_ringBuffer.worker = false;
            }
            _pco_SetRecordingState(0, error);
            PCO_THROW_OR_TRACE(error, "PCO_SetRecordingState after thread creation failure");
            THROW_HW_ERROR(Error) << "Cannot start the ring buffer acquisition thread";
        }
        return;
    }

    if (!_isRunAfterAssign())
    {
        DEB_TRACE() << "========================= recordingState 1 - BEFORE "
                       "ASSIGN (startAcq)";
        _pco_SetRecordingState(1, error);
    }

    if (_isCameraType(Edge))
    {
        _beginthread(_pco_acq_thread_edge, 0, (void *)this);

#if 0
        AutoMutex lock(m_cond.mutex());

        bool resWait;
        int retry = 3;
        int val, val0; val0 = pcoAcqRecordStart;

        while (((val = m_sync->getExposing()) != val0) && retry--)
        {
            DEB_TRACE() << "+++ getExposing / pcoAcqRecordStart - WAIT - " << DEB_VAR3(val, val0, retry);
            resWait = m_cond.wait(2.);
        }
        DEB_TRACE() << "+++ getExposing / pcoAcqRecordStart - EXIT - " << DEB_VAR3(val, val0, retry);
        lock.unlock();
#endif

        m_pcoData->traceAcq.msStartAcqEnd = msElapsedTime(tStart);
        return;
    }

    if (_isCameraType(Dimax | Pco2k | Pco4k))
    {
        int iRequestedFrames;
        m_sync->getNbFrames(iRequestedFrames);

        _pco_SetRecordingState(1, error);

        int forcedFifo = 0;
        getRecorderForcedFifo(forcedFifo);

        if ((iRequestedFrames > 0) && (forcedFifo == 0))
        {
            _beginthread(_pco_acq_thread_dimax, 0, (void *)this);
        }
        else
        {
            _beginthread(_pco_acq_thread_dimax_live, 0, (void *)this);
        }
        m_pcoData->traceAcq.msStartAcqEnd = msElapsedTime(tStart);
        return;
    }

    throw LIMA_HW_EXC(Error, "unkown camera type");
    return;
}
