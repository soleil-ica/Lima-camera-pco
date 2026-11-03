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
#ifndef PCOCAMERA_H
#define PCOCAMERA_H
#include "Pco.h"
#include "PCO_errt.h"
#include "lima/Debug.h"
#include "lima/Constants.h"
#include "lima/HwMaxImageSizeCallback.h"
#include "lima/HwInterface.h"
#include "PcoRingBuffer.h"

#define LIMA_PCO_RING_BUFFER_API_VERSION 1

//#include "PcoBufferCtrlObj.h"
#include "PcoHwEventCtrlObj.h"

#ifdef __linux__
//---- linux sdk [begin]
#    include "../sdkPco/include/sc2_SDKStructures.h"

#    include "Cpco_com.h"
#    include "Cpco_com_func.h"

#    include "Cpco_com_cl_me4.h"
#    include "Cpco_grab_cl_me4.h"
#    include "file12.h"
#    include "sc2_telegram.h"
//---- linux sdk [end]
#endif

// --------------------- cam utils

#define CAMINFO_ALL 0xffffffffffffffffLL

#define CAMINFO_BLOCK (0x1LL << 0)
#define CAMINFO_UNSORTED (0x1LL << 1)
#define CAMINFO_LOG (0x1LL << 2)
#define CAMINFO_ACQ (0x1LL << 3)

#define CAMINFO_PIXELRATE (0x1LL << 8)
#define CAMINFO_ADC (0x1LL << 9)
#define CAMINFO_FIRMWARE (0x1LL << 10)
#define CAMINFO_GENERAL (0x1LL << 11)
#define CAMINFO_VERSION (0x1LL << 12)
#define CAMINFO_DIMAX (0x1LL << 13)
#define CAMINFO_EXP (0x1LL << 14)
#define CAMINFO_ROI (0x1LL << 15)

#define CAMINFO_CAMERALINK (0x1LL << 16)
#define CAMINFO_CAMERATYPE (0x1LL << 17)
// ---------------------

#define RESET_CLOSE_INTERFACE 100
#define DISABLE_ACQ_ENBL_SIGNAL

#define BUFF_VERSION 2048

#define MAX_NR_STOP 0

#define NANO (1.0E-9)
#define MICRO (1.0E-6)
#define MILI (1.0E-3)

#define LEN_DESCRIPTION_NAME 64
#define LEN_BLA 1023
#define LEN_TALK 5000

//--------------------------------------- bits
#define BIT3 (0x00000001 << 3)
#define BIT8 (0x00000001 << 8)

//--------------------------------------- debug const for talk
#define DBG_BUFF 0x00000001
#define DBG_XFER2LIMA 0x00000002
#define DBG_LIMABUFF 0x00000004
#define DBG_EXP 0x00000008

#define DBG_XFERMULT 0x00000010
#define DBG_XFERMULT1 0x00000020
#define DBG_ASSIGN_BUFF 0x00000040
#define DBG_STATUS 0x00000080

#define DBG_DUMMY_IMG 0x00000100
#define DBG_WAITOBJ 0x00000200
#define DBG_XFER_IMG 0x00000400
#define DBG_TRACE_FIFO 0x00000800

#define DBG_ROI 0x00001000

#define DBG_FN1 0x00010000
#define DBG_FN2 0x00020000
#define DBG_FN3 0x00040000
#define DBG_FN4 0x00080000

//---------------------------------------

//--------------------------------------- test cmd mode
#define TESTCMDMODE_DIMAX_XFERMULTI                                            \
    (0x00000001 << 0) // _pco_acq_thread_dimax dimax: xferMulti or xfer
#define TESTCMDMODE_EDGE_XFER (0x00000001 << 1) // change EDGE xfer
#define TESTCMDMODE_2 (0x00000001 << 2)
#define TESTCMDMODE_3 (0x00000001 << 3)
#define TESTCMDMODE_4 (0x00000001 << 4)
#define TESTCMDMODE_PCO2K_XFER_WAITOBJ                                         \
    (0x00000001 << 5) // _pco_acq_thread_dimax dimax: xferMulti or xfer
#define TESTCMDMODE_6 (0x00000001 << 6)
#define TESTCMDMODE_7 (0x00000001 << 7)

#define TESTCMDMODE_8 (0x00000001 << 8)

//---------------------------------------

//--------------------------------------- camera state
#define CAMSTATE_RECORD_STATE (0x00000001 << 0) // record state

//---------------------------------------
//---------------------------------------

#define ALWAYS_NL "\n===\n"
#define KILOBYTE (1024LL)
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * MEGABYTE)

#define PCO_EDGE_PIXEL_RATE_MIN 95000000
#define PCO_EDGE_PIXEL_RATE_MAX 286000000
#define PCO_EDGE_PIXEL_RATE_LOW 100000000
#define PCO_EDGE_PIXEL_RATE_HIGH 286000000
#define PCO_EDGE_WIDTH_HIGH 1920
#define PCO_EDGE_LUT_SQRT 0x1612
#define PCO_EDGE_LUT_NONE 0

#define PCO_CL_BAUDRATE_115K2 115200

#define PCO_BUFFER_NREVENTS 4
struct stcXlatCode2Str
{
    int code;
    const char *str;
};

struct stcSegmentInfo
{
    int iSegId;
    int iErr;
    WORD wXRes;
    WORD wYRes;
    WORD wBinHorz;
    WORD wBinVert;
    WORD wRoiX0;
    WORD wRoiY0;
    WORD wRoiX1;
    WORD wRoiY1;
    DWORD dwValidImageCnt;
    DWORD dwMaxImageCnt;
};

//================================================================
// LINUX
//================================================================
#ifdef __linux__
typedef struct
{
    WORD wSize;      // Sizeof this struct
    WORD wSignalNum; // Index for strSignal
    WORD wEnabled;   // Flag shows enable state of the signal (0: off, 1: on)
    WORD wType;      // Selected signal type
    WORD wPolarity;  // Selected signal polarity
    WORD wFilterSetting; // Selected signal filter // 12
    WORD wSelected;      // Select signal (0: standard signal, >1 other signal)
    WORD ZZwReserved;
    DWORD ZZdwReserved[11]; // 60
} PCO_SignalLinux;
#endif
//================================================================
//================================================================

#define LEN_TRACEACQ_MSG 512
#define LEN_ERROR_MSG (512 - 1)
#define LEN_MSG (256 - 1)

#define PCO_MAXSEGMENTS 4

#define LEN_DUMP 128
char DLL_EXPORT *_hex_dump_bytes(void *obj, size_t lenObj, char *buff,
                                 size_t lenBuff);

long msElapsedTime(TIME_USEC &t0);
void msElapsedTimeSet(TIME_USEC &t0);

void usElapsedTimeSet(long long &us0);
long long usElapsedTime(long long &us0);

double usElapsedTimeTicsPerSec();

#define DIM_ACTION_TIMESTAMP 10
enum actionTimestamp
{
    tsConstructor = 0,
    tsStartAcq,
    tsStopAcq,
    tsPrepareAcq,
    tsReset
};

enum capsDesc
{
    capsCDI = 1,
    capsDoubleImage,
    capsRollingShutter,
    capsGlobalShutter,
    capsGlobalResetShutter,
    capsHWIO,
    capsCamRam,
    capsMetadata,
    capsTimestamp,
    capsTimestamp3,

};

enum timestampFmt
{
    Iso = 1,
    IsoHMS,
    FnFull,
    FnDate,
    IsoMilliSec
};
char *getTimestamp(timestampFmt fmtIdx, time_t xtime = 0);
time_t getTimestamp();

//================================================================
//================================================================
struct stcFrame
{
    BOOL changed;
    unsigned int nb;
    unsigned int done;

    unsigned int ram_current;

    unsigned long next2read;
};

//================================================================
//================================================================
#define RING_LOG_BUFFER_SIZE 64
class ringLog
{
    // enum { bufferSize = 64 };
    struct data
    {
        time_t timestamp;
        char str[RING_LOG_BUFFER_SIZE + 1];
    };

  public:
    ringLog(int size);
    ~ringLog();
    int add(const char *s);
    int size()
    {
        return m_size;
    };
    void dumpPrint(bool direction);
    void flush(int capacity);
    int dump(char *s, int lgMax, bool direction);

  private:
    int m_capacity;
    int m_capacity_max;
    int m_size;
    int m_head;
    struct data *buffer;
};

//================================================================
//================================================================
struct stcTemp
{
    short sCcd, sCam, sPower;
    short sMinCoolSet, sMaxCoolSet;
    short sSetpoint;
    short sDefaultCoolSet;
};

//================================================================
//================================================================
struct stcLongLongStr
{
    long long value;
    const char *desc;
};

//================================================================
//================================================================
typedef struct
{
    DWORD nrImgRecorded;
    DWORD maxImgCount;
    int lastImgFifo;
    int nrImgRequested;
    int nrImgRequested0;
    int nrImgAcquired;
    long msTotal, msRecord, msRecordLoop, msXfer, msTout;
    long msStartAcqStart, msStartAcqEnd, msStartAcqNow;
    int checkImgNrPco, checkImgNrPcoTimestamp, checkImgNrLima, checkImgNrOrder;
    bool checkImgNr;

#define LEN_TRACEACQ_TRHEAD 11
    // long msThreadBeforeXfer, msThreadAfterXfer, msThreadEnd;
    // long msThread[LEN_TRACEACQ_TRHEAD];
    long msReserved[15 - LEN_TRACEACQ_TRHEAD];

    struct stcLongLongStr usTicks[LEN_TRACEACQ_TRHEAD];
    double msImgCoc;
    double sExposure, sDelay;
    time_t endRecordTimestamp;
    time_t endXferTimestamp;
    const char *fnId;
    const char *fnIdXfer;
    const char *sPcoStorageRecorderMode;
    int iPcoStorageMode, iPcoRecorderSubmode;
    int iPcoBinHorz, iPcoBinVert;
    int iPcoRoiX0, iPcoRoiX1, iPcoRoiY0, iPcoRoiY1;
    const char *sPcoTriggerMode;
    const char *sLimaTriggerMode;
    int iPcoTriggerMode;

    const char *sPcoAcqMode;
    int iPcoAcqMode;

    double dLimaExposure, dLimaDelay;
    int iPcoExposure, iPcoExposureBase;
    int iPcoDelay, iPcoDelayBase;

    char msg[LEN_TRACEACQ_MSG + 1];

    time_t fnTimestampEntry, fnTimestampExit;
    int nrErrors;

    void traceMsg(char *s);

} STC_traceAcq;

//================================================================
//================================================================

namespace lima
{
    namespace Pco
    {
        class Camera;

    } // namespace Pco
} // namespace lima

enum enumChange
{
    Invalid,
    Valid,
    Changed,
};

enum enumStop
{
    stopNone = 0,
    stopRequest,
    // stopRequestAgain,
    // stopProcessing,
};

enum enumPcoFamily
{
    Dimax = 1 << 0,
    Edge = 1 << 1,
    EdgeGL = 1 << 2,
    EdgeRolling = 1 << 3,
    Pco2k = 1 << 4,
    Pco4k = 1 << 5,
    EdgeUSB = 1 << 6,
    EdgeHS = 1 << 7,
    DimaxHS = 1 << 8,
    DimaxHS1 = 1 << 9,
    DimaxHS2 = 1 << 10,
    DimaxHS4 = 1 << 11,
    camRAM = 1 << 12,
    Pixelfly = 1 << 13,
};

enum enumInterfaceTypes
{
    ifFirewire = 1 << 0,
    ifCameralink = 1 << 1,
    ifCameralinkHS = 1 << 2,
    ifCameralinkAll = 1 << 3,
    ifUsb = 1 << 4,
    ifUsb3 = 1 << 5,
    ifEth = 1 << 6,
    ifSerial = 1 << 7,
    ifCoaxpress = 1 << 8,

};

enum enumRoiError
{
    Xrange = 1 << 0,
    Yrange = 1 << 1,
    Xsteps = 1 << 2,
    Ysteps = 1 << 3,
    Xsym = 1 << 4,
    Ysym = 1 << 5,
};

enum enumPcoStorageMode
{
    Fifo = 1,
    RecSeq,
    RecRing,
    RecInvalid
};

enum enumTblXlatCode2Str
{
    ModelType,
    InterfaceType,
    ModelSubType
};

#if 0
struct stcBinning 
{
	enumChange	changed;		/* have values been changed ? */
	unsigned int x;			/* amount to bin/group x data.                 */
	unsigned int y;			/* amount to bin/group y data.                 */
};
#endif

enum enumTraceAcqId
{
    traceAcq_ExecTimeTot,
    traceAcq_Lima,
    traceAcq_GetImageEx,
    traceAcq_pcoSdk,
    traceAcq_GetRecordingState,
    traceAcq_SetRecordingState,
    traceAcq_CancelImages,
    traceAcq_xferImagBefore,
    traceAcq_xferImag,
    traceAcq_setExposing,
    traceAcq_stopAcq,
    traceAcq_uptoEndThread,

};

#define SIZEARR_stcPcoHWIOSignal 10
#define SIZESTR_PcoHWIOSignal 1024

namespace lima
{
    namespace Pco
    {
        class BufferCtrlObj;
        class SyncCtrlObj;
        class VideoCtrlObj;
        class Camera;

        //--------------------------------------- stcPcoData
        struct stcPcoData
        {
            PCO_General stcPcoGeneral;
            PCO_CameraType stcPcoCamType;
            PCO_Sensor stcPcoSensor;

#ifndef __linux__
            PCO_Description
                stcPcoDescription; /* camera description structure */
            PCO_Signal stcPcoHWIOSignal[SIZEARR_stcPcoHWIOSignal];
            PCO_Single_Signal_Desc
                stcPcoHWIOSignalDesc[SIZEARR_stcPcoHWIOSignal];
#else
            PCO_Description
                stcPcoDescriptionWin; /* camera description structure */
            SC2_Camera_Description_Response
                stcPcoDescription; /* camera description structure */

            SC2_Camera_Description_Response stcPcoDesc1;
            SC2_Camera_Description_2_Response stcPcoDesc2;
            PCO_SignalLinux stcPcoHWIOSignal[SIZEARR_stcPcoHWIOSignal];
            SC2_Get_HW_IO_Signal_Descriptor_Response
                stcPcoHWIOSignalDesc[SIZEARR_stcPcoHWIOSignal];
            char sPcoHWIOSignalDesc[SIZEARR_stcPcoHWIOSignal]
                                   [SIZESTR_PcoHWIOSignal + 1];
#endif

            const char *sClTransferParameterSettings;

            PCO_Timing stcPcoTiming;
            PCO_Storage stcPcoStorage;
            PCO_Recording stcPcoRecording;

            WORD wNrPcoHWIOSignal0;
            WORD wNrPcoHWIOSignal;
            unsigned long long debugLevel;
            unsigned long long testCmdMode;
            BYTE ipField[4];

#define PARAMS_NR 20
#define PARAMS_LEN_TOKEN (31)
#define PARAMS_LEN_BUFF (PARAMS_NR * (PARAMS_LEN_TOKEN + 1))

            struct stcParams
            {
                char *ptrKey[PARAMS_NR];
                char *ptrValue[PARAMS_NR];
                int nr;
                char buff[PARAMS_LEN_BUFF + 1];

                WORD logBits;
                char logPath[PATH_MAX];
                unsigned long long testMode;
            } params;

            DWORD dwPixelRateMax;

            char model[MODEL_TYPE_SIZE + 1];
            char modelSubType[MODEL_TYPE_SIZE + 1];
            char iface[INTERFACE_TYPE_SIZE + 1];
            char nameCamIf[LEN_DESCRIPTION_NAME];
            char nameCam[LEN_DESCRIPTION_NAME];
            char nameSensor[LEN_DESCRIPTION_NAME];

            PCO_SC2_CL_TRANSFER_PARAM clTransferParam;
            int pcoError;
            char pcoErrorMsg[ERR_SIZE + 1];

            double cocRunTime; /* cam operation code - delay & exposure time &
                                  readout in s*/
            double frameRate;

            WORD wActiveRamSegment; /* active ram segment */

            bool bExtTrigEnabled;
            WORD storage_mode;
            WORD recorder_submode;
            const char *storage_str;
            unsigned long frames_per_buffer;
            DWORD dwRamSize;
            WORD wPixPerPage;
            DWORD dwMaxFramesInSegment[4];
            DWORD dwSegmentSize[4];

            struct stcSegmentInfo m_stcSegmentInfo[PCO_MAXSEGMENTS];

            DWORD dwValidImageCnt[4];
            DWORD dwMaxImageCnt[4];

            WORD wRoiX0Now, wRoiY0Now, wRoiX1Now, wRoiY1Now;
            
            WORD m_wArmWidth, m_wArmHeight, m_wMaxWidth, m_wMaxHeight;
            WORD m_wBitPerPixel;

            WORD wCamType;
            DWORD dwSerialNumber;
            WORD wIfType;

            char camera_name0[CAMERA_NAME_SIZE];
            char camera_name_if[CAMERA_NAME_SIZE];
            char camera_name[CAMERA_NAME_SIZE];
            char sensor_name[CAMERA_NAME_SIZE];
            char sensor_type[64];

            WORD wNowADC, wNumADC;
            unsigned int maxwidth_step, maxheight_step;

            struct stcTemp temperature;

            WORD bMetaDataAllowed, wMetaDataMode, wMetaDataSize,
                wMetaDataVersion;

            long msAcqRec, msAcqXfer, msAcqTout, msAcqTnow, msAcqAll;
            time_t msAcqRecTimestamp, msAcqXferTimestamp, msAcqToutTimestamp,
                msAcqTnowTimestamp;

            STC_traceAcq traceAcq;

            DWORD dwPixelRate, dwPixelRateRequested;
            double fTransferRateMHzMax;

            WORD wLUT_Identifier, wLUT_Parameter;

            DWORD dwAllocatedBufferSize;
            int iAllocatedBufferNumber;
            int iAllocatedBufferNumberLima;
            bool bAllocatedBufferDone;
            DWORD dwRollingShutter;

            char *version;

            double min_exp_time, min_exp_time_err, step_exp_time;
            double max_exp_time, max_exp_time_err;
            double min_lat_time, min_lat_time_err, step_lat_time;
            double max_lat_time, max_lat_time_err;

            WORD wBitAlignment; // 0 = MSB (left) alignment

            struct
            {
                time_t constructor, startAcq, stopAcq;
            } timestamps;

            struct
            {
                time_t ts[DIM_ACTION_TIMESTAMP];
            } action_timestamp;

            stcPcoData();

            void traceAcqClean();
            void traceMsg(char *s);

            int testForceFrameFirst0;
            bool pcoLogActive;

            int acqTimeoutRetry; // max nr of timeout during acq (wait for mult
                                 // obj)

            bool params_xMinSize;
            bool params_ignoreMaxImages;

            char camerasFound[MSG1K];

            long reserved[32];

        }; // struct stcPcoData

        //--------------------------------------- class CehckImgNr
        class CheckImgNr
        {
          public:
            CheckImgNr(Camera *cam);
            ~CheckImgNr();
            void init(STC_traceAcq *traceAcq);
            void update(int iLimaFrame, void *ptrImage);

            int _get_imageNr_from_imageTimestamp(void *buf, int shift);
#ifndef __linux__
            int _get_time_from_imageTimestamp(void *buf, int shift, SYSTEMTIME *st);
#endif

          private:
            Camera *m_cam;
            STC_traceAcq *m_traceAcq;
            bool checkImgNr;
            int pcoImgNrDiff;
            int pcoImgNrOrder;
            int pcoImgNrLast;
            int alignmentShift;
            bool bNoTimestamp;            
        }; // class CheckImgNr

        //--------------------------------------- class Camera
        //-------------------------------------------------------------
        //-------------------------------------------------------------
        class DLL_EXPORT Camera : public HwMaxImageSizeCallbackGen
        {
            friend class Interface;
            friend class DetInfoCtrlObj;
            friend class SyncCtrlObj;
            friend class RoiCtrlObj;
            friend class BinCtrlObj;
            friend class BufferCtrlObj;
            friend class CheckImgNr;

            DEB_CLASS_NAMESPC(DebModCamera, "Camera", "Pco");

          public:
            Camera(const std::string &camPar);
            ~Camera();

            enum Status
            {
                Fault,
                Ready,
                Exposure,
                Readout,
                Latency,
                Config
            };

            void prepareAcq();
            void startAcq();
            void reset(int reset_level);
            void setRingBufferEnabled(bool enabled);
            bool getRingBufferEnabled();
            bool isRingBufferBusy();
            bool hasRingBufferFault();
            std::string getRingBufferStatus();
            void setRingBufferControl(RingBufferControl* control);
            void releaseRingBufferControl(RingBufferControl* control);
            void prepareRingBuffer();
            void cancelRingBufferPreparation();
            bool isRingBufferCompletionThread();
            void runRingBuffer();
            void setRingBufferStatus(const std::string& status);

#ifdef __linux__
            CPco_com *camera;
            CPco_grab_cl_me4 *grabber;
            CPco_Log *mylog;

            void stopAcq();
            void _stopAcq(bool waitForThread);
#endif

            // ----- BIN
            void setBin(const Bin &aBin);
            void getBin(Bin &aBin);
            void checkBin(Bin &aBin);
            // -----

            HANDLE &getHandle()
            {
                return m_handle;
            }

            // void getMaxWidthHeight(DWORD &xMax, DWORD &yMax);
            void getMaxWidthHeight(unsigned int &xMax, unsigned int &yMax);

            void getXYsteps(unsigned int &xSteps, unsigned int &ySteps);
            void getXYdescription(unsigned int &xSteps, unsigned int &ySteps,
                                  unsigned int &xMax, unsigned int &yMax,
                                  unsigned int &xMinSize,
                                  unsigned int &yMinSize);

            void getBytesPerPixel(unsigned int &pixbytes);
            void getBitsPerPixel(WORD &pixbits);

            int getNbAcquiredFrames() const
            {
                return m_acq_frame_nb;
            }

            void getCameraName(std::string &name);

            std::string talk(const std::string &cmd);
            const char *_talk(const char *cmd, char *output, int lg);

            unsigned long
            _pco_GetNumberOfImagesInSegment_MaxCalc(int segmentPco);

            unsigned long pcoGetFramesPerBuffer()
            {
                return m_pcoData->frames_per_buffer;
            }

            PcoHwEventCtrlObj *_getPcoHwEventCtrlObj()
            {
                return m_HwEventCtrlObj;
            }
            BufferCtrlObj *_getBufferCtrlObj()
            {
                return m_buffer;
            }
            SyncCtrlObj *_getSyncCtrlObj()
            {
                return m_sync;
            }
            struct stcPcoData *_getPcoData()
            {
                return m_pcoData;
            }

            char *_PcoCheckError(int line, const char *file, int err,
                                 int &error, const char *fn = "***");
            int pcoGetError()
            {
                return m_pcoData->pcoError;
            }

            int dumpRecordedImages(int &nrImages, int &error);

            bool _isCameraType(unsigned long long tp);
            bool _isInterfaceType(int tp);
            bool _isConfig()
            {
                return m_config;
            };
            void _pco_set_shutter_rolling_edge(int &error);
            void msgLog(const char *s);
            bool _getIsArmed()
            {
                return m_isArmed;
            };
            void _armRequired(bool armRequiered)
            {
                m_isArmed = !armRequiered;
            };
            void _traceMsg(char *s);

            void paramsInit(const char *str);

            bool paramsGet(const char *key, char *&value);
            time_t _getActionTimestamp(int action);
            void _setActionTimestamp(int action);

          private:
            RingBufferState m_ringBuffer;
            PcoHwEventCtrlObj *m_HwEventCtrlObj;
            SyncCtrlObj *m_sync;
            BufferCtrlObj *m_buffer;
            HANDLE m_handle; /* handle of opened camera */

            std::string m_log;
            // char pcoErrorMsg[ERR_SIZE+1];

            struct stcPcoData *m_pcoData;

            bool m_cam_connected;

            Cond m_cond;

#ifdef __linux__
            enum threadType{threadAcq = 1, threadSw};

            class _AcqThread;
            friend class _AcqThread;
            _AcqThread *m_acq_threadAcq;
            _AcqThread *m_acq_threadSw;
#endif
            volatile bool m_wait_flag;
            volatile bool m_quit;
            volatile bool m_thread_running;

            volatile bool m_wait_flag_rolling;
            volatile bool m_quit_rolling;
            volatile bool m_thread_running_rolling;

            int m_pcoError;

            Roi m_Roi_lastFixed_hw;
            Roi m_Roi_lastFixed_requested;
            time_t m_Roi_lastFixed_time;

            // struct stcSize m_size;

            int m_acq_frame_nb;
            bool m_config;

            bool m_isArmed;
            long long m_state;

            int m_pco_buffer_nrevents;

            bool m_bRecorderForcedFifo;
            int m_iRecorderStopRequest;
            int m_iPcoAcqMode;

            //----------------------------------

            Camera::Status m_status;

            //------------- linux sdk
            SHORT sTempCcd, sTempCam, sTempPS;
            SHORT sCoolingSetpoint;

            WORD camtype;
            DWORD serialnumber;
            WORD wLutActive, wLutParam;
            int board;

            DWORD m_dwRollingShutterNew;

            PCO_SC2_CL_TRANSFER_PARAM clpar;

            DWORD dwPixelRateActual;
            DWORD dwPixelRateValid[4];
            DWORD dwPixelRateMax;
            int iPixelRateValidNr;

            //------------- linux sdk

            int PcoCheckError(int line, const char *file, int err,
                              const char *fn = "***",
                              const char *comments = "");

            void _allocBuffer();

            void _presetPixelRate(DWORD &pixRate, int &error);
            void _get_shutter_rolling_edge(DWORD &dwRolling, int &error);
            void _set_shutter_rolling_edge(DWORD dwRolling, int &error);

            void _init();
            void _init_edge();
            void _init_dimax();

            void _pco_SetPixelRate(int &error);
            // char *_pco_SetPixelRate(int &error);

            bool _isValid_pixelRate(DWORD dwPixelRate);
            bool _isValid_rollingShutter(DWORD dwRollingShutter);

            void getRoiSymetrie(bool &bSymX, bool &bSymY);
            void _get_logLastFixedRoi(Roi &requested_roi, Roi &fixed_roi,
                                      time_t &dt);
            void _set_logLastFixedRoi(const Roi &requested_roi,
                                      const Roi &fixed_roi);

            int _checkValidRoi(const Roi &new_roi, Roi &fixed_roi);
            int _fixValidRoi(unsigned int &x0, unsigned int &x1,
                             unsigned int xMax, unsigned int xSteps,
                             unsigned int xMinSize, bool bSymX);

            void _get_MaxRoi(Roi &roi);
            void _get_RoiSize(Size &roi_size);

            void _get_ImageType(ImageType &image_type);
            void _get_PixelSize(double &x_size, double &y_size);
            void _get_XYsteps(Point &xy_steps);
            void _set_ImageType(ImageType curr_image_type);
            void _get_DetectorType(std::string &det_type);
            void _get_MaxImageSize(Size &max_image_size);
            unsigned long long _getDebug(unsigned long long mask);

            ringLog *m_msgLog;
            ringLog *m_tmpLog;

            CheckImgNr *m_checkImgNr;
            char *mybla, *myblamax;
            char *mytalk, *mytalkmax;

            const char *_checkLogFiles(bool firstCall = false);
            char *_camInfo(char *ptr, char *ptrMax, long long int flag);

            WORD _getInterfaceType();
            const char *_getInterfaceTypeStr();

            WORD _getCameraType();
            const char *_getCameraTypeStr();

            WORD _getCameraSubType();
            const char *_getCameraSubTypeStr();

            DWORD _getCameraSerialNumber();

            const char *_xlatPcoCode2Str(int code, enumTblXlatCode2Str table,
                                         int &err);
            const char *xlatCode2Str(int code, struct stcXlatCode2Str *stc);

            bool _getCameraState(long long flag);
            void _setCameraState(long long flag, bool val);

#ifndef __linux__
            bool _isRunAfterAssign();
#endif

            bool _isCapsDesc(int caps);
            void _pco_GetAcqEnblSignalStatus(WORD &wAcquEnableState, int &err);

            void _pco_GetGeneralCapsDESC(DWORD &capsDesc1, int &err);

            //----
            void _pco_SetCameraToCurrentTime(int &error);
            void _pco_SetCamLinkSetImageParameters(int &error);

#ifdef __linux__
            void _pco_GetLut(int &err);
            void _pco_Open_Cam(int &err);
            void _pco_Open_Grab(int &err);
            void _pco_GetCameraInfo(int &error);
            void _pco_ResetSettingsToDefault(int &err);
#endif
            void _pco_GetSizes(WORD *wXResActual, WORD *wYResActual,
                               WORD *wXResMax, WORD *wYResMax, int &error);
            void _pco_SetTransferParameter_SetActiveLookupTable_win(int &error);
            void _pco_SetTransferParameter_SetActiveLookupTable(int &error);

          public:
            //----------- attributes

            void getAcqTimeoutRetry(int &val);
            void setAcqTimeoutRetry(int val);

            void getAdc(int &val);
            void setAdc(int val);
            void getAdcMax(int &val);

            void getCamInfo(std::string &o_sn);
            void getCamType(std::string &o_sn);
            void getVersion(std::string &o_sn);
            void getPixelRateInfo(std::string &o_sn);

            void getClTransferParam(std::string &o_sn);
            void getLastError(std::string &o_sn);
            void getTraceAcq(std::string &o_sn);
            void getPixelRateValidValues(std::string &o_sn);

            void getCocRunTime(double &coc);
            void getFrameRate(double &framerate);

            void getLastImgRecorded(unsigned long &img);
            void getLastImgAcquired(unsigned long &img);

            void getMaxNbImages(unsigned long &nr);
            void getPcoLogsEnabled(int &enabled);

            void getLastFixedRoi(std::string &o_sn);

            void getPixelRate(int &val);
            void setPixelRate(int val);

            void getRollingShutterInfo(std::string &o_sn);
            void getRollingShutter(int &val);
            void setRollingShutter(int val);
            void setRollingShutterStr(std::string &i_sn);

            void getCDIMode(int &val);
            void setCDIMode(int val);

            void getTemperatureInfo(std::string &o_sn);
            void getCoolingTemperature(int &val);
            void setCoolingTemperature(int val);

            void getSdkRelease(std::string &o_sn);
            void getCameraNameEx(std::string &o_sn);
            void getCameraNameBase(std::string &o_sn);

            void getBinningInfo(std::string &o_sn);
            void getFirmwareInfo(std::string &o_sn);
            void getRoiInfo(std::string &o_sn);

            void getMsgLog(std::string &o_sn);

            //--------------------------------------

          public: //----------- pco sdk functions
            void _pco_GetActiveRamSegment(
                WORD &, int &); // {return m_pcoData->wActiveRamSegment;}

            const char *_pco_SetRecordingState(int state, int &error);

            void _pco_SetTriggerMode_SetAcquireMode(int &error);
            void _pco_SetStorageMode_SetRecorderSubmode(enumPcoStorageMode,
                                                        int &error);
            int _pco_GetStorageMode_GetRecorderSubmode();

            void _pco_SetDelayExposureTime(int &error, int ph = 0);
            void _pco_SetImageParameters(int &error);

            void _pco_GetCameraType(int &error);
            // char *_pco_GetTemperatureInfo(int &error);
            void _pco_GetTemperatureInfo(int &error);
            void _pco_GetTemperatureInfo(char *ptr, char *ptrMax, int &error);
            void _pco_GetCoolingSetpointTemperature(int &val, int &error);
            void _pco_SetCoolingSetpointTemperature(int val, int &error);

            bool _isCooledCamera();

            void _pco_GetPixelRate(DWORD &pixRate, DWORD &pixRateNext,
                                   int &error);
            // char *_pco_SetCameraSetup(DWORD dwSetup, int &error);

            void _pco_SetMetaDataMode(WORD wMetaDataMode, int &error);

            void _pco_GetHWIOSignalAll(int &error);
            void _pco_SetHWIOSignal(int sigNum, int &error);

            void _pco_initHWIOSignal(int mode, WORD wVar,
                                     int &error); // TODO sync

            void _setStatus(Camera::Status status, bool force);
            void getStatus(Camera::Status &status);

          public:
            int _pco_GetADCOperation(int &adc_working, int &adc_max);
            int _pco_SetADCOperation(int adc_new, int &adc_working);
            int _pco_GetImageTiming(double &frameTime, double &expTime,
                                    double &sysDelay, double &sysJitter,
                                    double &trigDelay);
            int _pco_GetBitAlignment(int &alignment);
            int _pco_SetBitAlignment(int alignment);

            void _pco_SetTimestampMode(WORD mode, int &err);
            void _pco_GetTimestampMode(WORD &mode, int &err);
            void _pco_GetTransferParameter(int &err);

            double pcoGetCocRunTime();
            double pcoGetFrameRate();
            void _pco_GetCameraMinSizeCalc(WORD &wMinSizeHorz,
                                           WORD &wMinSizeVert);

            void _pco_GetCOCRuntime(int &err);

            void _pco_GetSegmentInfo(int &err);

            void _pco_GetNumberOfImagesInSegment(WORD wSegment,
                                                 DWORD &dwValidImageCnt,
                                                 DWORD &dwMaxImageCnt,
                                                 int &err);

            void _pco_GetCDIMode(WORD &wCDIMode, int &err);
            void _pco_SetCDIMode(WORD wCDIMode, int &err);

            void _pco_GetDoubleImageMode(WORD &wDoubleImage, int &err);
            void _pco_SetDoubleImageMode(WORD wDoubleImage, int &err);

            void _pco_FillStructures(int &err);
            void _pco_CloseCamera(int &err);

            WORD _pco_GetRecordingState(int &err);

            void dummySip();

#ifdef __linux__
            void _waitForRecording(int nrFrames, DWORD &_dwValidImageCnt,
                                   DWORD &_dwMaxImageCnt, int &error);
#endif
            //----
            void _pco_GetInfoString(int infotype, char *buf_in, int size_in,
                                    int &error);
            void _pco_GetBinning(Bin &bin, int &err);
            void _pco_SetBinning(Bin binNew, Bin &binActual, int &err);
            int _binning_fit(int binRequested, int binMax, int binMode);
            void _pco_GetBinningInfo(char *buf_in, int size_in, int &err);

            void _pco_SetROI(Roi roi, int &err);
            void _pco_GetROI(Roi &roi, int &err);
            void _xlatRoi_lima2pco(Roi roiLima, unsigned int &x0,
                                   unsigned int &x1, unsigned int &y0,
                                   unsigned int &y1);
            void _xlatRoi_pco2lima(Roi &roiLima, unsigned int x0,
                                   unsigned int x1, unsigned int y0,
                                   unsigned int y1);
            void _pco_GetRoiInfo(char *buf_in, int size_in, int &err);

            void _pco_GetFirmwareInfo(char *buf_in, int size_in, int &err);

            const char *_sprintComment(bool bAlways, const char *comment,
                                       const char *comment1 = "",
                                       const char *comment2 = "");

            void _pco_ArmCamera(int &err);
            void _pco_SetRecordStopEvent(WORD wRecordStopEventMode,
                                         DWORD dwRecordStopDelayImages,
                                         int &err);

            //----
            void _pco_FreeBuffer(int bufIdx, int &err);

            void _pco_AllocateBuffer(SHORT *sBufNr, DWORD dwSize, WORD **wBuf,
                                     void **hEvent, int &err);

            void _pco_GetImageEx(WORD wSegment, DWORD dw1stImage,
                                 DWORD dwLastImage, SHORT sBufNr, WORD wXRes,
                                 WORD wYRes, WORD wBitPerPixel, int &err);

            void _pco_GetBufferStatus(SHORT sBufNr, DWORD *dwStatusDll,
                                      DWORD *dwStatusDrv, int &err);

            void _pco_AddBufferExtern(HANDLE hEvent, WORD wActSeg,
                                      DWORD dw1stImage, DWORD dwLastImage,
                                      DWORD dwSynch, void *pBuf, DWORD dwLen,
                                      DWORD *dwStatus, int &err);

            void _pco_AddBufferEx(DWORD dw1stImage, DWORD dwLastImage,
                                  SHORT sBufNr, WORD wXRes, WORD wYRes,
                                  WORD wBitPerPixel, int &err);

            //----
            void _pco_RebootCamera(int &err);
            void _pco_OpenCamera(int &err);
            void _pco_GetCameraRamSize(DWORD &dwRamSize, WORD &wPageSizeint,
                                       int &err);
            void _pco_ResetLib(int &err);
            void _pco_GetCameraSetup(WORD &wType, DWORD &dwSetup, WORD &wLen,
                                     int &err);
            void _pco_SetCameraSetup(WORD wType, DWORD &dwSetup, WORD wLen,
                                     int &err);
            void _pco_SetTimeouts(void *buf_in, unsigned int size_in, int &err);
            void _pco_GetCameraRamSegmentSize(DWORD *dwRamSegSize, int &err);
            void _pco_SetCameraRamSegmentSize(DWORD *dwRamSegSize, int &err);

            void _pco_OpenCameraSn(DWORD sn, int &err);
            void _pco_GetCameraTypeOnly(int &err);
            const char *_getCameraIdn();
            void getCamerasFound(std::string &o_sn);

            void getDoubleImageMode(int &val);
            void setDoubleImageMode(int val);

            void getDebugIntTypes(std::string &o_sn);
            void getDebugInt(std::string &o_sn);
            void setDebugInt(std::string &i_sn);

            void setTest(std::string &i_sn);
            void getTest(std::string &o_sn);

            void setTimestampMode(int mode);
            void getTimestampMode(int &mode);

            void setBitAlignment(std::string &i_sn);
            void getBitAlignment(std::string &o_sn);

            void getRecorderForcedFifo(int &val);
            void setRecorderForcedFifo(int val);

            void getNrEvents(int &val);
            void setNrEvents(int val);

            void getRecorderStopRequest(int &val);
            void setRecorderStopRequest(int val);
            int _getRecorderStopRequest();
            int _setRecorderStopRequest(int val);

            void getLastImgFifo(int &val);

            void Sleep_ms(unsigned int time_ms);

            void getGeneralCAPS1(std::string &o_sn);

            void getParamsInfo(std::string &o_sn);

        }; // class camera
    }      // namespace Pco
} // namespace lima

void _pco_time2dwbase(double exp_time, DWORD &dwExp, WORD &wBase);

int __sprintfSExt(char *ptr, size_t nrMax, const char *format, ...);

char *str_trim_left(char *s);
char *str_trim_right(char *s);
char *str_trim(char *s);
char *str_toupper(char *s);

//================================================================
// LINUX
//================================================================
//--------------------- dummies for linux
#ifdef __linux__

int PCO_GetActiveRamSegment(HANDLE ph, WORD *);
int PCO_GetActiveLookupTable(HANDLE ph, WORD *wIdentifier, WORD *wParameter);
int PCO_SetActiveLookupTable(HANDLE ph, WORD *wIdentifier, WORD *wParameter);
int PCO_CamLinkSetImageParameters(HANDLE ph, WORD wxres, WORD wyres);

enum traceAcqId
{
    traceAcq_execTimeTot,
};
int image_nr_from_timestamp(void *buf, int shift, bool bDisable);

#endif
//================================================================
//================================================================

#endif
