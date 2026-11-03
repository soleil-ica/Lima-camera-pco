#include "PcoCamera.h"
#include "PcoBufferCtrlObj.h"
#include "PcoSyncCtrlObj.h"
#include "lima/Exceptions.h"
#include <exception>

using namespace lima;
using namespace lima::Pco;

namespace
{
/** @brief Identify the calling thread without an unexported LimaCore symbol on Windows. */
pid_t getRingBufferThreadId()
{
#if defined(_WIN32)
    return ::GetCurrentThreadId();
#else
    return lima::GetThreadID();
#endif
}
}

/**
 * @brief Enable or disable Dimax ring mode while idle, clearing the previous fault.
 * Enabling requires Windows and a registered export controller.
 */
void Camera::setRingBufferEnabled(bool enabled)
{
    DEB_MEMBER_FUNCT();
    if (enabled && !_isCameraType(Dimax))
        THROW_HW_ERROR(NotSupported) << "Ring buffer export is only supported on Dimax";
#ifdef __linux__
    if (enabled)
        THROW_HW_ERROR(NotSupported) << "Dimax ring buffer export requires the Windows PCO SDK";
#endif
    if (!isRingBufferBusy() && !m_sync->getStarted())
        cancelRingBufferPreparation();
    AutoMutex lock(m_ringBuffer.cond.mutex());
    if (m_ringBuffer.worker || m_ringBuffer.prepared || m_sync->getStarted())
        THROW_HW_ERROR(Error) << "Cannot change ring buffer mode during a session";
    if (enabled && !m_ringBuffer.control)
        THROW_HW_ERROR(Error) << "No PCO ring buffer export controller registered";
    m_ringBuffer.enabled = enabled;
    m_ringBuffer.fault = false;
    m_ringBuffer.status = enabled ? "Ring buffer enabled" : "Ring buffer disabled";
}

/** @brief Return the configured ring-mode flag, independently of session activity. */
bool Camera::getRingBufferEnabled()
{
    AutoMutex lock(m_ringBuffer.cond.mutex());
    return m_ringBuffer.enabled;
}

/** @brief Report whether the worker is active, including export and finalization. */
bool Camera::isRingBufferBusy()
{
    AutoMutex lock(m_ringBuffer.cond.mutex());
    return m_ringBuffer.worker;
}

/** @brief Return the ring-session fault flag maintained under the state mutex. */
bool Camera::hasRingBufferFault()
{
    AutoMutex lock(m_ringBuffer.cond.mutex());
    return m_ringBuffer.fault;
}

/** @brief Return a synchronized snapshot of the ring-session status text. */
std::string Camera::getRingBufferStatus()
{
    AutoMutex lock(m_ringBuffer.cond.mutex());
    return m_ringBuffer.status;
}

/** @brief Update the status text shared by the worker and device status reader. */
void Camera::setRingBufferStatus(const std::string& status)
{
    AutoMutex lock(m_ringBuffer.cond.mutex());
    m_ringBuffer.status = status;
}

/**
 * @brief Register a non-owned export controller while no session is prepared or running.
 * Replacing a different registered controller requires releasing it first.
 */
void Camera::setRingBufferControl(RingBufferControl* control)
{
    DEB_MEMBER_FUNCT();
    AutoMutex lock(m_ringBuffer.cond.mutex());
    if (m_ringBuffer.worker || m_ringBuffer.prepared || m_sync->getStarted())
        THROW_HW_ERROR(Error) << "Cannot replace an active ring buffer controller";
    if (m_ringBuffer.control && m_ringBuffer.control != control)
        THROW_HW_ERROR(Error) << "A PCO ring buffer controller is already registered";
    m_ringBuffer.control = control;
}

/**
 * @brief Stop and wait for the matching controller's session before detaching it.
 * The controller remains caller-owned; ring mode is disabled on release.
 */
void Camera::releaseRingBufferControl(RingBufferControl* control)
{
    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        if (m_ringBuffer.control != control)
            return;
    }
    try
    {
        m_sync->stopAcq();
        cancelRingBufferPreparation();
    }
    catch (...)
    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        m_ringBuffer.control = 0;
        m_ringBuffer.enabled = false;
        throw;
    }
    AutoMutex lock(m_ringBuffer.cond.mutex());
    while (m_ringBuffer.worker)
        m_ringBuffer.cond.wait();
    m_ringBuffer.control = 0;
    m_ringBuffer.enabled = false;
}

/**
 * @brief Validate ring settings and prepare the controller with the finite history.
 * Requires EXTERNAL_SINGLE, positive history, no forced FIFO and even Double Image count.
 * Does nothing when ring mode is disabled; recording is not started here.
 */
void Camera::prepareRingBuffer()
{
    DEB_MEMBER_FUNCT();
    RingBufferControl* control;
    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        if (!m_ringBuffer.enabled)
            return;
        if (m_ringBuffer.worker || m_sync->getStarted())
            THROW_HW_ERROR(Error) << "Previous ring buffer session is not finished";
        // A generic preparation may have failed after our hardware hook.
        m_ringBuffer.prepared = false;
        control = m_ringBuffer.control;
        m_ringBuffer.fault = false;
    }
    if (!control)
        THROW_HW_ERROR(Error) << "No PCO ring buffer export controller registered";
    if (m_pco_buffer_nrevents <= 0 || m_pco_buffer_nrevents > PCO_MAX_NR_ALLOCATED_BUFFERS)
        THROW_HW_ERROR(Error) << "Invalid number of SDK ring buffer transfer buffers";
    TrigMode trigger;
    m_sync->getTrigMode(trigger);
    int history, forcedFifo;
    m_sync->getNbFrames(history);
    getRecorderForcedFifo(forcedFifo);
    if (trigger != ExtTrigSingle || history <= 0 || forcedFifo)
        THROW_HW_ERROR(InvalidValue)
            << "Ring buffer requires Snap, nbFrames > 0, EXTERNAL_SINGLE and forcedFIFOMode=false";
    int error;
    WORD doubleImage;
    _pco_GetDoubleImageMode(doubleImage, error);
    PCO_THROW_OR_TRACE(error, "PCO_GetDoubleImageMode");
    if (doubleImage && history % 2)
        THROW_HW_ERROR(InvalidValue) << "Double Image requires an even nbFrames";
    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        m_ringBuffer.history = history;
        m_ringBuffer.prepared = true;
    }
    try
    {
        control->prepare(history);
    }
    catch (const std::exception& error)
    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        m_ringBuffer.prepared = false;
        THROW_HW_ERROR(Error) << error.what();
    }
    catch (...)
    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        m_ringBuffer.prepared = false;
        throw;
    }
    setRingBufferStatus("Ring buffer prepared");
}

/**
 * @brief Undo a prepared session that has no worker, including a failed start.
 * Stop SDK recording/transfers and notify the controller; cleanup errors are reported.
 */
void Camera::cancelRingBufferPreparation()
{
    DEB_MEMBER_FUNCT();
    RingBufferControl* control = 0;
    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        if (m_ringBuffer.prepared && !m_ringBuffer.worker)
        {
            m_ringBuffer.prepared = false;
            control = m_ringBuffer.control;
        }
    }
    if (!control)
        return;
    std::string failure;
#ifndef __linux__
    try
    {
        int error;
        _pco_SetRecordingState(0, error);
        PCO_THROW_OR_TRACE(error, "PCO_SetRecordingState cancelling preparation/start");
        error = PCO_CancelImages(m_handle);
        PCO_THROW_OR_TRACE(error, "PCO_CancelImages cancelling preparation/start");
    }
    catch (const Exception& error)
    {
        failure = error.getErrMsg();
        DEB_ERROR() << failure;
        AutoMutex lock(m_ringBuffer.cond.mutex());
        m_ringBuffer.fault = true;
        m_ringBuffer.status = failure;
    }
#endif
    control->completed(true, failure);
    if (!failure.empty())
        THROW_HW_ERROR(Error) << failure;
}

/**
 * @brief Detect reentrant hardware stop calls from the worker's completion callback.
 * Allows stopAcq to avoid waiting for the calling worker to finish itself.
 */
bool Camera::isRingBufferCompletionThread()
{
    AutoMutex lock(m_ringBuffer.cond.mutex());
    return m_ringBuffer.worker && m_ringBuffer.completionThread == getRingBufferThreadId();
}

/**
 * @brief Run live preview, then private RAM export unless the session was cancelled.
 * Handle SDK/export errors, complete the controller and publish the terminal status
 * before releasing threads waiting for the worker to finish.
 */
void Camera::runRingBuffer()
{
    DEB_MEMBER_FUNCT();
    RingBufferControl* control;
    int history;
    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        control = m_ringBuffer.control;
        history = m_ringBuffer.history;
    }
    bool cancelled = false;
    std::string failure;
    try
    {
        m_sync->setExposing(pcoAcqRecordStart);
        m_sync->setAcqFrames(0);
        setRingBufferStatus("Recording circular history; live preview is not saved");
        cancelled = m_buffer->_ringBufferPreview();
#ifndef __linux__
        int error;
        _pco_SetRecordingState(0, error);
        PCO_THROW_OR_TRACE(error, "PCO_SetRecordingState");
        error = PCO_CancelImages(m_handle);
        PCO_THROW_OR_TRACE(error, "PCO_CancelImages");
#endif
        int retries;
        cancelled = cancelled || m_sync->_getRequestStop(retries) == stopRequest;
        if (!cancelled)
            cancelled = m_buffer->_ringBufferExport(*control, history);
    }
    catch (const Exception& error)
    {
        failure = error.getErrMsg();
    }
    catch (const std::exception& error)
    {
        failure = error.what();
    }
    catch (...)
    {
        failure = "Unknown exception in PCO ring buffer acquisition";
    }

    if (!failure.empty())
    {
        DEB_ERROR() << failure;
#ifndef __linux__
        try
        {
            int error;
            _pco_SetRecordingState(0, error);
            PCO_THROW_OR_TRACE(error, "PCO_SetRecordingState during error cleanup");
            error = PCO_CancelImages(m_handle);
            PCO_THROW_OR_TRACE(error, "PCO_CancelImages during error cleanup");
        }
        catch (const Exception& error)
        {
            failure += "; cleanup: " + error.getErrMsg();
            DEB_ERROR() << failure;
        }
#endif
    }

    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        // CtControl::stopAcq may reenter the hardware stop from this callback.
        m_ringBuffer.completionThread = getRingBufferThreadId();
    }
    try
    {
        control->completed(cancelled, failure);
    }
    catch (const Exception& error)
    {
        failure += "; ring buffer completion: " + error.getErrMsg();
        DEB_ERROR() << failure;
    }
    catch (const std::exception& error)
    {
        failure += std::string("; ring buffer completion: ") + error.what();
        DEB_ERROR() << failure;
    }
    catch (...)
    {
        failure += "; ring buffer completion failed";
        DEB_ERROR() << failure;
    }
    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        m_ringBuffer.fault = !failure.empty();
        if (!failure.empty())
            m_ringBuffer.status = failure;
        else if (cancelled)
            m_ringBuffer.status = "Ring buffer cancelled; any partial file is incomplete";
        else
            m_ringBuffer.status = "Ring buffer export completed";
        m_ringBuffer.prepared = false;
    }
    if (!failure.empty())
    {
        try
        {
            m_HwEventCtrlObj->reportEvent(
                new Event(Hardware, Event::Error, Event::Camera, Event::CamFault, failure));
        }
        catch (const Exception& error)
        {
            DEB_ERROR() << "Cannot report ring buffer fault: " << error.getErrMsg();
        }
        catch (const std::exception& error)
        {
            DEB_ERROR() << "Cannot report ring buffer fault: " << error.what();
        }
        catch (...)
        {
            DEB_ERROR() << "Cannot report ring buffer fault: unknown callback exception";
        }
    }
    {
        AutoMutex lock(m_ringBuffer.cond.mutex());
        m_sync->setExposing(failure.empty() ? pcoAcqTransferEnd : pcoAcqError);
        m_sync->setStarted(false);
        m_ringBuffer.worker = false;
        m_ringBuffer.completionThread = 0;
        m_ringBuffer.cond.broadcast();
    }
}
