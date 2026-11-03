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

#include "lima/Exceptions.h"
#include "PcoBufferCtrlObj.h"
#include "PcoSyncCtrlObj.h"
#include "PcoCamera.h"
#include <sys/timeb.h>
#include <time.h>

#undef DEBUG_XFER_IMAG
#define COMPILE_WAIT_CONDITION
#undef COMPILEIT
#define USING_PCO_ALLOCATED_BUFFERS
#define EVENT_WAIT_TMOUT_MS 10000

#define THROW_LIMA_HW_EXC(e, x)                                                \
    {                                                                          \
        printf("========*** LIMA_HW_EXC %s\n", x);                             \
        throw LIMA_HW_EXC(e, x);                                               \
    }

using namespace lima;
using namespace lima::Pco;

//=========================================================================================================
const char *_timestamp_pcobufferctrlobj()
{
    return ID_FILE_TIMESTAMP;
}
//=========================================================================================================

//=========================================================================================================
//=========================================================================================================
BufferCtrlObj::BufferCtrlObj(Camera *cam)
    : m_cam(cam), m_pcoData(m_cam->_getPcoData())
// m_status(0)
{
    DEB_CONSTRUCTOR();

    // SoftBufferCtrlObj::Sync &m_bufferSync = *getBufferSync(cond);
    m_bufferSync = getBufferSync(cond);

    //----------------------------------------------- initialization buffers &
    // creating events

    for (int i = 0; i < PCO_MAX_NR_ALLOCATED_BUFFERS; i++)
    {
        m_allocBuff.pcoAllocBufferNr[i] = -1;
        m_allocBuff.pcoAllocBufferPtr[i] = NULL;
        m_allocBuff.dwPcoAllocBufferSize[i] = 0;

        m_allocBuff.limaAllocBufferPtr[i] = NULL;
        m_allocBuff.limaAllocBufferPtr1[i] = NULL;
        m_allocBuff.dwLimaAllocBufferSize[i] = 0;
    }
    m_allocBuff.pcoAllocBufferDone = false;
    m_allocBuff.createEventsDone = false;
}
//=========================================================================================================
//=========================================================================================================
void BufferCtrlObj::prepareAcq()
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[ENTRY]");
    _pcoAllocBuffers();

    FrameDim dim;

    getFrameDim(dim);

    m_ImageBufferSize = dim.getMemSize();

    DEB_TRACE() << "[exit] " << DEB_VAR2(dim, m_ImageBufferSize);
}

//=========================================================================================================
//=========================================================================================================
void BufferCtrlObj::startAcq()
{
    DEB_MEMBER_FUNCT();
    std::string name;

    StdBufferCbMgr &buffer_mgr = m_buffer_cb_mgr;
    buffer_mgr.setStartTimestamp(Timestamp::now());
}

//===================================================================================================================
// linux break
//===================================================================================================================

//===================================================================================================================
//===================================================================================================================
void *BufferCtrlObj::_getFrameBufferPtr(int lima_buffer_nb,
                                        int &nb_allocated_buffers)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    const char *msg;
    double timeout = 30;
    StdBufferCbMgr &buffer_mgr = m_buffer_cb_mgr;

    Sync::Status status;
    AutoMutex lock(cond.mutex());

    void *myBuffer = NULL;

    Timestamp t0 = Timestamp::now();

    for (bool doit = true; doit;)
    {
        double wait_timeout = timeout - double(Timestamp::now() - t0);
        if (wait_timeout <= 0)
        {
            msg = "=== Sync wait INTERRUPTED + TIMEOUT === ";
            DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
            return NULL;
        }

        status = m_bufferSync->wait(lima_buffer_nb, wait_timeout);
        if (m_cam->_getDebug(DBG_LIMABUFF))
        {
            DEB_ALWAYS() << DEB_VAR3(lima_buffer_nb, timeout, status);
        }

        switch (status)
        {
            case Sync::AVAILABLE:
                myBuffer = buffer_mgr.getFrameBufferPtr(lima_buffer_nb);
                buffer_mgr.getNbBuffers(nb_allocated_buffers);
                doit = false;
                break;
            case Sync::TIMEOUT:
                msg = "=== Sync wait TIMEOUT === ";
                DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                return NULL;
                break;

            case Sync::INTERRUPTED:
                msg = "=== Sync wait INTERRUPTED === ";
                DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                break;

            default:
                msg = "=== Sync wait UNKNOWN STATUS === ";
                DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
        }
    }

    return myBuffer;
}
//===================================================================================================================
//===================================================================================================================

//===================================================================================================================
// win break
//===================================================================================================================

//===================================================================================================================
//===================================================================================================================
/********************************************************************************************************
  4.7.22 PCO_AddBufferExtern (Only for experienced users!)

  Adds a buffer to the driver queue. This function returns immediately. If
the desired image is transferred to the buffer the buffer event will be fired.
The user can start a thread, which can wait for the event of the buffer
(WaitFor(Single/Multiple)Objects). This function can be used to view images
while the recording is enabled (the user must set dw1stImage=dwLastImage=0). To
read out previously recorded images with recording disabled, the user can call
  PCO_GetImageEx. Nevertheless you can use this function to read out single
images while the camera is not in recording state, by setting
dw1stImage=dwLastImage=x, where x is a valid image number (1 ... max available).

  a.) Prototype:
  SC2_SDK_FUNC int WINAPI PCO_AddBufferExtern(HANDLE ph, HANDLE hEvent,
DWORD dw1stImage, DWORD dwLastImage, DWORD dwSynch, void* pBuf, DWORD dwLen,
DWORD* dwStatus)

b.) Input parameter:
  · HANDLE ph: Handle to a previously opened camera device.
  · HANDLE hEvent: Handle to an externally allocated event.
  · DWORD dw1stImage: Set dw1stImage=dwLastImage=0 during record for actual
image · DWORD dwLastImage: Set dw1stImage=dwLastImage=x after record for desired
image · DWORD dwSynch: Synchronization paremeter, usually 0. · void *pBuf:
Pointer to the buffer to receive the transferred image. · DWORD dwLen: Length of
the buffer. · DWORD *dwStatus: Driver status.

  The input data should be filled with the following parameter:
  · hEvent = externally created event used to signal an occurred transfer.

  · dw1stImage = set to 0 for live view mode("live view" transfers the most
recent image to the PC for viewing / monitoring)
  - 0 = live view mode. x = set to the same value as dwLastImage. Has to be
a valid image number (see PCO_GetNumberOfImagesInSegment, 1 .. max available).

  · dwLastImage = set to 0 in preview mode.
  - 0 = live view mode. x = set to the same value as dw1stImage. Has to be a
valid image number (see PCO_GetNumberOfImagesInSegment, 1 ... max available).

  · dwSynch: set to 0.
  · pBuf: Address of the first buffer element to which the image should be
transferred. · dwLen: Length of the buffer in bytes. · dwStatus: Address of a
DWORD to receive the buffer status.

  c.) Return value:
  · int: Error message, 0 in case of success else less than 0: see Error /
Warning Codes
********************************************************************************************************/
//===================================================================================================================
//===================================================================================================================
#define BUFFER_DUMMY_IMG_LEN (2 * 2016 * 2016)

int BufferCtrlObj::_assignImage2Buffer(DWORD &dwFrameFirst, DWORD &dwFrameLast,
                                       DWORD dwRequestedFrames, int bufIdx,
                                       bool live_mode, WORD wDoubleImage)
{
    DEF_FNID;
    // static char _buffer[2][BUFFER_DUMMY_IMG_LEN];
    DEB_MEMBER_FUNCT();
    int error = 0;
    char *sErr;
    void *myLimaBufferPtr;
    void *myLimaBufferPtr1;
    int myLimaBufferNr, myLimaBufferNr1;
    DWORD myLimaBufferLen;
    DWORD myPcoBufferLen;
    int lima_buffer_nb;
    double timeout = 30;
    const char *msg;

    unsigned long long dbgWaitobj = m_cam->_getDebug(DBG_WAITOBJ);

    if (m_cam->_getDebug(DBG_ASSIGN_BUFF))
    {
        DEB_ALWAYS() << "entry -> "
                     << DEB_VAR4(dwFrameFirst, dwFrameLast, dwRequestedFrames,
                                 bufIdx);
    }

    StdBufferCbMgr &buffer_mgr = m_buffer_cb_mgr;

    buffer_mgr.setStartTimestamp(Timestamp::now());

    //------------------------------------------------------------------------------------------
    //---------- get the 1rst lima buffer ptr (for Double Image - 2 consecutive
    // buffers
    //------------------------------------------------------------------------------------------

    lima_buffer_nb = wDoubleImage ? (dwFrameFirst - 1) * 2 : dwFrameFirst - 1;

#ifdef COMPILE_WAIT_CONDITION
    Sync::Status status;
    AutoMutex lock(cond.mutex());

    Timestamp t0 = Timestamp::now();

    for (bool doit = true; doit;)
    {
        double wait_timeout = timeout - double(Timestamp::now() - t0);
        if (wait_timeout <= 0)
        {
            msg = "=== Sync wait INTERRUPTED + TIMEOUT === ";
            DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
            return -1;
        }

        status = m_bufferSync->wait(lima_buffer_nb, wait_timeout);
        if (m_cam->_getDebug(DBG_LIMABUFF))
        {
            DEB_ALWAYS() << DEB_VAR3(lima_buffer_nb, timeout, status);
        }

        switch (status)
        {
            case Sync::AVAILABLE:
                myLimaBufferPtr = buffer_mgr.getFrameBufferPtr(lima_buffer_nb);
                myLimaBufferNr = lima_buffer_nb;
                buffer_mgr.getNbBuffers(m_pcoData->iAllocatedBufferNumberLima);
                doit = false;
                break;
            case Sync::TIMEOUT:
                msg = "=== Sync wait TIMEOUT === ";
                DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                return -1;
            case Sync::INTERRUPTED:
                msg = "=== Sync wait INTERRUPTED === ";
                DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                break;
            default:
                msg = "=== Sync wait UNKNOWN STATUS === ";
                DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                return -1;
        } // switch
    }     // for

    myLimaBufferPtr1 = NULL;

    //------------------------------------------------------------------------------------------
    //---------- get the 2nd lima buffer ptr (for Double Image)
    //------------------------------------------------------------------------------------------
    if (wDoubleImage)
    {
        for (bool doit = true; doit;)
        {
            double wait_timeout = timeout - double(Timestamp::now() - t0);
            if (wait_timeout <= 0)
            {
                msg = "=== Sync wait INTERRUPTED + TIMEOUT === ";
                DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                return -1;
            }

            status = m_bufferSync->wait(lima_buffer_nb, wait_timeout);
            if (m_cam->_getDebug(DBG_LIMABUFF))
            {
                DEB_ALWAYS() << DEB_VAR3(lima_buffer_nb, timeout, status);
            }

            switch (status)
            {
                case Sync::AVAILABLE:
                    myLimaBufferPtr1 =
                        buffer_mgr.getFrameBufferPtr(lima_buffer_nb + 1);
                    myLimaBufferNr1 = lima_buffer_nb + 1;
                    buffer_mgr.getNbBuffers(
                        m_pcoData->iAllocatedBufferNumberLima);
                    doit = false;
                    break;
                case Sync::TIMEOUT:
                    msg = "=== Sync wait TIMEOUT === ";
                    DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                    return -1;
                case Sync::INTERRUPTED:
                    msg = "=== Sync wait INTERRUPTED === ";
                    DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                    break;
                default:
                    msg = "=== Sync wait UNKNOWN STATUS === ";
                    DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                    return -1;
            } // switch
        }     // for
    }         // if double img

#else
#    pragma message(                                                           \
        "============================================== BYPASSED ----- COMPILE_WAIT_CONDITION -----")

    myLimaBufferPtr = buffer_mgr.getFrameBufferPtr(lima_buffer_nb);
#endif

    unsigned int uiMaxWidth, uiMaxHeight;

    WORD wArmWidth = m_pcoData->m_wArmWidth;
    WORD wArmHeight = m_pcoData->m_wArmHeight;
    WORD _wMaxWidth = m_pcoData->m_wMaxWidth;
    WORD _wMaxHeight = m_pcoData->m_wMaxHeight;
    WORD wBitPerPixel;
    unsigned int bytesPerPixel;

    //WORD _wMaxWidth, _wMaxHeight;
    m_cam->_pco_GetSizes(&wArmWidth, &wArmHeight, &_wMaxWidth, &_wMaxHeight,
                         error);
    m_cam->getMaxWidthHeight(uiMaxWidth, uiMaxHeight);
    m_cam->getBytesPerPixel(bytesPerPixel);
    m_cam->getBitsPerPixel(wBitPerPixel);

    DWORD dwArmLen = wArmWidth * wArmHeight * bytesPerPixel;
    // DWORD dwAllocatedBufferSize = dwMaxWidth * dwMaxHeight * (DWORD)
    // bytesPerPixel;

    if (dbgWaitobj)
    {
        DEB_ALWAYS() << "...  "
                     << DEB_VAR6(dwArmLen, wArmWidth, wArmHeight, bytesPerPixel,
                                 _wMaxWidth, _wMaxHeight);
    }
    // in double image, wArmHeight reports the doble size!
    // wArmWidth=2048, wArmHeight=4096, bytesPerPixel=2, _wMaxWidth=2112,
    // _wMaxHeight=4144

    myLimaBufferLen = wDoubleImage ? dwArmLen / 2 : dwArmLen;
    m_ImageBufferSize = dwArmLen;
    // DWORD myPcoBufferLenReal = dwArmLen;

#ifdef DEBUG_XFER_IMAG
    DEB_ALWAYS() << "_assignImage2Buffer: "
                 << DEB_VAR3(myBufferLen, myPcoBufferLen, dwArmLen);
#endif

    FrameDim dim;
    getFrameDim(dim);
    int dimSize = dim.getMemSize();

    if (m_cam->_getDebug(DBG_ASSIGN_BUFF))
    {
        DEB_ALWAYS() << DEB_VAR5(dwArmLen, dimSize, myLimaBufferPtr,
                                 lima_buffer_nb,
                                 m_pcoData->iAllocatedBufferNumberLima);
    }

    if (myLimaBufferPtr == NULL)
    {
        DEB_ALWAYS() << "ERROR myLimaBufferPtr "
                     << DEB_VAR5(dwArmLen, dimSize, myLimaBufferPtr,
                                 lima_buffer_nb,
                                 m_pcoData->iAllocatedBufferNumberLima);
        return -1;
    }

    if (wDoubleImage && (myLimaBufferPtr1 == NULL))
    {
        DEB_ALWAYS() << "ERROR myLimaBufferPtr1 "
                     << DEB_VAR5(dwArmLen, dimSize, myLimaBufferPtr,
                                 lima_buffer_nb,
                                 m_pcoData->iAllocatedBufferNumberLima);
        return -1;
    }

    m_allocBuff.limaAllocBufferPtr[bufIdx] = (WORD *)myLimaBufferPtr;
    m_allocBuff.limaAllocBufferPtr1[bufIdx] = (WORD *)myLimaBufferPtr1;
    m_allocBuff.dwLimaAllocBufferSize[bufIdx] = myLimaBufferLen;

    m_allocBuff.limaAllocBufferNr[bufIdx] = myLimaBufferNr;
    m_allocBuff.limaAllocBufferNr1[bufIdx] = myLimaBufferNr1;

    myPcoBufferLen = m_allocBuff.dwPcoAllocBufferSize[bufIdx];
    void *myPcoBufferPtr = m_allocBuff.pcoAllocBufferPtr[bufIdx];

#if 0
    if (myPcoBufferLen != myPcoBufferLenReal)
    {
        DEB_ALWAYS() << "ERROR myPcoBufferLen != myPcoBufferLenReal "
            << DEB_VAR2(myPcoBufferLen, myPcoBufferLenReal);
        return -1;
    }

#endif

    if (dbgWaitobj)
    {
        DEB_ALWAYS() << "... assign ... "
                     << DEB_VAR6(bufIdx, myLimaBufferPtr, myLimaBufferPtr1,
                                 myPcoBufferPtr, myLimaBufferLen,
                                 myPcoBufferLen);
    }

    //------------------------------------------------------------------------------------------
    //---------- add a new PCO buffer (in case of Double images, also is
    // one!
    //------------------------------------------------------------------------------------------

    // ---------- NEW function with our assigned buffers
    // SC2_SDK_FUNC int WINAPI PCO_AddBufferExtern(HANDLE ph, HANDLE hEvent,
    // DWORD dw1stImage,
    //        DWORD dwLastImage, DWORD dwSynch, void* pBuf, DWORD dwArmLen,
    //        DWORD* dwStatus)

    // DWORD dwSynch = 0;  // must be 0
    // DWORD dwStatus = 0;
    // HANDLE hEvent = m_allocBuff.bufferAllocEvent[bufIdx];   // assigned
    // in the constructor of  BufferCtrlObj

#ifndef USING_PCO_ALLOCATED_BUFFERS
    WORD wActSeg;
    m_cam->_pco_GetActiveRamSegment(wActSeg, error);
    m_cam->_pco_GetActiveRamSegment(wActSeg, error);
    //_PCO_TRACE("PCO_GetActiveRamSegment", sErr) ;

    // the data transfer is made directly to the buffer allocated by LIMA

    m_cam->_pco_AddBufferExtern(hEvent, wActSeg, dwFrameFirst, dwFrameLast,
                                dwSynch, myLimaBufferPtr, dwArmLen, &dwStatus,
                                error);
    if (error)
    {
        DEB_TRACE() << sErr;
        DEB_TRACE() << DEB_VAR3(wActSeg, dwFrameFirst, dwFrameLast);
        DEB_TRACE() << DEB_VAR3(dwSynch, myLimaBufferPtr, dwArmLen);
        THROW_HW_ERROR(NotSupported) << sErr;
    }
#else
    // the data transfer is made to the buffer allocated by PCO, after that we
    // must to copy this buffer to the LIMA allocated one

    int iPcoBufIdx = m_allocBuff.pcoAllocBufferNr[bufIdx];

    DWORD dwFrame = ((m_cam->_isCameraType(Edge)) || (live_mode)
                     //	|| (m_cam->_isCameraType(Pco2k | Pco4k))
                     )
                        ? 0
                        : dwFrameFirst;

    // testForceFrameFirst0 = 1 - force the first/last frame to 0 (last frame
    // acquired)
    //                      = 0 - not modify received dwFrame -> 1, 2, 3, ....
    if (m_pcoData->testForceFrameFirst0 &&
        (m_cam->_isCameraType(Edge) || m_cam->_isCameraType(Pco2k)))
    {
        dwFrame = 0;
    }

    if (m_cam->_getDebug(DBG_ASSIGN_BUFF))
    {
        DEB_ALWAYS() << "PCO_AddBufferEx -> "
                     << DEB_VAR6(dwFrame, iPcoBufIdx, bufIdx, wArmWidth,
                                 wArmHeight, wBitPerPixel);
    }

    m_cam->_pco_AddBufferEx(dwFrame, dwFrame, iPcoBufIdx, wArmWidth, wArmHeight,
                            wBitPerPixel, error);

    if (m_cam->_getDebug(DBG_ASSIGN_BUFF))
    {
        DEB_ALWAYS() << "PCO_AddBufferEx -> "
                     << DEB_VAR6(error, dwFrame, iPcoBufIdx, bufIdx, wArmWidth,
                                 wArmHeight);
    }

    if (error)
    {
        DEB_ALWAYS() << "\n*** ERROR - SDK ***"
                     << "\n    " << sErr << "\n    "
                     << DEB_VAR6(dwFrame, dwFrameFirst, dwFrameLast, wArmWidth,
                                 wArmHeight, wBitPerPixel);
        return -1;
    }

#endif

    m_allocBuff.bufferAssignedFrameFirst[bufIdx] = dwFrameFirst;
    m_allocBuff.bufferAssignedFrameLast[bufIdx] = dwFrameLast;
    m_allocBuff.bufferReady[bufIdx] = 0;

    //----- prepartion of dwFrameFirst2assign & dwFrameLast2assign for the NEXT
    // call to addBuffer
    dwFrameFirst = dwFrameLast + 1;
    dwFrameLast = dwFrameFirst + m_cam->pcoGetFramesPerBuffer() - 1;
    if (dwFrameLast > dwRequestedFrames)
        dwFrameLast = dwRequestedFrames;

    if (m_cam->_getDebug(DBG_ASSIGN_BUFF))
    {
        DEB_ALWAYS() << "exit -> "
                     << DEB_VAR4(dwFrameFirst, dwFrameLast, dwRequestedFrames,
                                 bufIdx);
    }

    return 0;
}

//===================================================================================================================
//===================================================================================================================
int BufferCtrlObj::_xferImag_buff2lima(DWORD &dwFrameIdx, int &bufIdx)

{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    int lima_buffer_nb, iLimaFrame;
    SHORT sBufNr = m_allocBuff.pcoAllocBufferNr[bufIdx];
    int _nrStop = 0;
    unsigned long long dbgWaitobj = m_cam->_getDebug(DBG_WAITOBJ);

    // lima frame nr is from 0 .... N-1, PCO nr is from 1 ... N
    lima_buffer_nb =
        dwFrameIdx - 1; // this frame was already readout to the buffer

    if (dbgWaitobj)
    {
        char msg[256];
        __sprintfSExt(msg, sizeof(msg),
                      "... PROCESS PCObuff[%d] TO LIMAbuff[%d] frame[%d]",
                      bufIdx, lima_buffer_nb, dwFrameIdx);
        DEB_ALWAYS() << msg;
    }
    m_pcoData->traceAcq.nrImgAcquired = dwFrameIdx;

#ifndef USING_PCO_ALLOCATED_BUFFERS
#    error ERROR --- USING_PCO_ALLOCATED_BUFFERS is not defined
#endif

    // we are using the PCO allocated buffer, so this buffer must be copied to
    // the lima buffer
    void *ptrDest = (void *)m_allocBuff.limaAllocBufferPtr[bufIdx];
    void *ptrSrc = (void *)m_allocBuff.pcoAllocBufferPtr[bufIdx];
    size_t sizeLima = m_allocBuff.dwLimaAllocBufferSize[bufIdx];
    size_t size = m_allocBuff.dwPcoAllocBufferSize[bufIdx];

    size = sizeLima;
    if ((m_sync->_getRequestStop(_nrStop) == stopRequest) &&
        (_nrStop > MAX_NR_STOP))
    {
        return pcoAcqStop;
    }

    if (m_cam->_getDebug(DBG_DUMMY_IMG))
    {
        // creating a dummy image
        int val = dwFrameIdx & 0xf;
        memset(ptrDest, val, size);
        DEB_ALWAYS() << "===== dummy image!!! " << DEB_VAR1(val);
    }
    else
    {
        // copy the real imaga ptrSrc -> ptrDest
        memcpy(ptrDest, ptrSrc, size);
    }

    //----- the image dwFrameIdx is already in the buffer -> callback
    // newFrameReady
    //----- lima frame (0 ... N-1) PCO frame (1 ... N)
    HwFrameInfoType frame_info;
    iLimaFrame = frame_info.acq_frame_nb = lima_buffer_nb;
    m_buffer_cb_mgr.newFrameReady(frame_info);

    //=============================== check PCO ImgNr with the limaFrame
    m_cam->m_checkImgNr->update(iLimaFrame, ptrSrc);

    //----- the image dwFrameIdx is already in the buffer -> callback!
    if ((m_sync->_getRequestStop(_nrStop) == stopRequest) &&
        (_nrStop > MAX_NR_STOP))
    {
        return pcoAcqStop;
    }

    return pcoAcqOK;
}

//===================================================================================================================
//===================================================================================================================
int BufferCtrlObj::_xferImag()
{
#ifndef __linux__
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    DWORD dwFrameIdx;
    DWORD dwFrameFirst2assign, dwFrameLast2assign;
    int bufIdx;
    int error;
    bool live_mode;
    int _nrStop;
    char msg[RING_LOG_BUFFER_SIZE + 1];
    const char *pmsg = msg;
    m_cam->m_tmpLog->flush(-1);
    int maxWaitTimeout;
    m_cam->getAcqTimeoutRetry(maxWaitTimeout);



    WORD wArmWidth, wArmHeight;
    WORD _wMaxWidth, _wMaxHeight;
    unsigned int bytesPerPixel;
    WORD wBitPerPixel;

    m_cam->_pco_GetSizes(&wArmWidth, &wArmHeight, &_wMaxWidth, &_wMaxHeight,
                         error);
    m_cam->getBytesPerPixel(bytesPerPixel);
    m_cam->getBitsPerPixel(wBitPerPixel);
    // DWORD dwLen = wArmWidth * wArmHeight * bytesPerPixel;


    unsigned long long dbgWaitobj = m_cam->_getDebug(DBG_WAITOBJ);
    unsigned long long dbgTraceFifo = m_cam->_getDebug(DBG_TRACE_FIFO);

    m_cam->m_checkImgNr->init(&m_pcoData->traceAcq);

    int forcedFifo = 0;
    m_cam->getRecorderForcedFifo(forcedFifo);

    // --------------- get the requested nr of images
    int requested_nb_frames;
    DWORD dwFramesPerBuffer, dwRequestedFrames;
    DWORD dwRequestedFramesMax = DWORD_MAX;
    int iRequestedFramesMax = INT_DWORD_MAX;

    // --------------- live video -> nr frames = 0 / idx lima buffers 32b
    // (0...ffff)
    m_sync->getNbFrames(requested_nb_frames);

    if ((requested_nb_frames > 0) && (forcedFifo == 0))
    {
        dwRequestedFrames = (DWORD)requested_nb_frames;
        live_mode = false;
    }
    else
    {
        if ((requested_nb_frames == 0) ||
            (requested_nb_frames > iRequestedFramesMax))
            dwRequestedFrames = dwRequestedFramesMax;
        else
            dwRequestedFrames = (DWORD)requested_nb_frames;

        live_mode = true;
    }

    dwRequestedFrames = (requested_nb_frames > 0) ? (DWORD)requested_nb_frames
                                                  : dwRequestedFramesMax;
    dwFramesPerBuffer = m_cam->pcoGetFramesPerBuffer(); // for dimax = 1

    // -----------------------------------------------------------------------------------------
    // --------------- ENTRY MSG
    // -----------------------------------------------------------------------------------------
    DEB_ALWAYS() << "\n... "
                 << DEB_VAR6(requested_nb_frames, dwRequestedFrames, live_mode,
                             forcedFifo, dbgTraceFifo, dbgWaitobj)
                 << m_cam->_sprintComment(
                        false, fnId, "[WaitForMultipleObjects]", "[ENTRY]");

    //----------------- traceAcq init

    m_pcoData->traceAcq.fnIdXfer = fnId;
    m_pcoData->traceAcq.msImgCoc = (m_cam->pcoGetCocRunTime() * 1000.);

    TIME_USEC tStart;
    msElapsedTimeSet(tStart);
    m_pcoData->traceAcq.nrImgRequested = dwRequestedFrames;

#    if 0
    if (!m_cam->_isRunAfterAssign())
    {
        DEB_TRACE() << "========================= recordingState 1 - BEFORE ASSIGN";
        m_cam->_pco_SetRecordingState(1, error);
    }
#    endif

    // cleaning the buffers
    for (int _id = 0; _id < m_cam->m_pco_buffer_nrevents; _id++)
    {
        m_allocBuff.bufferReady[_id] = 0;
        m_allocBuff.bufferAssignedFrameFirst[_id] = -1;
        m_allocBuff.limaAllocBufferPtr[_id] = NULL;
        m_allocBuff.limaAllocBufferPtr1[_id] = NULL;
        m_allocBuff.dwLimaAllocBufferSize[_id] = 0;
    }

    // --------------- prepare the first buffer
    // ------- in PCO DIMAX only 1 image can be retreived
    //         (dwFramesPerBuffer = 1) ====> (dwFrameLast2assign =
    //         dwFrameFirst2assign)
    dwFrameFirst2assign = 1;
    dwFrameLast2assign = dwFrameFirst2assign + dwFramesPerBuffer - 1;
    if (dwFrameLast2assign > dwRequestedFrames)
        dwFrameLast2assign = dwRequestedFrames;

    for (int i = 0; i < m_cam->m_pco_buffer_nrevents; i++)
    {
        // --------------- if needed prepare the next buffer
        if (dwFrameFirst2assign > dwRequestedFrames)
            break;
        bufIdx = i;

        bool recording = m_cam->_getCameraState(CAMSTATE_RECORD_STATE);
        bool runAfterAssign = m_cam->_isRunAfterAssign();

        if ((!runAfterAssign) || (!recording && runAfterAssign))
        {
            if (dbgTraceFifo)
            {
                printf("---TRACE - ASSIGN BUFFER[%d] frame[%d] recordState[%d] "
                       "live[%d]\n",
                       bufIdx, dwFrameFirst2assign, recording, live_mode);
            }
            if (dbgWaitobj)
            {
                char msg[512];
                __sprintfSExt(
                    msg, sizeof(msg),
                    "... ASSIGN BUFFER[%d] frame[%d] recordState[%d] live[%d]",
                    bufIdx, dwFrameFirst2assign, recording, live_mode);
                m_cam->m_tmpLog->add(msg);
                DEB_ALWAYS() << msg;
            }
            if ((error = _assignImage2Buffer(
                     dwFrameFirst2assign, dwFrameLast2assign, dwRequestedFrames,
                     bufIdx, live_mode)))
            {
                DEB_ALWAYS() << "... ERROR _assignImage2Buffer / return";
                return pcoAcqPcoError;
            }

            if (dbgWaitobj)
            {
                SHORT sBufNr = m_allocBuff.pcoAllocBufferNr[bufIdx];
                DWORD dwStatusDll, dwStatusDrv;
                int errPco;

                m_cam->_pco_GetBufferStatus(sBufNr, &dwStatusDll, &dwStatusDrv,
                                            errPco);
                char msg[128];
                __sprintfSExt(
                    msg, sizeof(msg),
                    "buffNr[%d] dwStatusDll[%08lx] dwStatusDrv[%08lx] err[%x]",
                    sBufNr, dwStatusDll, dwStatusDrv, errPco);
                DEB_ALWAYS() << "... PCO_GetBufferStatus: " << msg;
            }
        }
        else
        {
            DEB_ALWAYS() << "ERROR _assignImage2Buffer with wrong recordState "
                            "/ IGNORED!!!!"
                         << DEB_VAR2(recording, runAfterAssign);
        } // if((!runAfterAssign)
    }     // for(int i = 0; i <m_cam->m_pco_buffer_nrevents;


    // Edge cam must be started just after assign buff to avoid lost of img
    if (m_cam->_isRunAfterAssign())
    {
        DWORD sleepMs = 1;
        ::Sleep(sleepMs);
        DEB_TRACE() << "========================= recordingState 1 - AFTER "
                       "ASSIGN (_xferImag)";
        if (dbgWaitobj)
        {
            pmsg = "... EDGE - recordingState 1";
            m_cam->m_tmpLog->add(pmsg);
            DEB_TRACE() << pmsg;
        }
        m_cam->_pco_SetRecordingState(1, error);
    }

    // -----------------------------------------------------------------------------------------
    // loop - process the N frames
    // (dwFrameIdx <= dwRequestedFrames)
    // -----------------------------------------------------------------------------------------

    bool bReadRAM = false;
    bool bCamRAM = m_cam->_isCameraType(camRAM);
    for (dwFrameIdx = 1; dwFrameIdx <= dwRequestedFrames;)
    {
        DEB_TRACE() << "[LOOP] --- " <<DEB_VAR4(fnId, dwFrameIdx, dwRequestedFrames, forcedFifo);
        if (dbgWaitobj)
        {
            char msg[256];
            __sprintfSExt(
                msg, sizeof(msg),
                "... LOOP inside while dwFrameIdx[%d] <= dwRequestedFrames[%d]",
                dwFrameIdx, dwRequestedFrames);
            DEB_ALWAYS() << msg;
        }

        if ((m_sync->_getRequestStop(_nrStop) == stopRequest) &&
            (_nrStop > MAX_NR_STOP))
        {
            goto _EXIT_STOP;
        }

        // --------------- look if one of buffer is READY and has the NEXT frame
        // => proccess it m_allocatedBufferAssignedFrameFirst[bufIdx] -> first
        // frame in the buffer (we are using only 1 frame per buffer)
        // m_allocatedBufferReady[bufIdx] -> is already filled by sdk (ready)

        {
            int iPending;
            PCO_GetPendingBuffer(m_cam->m_handle, &iPending);

            if (dbgWaitobj)
            {
                DEB_ALWAYS()
                    << "... PCO_GetPendingBuffer: " << DEB_VAR1(iPending);
            }
        }

        // -----------------------------------------------------------------------------------------
        // ---------------------------------- find buffIdx associated with the
        // next image dwFrameIdx PCO frame 1 ...
        // -----------------------------------------------------------------------------------------
        SHORT sBufNr;
        DWORD dwBuffFrame;
        bool found = false;
        for (bufIdx = 0; bufIdx < m_cam->m_pco_buffer_nrevents; bufIdx++)
        {
            sBufNr = m_allocBuff.pcoAllocBufferNr[bufIdx];
            dwBuffFrame = m_allocBuff.bufferAssignedFrameFirst[bufIdx];
            if (dwBuffFrame == dwFrameIdx)
            {
                found = true;
                if (dbgTraceFifo)
                {
                    printf("---TRACE WAITING FOR sBufNr[%d] dwFrameIdx[%d]\n",
                           sBufNr, dwFrameIdx);
                }
                break;
            }
        }

        if (!found)
        {
            char msg[256];
            __sprintfSExt(
                msg, sizeof(msg),
                "... ERROR - dwFrameIdx[%d] not found in the assigned buffers",
                dwFrameIdx);
            DEB_ALWAYS() << msg;
        }

        // -----------------------------------------------------------------------------------------
        // ---------------------------------- loop of WaitForMultipleObjects
        // waiting the buff sBufNr is fired
        // -----------------------------------------------------------------------------------------
        int msTimeout = 100;
        int nrEvents = m_cam->m_pco_buffer_nrevents;
        int eventMin = WAIT_OBJECT_0;
        int eventMax = eventMin + nrEvents;
        DWORD dwStatusDll, dwStatusDrv;
        // int iPending = 0;
        int iLoopsPolled = 0;
        int errPco;
        do
        {
            if ((m_sync->_getRequestStop(_nrStop) == stopRequest) &&
                (_nrStop > MAX_NR_STOP))
            {
                goto _EXIT_STOP;
            }

            // Sleep(msTimeout);
            int eventRet; // = WaitForMultipleObjects(2, &events[0], false, 10);

            eventRet = WaitForMultipleObjects(
                nrEvents,                     // number of objects in array
                m_allocBuff.bufferAllocEvent, // array of objects
                false,                        // wait for any object
                msTimeout);                   // ms wait timeout

            // GetPendingBuffer(hCamera, &ipending);// <--- This can indicate
            // that more than one buffer is ready
            // If this returns 3 in ipending, everyting is fine and you keep the
            // pace.

            if ((eventRet >= eventMin) && (eventRet <= eventMax))
            {
                if (dbgTraceFifo)
                {
                    printf("---TRACE WAITOBJ [%d] found\n", eventRet);
                }
                if (dbgWaitobj)
                {
                    char msg[256];
                    __sprintfSExt(msg, sizeof(msg), "... WAITOBJ [%d] found",
                                  eventRet);
                    DEB_ALWAYS() << msg;
                }
            }
            m_cam->_pco_GetBufferStatus(sBufNr, &dwStatusDll, &dwStatusDrv,
                                        errPco);
            if ((errPco != 0) || (dwStatusDrv != 0))
            {
                char msg[256];
                __sprintfSExt(
                    msg, sizeof(msg),
                    "err[0x%lx] dwStatusDll[0x%lx] dwStatusDrv[0x%lx] \n"
                    "sBufNr[%d] WAITOBJ[%d] dwFrameIdx[%d] iLoopsPolled[%d] "
                    "msTimeout[%d]\n",
                    errPco, dwStatusDll, dwStatusDrv, sBufNr, eventRet,
                    dwFrameIdx, iLoopsPolled, msTimeout);

                DEB_WARNING()
                    << "\n"
                    << msg
                    << m_cam->_PcoCheckError(__LINE__, __FILE__, dwStatusDrv,
                                             error)
                    << m_cam->_sprintComment(
                           false, fnId, "[PCO_GetBufferStatus]", "[ERROR]");
                // goto abnormal;
            }

            iLoopsPolled++;
        } while (!(dwStatusDll & 0x00008000));
        //      0x00008000 = Buffer event is set

        // dwStatusDll DWORD *Pointer to a DWORD variable to get the status
        // inside the SDK DLL :
        //      0x80000000 = Buffer is allocated
        //      0x40000000 = Buffer event created inside the SDK DLL
        //      0x20000000 = Buffer is allocated externally
        //      0x00008000 = Buffer event is set

        if (dbgTraceFifo)
        {
            printf("---TRACE iLoopsPolled[%d] dwStatusDll[0x%lx] sBufNr[%d] "
                   "dwFrameIdx[%d]\n",
                   iLoopsPolled, dwStatusDll, sBufNr, dwFrameIdx);
        }

        // -----------------------------------------------------------------------------------------
        // ---------------------------------- sBufNr was fired
        // -----------------------------------------------------------------------------------------

        if (dbgWaitobj)
        {
            char msg[256];
            __sprintfSExt(msg, sizeof(msg),
                          "... EVENT FIRED buffNr[%d] dwBuffFrame[%d] "
                          "dwFrameIdx[%d] dwStatusDll[0x%08lx] "
                          "dwStatusDrv[0x%08lx] err[0x%x], iLoopsPolled[%d]",
                          sBufNr, dwBuffFrame, dwFrameIdx, dwStatusDll,
                          dwStatusDrv, errPco, iLoopsPolled);
            DEB_ALWAYS() << msg;
        }

#    if 0
        // test code to stop the pco recording when the
        // last image was acquired
        // dwFrameIdx -> pco frame -> 1 ...
        DWORD dwFrameIdx1 = dwFrameIdx + 1;
        if ((dwFrameIdx) >= dwRequestedFrames)
        {
            int error;
            // ------------- stop the recording and clear buffers

            DEB_ALWAYS() << "\n ... _pco_SetRecordingState(0) when: "
                         << DEB_VAR3(dwFrameIdx, dwFrameIdx1,
                                     dwRequestedFrames);

            m_cam->_pco_SetRecordingState(0, error);
        }
#    endif

        int xferRet = _xferImag_buff2lima(dwFrameIdx, bufIdx);
        if (dbgTraceFifo)
        {
            printf("---TRACE  _xferImag_buff2lima sBufNr[%d] dwFrameIdx[%d]\n",
                   sBufNr, dwFrameIdx);
        }

        // int ret = ResetEvent(m_allocBuff.bufferAllocEvent[bufIdx]);
        // printf("---TRACE - ResetEvent[%d] buffIdx[%d] sBufNr[%d]
        // dwFrameIdx[%d]\n", ret, bufIdx, sBufNr, dwFrameIdx);

        if (xferRet == pcoAcqStop)
            goto _EXIT_STOP;

        // -----------------------------------------------------------------------------------------
        // ---------------------------------- assing the buff already free to
        // the next image
        // -----------------------------------------------------------------------------------------

        bool alwaysAssign = !!m_cam->_getDebug(DBG_FN1);
        bool useReset = !m_cam->_getDebug(DBG_FN2);

        if (alwaysAssign || (dwFrameFirst2assign <= dwRequestedFrames))
        {
            if ((m_cam->_getCameraState(CAMSTATE_RECORD_STATE)))
            {
                if (dbgWaitobj)
                {
                    char msg[256];
                    __sprintfSExt(msg, sizeof(msg),
                                  "... ASSIGN BUFFER[%d] frame[%d] live[%d]",
                                  bufIdx, dwFrameFirst2assign, live_mode);
                    DEB_ALWAYS() << msg;
                }

                if (dbgTraceFifo)
                {
                    printf("---TRACE - ASSIGN BUFFER[%d] frame[%d] live[%d]\n",
                           bufIdx, dwFrameFirst2assign, live_mode);
                }
                if ((error = _assignImage2Buffer(
                         dwFrameFirst2assign, dwFrameLast2assign,
                         dwRequestedFrames, bufIdx, live_mode)))
                {
                    DEB_ALWAYS() << "... ERROR _assignImage2Buffer / return";
                    return pcoAcqPcoError;
                }
            }
            else
            {
                DEB_ALWAYS() << "ERROR _assignImage2Buffer with recordState = "
                                "0 / IGNORED!!!!";
            }
        }
        else if (useReset)
        {
            if (dbgWaitobj)
            {
                char msg[256];
                __sprintfSExt(
                    msg, sizeof(msg),
                    "... BYPASSED ASSIGN BUFFER[%d] dwFrameFirst2assign[%d] "
                    "dwRequestedFrames[%d] live[%d]",
                    bufIdx, dwFrameFirst2assign, dwRequestedFrames, live_mode);
                DEB_ALWAYS() << msg;
            }

            void *ev = m_allocBuff.bufferAllocEvent[bufIdx];
            int ret = ResetEvent(ev);
            if (dbgTraceFifo)
            {
                printf("--- ResetEvent[%d] buffIdx[%d] sBufNr[%d] event[%p]\n",
                       ret, bufIdx, sBufNr, ev);
            }

            if (!ret)
            {
                DWORD errorMessageID = ::GetLastError();

                LPSTR messageBuffer = NULL;
                size_t size =
                    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                       FORMAT_MESSAGE_FROM_SYSTEM |
                                       FORMAT_MESSAGE_IGNORE_INSERTS,
                                   NULL, errorMessageID,
                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   (LPSTR)&messageBuffer, 0, NULL);

                std::string message(messageBuffer, size);

                // Free the buffer.
                LocalFree(messageBuffer);

                DEB_ALWAYS() << message;
            }
        }

        m_sync->setAcqFrames(dwFrameIdx);
        dwFrameIdx++;
        if (live_mode && bCamRAM && m_cam->_getRecorderStopRequest() &&
            (dwFrameIdx <= dwRequestedFrames))
        {
            bReadRAM = true;
            break;
        }
    } // for(dwFrameIdx = 1; dwFrameIdx <= dwRequestedFrames; )

    // -----------------------------------------------------------------------------------------
    // if the "live recording" was interrupted, begin to transfer all the images
    // in the memory
    // -----------------------------------------------------------------------------------------
    if (bReadRAM)
    {
        int error;
        DWORD _dwPcoValidImageCnt, _dwPcoMaxImageCnt;
        WORD wSegment;
        DWORD dwImgToRead, dwFirstImg, dwImgIdx;


        // ------------- stop the recording and clear buffers
        m_cam->_pco_SetRecordingState(0, error);

        bufIdx = 0;
        SHORT sBufNr = m_allocBuff.pcoAllocBufferNr[bufIdx];

        m_cam->_pco_GetActiveRamSegment(wSegment, error);
        m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwPcoValidImageCnt,
                                               _dwPcoMaxImageCnt, error);
        if (error)
        {
        }

        m_pcoData->dwValidImageCnt[wSegment - 1] =
            m_pcoData->traceAcq.nrImgRecorded = _dwPcoValidImageCnt;
        m_pcoData->dwMaxImageCnt[wSegment - 1] =
            m_pcoData->traceAcq.maxImgCount = _dwPcoMaxImageCnt;

        // ------------- save the actual image sent to lima / last "live image"
        // ------------- read the img recorded (all or partial) from the oldest
        // to the last one
        m_pcoData->traceAcq.lastImgFifo = dwFrameIdx;

        dwImgToRead = dwRequestedFrames - dwFrameIdx;
        dwFirstImg = (_dwPcoValidImageCnt < dwImgToRead)
                         ? 1
                         : _dwPcoValidImageCnt - dwImgToRead;

        for (dwImgIdx = dwFirstImg; dwImgIdx <= _dwPcoValidImageCnt; dwImgIdx++)
        {
            // ------------- _pco_GetImageEx only allows read 1 img, so
            // dwFirstImg = dwLastImg
            m_cam->_pco_GetImageEx(wSegment, dwImgIdx, dwImgIdx, sBufNr,
                                   m_pcoData->m_wArmWidth, m_pcoData->m_wArmHeight, 
                                   wBitPerPixel,
                                   error);

            int xferRet = _xferImag_buff2lima(dwFrameIdx, bufIdx);
            // if(xferRet == pcoAcqStop) goto _EXIT_STOP;

        } // for(dwImgIdx = dwFirstImg;
    }

    // -----------------------------------------------------------------------------------------
    //---------------------------------- end of the while / all img were
    // processed
    // -----------------------------------------------------------------------------------------

    m_pcoData->traceAcq.msXfer = msElapsedTime(tStart);
    m_pcoData->traceAcq.endXferTimestamp = getTimestamp();

    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[EXIT]");

    if (bReadRAM)
    {
        return pcoAcqRecordStop;
    }

    return pcoAcqTransferEnd;

_EXIT_STOP:
    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[STOP REQUESTED]",
                                          "[EXIT]");
    DEB_TRACE() << DEB_VAR3(_nrStop, dwFrameIdx, dwRequestedFrames);

    m_pcoData->traceAcq.msXfer = msElapsedTime(tStart);
    m_pcoData->traceAcq.endXferTimestamp = getTimestamp();

    return pcoAcqTransferStop;

#else
    return -1;
#endif
}

//===================================================================================================================
//===================================================================================================================
void *BufferCtrlObj::_getLimaBuffer(int lima_buffer_nb, Sync::Status &status)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    void *myBuffer;
    double timeout = 30;
    const char *msg;

    StdBufferCbMgr &buffer_mgr = m_buffer_cb_mgr;

    buffer_mgr.setStartTimestamp(Timestamp::now());

#ifdef COMPILE_WAIT_CONDITION
    AutoMutex lock(cond.mutex());

    Timestamp t0 = Timestamp::now();

    while (true)
    {
        double wait_timeout = timeout - double(Timestamp::now() - t0);
        if (wait_timeout <= 0)
        {
            msg = "=== Sync wait INTERRUPTED + TIMEOUT === ";
            DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
            return NULL;
        }

        status = m_bufferSync->wait(lima_buffer_nb, wait_timeout);
        if (m_cam->_getDebug(DBG_LIMABUFF))
        {
            DEB_ALWAYS() << DEB_VAR3(lima_buffer_nb, timeout, status);
        }

        switch (status)
        {
            case Sync::AVAILABLE:
                myBuffer = buffer_mgr.getFrameBufferPtr(lima_buffer_nb);
                buffer_mgr.getNbBuffers(m_pcoData->iAllocatedBufferNumberLima);
                return myBuffer;

            case Sync::TIMEOUT:
                msg = "=== Sync wait TIMEOUT === ";
                DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                return NULL;

            case Sync::INTERRUPTED:
                msg = "=== Sync wait INTERRUPTED === ";
                DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                break;

            default:
                msg = "=== Sync wait UNKNOWN STATUS === ";
                DEB_ALWAYS() << msg << DEB_VAR2(lima_buffer_nb, timeout);
                return NULL;
        } // switch
    }     // while true

#else
#    pragma message(                                                           \
        "============================================== BYPASSED ----- COMPILE_WAIT_CONDITION -----")

    myBuffer = buffer_mgr.getFrameBufferPtr(lima_buffer_nb);
    status = 0;
    return myBuffer;
#endif
}

//===================================================================================================================
//===================================================================================================================

/********************************************************************************

-------------- in dimax is NOT possible read MULTIPLES images (PCO_GetImageEx)

On 2013/09/17 11:44, PCO Support Team wrote:

> So there are a few camera, which support reading multiple images (i.e.
> the 1200hs), but the pco.dimax does not. Also not all interfaces
> support the mode, Martin from the software and driver team has
> realized it for a few CameraLink boards. Our experience is, that it is
> pretty difficult and time consuming to provide the feature for all
> cameras and all interfaces in a reliable way.
>
> Concisely, it's not possible for the pco.dimax, even if the command
> "PCO_GetImageEx" mentions the multiple reading mode.
********************************************************************************/

int BufferCtrlObj::_xferImag_getImage()
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    // int mode =  m_cam->_pco_GetStorageMode_GetRecorderSubmode() ;
    // bool continuous = (mode != RecSeq) ;

    m_pcoData->traceAcq.fnIdXfer = fnId;

    DWORD dwFrameIdx, dwFrameIdxCount;
    // long long nr =0;
    // long long bytesWritten = 0;
    int bufIdx;
    int error;
    bool live_mode;
    DWORD dwFrameIdxFirst, dwFrameIdxLast;
    int maxWaitTimeout;
    m_cam->getAcqTimeoutRetry(maxWaitTimeout);
    unsigned long long dbgWaitobj = m_cam->_getDebug(DBG_WAITOBJ);

    WORD _wBitPerPixel;
    char *sErr;
    void *ptrLimaBuffer;
    Sync::Status status;
    int requested_nb_frames;
    DWORD dwFramesPerBuffer, dwRequestedFrames;
    DWORD dwRequestedFramesMax = DWORD_MAX;
    unsigned int _uiBytesPerPixel;
    WORD _wArmWidth, _wArmHeight;
    int _iPcoAllocatedBuffNr;
    DWORD _dwPcoAllocatedBuffSize;
    SHORT sBufNr;
    WORD wSegment;
    int iLimaFrame;
    DWORD dwFrameSize;
    int _retStatus, _stopReq, _nrStop;
    int _newFrameReady = -1;

    TIME_USEC tStart;
    msElapsedTimeSet(tStart);

    long long usStart;
    usElapsedTimeSet(usStart);

    m_cam->m_checkImgNr->init(&m_pcoData->traceAcq);

    _pcoAllocBuffers(true); // allocate 2 pco buff at max size

    m_cam->_pco_GetActiveRamSegment(wSegment, error);

    //------------------- nr of frames per buffer
    m_cam->getBitsPerPixel(_wBitPerPixel);
    m_cam->getBytesPerPixel(_uiBytesPerPixel);

    WORD _wMaxWidth, _wMaxHeight;
    m_cam->_pco_GetSizes(&_wArmWidth, &_wArmHeight, &_wMaxWidth, &_wMaxHeight,
                         error);
    _pcoAllocBuffersInfo(_iPcoAllocatedBuffNr, _dwPcoAllocatedBuffSize);
    dwFrameSize =
        (DWORD)_wArmWidth * (DWORD)_wArmHeight * (DWORD)_uiBytesPerPixel;
    // dwFramesPerBuffer = _dwPcoAllocatedBuffSize / dwFrameSize ;
    dwFramesPerBuffer = 1;

    // --------------- live video -> nr frames = 0 / idx lima buffers 32b
    // (0...ffff)
    m_sync->getNbFrames(requested_nb_frames);

    if (requested_nb_frames > 0)
    {
        dwRequestedFrames = (DWORD)requested_nb_frames;
        live_mode = false;
    }
    else
    {
        dwRequestedFrames = dwRequestedFramesMax;
        live_mode = true;
    }

    dwRequestedFrames = (requested_nb_frames > 0) ? (DWORD)requested_nb_frames
                                                  : dwRequestedFramesMax;

    // lima frame nr is from 0 .... N-1
    // pco frame nr is from 1 .... N

    // --------------- loop - process the N frames
    dwFrameIdxCount = dwFrameIdx = 1;
    bufIdx = 0;

    DWORD _dwValidImageCnt, _dwMaxImageCnt;

    m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwValidImageCnt,
                                           _dwMaxImageCnt, error);
    if (error)
    {
        printf("=== %s [%d]> ERROR \n", fnId, __LINE__);
        throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
    }

    dwFrameIdx = (_dwValidImageCnt >= dwRequestedFrames)
                     ? _dwValidImageCnt - dwRequestedFrames + 1
                     : 1;

    DEB_TRACE() << "\n_xferImagMult_getImage() [entry]:"
                << "\n...   "
                << DEB_VAR2(_iPcoAllocatedBuffNr, _dwPcoAllocatedBuffSize)
                << "\n...   "
                << DEB_VAR4(_wArmWidth, _wArmHeight, _uiBytesPerPixel,
                            _wBitPerPixel)
                << "\n...   " << DEB_VAR2(dwFramesPerBuffer, dwFrameSize)
                << "\n...   "
                << DEB_VAR3(requested_nb_frames, dwRequestedFrames, live_mode)
                << "\n...   "
                << DEB_VAR3(dwFrameIdx, _dwValidImageCnt, _dwMaxImageCnt);

    m_pcoData->traceAcq.nrImgRequested = dwRequestedFrames;

    m_pcoData->traceAcq.usTicks[6].desc = "PCO_GetImageEx total execTime";
    m_pcoData->traceAcq.usTicks[7].desc = "xfer to lima / total execTime";

    for (iLimaFrame = 0; iLimaFrame < requested_nb_frames; iLimaFrame++)
    {
        bufIdx++;
        if (bufIdx >= _iPcoAllocatedBuffNr)
            bufIdx = 0;
        sBufNr = m_allocBuff.pcoAllocBufferNr[bufIdx];

        if (dwFrameIdx > _dwValidImageCnt)
        {
            DEB_ALWAYS() << "WARNING lost image: "
                         << DEB_VAR2(dwFrameIdx, _dwValidImageCnt);
            dwFrameIdx = _dwValidImageCnt;
        }
        dwFrameIdxLast = dwFrameIdxFirst = dwFrameIdx;

        if (m_cam->_getDebug(DBG_XFERMULT1))
        {
            DEB_ALWAYS() << DEB_VAR5(dwFrameIdx, dwFrameIdxFirst,
                                     dwFrameIdxLast, dwFramesPerBuffer,
                                     dwRequestedFrames);
        }

        usElapsedTimeSet(usStart);

        m_cam->_pco_GetImageEx(wSegment, dwFrameIdxFirst, dwFrameIdxLast,
                               sBufNr, _wArmWidth, _wArmHeight, _wBitPerPixel,
                               error);

        m_pcoData->traceAcq.usTicks[6].value += usElapsedTime(usStart);
        usElapsedTimeSet(usStart);

        if (error)
        {
            DEB_ALWAYS() << "PCO_GetImageEx() ===> "
                         << DEB_VAR4(sErr, wSegment, dwFrameIdxFirst,
                                     dwFrameIdxLast)
                         << DEB_VAR4(sBufNr, _wArmWidth, _wArmHeight,
                                     _wBitPerPixel);
        }

        void *ptrSrc = m_allocBuff.pcoAllocBufferPtr[bufIdx];

        ptrLimaBuffer = _getLimaBuffer(iLimaFrame, status);

        if (ptrLimaBuffer == NULL)
        {
            DEB_ALWAYS() << DEB_VAR3(ptrLimaBuffer, iLimaFrame, status);
            THROW_HW_ERROR(NotSupported) << "Lima ptr = NULL";
        }

        if (m_cam->_getDebug(DBG_XFERMULT1))
        {
            DEB_ALWAYS() << DEB_VAR3(ptrLimaBuffer, ptrSrc, dwFrameSize);
        }

        memcpy(ptrLimaBuffer, ptrSrc, dwFrameSize);

        //=============================== check PCO ImgNr with the limaFrame
        m_cam->m_checkImgNr->update(iLimaFrame, ptrSrc);

        ptrSrc = ((char *)ptrSrc) + dwFrameSize;

        HwFrameInfoType frame_info;
        frame_info.acq_frame_nb = _newFrameReady = iLimaFrame;
        m_buffer_cb_mgr.newFrameReady(frame_info);

        m_pcoData->traceAcq.nrImgAcquired = dwFrameIdxCount;

        if ((_stopReq = m_sync->_getRequestStop(_nrStop)) == stopRequest)
        {
            if (_nrStop > MAX_NR_STOP)
            {
                char msg[LEN_TRACEACQ_MSG + 1];
                snprintf(
                    msg, LEN_TRACEACQ_MSG,
                    "%s> STOP REQ (saving), framesReq[%d] frameReady[%d]\n",
                    fnId, requested_nb_frames, _newFrameReady);
                m_pcoData->traceMsg(msg);
                break;
            }
        }

        m_sync->setAcqFrames(dwFrameIdxCount);
        dwFrameIdx++;
        dwFrameIdxCount++;

        m_pcoData->traceAcq.usTicks[7].value += usElapsedTime(usStart);

    } // while(frameIdx ...

    // if(m_cam->_isCameraType(Edge)) {m_sync->setAcqFrames(dwFrameIdx-1);}

    switch (_stopReq)
    {
            // case stopProcessing:
        case stopRequest:
            _retStatus = pcoAcqTransferStop;
            break;
        default:
            _retStatus = pcoAcqTransferEnd;
    }

    DEB_TRACE() << "[exit]" << DEB_VAR3(_retStatus, _stopReq, _newFrameReady);

    return _retStatus;
}

//===================================================================================================================

int BufferCtrlObj::_xferImag_getImage_edge()
{
#ifndef __linux__
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    // int mode =  m_cam->_pco_GetStorageMode_GetRecorderSubmode() ;
    // bool continuous = (mode != RecSeq) ;

    m_pcoData->traceAcq.fnIdXfer = fnId;
    m_pcoData->traceAcq.msImgCoc = (m_cam->pcoGetCocRunTime() * 1000.);

    DWORD dwFrameIdx, dwFrameIdxCount;
    // long long nr =0;
    // long long bytesWritten = 0;
    int bufIdx;
    int error;
    bool live_mode;
    DWORD dwFrameIdxFirst, dwFrameIdxLast;
    int maxWaitTimeout;
    m_cam->getAcqTimeoutRetry(maxWaitTimeout);
    WORD _wBitPerPixel;
    char *sErr;
    void *ptrLimaBuffer;
    Sync::Status status;
    int requested_nb_frames;
    DWORD dwFramesPerBuffer, dwRequestedFrames;
    DWORD dwRequestedFramesMax = DWORD_MAX;
    unsigned int _uiBytesPerPixel;
    WORD _wArmWidth, _wArmHeight;
    int _iPcoAllocatedBuffNr;
    DWORD _dwPcoAllocatedBuffSize;
    SHORT sBufNr;
    WORD wSegment;
    int iLimaFrame;
    DWORD dwFrameSize;
    int _retStatus, _stopReq, _nrStop;
    int _newFrameReady = -1;
    unsigned long long dbgWaitobj = m_cam->_getDebug(DBG_WAITOBJ);

    TIME_USEC tStart;
    TIME_USEC tStartXfer;
    msElapsedTimeSet(tStart);

    long long usStart;
    usElapsedTimeSet(usStart);

    m_cam->m_checkImgNr->init(&m_pcoData->traceAcq);

    _pcoAllocBuffers(true); // allocate 2 pco buff at max size

    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[PCO_GetImageEx]",
                                          "[ENTRY]");

    m_cam->_pco_GetActiveRamSegment(wSegment,
                                    error); // = 1 / pco edge doesn't have mem

    //------------------- nr of frames per buffer
    m_cam->getBitsPerPixel(_wBitPerPixel);
    m_cam->getBytesPerPixel(_uiBytesPerPixel);

    WORD _wMaxWidth, _wMaxHeight;
    m_cam->_pco_GetSizes(&_wArmWidth, &_wArmHeight, &_wMaxWidth, &_wMaxHeight,
                         error);
    _pcoAllocBuffersInfo(_iPcoAllocatedBuffNr, _dwPcoAllocatedBuffSize);
    dwFrameSize =
        (DWORD)_wArmWidth * (DWORD)_wArmHeight * (DWORD)_uiBytesPerPixel;
    dwFramesPerBuffer = 1;

    // --------------- live video -> nr frames = 0 / idx lima buffers 32b
    // (0...ffff)
    m_sync->getNbFrames(requested_nb_frames);

    if (requested_nb_frames > 0)
    {
        dwRequestedFrames = (DWORD)requested_nb_frames;
        live_mode = false;
    }
    else
    {
        dwRequestedFrames = dwRequestedFramesMax;
        live_mode = true;
    }

    dwRequestedFrames = (requested_nb_frames > 0) ? (DWORD)requested_nb_frames
                                                  : dwRequestedFramesMax;

    // lima frame nr is from 0 .... N-1
    // pco frame nr is from 1 .... N

    // --------------- loop - process the N frames
    dwFrameIdxCount = dwFrameIdx = 1;
    bufIdx = 0;

    DWORD _dwValidImageCnt, _dwMaxImageCnt;

    // Edge cam must be started just after assign buff to avoid lost of img
    if (m_cam->_isRunAfterAssign())
    {
        DWORD sleepMs = 1;
        ::Sleep(sleepMs);
        if (dbgWaitobj)
        {
            const char *pmsg = "... EDGE - recordingState 1";
            m_cam->m_tmpLog->add(pmsg);
            DEB_ALWAYS() << pmsg;
        }
        DEB_TRACE() << "========================= recordingState 1 - AFTER "
                       "ASSIGN (_xferImag_getImage_edge";
        m_cam->_pco_SetRecordingState(1, error);
    }

    _dwValidImageCnt = dwRequestedFrames;
    dwFrameIdx = (_dwValidImageCnt >= dwRequestedFrames)
                     ? _dwValidImageCnt - dwRequestedFrames + 1
                     : 1;

    DEB_TRACE() << "\n...   "
                << DEB_VAR2(_iPcoAllocatedBuffNr, _dwPcoAllocatedBuffSize)
                << "\n...   "
                << DEB_VAR4(_wArmWidth, _wArmHeight, _uiBytesPerPixel,
                            _wBitPerPixel)
                << "\n...   " << DEB_VAR2(dwFramesPerBuffer, dwFrameSize)
                << "\n...   "
                << DEB_VAR3(requested_nb_frames, dwRequestedFrames, live_mode)
                << "\n...   "
                << DEB_VAR3(dwFrameIdx, _dwValidImageCnt, _dwMaxImageCnt);

    m_pcoData->traceAcq.nrImgRequested = dwRequestedFrames;

    m_pcoData->traceAcq.usTicks[6].desc = "PCO_GetImageEx total execTime";
    m_pcoData->traceAcq.usTicks[7].desc = "xfer to lima / total execTime";

    msElapsedTimeSet(tStartXfer);
    for (iLimaFrame = 0; iLimaFrame < requested_nb_frames; iLimaFrame++)
    {
        bufIdx++;
        if (bufIdx >= _iPcoAllocatedBuffNr)
            bufIdx = 0;
        sBufNr = m_allocBuff.pcoAllocBufferNr[bufIdx];

        if (dwFrameIdx > _dwValidImageCnt)
        {
            DEB_ALWAYS() << "WARNING lost image: "
                         << DEB_VAR2(dwFrameIdx, _dwValidImageCnt);
            dwFrameIdx = _dwValidImageCnt;
        }
        dwFrameIdxLast = dwFrameIdxFirst = dwFrameIdx;
        dwFrameIdxLast = dwFrameIdxFirst = 0;

        usElapsedTimeSet(usStart);

        m_cam->_pco_GetImageEx(wSegment, dwFrameIdxFirst, dwFrameIdxLast,
                               sBufNr, _wArmWidth, _wArmHeight, _wBitPerPixel,
                               error);

        m_pcoData->traceAcq.usTicks[6].value += usElapsedTime(usStart);
        usElapsedTimeSet(usStart);

        if (error)
        {
            DEB_ALWAYS() << "PCO_GetImageEx() ===> "
                         << DEB_VAR4(sErr, wSegment, dwFrameIdxFirst,
                                     dwFrameIdxLast)
                         << DEB_VAR4(sBufNr, _wArmWidth, _wArmHeight,
                                     _wBitPerPixel);
        }

        void *ptrSrc = m_allocBuff.pcoAllocBufferPtr[bufIdx];

        ptrLimaBuffer = _getLimaBuffer(iLimaFrame, status);

        if (ptrLimaBuffer == NULL)
        {
            DEB_ALWAYS() << DEB_VAR3(ptrLimaBuffer, iLimaFrame, status);
            THROW_HW_ERROR(NotSupported) << "Lima ptr = NULL";
        }

        if (m_cam->_getDebug(DBG_XFERMULT1))
        {
            DEB_ALWAYS() << DEB_VAR3(ptrLimaBuffer, ptrSrc, dwFrameSize);
        }

        memcpy(ptrLimaBuffer, ptrSrc, dwFrameSize);

        //=============================== check PCO ImgNr with the limaFrame
        m_cam->m_checkImgNr->update(iLimaFrame, ptrSrc);

        ptrSrc = ((char *)ptrSrc) + dwFrameSize;

        HwFrameInfoType frame_info;
        frame_info.acq_frame_nb = _newFrameReady = iLimaFrame;
        m_buffer_cb_mgr.newFrameReady(frame_info);

        m_pcoData->traceAcq.nrImgAcquired = dwFrameIdxCount;

        if ((_stopReq = m_sync->_getRequestStop(_nrStop)) == stopRequest)
        {
            if (_nrStop > MAX_NR_STOP)
            {
                char msg[LEN_TRACEACQ_MSG + 1];
                snprintf(
                    msg, LEN_TRACEACQ_MSG,
                    "%s> STOP REQ (saving), framesReq[%d] frameReady[%d]\n",
                    fnId, requested_nb_frames, _newFrameReady);
                m_pcoData->traceMsg(msg);
                break;
            }
        }

        m_sync->setAcqFrames(dwFrameIdxCount);
        dwFrameIdx++;
        dwFrameIdxCount++;

        m_pcoData->traceAcq.usTicks[7].value += usElapsedTime(usStart);

    } // while(frameIdx ...

    // if(m_cam->_isCameraType(Edge)) {m_sync->setAcqFrames(dwFrameIdx-1);}

    switch (_stopReq)
    {
            // case stopProcessing:
        case stopRequest:
            _retStatus = pcoAcqTransferStop;
            break;
        default:
            _retStatus = pcoAcqTransferEnd;
    }

    m_pcoData->traceAcq.msXfer = msElapsedTime(tStartXfer);
    m_pcoData->traceAcq.endXferTimestamp = getTimestamp();

    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[EXIT]");
    DEB_TRACE() << DEB_VAR3(_retStatus, _stopReq, _newFrameReady);

    return _retStatus;
#else
    return -1;
#endif
}

//===================================================================================================================
//===================================================================================================================

/********************************************************************************

------------- in dimax is NOT possible read MULTIPLES images (PCO_GetImageEx)

On 2013/09/17 11:44, PCO Support Team wrote:

> So there are a few camera, which support reading multiple images (i.e.
> the 1200hs), but the pco.dimax does not. Also not all interfaces
> support the mode, Martin from the software and driver team has
> realized it for a few CameraLink boards. Our experience is, that it is
> pretty difficult and time consuming to provide the feature for all
> cameras and all interfaces in a reliable way.
>
> Concisely, it's not possible for the pco.dimax, even if the command
> "PCO_GetImageEx" mentions the multiple reading mode.
********************************************************************************/

int BufferCtrlObj::_xferImagMult()
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    int mode = m_cam->_pco_GetStorageMode_GetRecorderSubmode();
    bool continuous = (mode != RecSeq);

    enum
    {
        tracePco = 5,
        tracePcoAll = 6,
        traceLima = 7
    };

    m_pcoData->traceAcq.fnIdXfer = fnId;
    m_pcoData->traceAcq.usTicks[tracePco].desc = "PCO_GetImageEx only";
    m_pcoData->traceAcq.usTicks[tracePcoAll].desc =
        "PCO_GetImageEx / API total execTime";
    m_pcoData->traceAcq.usTicks[traceLima].desc =
        "xfer to lima / total execTime";

    DWORD dwFrameIdx;
    // long long nr =0;
    // long long bytesWritten = 0;
    int bufIdx;
    int error;
    bool live_mode;
    DWORD dwFrameIdxFirst, dwFrameIdxLast;
    int maxWaitTimeout;
    m_cam->getAcqTimeoutRetry(maxWaitTimeout);
    WORD _wBitPerPixel;
    char *sErr;
    void *ptrLimaBuffer;
    Sync::Status status;
    int requested_nb_frames;
    DWORD dwFramesPerBuffer, dwRequestedFrames;
    DWORD dwRequestedFramesMax = DWORD_MAX;
    unsigned int _uiBytesPerPixel;
    WORD _wArmWidth, _wArmHeight;
    int _iPcoAllocatedBuffNr;
    DWORD _dwPcoAllocatedBuffSize;
    SHORT sBufNr;
    WORD wSegment;
    int iPcoFrame, iLimaFrame;
    DWORD dwFrameSize;
    int _retStatus, _stopReq, _nrStop;
    int _newFrameReady = -1;

    TIME_USEC tStart;
    msElapsedTimeSet(tStart);

    long long usStart, usStartPco;
    usElapsedTimeSet(usStart);
    usElapsedTimeSet(usStartPco);

    double msPerFrame = (m_cam->pcoGetCocRunTime() * 1000.);
    DWORD dwMsSleepOneFrame = (DWORD)(msPerFrame / 10.0); // 4/5 rounding
    if (dwMsSleepOneFrame == 0)
        dwMsSleepOneFrame = 1; // min sleep

    //_pcoAllocBuffers(true); // allocate 2 pco buff at max size

    m_cam->_pco_GetActiveRamSegment(wSegment, error);

    //------------------- nr of frames per buffer
    m_cam->getBitsPerPixel(_wBitPerPixel);
    m_cam->getBytesPerPixel(_uiBytesPerPixel);

    WORD _wMaxWidth, _wMaxHeight;
    m_cam->_pco_GetSizes(&_wArmWidth, &_wArmHeight, &_wMaxWidth, &_wMaxHeight,
                         error);
    _pcoAllocBuffersInfo(_iPcoAllocatedBuffNr, _dwPcoAllocatedBuffSize);

    WORD _wRoiWidth = m_pcoData->wRoiX1Now - m_pcoData->wRoiX0Now + 1;
    WORD _wRoiHeight = m_pcoData->wRoiX1Now - m_pcoData->wRoiX0Now + 1;

    // dwFrameSize = (DWORD) _wRoiWidth * (DWORD) _wRoiHeight * (DWORD)
    // _uiBytesPerPixel;

    dwFrameSize =
        (DWORD)_wArmWidth * (DWORD)_wArmHeight * (DWORD)_uiBytesPerPixel;
    // dwFramesPerBuffer = _dwPcoAllocatedBuffSize / dwFrameSize ;
    dwFramesPerBuffer = 1;

    Roi roiNow;
    m_cam->_pco_GetROI(roiNow, error);

    // --------------- live video -> nr frames = 0 / idx lima buffers 32b
    // (0...ffff)
    m_sync->getNbFrames(requested_nb_frames);

    if (requested_nb_frames > 0)
    {
        dwRequestedFrames = (DWORD)requested_nb_frames;
        live_mode = false;
    }
    else
    {
        dwRequestedFrames = dwRequestedFramesMax;
        live_mode = true;
    }

    dwRequestedFrames = (requested_nb_frames > 0) ? (DWORD)requested_nb_frames
                                                  : dwRequestedFramesMax;

    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[PCO_GetImageEx]",
                                          "[ENTRY]");

    DEB_TRACE() << "    "
                << DEB_VAR2(_iPcoAllocatedBuffNr, _dwPcoAllocatedBuffSize)
                << "\n"
                << "    " << DEB_VAR2(_wArmWidth, _wArmHeight) << "\n"
                << "    " << DEB_VAR1(roiNow) << "\n"
                << "    "
                << DEB_VAR4(_wRoiWidth, _wRoiHeight, _uiBytesPerPixel,
                            _wBitPerPixel)
                << "\n"
                << "    " << DEB_VAR2(dwFramesPerBuffer, dwFrameSize) << "\n"
                << "    "
                << DEB_VAR3(requested_nb_frames, dwRequestedFrames, live_mode);

    // lima frame nr is from 0 .... N-1
    // pco frame nr is from 1 .... N

    // --------------- loop - process the N frames
    dwFrameIdx = 1;
    bufIdx = 0;

    DWORD _dwValidImageCnt, _dwMaxImageCnt;
    DWORD _dwValidImageCntLast = 0;

    m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwValidImageCnt,
                                           _dwMaxImageCnt, error);
    if (error)
    {
        printf("=== %s [%d]> ERROR\n", fnId, __LINE__);
        throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
    }

    if (m_cam->_getDebug(DBG_XFERMULT))
    {
        DEB_ALWAYS() << "\n     "
                     << DEB_VAR3(dwRequestedFrames, _dwValidImageCnt,
                                 _dwMaxImageCnt);
    }

    m_pcoData->traceAcq.nrImgRequested = dwRequestedFrames;

    while (dwFrameIdx <= dwRequestedFrames)
    {
        bufIdx++;
        if (bufIdx >= _iPcoAllocatedBuffNr)
            bufIdx = 0;
        sBufNr = m_allocBuff.pcoAllocBufferNr[bufIdx];

        if (continuous)
        {
            dwFrameIdxLast = dwFrameIdxFirst = 0;
        }
        else
        {
            dwFrameIdxLast = dwFrameIdxFirst = dwFrameIdx;
        }

        if (m_cam->_getDebug(DBG_XFERMULT1))
        {
            DEB_ALWAYS() << "\n     PCO_GetImageEx() ===>"
                         << "\n     "
                         << DEB_VAR5(dwFrameIdx, dwFrameIdxFirst,
                                     dwFrameIdxLast, dwFramesPerBuffer,
                                     dwRequestedFrames)
                         << "\n     "
                         << DEB_VAR5(wSegment, sBufNr, _wArmWidth, _wArmHeight,
                                     _wBitPerPixel);
        }

        usElapsedTimeSet(usStart);

        //========================================================================================
        //   PCO_GetImageEx has a fixex timeout of about 5s, so the call for
        //   long exp time fails included a waiting loop to chech when the image
        //   is ready
        //========================================================================================

        // recording loop / waits up to recorded images _dwValidImageCnt >=
        // dwFrameIdxFirst in continuos mode (dwFrameIdxFirst == 0) is bypassed
        while (1)
        {
            m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwValidImageCnt,
                                                   _dwMaxImageCnt, error);
            if (error)
            {
                DEB_ALWAYS() << "ERROR PCO_GetNumberOfImagesInSegment "
                             << DEB_VAR2(error, sErr);
                throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
            }

            if ((_dwValidImageCnt > 0) &&
                ((_dwValidImageCnt >= dwFrameIdxFirst) ||
                 (dwFrameIdxFirst == 0)))
                break;

            ::Sleep(dwMsSleepOneFrame); // sleep 1 frame
        }

        if (_dwValidImageCnt != _dwValidImageCntLast)
        {
            DEB_TRACE() << "=== _dwValidImageCnt != _dwValidImageCntLast "
                        << DEB_VAR3(_dwValidImageCnt, dwFrameIdxFirst,
                                    _dwValidImageCntLast);
            _dwValidImageCntLast = _dwValidImageCnt;
        }

#if 0
        DEB_TRACE()
            << "\n     PCO_GetImageEx() ===>"
            << "\n     " << DEB_VAR5(dwFrameIdx, dwFrameIdxFirst, dwFrameIdxLast, dwFramesPerBuffer, dwRequestedFrames)
            << "\n     " << DEB_VAR5(wSegment, sBufNr, _wArmWidth, _wArmHeight, _wBitPerPixel);
#endif

        usElapsedTimeSet(usStartPco);

        m_cam->_pco_GetImageEx(wSegment, dwFrameIdxFirst, dwFrameIdxLast,
                               sBufNr, _wArmWidth, _wArmHeight, _wBitPerPixel,
                               error);
        // TODO PRINT ERROR

        m_pcoData->traceAcq.usTicks[tracePco].value +=
            usElapsedTime(usStartPco);

        m_pcoData->traceAcq.usTicks[tracePcoAll].value +=
            usElapsedTime(usStart);
        usElapsedTimeSet(usStart);

        if (error)
        {
            DEB_ALWAYS() << "\n     PCO_GetImageEx() ===> "
                         << "\n     "
                         << DEB_VAR4(sErr, wSegment, dwFrameIdxFirst,
                                     dwFrameIdxLast)
                         << "\n     "
                         << DEB_VAR4(sBufNr, _wArmWidth, _wArmHeight,
                                     _wBitPerPixel);
        }

        void *ptrSrc = m_allocBuff.pcoAllocBufferPtr[bufIdx];

        iPcoFrame = dwFrameIdx;
        iLimaFrame = iPcoFrame - 1;

        ptrLimaBuffer = _getLimaBuffer(iLimaFrame, status);

        if (ptrLimaBuffer == NULL)
        {
            DEB_ALWAYS() << "\n     "
                         << DEB_VAR3(ptrLimaBuffer, iLimaFrame, status);
            THROW_HW_ERROR(NotSupported) << "Lima ptr = NULL";
        }

        if (m_cam->_getDebug(DBG_XFERMULT1))
        {
            DEB_ALWAYS() << "\n     "
                         << DEB_VAR3(ptrLimaBuffer, ptrSrc, dwFrameSize);
        }

        memcpy(ptrLimaBuffer, ptrSrc, dwFrameSize);
        ptrSrc = ((char *)ptrSrc) + dwFrameSize;

        HwFrameInfoType frame_info;
        frame_info.acq_frame_nb = _newFrameReady = iLimaFrame;
        m_buffer_cb_mgr.newFrameReady(frame_info);

        m_pcoData->traceAcq.nrImgAcquired = dwFrameIdx;

        if ((_stopReq = m_sync->_getRequestStop(_nrStop)) == stopRequest)
        {
            if (_nrStop > MAX_NR_STOP)
            {
                char msg[LEN_TRACEACQ_MSG + 1];
                snprintf(
                    msg, LEN_TRACEACQ_MSG,
                    "%s> STOP REQ (saving), framesReq[%d] frameReady[%d]\n",
                    fnId, requested_nb_frames, _newFrameReady);
                m_pcoData->traceMsg(msg);
                break;
            }
        }

        m_sync->setAcqFrames(dwFrameIdx);
        dwFrameIdx++;

        m_pcoData->traceAcq.usTicks[traceLima].value += usElapsedTime(usStart);

    } // while(frameIdx ...

    // if(m_cam->_isCameraType(Edge)) {m_sync->setAcqFrames(dwFrameIdx-1);}

    switch (_stopReq)
    {
            // case stopProcessing:
        case stopRequest:
            _retStatus = pcoAcqTransferStop;
            break;
        default:
            _retStatus = pcoAcqTransferEnd;
    }

    DEB_TRACE() << "\n>>> " << fnId << " [ EXIT]:\n"
                << "\n     " << DEB_VAR3(_retStatus, _stopReq, _newFrameReady);

    return _retStatus;
}

//===================================================================================================================
//===================================================================================================================
#define BUFFER_DUMMY_IMG_LEN (2 * 2016 * 2016)
// called by startAcq
void BufferCtrlObj::_pcoAllocBuffers(bool max)
{
#ifndef __linux__

    DEB_MEMBER_FUNCT();
    DEF_FNID;

    int bufIdx;

#    if 0

    if (!m_allocBuff.createEventsDone) {
        for (bufIdx = 0; bufIdx < m_cam->m_pco_buffer_nrevents; bufIdx++) {
            // Create two event objects
            m_allocBuff.bufferAllocEvent[bufIdx] = CreateEvent(
                NULL,   // default security attributes
                true,  // auto-reset event object = false / manual reset = true
                false,  // initial state is nonsignaled
                NULL);  // unnamed object

            if (!m_allocBuff.bufferAllocEvent[bufIdx])
            {
                THROW_HW_ERROR(NotSupported) << "CreateEvent error";
            }
        }
        m_allocBuff.createEventsDone = true;

    }
#    endif

#    ifdef USING_PCO_ALLOCATED_BUFFERS

    _pcoAllocBuffersFree();

    // we are using pco allocated buffer, we must to allocate them
    int error = 0;
    // char *sErr;
    DWORD _dwMaxWidth, _dwMaxHeight;
    unsigned int _uiMaxWidth, _uiMaxHeight;
    WORD _wArmWidth, _wArmHeight;
    WORD _wBitPerPixel;
    unsigned int _bytesPerPixel;

    if (!m_allocBuff.pcoAllocBufferDone)
    {
        m_cam->getBytesPerPixel(_bytesPerPixel);
        m_cam->getBitsPerPixel(_wBitPerPixel);
        // WORD _wPixPerPage = m_cam->m_pcoData->wPixPerPage;

        m_cam->getMaxWidthHeight(_uiMaxWidth, _uiMaxHeight); // max
        _dwMaxWidth = _uiMaxWidth;
        _dwMaxHeight = _uiMaxHeight;

        WORD _wMaxWidth, _wMaxHeight;
        m_cam->_pco_GetSizes(&_wArmWidth, &_wArmHeight, &_wMaxWidth,
                             &_wMaxHeight, error);
        PCO_THROW_OR_TRACE(error, "PCO_GetSizes before buffer allocation");

        DWORD _dwAllocatedBufferSizeMax =
            (DWORD)_wMaxWidth * (DWORD)_wMaxHeight * (DWORD)_bytesPerPixel;

        DWORD _dwArmSize =
            (DWORD)_wArmWidth * (DWORD)_wArmHeight * (DWORD)_bytesPerPixel;

        // CDI and Double Image requires a double size + 1 page for metadata
        WORD wCdi, wDoubleImage;
        int err;
        m_cam->_pco_GetCDIMode(wCdi, err);
        m_cam->_pco_GetDoubleImageMode(wDoubleImage, err);

        // reserve ImageWidth * ImageHeight * 2, for standard mode and CDI mode.
        // Double this for double image.
        // if(wDoubleImage) _dwArmSize = _dwArmSize*2;

        //_dwAllocatedBufferSizeMax -= _dwAllocatedBufferSizeMax % _dwArmSize;

        DWORD _dwAllocatedBufferSize =
            max ? _dwAllocatedBufferSizeMax : _dwArmSize;

        if (m_cam->_getDebug(DBG_BUFF))
        {
            DEB_ALWAYS() << DEB_VAR4(_dwAllocatedBufferSize, _wArmWidth,
                                     _wArmHeight, _bytesPerPixel);
        }

        m_pcoData->iAllocatedBufferNumber = m_cam->m_pco_buffer_nrevents;
        m_pcoData->dwAllocatedBufferSize = _dwAllocatedBufferSize;

        DEB_TRACE() << "\n ... "
                    << DEB_VAR4(_dwAllocatedBufferSizeMax, _dwMaxWidth,
                                _dwMaxHeight, _bytesPerPixel)
                    << "\n ... "
                    << DEB_VAR4(_dwArmSize, _wArmWidth, _wArmHeight,
                                _bytesPerPixel)
                    << "\n ... "
                    << DEB_VAR4(_dwAllocatedBufferSize, max,
                                _dwAllocatedBufferSizeMax, _dwArmSize);

        for (bufIdx = 0; bufIdx < m_cam->m_pco_buffer_nrevents; bufIdx++)
        {
            m_allocBuff.bufferAllocEvent[bufIdx] = NULL;
            m_allocBuff.pcoAllocBufferNr[bufIdx] = -1;
            m_allocBuff.pcoAllocBufferPtr[bufIdx] = NULL;

            error = PCO_AllocateBuffer(
                m_cam->m_handle, &m_allocBuff.pcoAllocBufferNr[bufIdx],
                _dwAllocatedBufferSize, &m_allocBuff.pcoAllocBufferPtr[bufIdx],
                &m_allocBuff.bufferAllocEvent[bufIdx]);

            if (error)
            {
                const int allocationError = error;
                m_allocBuff.pcoAllocBufferDone = true;
                _pcoAllocBuffersFree();
                PCO_THROW_OR_TRACE(allocationError, "PCO_AllocateBuffer");
            }
            m_allocBuff.dwPcoAllocBufferSize[bufIdx] = _dwAllocatedBufferSize;
            m_allocBuff.createEventsDone = true;
        }

#        if 0
        //-------------- allocate 2 buffers (0,1) and received the handle, mem ptr, events
        for (bufIdx = 0; bufIdx < m_cam->m_pco_buffer_nrevents; bufIdx++)
        {
            m_allocBuff.pcoAllocBufferNr[bufIdx] = -1;
            m_cam->_pco_AllocateBuffer(
                &m_allocBuff.pcoAllocBufferNr[bufIdx], \
                _dwAllocatedBufferSize, \
                & m_allocBuff.pcoAllocBufferPtr[bufIdx], \
                & m_allocBuff.bufferAllocEvent[bufIdx], error);
            // PRINT ERROR	, error, "PCO_AllocateBuffer");

            DEB_TRACE() << fnId << " " << DEB_VAR2(bufIdx, _dwAllocatedBufferSize);

            if (error) {
                int nrEvents = m_cam->m_pco_buffer_nrevents;
                DEB_ALWAYS() << sErr << "\n"
                    << DEB_VAR3(nrEvents, bufIdx, _dwAllocatedBufferSize);
                THROW_HW_ERROR(NotSupported) << sErr;
            }
            m_allocBuff.dwPcoAllocBufferSize[bufIdx] = _dwAllocatedBufferSize;

        }

#        endif
        m_pcoData->bAllocatedBufferDone = m_allocBuff.pcoAllocBufferDone = true;
    }
#    endif

    if (m_cam->_getDebug(DBG_BUFF))
    {
        void *ptrEvent, *ptrBuff;
        int iPcoBufIdx;

        for (bufIdx = 0; bufIdx < m_cam->m_pco_buffer_nrevents; bufIdx++)
        {
            ptrEvent = (void *)m_allocBuff.bufferAllocEvent[bufIdx];
            ptrBuff = (void *)m_allocBuff.pcoAllocBufferPtr[bufIdx];
            iPcoBufIdx = m_allocBuff.pcoAllocBufferNr[bufIdx];
            DEB_TRACE() << DEB_VAR4(bufIdx, ptrEvent, ptrBuff, iPcoBufIdx);
        }
    }

#endif
}

//===================================================================================================================
//===================================================================================================================
void BufferCtrlObj::_pcoAllocBuffersInfo(int &nr, DWORD &size)
{
    DEB_MEMBER_FUNCT();

    nr = m_pcoData->iAllocatedBufferNumber;
    size = m_pcoData->dwAllocatedBufferSize;
}

//===================================================================================================================
//===================================================================================================================

void BufferCtrlObj::_pcoAllocBuffersFree()
{
    DEF_FNID;
    DEB_MEMBER_FUNCT();

    DEB_TRACE() << fnId << " [ENTRY]";

#ifdef USING_PCO_ALLOCATED_BUFFERS
    // free the pco allocated buffers
    int error = 0;

    // SC2_SDK_FUNC int WINAPI PCO_FreeBuffer(HANDLE ph, SHORT sBufNr)

    //-------------- allocate 2 buffers (0,1) and received the handle, mem ptr,
    // events
    for (int bufIdx = 0; bufIdx < PCO_MAX_NR_ALLOCATED_BUFFERS; bufIdx++)
    {
        SHORT sNrBuff = m_allocBuff.pcoAllocBufferNr[bufIdx]; // 0 ... 7
        if ((sNrBuff >= 0) && (sNrBuff <= 7))
        {
            m_cam->_pco_FreeBuffer(sNrBuff, error);
        }
        m_allocBuff.pcoAllocBufferNr[bufIdx] = -1;
        m_allocBuff.dwPcoAllocBufferSize[bufIdx] = 0;
        m_allocBuff.pcoAllocBufferPtr[bufIdx] = NULL;
    }
    m_allocBuff.pcoAllocBufferDone = false;
#endif

#if 0
#    ifdef USING_PCO_ALLOCATED_BUFFERS 
    // free the pco allocated buffers
    error = 0;
    char* sErr;

    if (m_allocBuff.pcoAllocBufferDone) {

        //SC2_SDK_FUNC int WINAPI PCO_FreeBuffer(HANDLE ph, SHORT sBufNr)

            //-------------- allocate 2 buffers (0,1) and received the handle, mem ptr, events
        for (int bufIdx = 0; bufIdx < m_pco_buffer_nrevents; bufIdx++) {
            m_cam->_pco_FreeBuffer(m_allocBuff.pcoAllocBufferNr[bufIdx], error);

            if (error) {
                DEB_TRACE() << sErr;
                THROW_HW_ERROR(NotSupported) << sErr;
            }
            m_allocBuff.pcoAllocBufferNr[bufIdx] = -1;
            m_allocBuff.dwPcoAllocBufferSize[bufIdx] = 0;
            m_allocBuff.pcoAllocBufferPtr[bufIdx] = NULL;

        }
        m_allocBuff.pcoAllocBufferDone = false;
    }
#    endif
#endif
}
//===================================================================================================================
//===================================================================================================================
int BufferCtrlObj::_xferImagDoubleImage()
{
#ifndef __linux__
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    DWORD dwPcoFrameIdx;
    DWORD dwPcoFrameFirst2assign, dwPcoFrameLast2assign;
    DWORD dwEvent;

    int bufIdx;
    int error;
    int lima_buffer_nb;
    bool live_mode;
    bool is_one_buffer_ready;
    int _nrStop;
    char msg[RING_LOG_BUFFER_SIZE + 1];
    const char *pmsg = msg;
    m_cam->m_tmpLog->flush(-1);
    int maxWaitTimeout;
    m_cam->getAcqTimeoutRetry(maxWaitTimeout);

    WORD wArmWidth, wArmHeight;
    unsigned int bytesPerPixel;
    WORD _wMaxWidth, _wMaxHeight;
    m_cam->_pco_GetSizes(&wArmWidth, &wArmHeight, &_wMaxWidth, &_wMaxHeight,
                         error);
    m_cam->getBytesPerPixel(bytesPerPixel);

    unsigned long long dbgWaitobj = m_cam->_getDebug(DBG_WAITOBJ);

    int iLimaFrame;

    m_cam->m_checkImgNr->init(&m_pcoData->traceAcq);

    DEB_ALWAYS() << m_cam->_sprintComment(
        false, fnId, "[WaitForMultipleObjects]", "[ENTRY]");

    // --------------- get the requested nr of images
    int requested_nb_frames;
    DWORD dwPcoFramesPerBuffer, dwPcoRequestedFrames, dwLimaRequestedFrames;
    DWORD dwRequestedFramesMax = DWORD_MAX;

    WORD wDoubleImage = 1;
    // int err;
    // m_cam->_pco_GetDoubleImageMode(wDoubleImage, err);

    // --------------- live video -> nr frames = 0 / idx lima buffers 32b
    // (0...ffff)
    m_sync->getNbFrames(requested_nb_frames);

    if (requested_nb_frames > 0)
    {
        dwLimaRequestedFrames = (DWORD)requested_nb_frames;
        live_mode = false;
    }
    else
    {
        dwLimaRequestedFrames = dwRequestedFramesMax;
        live_mode = true;
    }

    dwLimaRequestedFrames = (requested_nb_frames > 0)
                                ? (DWORD)requested_nb_frames
                                : dwRequestedFramesMax;
    dwPcoRequestedFrames = dwLimaRequestedFrames / 2; // for double image

    dwPcoFramesPerBuffer = m_cam->pcoGetFramesPerBuffer(); // for dimax = 1

    DEB_TRACE() << "\n"
                //		<< ">>> " << fnId << " (WaitForMultipleObjects) [
                // ENTRY]:\n"
                //		<< "    " << DEB_VAR2(_iPcoAllocatedBuffNr,
                //_dwPcoAllocatedBuffSize) << "\n"
                //		<< "    " << DEB_VAR2(_wArmWidth, _wArmHeight) << "\n"
                //		<< "    " << DEB_VAR1(roiNow) << "\n"
                //		<< "    " << DEB_VAR4(_wRoiWidth, _wRoiHeight,
                //_uiBytesPerPixel, _wBitPerPixel) << "\n"
                //		<< "    " << DEB_VAR2( dwPcoFramesPerBuffer,
                // dwFrameSize)
                //<< "\n"
                << "    "
                << DEB_VAR3(requested_nb_frames, dwPcoRequestedFrames,
                            live_mode);

    //----------------- traceAcq init

    m_pcoData->traceAcq.fnIdXfer = fnId;
    m_pcoData->traceAcq.msImgCoc = (m_cam->pcoGetCocRunTime() * 1000.);

    TIME_USEC tStart;
    msElapsedTimeSet(tStart);
    m_pcoData->traceAcq.nrImgRequested = dwLimaRequestedFrames;

    // cleaning the buffers
    for (int _id = 0; _id < m_cam->m_pco_buffer_nrevents; _id++)
    {
        m_allocBuff.bufferReady[_id] = 0;
        m_allocBuff.bufferAssignedFrameFirst[_id] = -1;
        m_allocBuff.limaAllocBufferPtr[_id] = NULL;
        m_allocBuff.limaAllocBufferPtr1[_id] = NULL;
        m_allocBuff.dwLimaAllocBufferSize[_id] = 0;
    }

    // --------------- prepare the first buffer
    // ------- in PCO DIMAX only 1 image can be retreived
    //         (dwPcoFramesPerBuffer = 1) ====> (dwPcoFrameLast2assign =
    //         dwPcoFrameFirst2assign)
    dwPcoFrameFirst2assign = 1;
    dwPcoFrameLast2assign = dwPcoFrameFirst2assign + dwPcoFramesPerBuffer - 1;
    if (dwPcoFrameLast2assign > dwPcoRequestedFrames)
        dwPcoFrameLast2assign = dwPcoRequestedFrames;

    for (int iEvent = 0; iEvent < m_cam->m_pco_buffer_nrevents; iEvent++)
    {
        // --------------- if needed prepare the next buffer
        if (dwPcoFrameFirst2assign > dwPcoRequestedFrames)
            break;
        bufIdx = iEvent;

        if (dbgWaitobj)
        {
            __sprintfSExt(msg, RING_LOG_BUFFER_SIZE,
                          "... ASSIGN BUFFER[%d] frame[%d]", bufIdx,
                          dwPcoFrameFirst2assign);
            m_cam->m_tmpLog->add(msg);
            DEB_ALWAYS() << msg;
        }

        bool recording = m_cam->_getCameraState(CAMSTATE_RECORD_STATE);
        bool runAfterAssign = m_cam->_isRunAfterAssign();

        if ((!runAfterAssign) || (!recording && runAfterAssign))
        {
            if ((error = _assignImage2Buffer(
                     dwPcoFrameFirst2assign, dwPcoFrameLast2assign,
                     dwPcoRequestedFrames, bufIdx, live_mode, wDoubleImage)))
            {
                DEB_ALWAYS() << "ERROR _assignImage2Buffer";
                return pcoAcqPcoError;
            }
        }
        else
        {
            DEB_ALWAYS() << "ERROR _assignImage2Buffer with wrong recordState "
                            "/ IGNORED!!!!"
                         << DEB_VAR2(recording, runAfterAssign);
        }

    } // for i nr events


    // Edge cam must be started just after assign buff to avoid lost of img
    if (m_cam->_isRunAfterAssign())
    {
        DWORD sleepMs = 1;
        ::Sleep(sleepMs);
        DEB_TRACE() << "========================= recordingState 1 - AFTER "
                       "ASSIGN (_xferImag)";
        if (dbgWaitobj)
        {
            pmsg = "... EDGE - recordingState 1";
            m_cam->m_tmpLog->add(pmsg);
            DEB_TRACE() << pmsg;
        }
        m_cam->_pco_SetRecordingState(1, error);
    }

    // --------------- loop - process the N frames

    dwPcoFrameIdx = 1;
    if (dbgWaitobj)
    {
        DEB_ALWAYS() << "FRAME IDX before while: "
                     << DEB_VAR2(dwPcoFrameIdx, dwPcoRequestedFrames);
    }
    while (dwPcoFrameIdx <= dwPcoRequestedFrames)
    {
        if (dbgWaitobj)
        {
            DEB_ALWAYS() << "FRAME IDX inside while: "
                         << DEB_VAR2(dwPcoFrameIdx, dwPcoRequestedFrames);
        }

    _RETRY:

        if ((m_sync->_getRequestStop(_nrStop) == stopRequest) &&
            (_nrStop > MAX_NR_STOP))
        {
            goto _EXIT_STOP;
        }

        // --------------- look if one of buffer is READY and has the NEXT frame
        // => proccess it
        // m_allocatedBufferAssignedFrameFirst[bufIdx] -> first frame in the
        // buffer (we are using only 1 frame per buffer)
        // m_allocatedBufferReady[bufIdx] -> is already filled by sdk (ready)

        for (bufIdx = 0; bufIdx < m_cam->m_pco_buffer_nrevents; bufIdx++)
        {
            DWORD _dwBufferAssignedFrameFirst =
                m_allocBuff.bufferAssignedFrameFirst[bufIdx];
            int _iBufferReady = m_allocBuff.bufferReady[bufIdx];

            if (dbgWaitobj)
            {
                DEB_ALWAYS() << DEB_VAR3(_dwBufferAssignedFrameFirst,
                                         dwPcoFrameIdx, _iBufferReady);
            }
            if ((m_allocBuff.bufferAssignedFrameFirst[bufIdx] ==
                 dwPcoFrameIdx) &&
                m_allocBuff.bufferReady[bufIdx])
            {
                void *ptrLimaBuffer, *ptrPcoBuffer;
                size_t sizeLima, sizePco, size;
                SHORT sBufNr;
                DWORD dwStatusDll, dwStatusDrv;
                int errPco;
                char msg[MSG1K];

                if (dbgWaitobj)
                {
                    DEB_ALWAYS() << "... found buffer ... now img 1";
                }

                m_pcoData->traceAcq.nrImgAcquired = dwPcoFrameIdx;
                // lima_buffer_nb = (dwPcoFrameIdx -1) * 2; // this frame was
                // already readout to the buffer

                sizeLima = m_allocBuff.dwLimaAllocBufferSize[bufIdx];
                sizePco = m_allocBuff.dwPcoAllocBufferSize[bufIdx];
                size = sizeLima;
                sBufNr = m_allocBuff.pcoAllocBufferNr[bufIdx];

                //----------- img 1
                // we are using the PCO allocated buffer, so this buffer must be
                // copied to the lima buffer
                ptrLimaBuffer = (void *)m_allocBuff.limaAllocBufferPtr[bufIdx];
                ptrPcoBuffer = (void *)m_allocBuff.pcoAllocBufferPtr[bufIdx];
                lima_buffer_nb = m_allocBuff.limaAllocBufferNr[bufIdx];

                if ((m_sync->_getRequestStop(_nrStop) == stopRequest) &&
                    (_nrStop > MAX_NR_STOP))
                {
                    goto _EXIT_STOP;
                }

                m_cam->_pco_GetBufferStatus(sBufNr, &dwStatusDll, &dwStatusDrv,
                                            errPco);
                if ((dwStatusDll != 0x80000000) || dwStatusDrv || errPco)
                {
                    __sprintfSExt(msg, sizeof(msg),
                                  "SDK ERROR got frame[%d / %d] bufIdx[%d] "
                                  "size[%ld] dest[%p] src[%p] \n"
                                  "dwStatusDll[%08x] dwStatusDrv[%08x] "
                                  "errPco[%08x] err[%s]\n",
                                  dwPcoFrameIdx, dwPcoRequestedFrames, bufIdx,
                                  size, ptrLimaBuffer, ptrPcoBuffer,
                                  dwStatusDll, dwStatusDrv, errPco,
                                  m_cam->_PcoCheckError(__LINE__, __FILE__,
                                                        dwStatusDrv, error));
                    DEB_ALWAYS() << msg;
                }

                if (dbgWaitobj)
                {
                    DEB_ALWAYS()
                        << "... IMG 1"
                        << DEB_VAR7(ptrLimaBuffer, ptrPcoBuffer, size, sizeLima,
                                    sizePco, sBufNr, lima_buffer_nb);
                }
                // copy the real imaga ptrPcoBuffer -> ptrLimaBuffer
                memcpy(ptrLimaBuffer, ptrPcoBuffer, size);

                //----- the image dwPcoFrameIdx is already in the buffer ->
                // callback newFrameReady
                //----- lima frame (0 ... N-1) PCO frame (1 ... N)

                {
                    HwFrameInfoType frame_info;
                    iLimaFrame = frame_info.acq_frame_nb = lima_buffer_nb;
                    m_buffer_cb_mgr.newFrameReady(frame_info);
                }
                //----------- img1 / end

                // lima_buffer_nb++;

                //----------- img 2
                // we are using the PCO allocated buffer, so this buffer must be
                // copied to the lima buffer
                ptrLimaBuffer = (void *)m_allocBuff.limaAllocBufferPtr1[bufIdx];
                ptrPcoBuffer =
                    (void *)((char *)m_allocBuff.pcoAllocBufferPtr[bufIdx] +
                             sizeLima);
                lima_buffer_nb = m_allocBuff.limaAllocBufferNr1[bufIdx];

                if ((m_sync->_getRequestStop(_nrStop) == stopRequest) &&
                    (_nrStop > MAX_NR_STOP))
                {
                    goto _EXIT_STOP;
                }

                if (dbgWaitobj)
                {
                    DEB_ALWAYS()
                        << "... IMG 2"
                        << DEB_VAR7(ptrLimaBuffer, ptrPcoBuffer, size, sizeLima,
                                    sizePco, sBufNr, lima_buffer_nb);
                }
                // copy the real imag ptrPcoBuffer -> ptrLimaBuffer
                memcpy(ptrLimaBuffer, ptrPcoBuffer, size);

                //----- the image dwPcoFrameIdx is already in the buffer ->
                // callback newFrameReady
                //----- lima frame (0 ... N-1) PCO frame (1 ... N)

                {
                    HwFrameInfoType frame_info;
                    iLimaFrame = frame_info.acq_frame_nb = lima_buffer_nb;
                    m_buffer_cb_mgr.newFrameReady(frame_info);
                }
                //----------- img2 / end

                //=============================== check PCO ImgNr with the
                // limaFrame
                m_cam->m_checkImgNr->update(iLimaFrame, ptrPcoBuffer);

                //----- the image dwPcoFrameIdx is already in the buffer ->
                // callback!
                if ((m_sync->_getRequestStop(_nrStop) == stopRequest) &&
                    (_nrStop > MAX_NR_STOP))
                {
                    goto _EXIT_STOP;
                }
                if (dwPcoFrameFirst2assign <= dwPcoRequestedFrames)
                {
                    if ((m_cam->_getCameraState(CAMSTATE_RECORD_STATE) &&
                         m_cam->_isRunAfterAssign()) ||
                        (!m_cam->_isRunAfterAssign()))
                    {
                        if ((error = _assignImage2Buffer(
                                 dwPcoFrameFirst2assign, dwPcoFrameLast2assign,
                                 dwPcoRequestedFrames, bufIdx, live_mode,
                                 wDoubleImage)))
                        {
                            return pcoAcqPcoError;
                        }
                    }
                    else
                    {
                        DEB_ALWAYS() << "ERROR _assignImage2Buffer with "
                                        "recordState = 0 / IGNORED!!!!";
                    }
                }

                if (dbgWaitobj)
                {
                    DEB_ALWAYS() << "... goto _WHILE_CONTINUE";
                }
                goto _WHILE_CONTINUE;
            }
        } // for(bufIdx = 0; bufIdx <m_cam->m_pco_buffer_nrevents; bufIdx++)

    _RETRY_WAIT:

        if ((m_sync->_getRequestStop(_nrStop) == stopRequest) &&
            (_nrStop > MAX_NR_STOP))
        {
            goto _EXIT_STOP;
        }

        // --------------- check if there is some buffer ready
        if (dbgWaitobj)
        {
            pmsg = "... WaitForMultipleObjects - waiting";
            m_cam->m_tmpLog->add(pmsg);
            DEB_ALWAYS() << pmsg;
        }
        dwEvent = WaitForMultipleObjects(
            m_cam->m_pco_buffer_nrevents, // number of objects in array
            m_allocBuff.bufferAllocEvent, // array of objects
            false,                        // wait for any object
            EVENT_WAIT_TMOUT_MS);         // ms wait

#    if PCO_BUFFER_NREVENTS != 4
#        error ============================================== ABORT - wrong nr of WAIT_OBJECT
#    endif

        if (dwEvent == WAIT_TIMEOUT)
        {
            maxWaitTimeout--;
            if (dbgWaitobj)
            {
                m_cam->m_tmpLog->dumpPrint(true);
            }

            char errstr[MSG4K + 1];
            char *ptr = errstr;
            char *ptrMax = errstr + MSG4K;
            const char *flag;
            for (int _id = 0; _id < m_cam->m_pco_buffer_nrevents; _id++)
            {
                flag = m_allocBuff.bufferAssignedFrameFirst[_id] ==
                                dwPcoFrameIdx
                            ? "***"
                            : "   ";
                ptr += __sprintfSExt(
                    ptr, ptrMax - ptr,
                    "\n%s [%d] pcoBuffNr[%d] ready[%d] limaFrame[%d] "
                    "limaPtr[%p] limaPtr1[%p] pcoPtr[%p] limaSize[%d] "
                    "pcoSize[%d]",
                    flag, _id, m_allocBuff.pcoAllocBufferNr[_id],
                    m_allocBuff.bufferReady[_id],
                    m_allocBuff.bufferAssignedFrameFirst[_id],
                    (void *)m_allocBuff.limaAllocBufferPtr[_id],
                    (void *)m_allocBuff.limaAllocBufferPtr1[_id],
                    (void *)m_allocBuff.pcoAllocBufferPtr[_id],
                    m_allocBuff.dwLimaAllocBufferSize[_id],
                    m_allocBuff.dwPcoAllocBufferSize[_id]);
            }

            if (maxWaitTimeout)
            {
                DEB_ALWAYS()
                    << "\nWAITOBJ ERROR - TIMEOUT - RETRY "
                    << DEB_VAR2(maxWaitTimeout, dwPcoFrameIdx) << errstr;
                goto _RETRY_WAIT; // retry when >0 (counting down up to 0) /
                                    // <0 infinite
            }
            else
            {
                DEB_ALWAYS()
                    << "\nWAITOBJ ERROR - TIMEOUT - ABORT "
                    << DEB_VAR2(maxWaitTimeout, dwPcoFrameIdx) << errstr;
                return pcoAcqWaitTimeout;
            }
        }
        else
        {
            // WaitForMultipleObjects might return with 2 or more events set,
            // so all buffers must be checked with WaitForSingleObject

            // This will be true if at least one buffer is ready
            is_one_buffer_ready = false;

            for(bufIdx = 0; bufIdx < m_cam->m_pco_buffer_nrevents; bufIdx++)
            {
                if(WaitForSingleObject(m_allocBuff.bufferAllocEvent[bufIdx], 0) == WAIT_OBJECT_0)
                {
                    m_allocBuff.bufferReady[bufIdx] = 1;
                    is_one_buffer_ready = true;
                    if (dbgWaitobj)
                    {
                        pmsg = "... WAITOBJ 0 found";
                        m_cam->m_tmpLog->add(pmsg);
                        DEB_ALWAYS() << pmsg;
                    }
                }
            }
            if(is_one_buffer_ready) goto _RETRY;
            else
            {
                printf("=== %s> WAITOBJ default ????\n", fnId);
                return pcoAcqWaitError;
            }
        }

    _WHILE_CONTINUE:
        m_sync->setAcqFrames(dwPcoFrameIdx * 2);
        dwPcoFrameIdx++;
    } // while(frameIdx ...

    if (dbgWaitobj)
    {
        m_cam->m_tmpLog->dumpPrint(true);
    } // true first .... last

    m_pcoData->traceAcq.msXfer = msElapsedTime(tStart);
    m_pcoData->traceAcq.endXferTimestamp = getTimestamp();

    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[EXIT]");

    return pcoAcqTransferEnd;

_EXIT_STOP:
    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[STOP REQUESTED]",
                                          "[EXIT]");
    DEB_TRACE() << DEB_VAR3(_nrStop, dwPcoFrameIdx, dwPcoRequestedFrames);

    m_pcoData->traceAcq.msXfer = msElapsedTime(tStart);
    m_pcoData->traceAcq.endXferTimestamp = getTimestamp();

    return pcoAcqTransferStop;
#else
    return -1;
#endif
}

//===================================================================================================================
//===================================================================================================================
int BufferCtrlObj::_xferImagMultDoubleImage()
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    int mode = m_cam->_pco_GetStorageMode_GetRecorderSubmode();
    bool continuous = (mode != RecSeq);

    enum
    {
        tracePco = 5,
        tracePcoAll = 6,
        traceLima = 7
    };

    m_pcoData->traceAcq.fnIdXfer = fnId;
    m_pcoData->traceAcq.usTicks[tracePco].desc =
        "PCO_GetImageEx only Double Image";
    m_pcoData->traceAcq.usTicks[tracePcoAll].desc =
        "PCO_GetImageEx / API total execTime";
    m_pcoData->traceAcq.usTicks[traceLima].desc =
        "xfer to lima / total execTime";

    DWORD dwPcoFrameIdx;
    int bufIdx;
    int error;
    DWORD dwPcoFrameIdxFirst, dwPcoFrameIdxLast;
    int maxWaitTimeout;
    m_cam->getAcqTimeoutRetry(maxWaitTimeout);
    WORD _wBitPerPixel;
    char *sErr;
    void *ptrLimaBuffer;
    void *ptrPcoBuffer;
    Sync::Status status;
    int iLimaRequestedFrames;
    DWORD dwPcoRequestedFrames;
    DWORD dwRequestedFramesMax = DWORD_MAX;
    unsigned int _uiBytesPerPixel;
    WORD _wArmWidth, _wArmHeight;
    int _iPcoAllocatedBuffNr;
    DWORD _dwPcoAllocatedBuffSize;
    SHORT sBufNr;
    WORD wSegment;
    int iPcoFrame, iLimaFrame;
    DWORD dwPcoFrameSize, dwLimaFrameSize;
    int _retStatus, _stopReq, _nrStop;
    int _newFrameReady = -1;

    WORD wDoubleImage;
    m_cam->_pco_GetDoubleImageMode(wDoubleImage, error);

    TIME_USEC tStart;
    msElapsedTimeSet(tStart);

    long long usStart, usStartPco;
    usElapsedTimeSet(usStart);
    usElapsedTimeSet(usStartPco);

    double msPerFrame = (m_cam->pcoGetCocRunTime() * 1000.);
    DWORD dwMsSleepOneFrame = (DWORD)(msPerFrame / 10.0); // 4/5 rounding
    if (dwMsSleepOneFrame == 0)
        dwMsSleepOneFrame = 1; // min sleep

    //_pcoAllocBuffers(true); // allocate 2 pco buff at max size

    m_cam->_pco_GetActiveRamSegment(wSegment, error);

    //------------------- nr of frames per buffer
    m_cam->getBitsPerPixel(_wBitPerPixel);
    m_cam->getBytesPerPixel(_uiBytesPerPixel);

    WORD _wMaxWidth, _wMaxHeight;
    m_cam->_pco_GetSizes(&_wArmWidth, &_wArmHeight, &_wMaxWidth, &_wMaxHeight,
                         error);
    _pcoAllocBuffersInfo(_iPcoAllocatedBuffNr, _dwPcoAllocatedBuffSize);

    // dwPcoFrameSize = (DWORD) _wRoiWidth * (DWORD) _wRoiHeight * (DWORD)
    // _uiBytesPerPixel;
    dwPcoFrameSize =
        (DWORD)_wArmWidth * (DWORD)_wArmHeight * (DWORD)_uiBytesPerPixel;

    dwLimaFrameSize = wDoubleImage ? dwPcoFrameSize / 2 : dwPcoFrameSize;

    // --------------- live video -> nr frames = 0 / idx lima buffers 32b
    // (0...ffff)
    m_sync->getNbFrames(iLimaRequestedFrames);
    dwPcoRequestedFrames = (iLimaRequestedFrames > 0)
                               ? (DWORD)iLimaRequestedFrames
                               : dwRequestedFramesMax;
    if (wDoubleImage)
        dwPcoRequestedFrames /= 2;

    DEB_ALWAYS() << m_cam->_sprintComment(
        false, fnId, "[PCO_GetImageEx - Double Image]", "[ENTRY]");

    DEB_TRACE() << "    "
                << DEB_VAR2(_iPcoAllocatedBuffNr, _dwPcoAllocatedBuffSize)
                << "\n"
                << "    " << DEB_VAR2(_wArmWidth, _wArmHeight) << "\n"
                << "    " << DEB_VAR2(_uiBytesPerPixel, _wBitPerPixel) << "\n"
                << "    " << DEB_VAR2(dwPcoFrameSize, dwLimaFrameSize) << "\n"
                << "    "
                << DEB_VAR2(iLimaRequestedFrames, dwPcoRequestedFrames);

    // lima frame nr is from 0 .... N-1
    // pco frame nr is from 1 .... N

    // --------------- loop - process the N frames
    dwPcoFrameIdx = 1;
    bufIdx = 0;

    DWORD _dwValidImageCnt, _dwMaxImageCnt;
    DWORD _dwValidImageCntLast = 0;

    m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwValidImageCnt,
                                           _dwMaxImageCnt, error);
    if (error)
    {
        printf("=== %s [%d]> ERROR\n", fnId, __LINE__);
        throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
    }

    if (m_cam->_getDebug(DBG_XFERMULT))
    {
        DEB_ALWAYS() << "\n     "
                     << DEB_VAR3(dwPcoRequestedFrames, _dwValidImageCnt,
                                 _dwMaxImageCnt);
    }

    // m_pcoData->traceAcq.nrImgRequested = dwRequestedFrames;
    m_pcoData->traceAcq.nrImgRequested = iLimaRequestedFrames;

    while (dwPcoFrameIdx <= dwPcoRequestedFrames)
    {
        bufIdx++;
        if (bufIdx >= _iPcoAllocatedBuffNr)
            bufIdx = 0;
        sBufNr = m_allocBuff.pcoAllocBufferNr[bufIdx];

        if (continuous)
        {
            dwPcoFrameIdxLast = dwPcoFrameIdxFirst = 0;
        }
        else
        {
            dwPcoFrameIdxLast = dwPcoFrameIdxFirst = dwPcoFrameIdx;
        }

        if (m_cam->_getDebug(DBG_XFERMULT1))
        {
            DEB_ALWAYS() << "\n     PCO_GetImageEx() ===>"
                         << "\n     "
                         << DEB_VAR4(dwPcoFrameIdx, dwPcoFrameIdxFirst,
                                     dwPcoFrameIdxLast, dwPcoRequestedFrames)
                         << "\n     "
                         << DEB_VAR5(wSegment, sBufNr, _wArmWidth, _wArmHeight,
                                     _wBitPerPixel);
        }

        usElapsedTimeSet(usStart);

        //========================================================================================
        //   PCO_GetImageEx has a fixex timeout of about 5s, so the call for
        //   long exp time fails included a waiting loop to chech when the image
        //   is ready
        //========================================================================================

        while (1)
        {
            if ((_dwValidImageCnt > 0) &&
                ((_dwValidImageCnt >= dwPcoFrameIdxFirst) ||
                 (dwPcoFrameIdxFirst == 0)))
                break;

            m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwValidImageCnt,
                                                   _dwMaxImageCnt, error);
            if (error)
            {
                DEB_ALWAYS() << "ERROR PCO_GetNumberOfImagesInSegment "
                             << DEB_VAR2(error, sErr);
                throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
            }

            if ((_dwValidImageCnt > 0) &&
                ((_dwValidImageCnt >= dwPcoFrameIdxFirst) ||
                 (dwPcoFrameIdxFirst == 0)))
                break;

            ::Sleep(dwMsSleepOneFrame); // sleep 1 frame
        }

        if (_dwValidImageCnt != _dwValidImageCntLast)
        {
            DEB_TRACE() << "=== _dwValidImageCnt != _dwValidImageCntLast "
                        << DEB_VAR3(_dwValidImageCnt, dwPcoFrameIdxFirst,
                                    _dwValidImageCntLast);
            _dwValidImageCntLast = _dwValidImageCnt;
        }

        usElapsedTimeSet(usStartPco);

        // --- image 1
        m_cam->_pco_GetImageEx(wSegment, dwPcoFrameIdxFirst, dwPcoFrameIdxLast,
                               sBufNr, _wArmWidth, _wArmHeight, _wBitPerPixel,
                               error);

        m_pcoData->traceAcq.usTicks[tracePco].value +=
            usElapsedTime(usStartPco);

        m_pcoData->traceAcq.usTicks[tracePcoAll].value +=
            usElapsedTime(usStart);
        usElapsedTimeSet(usStart);

        if (error)
        {
            DEB_ALWAYS() << "\n     PCO_GetImageEx() ===> "
                         << "\n     "
                         << DEB_VAR4(sErr, wSegment, dwPcoFrameIdxFirst,
                                     dwPcoFrameIdxLast)
                         << "\n     "
                         << DEB_VAR4(sBufNr, _wArmWidth, _wArmHeight,
                                     _wBitPerPixel);
        }

        ptrPcoBuffer = m_allocBuff.pcoAllocBufferPtr[bufIdx];

        iPcoFrame = dwPcoFrameIdx;
        iLimaFrame = wDoubleImage ? (iPcoFrame - 1) * 2 : (iPcoFrame - 1);

        ptrLimaBuffer = _getLimaBuffer(iLimaFrame, status);

        if (ptrLimaBuffer == NULL)
        {
            DEB_ALWAYS() << "\n     "
                         << DEB_VAR3(ptrLimaBuffer, iLimaFrame, status);
            THROW_HW_ERROR(NotSupported) << "Lima ptr = NULL";
        }

        if (m_cam->_getDebug(DBG_XFERMULT1))
        {
            DEB_ALWAYS() << "\n     "
                         << DEB_VAR4(ptrLimaBuffer, ptrPcoBuffer,
                                     dwLimaFrameSize, dwPcoFrameSize);
        }

        memcpy(ptrLimaBuffer, ptrPcoBuffer, dwLimaFrameSize);

        _setNewFrameReady(_newFrameReady = iLimaFrame);

        m_pcoData->traceAcq.nrImgAcquired = iLimaFrame;

        // ---

        // --- image

        ptrPcoBuffer = m_allocBuff.pcoAllocBufferPtr[bufIdx];

        iLimaFrame++;

        ptrLimaBuffer = _getLimaBuffer(iLimaFrame, status);
        ptrPcoBuffer = ((char *)ptrPcoBuffer) + dwLimaFrameSize;

        if (ptrLimaBuffer == NULL)
        {
            DEB_ALWAYS() << "\n     "
                         << DEB_VAR3(ptrLimaBuffer, iLimaFrame, status);
            THROW_HW_ERROR(NotSupported) << "Lima ptr = NULL";
        }

        if (m_cam->_getDebug(DBG_XFERMULT1))
        {
            DEB_ALWAYS() << "\n     "
                         << DEB_VAR4(ptrLimaBuffer, ptrPcoBuffer,
                                     dwLimaFrameSize, dwPcoFrameSize);
        }

        memcpy(ptrLimaBuffer, ptrPcoBuffer, dwLimaFrameSize);

        _setNewFrameReady(_newFrameReady = iLimaFrame);

        m_pcoData->traceAcq.nrImgAcquired = iLimaFrame;

        // ---

        if ((_stopReq = m_sync->_getRequestStop(_nrStop)) == stopRequest)
        {
            if (_nrStop > MAX_NR_STOP)
            {
                char msg[LEN_TRACEACQ_MSG + 1];
                snprintf(
                    msg, LEN_TRACEACQ_MSG,
                    "%s> STOP REQ (saving), framesReq[%d] frameReady[%d]\n",
                    fnId, iLimaRequestedFrames, _newFrameReady);
                m_pcoData->traceMsg(msg);
                break;
            }
        }

        m_sync->setAcqFrames(iLimaFrame);
        dwPcoFrameIdx++;

        m_pcoData->traceAcq.usTicks[traceLima].value += usElapsedTime(usStart);

    } // while(frameIdx ...

    // if(m_cam->_isCameraType(Edge)) {m_sync->setAcqFrames(dwPcoFrameIdx-1);}

    switch (_stopReq)
    {
            // case stopProcessing:
        case stopRequest:
            _retStatus = pcoAcqTransferStop;
            break;
        default:
            _retStatus = pcoAcqTransferEnd;
    }

    DEB_TRACE() << "\n>>> " << fnId << " [ EXIT]:\n"
                << "\n     " << DEB_VAR3(_retStatus, _stopReq, _newFrameReady);

    return _retStatus;
}

//===================================================================================================================
//===================================================================================================================
void BufferCtrlObj::_setNewFrameReady(int iLimaFrame)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;
    HwFrameInfoType frame_info;
    frame_info.acq_frame_nb = iLimaFrame;
    m_buffer_cb_mgr.newFrameReady(frame_info);
}
