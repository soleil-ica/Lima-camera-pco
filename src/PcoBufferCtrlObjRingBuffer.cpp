#include "PcoCamera.h"
#include "PcoBufferCtrlObj.h"
#include "PcoSyncCtrlObj.h"
#include "lima/Exceptions.h"
#include <algorithm>
#include <climits>
#include <cstring>

using namespace lima;
using namespace lima::Pco;

/**
 * @brief Publish lossy live previews to LImA while the camera records in its ring.
 * @return True on user/callback cancellation, false on hardware recording stop.
 * SDK failures and transfer timeouts raise an exception.
 */
bool BufferCtrlObj::_ringBufferPreview()
{
    DEB_MEMBER_FUNCT();
#ifdef __linux__
    THROW_HW_ERROR(NotSupported) << "Ring buffer preview requires the Windows SDK";
#else
    m_sync = m_cam->_getSyncCtrlObj();
    int error, retries;
    WORD width, height, maxWidth, maxHeight, doubleImage;
    m_cam->_pco_GetSizes(&width, &height, &maxWidth, &maxHeight, error);
    PCO_THROW_OR_TRACE(error, "PCO_GetSizes");
    m_cam->_pco_GetDoubleImageMode(doubleImage, error);
    PCO_THROW_OR_TRACE(error, "PCO_GetDoubleImageMode");
    const int imagesPerRead = doubleImage ? 2 : 1;
    FrameDim dim;
    getFrameDim(dim);
    const size_t bytes = dim.getMemSize();
    if (dim.getImageType() != Bpp16 ||
        size_t(width) * height * sizeof(WORD) != bytes * imagesPerRead ||
        bytes * imagesPerRead > m_allocBuff.dwPcoAllocBufferSize[0])
        THROW_HW_ERROR(Error) << "Inconsistent SDK/LImA ring buffer image dimensions";
    const SHORT buffer = m_allocBuff.pcoAllocBufferNr[0];
    HANDLE event = m_allocBuff.bufferAllocEvent[0];
    if (buffer < 0 || !event || !m_allocBuff.pcoAllocBufferPtr[0])
        THROW_HW_ERROR(Error) << "No allocated SDK ring preview buffer";
    int published = 0;
    WORD bits;
    m_cam->getBitsPerPixel(bits);
    const double timeout = (std::max)(5., m_pcoData->cocRunTime + 5.);
    for (;;)
    {
        if (m_sync->_getRequestStop(retries) == stopRequest)
            return true;
        WORD recording = m_cam->_pco_GetRecordingState(error);
        PCO_THROW_OR_TRACE(error, "PCO_GetRecordingState");
        if (!recording)
            return false;
        if (!ResetEvent(event))
            THROW_HW_ERROR(Error) << "Cannot reset PCO ring preview event: " << GetLastError();
        error = PCO_AddBufferEx(m_cam->m_handle, 0, 0, buffer, width, height, bits);
        if (static_cast<DWORD>(error) == PCO_ERROR_SDKDLL_RECORDINGMUSTBEON)
        {
            int stateError;
            recording = m_cam->_pco_GetRecordingState(stateError);
            PCO_THROW_OR_TRACE(stateError, "PCO_GetRecordingState after preview request");
            if (!recording)
            {
                DEB_TRACE() << "Preview request raced with the hardware recording stop";
                return false;
            }
        }
        PCO_THROW_OR_TRACE(error, "PCO_AddBufferEx (ring preview)");
        const Timestamp start = Timestamp::now();
        for (;;)
        {
            if (m_sync->_getRequestStop(retries) == stopRequest)
                return true;
            recording = m_cam->_pco_GetRecordingState(error);
            PCO_THROW_OR_TRACE(error, "PCO_GetRecordingState");
            if (!recording)
                return false;
            DWORD wait = WaitForSingleObject(event, 20);
            if (wait == WAIT_OBJECT_0)
                break;
            if (wait != WAIT_TIMEOUT)
                THROW_HW_ERROR(Error) << "PCO preview buffer event wait failed: " << GetLastError();
            if (double(Timestamp::now() - start) > timeout)
                THROW_HW_ERROR(Error) << "Timeout waiting for a PCO ring preview image";
        }
        DWORD dllStatus = 0, driverStatus = 0;
        error = PCO_GetBufferStatus(m_cam->m_handle, buffer, &dllStatus, &driverStatus);
        PCO_THROW_OR_TRACE(error, "PCO_GetBufferStatus");
        if (m_sync->_getRequestStop(retries) == stopRequest)
            return true;
        recording = m_cam->_pco_GetRecordingState(error);
        PCO_THROW_OR_TRACE(error, "PCO_GetRecordingState after preview transfer");
        if (!recording &&
            (!driverStatus || driverStatus == PCO_ERROR_DRIVER_BUFFER_CANCELLED))
        {
            DEB_TRACE() << "Discarding preview buffer at hardware recording stop";
            return false;
        }
        PCO_THROW_OR_TRACE(driverStatus, "PCO preview buffer driver status");
        if (!(dllStatus & 0x8000))
            THROW_HW_ERROR(Error) << "PCO preview event without a completed buffer";
        if (m_sync->_getRequestStop(retries) == stopRequest)
            return true;
        for (int part = 0; part < imagesPerRead; ++part)
        {
            if (published == INT_MAX)
                THROW_HW_ERROR(Error) << "LImA preview frame counter limit reached";
            {
                AutoMutex lock(cond.mutex());
                const Sync::Status status = m_bufferSync->wait(published, 0.001);
                if (status == Sync::TIMEOUT)
                    continue; // Preview is deliberately lossy when processing cannot keep up.
                if (status != Sync::AVAILABLE)
                    THROW_HW_ERROR(Error) << "LImA ring preview buffer wait interrupted";
            }
            void* target = m_buffer_cb_mgr.getFrameBufferPtr(published);
            if (!target)
                THROW_HW_ERROR(Error) << "No LImA ring preview buffer";
            memcpy(target, reinterpret_cast<char*>(m_allocBuff.pcoAllocBufferPtr[0]) +
                           bytes * part, bytes);
            HwFrameInfoType frame;
            frame.acq_frame_nb = published++;
            m_sync->setAcqFrames(published);
            if (!m_buffer_cb_mgr.newFrameReady(frame))
            {
                m_sync->_setRequestStop(stopRequest);
                return true;
            }
        }
        // Bound preview processing, independently of the hardware recording rate.
        for (int i = 0; i < 5; ++i)
        {
            if (m_sync->_getRequestStop(retries) == stopRequest)
                return true;
            m_cam->Sleep_ms(20);
        }
    }
#endif
}

/**
 * @brief Wait for stopped RAM to settle, then export its latest history privately.
 * @param control Synchronous sink consuming SDK pixels without LImA publication.
 * @param history Maximum number of individual images.
 * RAM indices are assumed chronological; no frame is sent to newFrameReady.
 * @return True on cancellation (possibly a partial file), false on completed export.
 * SDK and sink errors propagate to the acquisition worker.
 */
bool BufferCtrlObj::_ringBufferExport(RingBufferControl& control, int history)
{
    DEB_MEMBER_FUNCT();
#ifdef __linux__
    THROW_HW_ERROR(NotSupported) << "Ring buffer export requires the Windows SDK";
#else
    int error, retries;
    const WORD segment = m_pcoData->wActiveRamSegment;
    DWORD valid = 0, maximum = 0, previous = DWORD_MAX;
    int stable = 0;
    m_cam->setRingBufferStatus("Waiting for the final camera RAM image");
    const Timestamp start = Timestamp::now();
    const double timeout = (std::max)(5., m_pcoData->cocRunTime + 5.);
    while (stable < 3)
    {
        if (m_sync->_getRequestStop(retries) == stopRequest)
            return true;
        const WORD recording = m_cam->_pco_GetRecordingState(error);
        PCO_THROW_OR_TRACE(error, "PCO_GetRecordingState");
        m_cam->_pco_GetNumberOfImagesInSegment(segment, valid, maximum, error);
        PCO_THROW_OR_TRACE(error, "PCO_GetNumberOfImagesInSegment");
        // The SDK busy flag reports trigger readiness, not image completion.
        stable = !recording && valid == previous ? stable + 1 : 0;
        previous = valid;
        const double elapsed = double(Timestamp::now() - start);
        if (elapsed > timeout)
            THROW_HW_ERROR(Error)
                << "Camera RAM did not settle after the recording stop"
                << " (elapsed=" << elapsed << " s, timeout=" << timeout << " s"
                << ", recording=" << recording << ", segment=" << segment
                << ", valid=" << valid << ", maximum=" << maximum
                << ", stable=" << stable << "/3)";
        m_cam->Sleep_ms(20);
    }
    RingBufferInfo info;
    FrameDim dim;
    getFrameDim(dim);
    info.width = dim.getSize().getWidth();
    info.height = dim.getSize().getHeight();
    WORD doubleImage, timestamp;
    m_cam->_pco_GetDoubleImageMode(doubleImage, error);
    PCO_THROW_OR_TRACE(error, "PCO_GetDoubleImageMode");
    m_cam->_pco_GetTimestampMode(timestamp, error);
    PCO_THROW_OR_TRACE(error, "PCO_GetTimestampMode");
    error = m_cam->_pco_GetBitAlignment(info.bitAlignment);
    PCO_THROW_OR_TRACE(error, "PCO_GetBitAlignment");
    info.doubleImage = doubleImage != 0;
    info.timestampMode = timestamp;
    info.dynamicBits = m_pcoData->stcPcoDescription.wDynResDESC;
    info.requestedFrames = history;
    const int imagesPerRead = doubleImage ? 2 : 1;
    const unsigned long long available = static_cast<unsigned long long>(valid) * imagesPerRead;
    info.frames = static_cast<int>((std::min)(static_cast<unsigned long long>(history), available));
    m_sync->getExpTime(info.exposure);
    m_sync->getLatTime(info.latency);
    info.framePeriod = m_pcoData->cocRunTime;
    const DWORD ramFrames = info.frames / imagesPerRead;
    const DWORD first = valid - ramFrames + 1;
    WORD width, height, maxWidth, maxHeight;
    m_cam->_pco_GetSizes(&width, &height, &maxWidth, &maxHeight, error);
    PCO_THROW_OR_TRACE(error, "PCO_GetSizes");
    error = PCO_SetImageParameters(m_cam->m_handle, width, height,
                                  IMAGEPARAMETERS_READ_FROM_SEGMENTS, NULL, 0);
    PCO_THROW_OR_TRACE(error, "PCO_SetImageParameters (RAM export)");
    if (m_sync->_getRequestStop(retries) == stopRequest)
        return true;
    m_cam->setRingBufferStatus("Saving camera RAM to private NeXus file");
    control.beginExport(info);
    bool cancelled = false;
    int written = 0;
    for (DWORD offset = 0; offset < ramFrames; ++offset)
    {
        if (m_sync->_getRequestStop(retries) == stopRequest)
        {
            cancelled = true;
            break;
        }
        const DWORD index = first + offset;
        error = PCO_GetImageEx(m_cam->m_handle, segment, index, index,
                              m_allocBuff.pcoAllocBufferNr[0], width, height,
                              static_cast<WORD>(info.dynamicBits));
        PCO_THROW_OR_TRACE(error, "PCO_GetImageEx (private RAM export)");
        for (int part = 0; part < imagesPerRead; ++part)
        {
            if (m_sync->_getRequestStop(retries) == stopRequest)
            {
                cancelled = true;
                break;
            }
            WORD* pixels = m_allocBuff.pcoAllocBufferPtr[0] +
                           size_t(info.width) * info.height * part;
            control.writeFrame(pixels, written++, index);
        }
        if (cancelled)
            break;
    }
    cancelled = cancelled || m_sync->_getRequestStop(retries) == stopRequest;
    m_cam->setRingBufferStatus("Finalizing private NeXus file");
    control.endExport(cancelled);
    return cancelled;
#endif
}
