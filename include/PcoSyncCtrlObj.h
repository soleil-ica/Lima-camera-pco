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
#ifndef PCOSYNCCTRLOBJ_H
#define PCOSYNCCTRLOBJ_H

#include "Pco.h"

#include "lima/HwSyncCtrlObj.h"
#include "lima/HwInterface.h"

enum pcoAcqStatus
{
    pcoAcqOK = 0,
    pcoAcqIdle,
    pcoAcqStart,
    pcoAcqRecordStart,
    pcoAcqRecordEnd,
    pcoAcqRecordStop,
    pcoAcqRecordTimeout,
    pcoAcqTransferStart,
    pcoAcqTransferEnd,
    pcoAcqStop,
    pcoAcqTransferStop,
    pcoAcqWaitTimeout,
    pcoAcqWaitError,
    pcoAcqError,
    pcoAcqPcoError,
};

namespace lima
{
    namespace Pco
    {
        class Camera;
        class BufferCtrlObj;

        class DLL_EXPORT SyncCtrlObj : public HwSyncCtrlObj
        {
            DEB_CLASS_NAMESPC(DebModCamera, "SyncCtrlObj", "Pco");

          public:
            SyncCtrlObj(Camera *, BufferCtrlObj *);
            virtual ~SyncCtrlObj();

            virtual bool checkTrigMode(TrigMode trig_mode);
            virtual void setTrigMode(TrigMode trig_mode);
            virtual void getTrigMode(TrigMode &trig_mode);

            virtual void setExpTime(double exp_time);
            virtual void getExpTime(double &exp_time);

            virtual void setLatTime(double lat_time);
            virtual void getLatTime(double &lat_time);

            virtual void setNbFrames(int nb_frames);
            virtual void getNbFrames(int &nb_frames);
            // these two functions calls the upper ones get/setNbFrames
            virtual void setNbHwFrames(int nb_frames);
            virtual void getNbHwFrames(int &nb_frames);

            void setAcqFrames(int nb_acq_frames)
            {
                AutoMutex lock(m_cond.mutex());
                m_nb_acq_frames = nb_acq_frames;
            }
            void getAcqFrames(int &nb_acq_frames)
            {
                AutoMutex lock(m_cond.mutex());
                nb_acq_frames = m_nb_acq_frames;
            }

            virtual void getValidRanges(ValidRangesType &valid_ranges);

            void startAcq();
            void stopAcq(bool clearQueue = true);

            void getStatus(HwInterface::StatusType &);

            void xlatLimaTrigMode2Pco(lima::TrigMode limaTrigMode,
                                      WORD &pcoTrigMode, WORD &pcoAcqMode,
                                      bool &extTrig, int &err);

            // BufferCtrlObj* _getBufferCtrlObj() {return m_buffer;}
            void setExposing(pcoAcqStatus exposing);
            void setStarted(bool started);
            bool getStarted();
            pcoAcqStatus getExposing();

            int _getRequestStop(int &nrStop);
            void _setRequestStop(int requestStop);

          private:
            double m_exp_time; /* exposure time in s */
            double m_lat_time; /* lattency - delay? */

            Camera *m_cam;
            HANDLE &m_handle;
            TrigMode m_trig_mode;
            BufferCtrlObj *m_buffer;
            int m_nb_frames;
            int m_nb_acq_frames;
            bool m_started;
            pcoAcqStatus m_exposing;
            struct stcPcoData *m_pcoData;
            Cond m_cond;
            int m_requestStop, m_requestStopRetry;

            bool m_extTrigSingle_eq_Multi;
        };

    } // namespace Pco
} // namespace lima

#endif // PCOSYNCCTRLOBJ_H
