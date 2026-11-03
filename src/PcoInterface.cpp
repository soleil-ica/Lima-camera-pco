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

#include "PcoInterface.h"
#include "PcoCamera.h"
#include "PcoDetInfoCtrlObj.h"
#include "PcoBufferCtrlObj.h"
#include "PcoSyncCtrlObj.h"
#include "PcoRoiCtrlObj.h"
#include "PcoBinCtrlObj.h"
#include "PcoHwEventCtrlObj.h"

using namespace lima;
using namespace lima::Pco;

//=========================================================================================================
const char *_timestamp_pcointerface()
{
    return ID_FILE_TIMESTAMP;
}
//=========================================================================================================

//=========================================================================================================
//=========================================================================================================
Interface::Interface(Camera *cam) : m_cam(cam)
{
    DEB_CONSTRUCTOR();

    // cam->m_HwEventCtrlObj = m_HwEventCtrlObj = new PcoHwEventCtrlObj(cam);
    cam->m_HwEventCtrlObj = m_HwEventCtrlObj = new PcoHwEventCtrlObj();

    m_BinCtrlObj = new BinCtrlObj(*cam);
    m_RoiCtrlObj = new RoiCtrlObj(cam);
    m_det_info = new DetInfoCtrlObj(cam);

    cam->m_buffer = m_buffer = new BufferCtrlObj(cam);

    cam->m_sync = m_sync = new SyncCtrlObj(cam, m_buffer);

    DEB_TRACE() << DEB_VAR5(cam, m_buffer, m_sync, m_det_info, m_RoiCtrlObj);

    if (m_buffer)
    {
        m_buffer->m_sync = m_sync;
    }
}

//=========================================================================================================
//=========================================================================================================

Interface::~Interface()
{
    DEB_DESTRUCTOR();
    delete m_HwEventCtrlObj;
    delete m_RoiCtrlObj;
    delete m_BinCtrlObj;
    delete m_buffer;
    delete m_det_info;
    delete m_sync;
}

//=========================================================================================================
//=========================================================================================================
void Interface::getCapList(CapList &cap_list) const
{
    cap_list.push_back(HwCap(m_HwEventCtrlObj));
    cap_list.push_back(HwCap(m_RoiCtrlObj));
    cap_list.push_back(HwCap(m_BinCtrlObj));
    cap_list.push_back(HwCap(m_sync));
    cap_list.push_back(HwCap(m_det_info));
    cap_list.push_back(HwCap(m_buffer));
}

//=========================================================================================================
//=========================================================================================================
void Interface::reset(ResetLevel reset_level)
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    int intLevel = reset_level;

    m_cam->_setActionTimestamp(tsReset);

    DEB_TRACE() << fnId << ": " DEB_VAR2(reset_level, intLevel);

    m_sync->stopAcq();
    m_cam->reset(intLevel);
}

//=========================================================================================================
//=========================================================================================================
void Interface::prepareAcq()
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[ENTRY]");

    m_cam->_setActionTimestamp(tsPrepareAcq);

    m_cam->prepareRingBuffer();
    try
    {
        if (m_buffer)
            m_buffer->prepareAcq();
        m_cam->prepareAcq();
    }
    catch (...)
    {
        m_cam->cancelRingBufferPreparation();
        throw;
    }
}

//=========================================================================================================
//=========================================================================================================
void Interface::startAcq()
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[ENTRY]");

    m_cam->_setActionTimestamp(tsStartAcq);

    if (m_buffer)
        m_buffer->getBuffer().setStartTimestamp(Timestamp::now());
    m_sync->startAcq();
}

//=========================================================================================================
//=========================================================================================================
void Interface::stopAcq()
{
    DEB_MEMBER_FUNCT();
    DEF_FNID;

    DEB_ALWAYS() << m_cam->_sprintComment(false, fnId, "[ENTRY]");

    m_cam->_setActionTimestamp(tsStopAcq);
    m_sync->stopAcq();
}

//=========================================================================================================
//=========================================================================================================
void Interface::getStatus(StatusType &status)
{
    DEB_MEMBER_FUNCT();

#ifndef __linux__
    if (m_cam->_isConfig())
    {
        status.acq = AcqConfig;
        status.det = DetIdle;
    }
    else
    {
        m_sync->getStatus(status);
    }

#else

    Camera::Status _status = Camera::Ready;
    m_cam->getStatus(_status);
    switch (_status)
    {
        case Camera::Fault:
            status.set(HwInterface::StatusType::Fault); // 0
            break;
        case Camera::Ready:
            status.set(HwInterface::StatusType::Ready); // 1
            break;
        case Camera::Exposure:
            status.set(HwInterface::StatusType::Exposure); // 2
            break;
        case Camera::Readout:
            status.set(HwInterface::StatusType::Readout); // 3
            break;
        case Camera::Latency:
            status.set(HwInterface::StatusType::Latency); // 4
            break;
        case Camera::Config:
            status.set(HwInterface::StatusType::Config); // 5
            break;
    }

#endif

    // DEB_RETURN() << DEB_VAR1(status);
}

//=========================================================================================================
//=========================================================================================================
int Interface::getNbAcquiredFrames()
{
    DEB_MEMBER_FUNCT();

    int aNbAcquiredFrames = getNbHwAcquiredFrames();

    DEB_RETURN() << DEB_VAR1(aNbAcquiredFrames);
    return aNbAcquiredFrames;
}

//=========================================================================================================
//=========================================================================================================
int Interface::getNbHwAcquiredFrames()
{
    int nb_acq_frames;
    m_sync->getAcqFrames(nb_acq_frames);
    return nb_acq_frames;
}
