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
#include <process.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <time.h>

#include "lima/Exceptions.h"
#include "lima/HwSyncCtrlObj.h"

#include "PcoCamera.h"
#include "PcoSyncCtrlObj.h"

using namespace lima;
using namespace lima::Pco;

void _pco_time2dwbase(double exp_time, DWORD &dwExp, WORD &wBase);

//=================================================================================================
//=================================================================================================

// 4294967295.0 = double(DWORD(0xFFFFFFFF)) 
#define DWORD_MAX_FLOAT 4294967295.0

#define	MAX_DWORD_MS (double(4294967295.0e-3))
#define	MAX_DWORD_US (double(4294967295.0e-6))
#define	MAX_DWORD_NS (double(4294967295.0e-9))


void _pco_time2dwbase(double exp_time, DWORD &dwExp, WORD &wBase) {
	// conversion time(s) to PCO standard DWORD + UNIT(ms, us, ns)
		// exp & lat time is saved in seconds (LIMA). 
		// PCO requires them expressed in DWORD as ms(base=2), us(base=1) or ns(base=0)
		// max DWORD 0xFFFFFFFF = 4294967295.0
		// find the lowest unit (ns -> us -> ms) which does not overflow DWORD
    
	if(exp_time <= MAX_DWORD_NS) {   
		dwExp = DWORD(exp_time * 1.0e9); wBase = 0; // ns
	} else 	if(exp_time <= MAX_DWORD_US) {  
		dwExp = DWORD(exp_time * 1.0e6); wBase = 1; // us
	} else {  
		dwExp = DWORD(exp_time * 1.0e3); wBase = 2; // ms
	}

	DWORD mask = 0x7;
	DWORD min = 0x1000;

	if(dwExp > min){
		dwExp |= mask;
		dwExp ^= mask;
	}

	return;
}

//=================================================================================================
//=================================================================================================
#define LEN_ERRSTR 127

WORD Camera::_pco_GetActiveRamSegment()
{
	DEB_MEMBER_FUNCT();

	WORD act_segment;
	int err;
	char errstr[LEN_ERRSTR+1];
	DWORD capsDesc1;

	_pco_GetGeneralCapsDESC(capsDesc1, err);
	if((capsDesc1 & GENERALCAPS1_NO_RECORDER)==0)
	{
		err=PCO_GetActiveRamSegment(m_handle,&act_segment);
		if(err!=PCO_NOERROR)
		{
			PCO_GetErrorText(err, errstr,LEN_ERRSTR);
			DEB_ALWAYS() << "ERROR: " << DEB_VAR2(err, errstr);
		}
	}
	else
		act_segment=1;

	m_pcoData->wActiveRamSegment = act_segment;

	return act_segment;
}

//=================================================================================================
//=================================================================================================

// wBitAlignment:
// - 0x0000 = [MSB aligned]; all raw image data will be aligned to the MSB. This is thedefault setting.
// - 0x0001 = [LSB aligned]; all raw image data will be aligned to the LSB.

int Camera::_pco_SetBitAlignment(int alignment){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	int error = 0;
	int alignment1;
	char *msg;
	WORD wBitAlignment;

	if((alignment <0) ||(alignment >1)){
		DEB_ALWAYS() << "ERROR - invalid value " << DEB_VAR1(alignment);
		return -1;
	}

	wBitAlignment = int(alignment);

	PCO_FN2(error, msg,PCO_SetBitAlignment, m_handle, wBitAlignment);
	PCO_THROW_OR_TRACE(error, msg) ;

	
	return _pco_GetBitAlignment(alignment1);

}

//=================================================================================================
//=================================================================================================
	//-------------------------------------------------------------------------------------------------
	// PCO_SetADCOperation
    // Set analog-digital-converter (ADC) operation for reading the image sensor data. Pixel data can be
    // read out using one ADC (better linearity) or in parallel using two ADCs (faster). This option is
    // only available for some camera models. If the user sets 2ADCs he must center and adapt the ROI
    // to symmetrical values, e.g. pco.1600: x1,y1,x2,y2=701,1,900,500 (100,1,200,500 is not possible).
    //
	// DIMAX -> 1 adc
	//-------------------------------------------------------------------------------------------------
int Camera::_pco_GetADCOperation(int &adc_working, int &adc_max)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;

	int error;
	WORD wADCOperation;

	adc_max = m_pcoData->stcPcoDescription.wNumADCsDESC; // nr of ADC in the system

	if(adc_max == 2) {
		PCO_FN2(error, msg,PCO_GetADCOperation, m_handle, &wADCOperation);
		if(error) wADCOperation = (WORD) 1;
	} else {
		adc_max = 1;
		wADCOperation = (WORD) 1;
	}

	adc_working = wADCOperation;
	m_pcoData->wNowADC= wADCOperation;

	return error;
}

//=================================================================================================
//=================================================================================================

int Camera::_pco_SetADCOperation(int adc_new, int &adc_working)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;

	int error, adc_max;

	error = _pco_GetADCOperation(adc_working, adc_max);

	DEB_ALWAYS() << fnId << ": " DEB_VAR2(adc_max, adc_working);

	if(error) return error;

	if((adc_new >=1) && (adc_new <= adc_max) && (adc_new != adc_working) ){
		PCO_FN2(error, msg,PCO_SetADCOperation, m_handle, (WORD) adc_new);
		_pco_GetADCOperation(adc_working, adc_max);
	}
	m_pcoData->wNowADC = adc_working;
	return error;
}


//=================================================================================================
//=================================================================================================
char *Camera::_pco_SetPixelRate(int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	error = 0;
	char *msg;
	DWORD _dwPixelRate, _dwPixelRateOld, _dwPixelRateReq, _dwPixelRateMax;

	if(_isCameraType(Edge)) {

		PCO_FN2(error, msg,PCO_GetPixelRate, m_handle, &m_pcoData->dwPixelRate);
		PCO_THROW_OR_TRACE(error, msg) ;

		_dwPixelRateOld = m_pcoData->dwPixelRate;
		_dwPixelRateReq = m_pcoData->dwPixelRateRequested;
		DEB_ALWAYS() << "PIXEL rate (actual/req): " << DEB_VAR2(_dwPixelRateOld, _dwPixelRateReq) ;

		if(_isValid_pixelRate(_dwPixelRateReq) && (_dwPixelRateOld != _dwPixelRateReq)) {

			PCO_FN2(error, msg,PCO_SetPixelRate, m_handle, _dwPixelRateReq);
			PCO_THROW_OR_TRACE(error, msg) ;

			PCO_FN2(error, msg,PCO_GetPixelRate, m_handle, &m_pcoData->dwPixelRate);
			PCO_THROW_OR_TRACE(error, msg) ;

			_dwPixelRate = m_pcoData->dwPixelRate;
			DEB_ALWAYS() << "PIXEL rate SET (old/new): "  << DEB_VAR2(_dwPixelRateOld, _dwPixelRate) ;

			_armRequired(true);
		}
		m_pcoData->dwPixelRateRequested = 0;
		return fnId;
	}

	if(_isCameraType(Pco2k | Pco4k)) {

		PCO_FN2(error, msg,PCO_GetPixelRate, m_handle, &m_pcoData->dwPixelRate);
		PCO_THROW_OR_TRACE(error, msg) ;

		_dwPixelRateOld = m_pcoData->dwPixelRate;
		_dwPixelRateMax = m_pcoData->dwPixelRateMax;
		_dwPixelRateReq = m_pcoData->dwPixelRateRequested;

		DEB_ALWAYS() << "PIXEL rate (requested/actual/max): " << DEB_VAR3(_dwPixelRateReq, _dwPixelRateOld, _dwPixelRateMax) ;

		if(_isValid_pixelRate(_dwPixelRateReq) && (_dwPixelRateOld != _dwPixelRateReq)) {
		//if(_dwPixelRateMax > _dwPixelRateOld) {

			PCO_FN2(error, msg,PCO_SetPixelRate, m_handle, _dwPixelRateReq);
			PCO_THROW_OR_TRACE(error, msg) ;

			PCO_FN2(error, msg,PCO_GetPixelRate, m_handle, &m_pcoData->dwPixelRate);
			PCO_THROW_OR_TRACE(error, msg) ;
			
			_dwPixelRate = m_pcoData->dwPixelRate;
			DEB_ALWAYS() << "PIXEL rate SET (old/new): "  << DEB_VAR2(_dwPixelRateOld, _dwPixelRate) ;

			_armRequired(true);
		}
		return fnId;
	}
return fnId;
}
//=================================================================================================
//=================================================================================================
int Camera::_pco_GetBitAlignment(int &alignment){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	int error = 0;
	char *msg;
	WORD wBitAlignment;

	PCO_FN2(error, msg,PCO_GetBitAlignment, m_handle, &wBitAlignment);
	PCO_THROW_OR_TRACE(error, msg) ;

	
	alignment = m_pcoData->wBitAlignment = wBitAlignment;

	return error;
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

  DWORD  TriggerSystemJitter_ns;       // Max. possible trigger jitter -0/+ ... ns

  DWORD  TriggerDelay_ns;              // Resulting trigger delay = system delay
  DWORD  TriggerDelay_s;               // + delay of SetDelayExposureTime ... // 9

} PCO_ImageTiming;
******************************************************************************************/


int Camera::_pco_GetImageTiming(double &frameTime, double &expTime, double &sysDelay, double &sysJitter, double &trigDelay ){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;


	int error;

	PCO_ImageTiming pstrImageTiming;

	PCO_FN2(error, msg,PCO_GetImageTiming, m_handle, &pstrImageTiming);

	frameTime = (pstrImageTiming.FrameTime_ns * NANO) + pstrImageTiming.FrameTime_s ;
	expTime = (pstrImageTiming.ExposureTime_ns * NANO) + pstrImageTiming.ExposureTime_s ;
	sysDelay = (pstrImageTiming.TriggerSystemDelay_ns * NANO) ;
	sysJitter = (pstrImageTiming.TriggerSystemJitter_ns * NANO) ;
	trigDelay = (pstrImageTiming.TriggerDelay_ns * NANO) + pstrImageTiming.TriggerDelay_s ;



	return error;
}
//=================================================================================================
//=================================================================================================
//SC2_SDK_FUNC int WINAPI PCO_SetImageParameters(HANDLE ph, WORD wxres, WORD wyres, DWORD dwflags, void* param, int ilen);
// Necessary while using a soft-roi enabled interface. It is recommended to use this function
// with all interface types of pco when soft-roi is enabled. This function can be used as a replacement for
// PCO_CamLinkSetImageParameters
// If there is a change in buffer size (ROI, binning) and/or ROI relocation this function must
// be called with the new x and y resolution. Additionally this function has to be called,
// if you switch to another camera internal memory segment with different x and y size or ROI and like to get images.
// In: HANDLE ph -> Handle to a previously opened camera.
//     WORD wxres -> X Resolution of the images to be transferred
//     WORD wyres -> Y Resolution of the images to be transferred
//     DWORD dwflags -> Flags to select the correct soft-ROI for interface preparation
//                      Set IMAGEPARAMETERS_READ_FROM_SEGMENTS when the next image operation will read images
//                      from one of the camera internal memory segments (if available).
//                      Set IMAGEPARAMETERS_READ_WHILE_RECORDING when the next image operation is a recording
//     void* param -> Pointer to a structure for future use (set to NULL); Currently not used.
//     int ilen -> int to hold the length of the param structure for future use (set to 0); Currently not used.
// Out: int -> Error message.

char *Camera::_pco_SetImageParameters(int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *pcoFn;
    WORD wXres, wYres;

	void *param = NULL;
	int iLenParam = 0;
	DWORD dwFlags = 0;

	dwFlags = _isCameraType(Edge) ? 
					IMAGEPARAMETERS_READ_WHILE_RECORDING :
					IMAGEPARAMETERS_READ_FROM_SEGMENTS;

    wXres= m_pcoData->wXResActual;
    wYres= m_pcoData->wYResActual;
	
	DEB_ALWAYS() << "PCO_SetImageParameters: " << DEB_VAR3(wXres, wYres, dwFlags);

	//error = PCO_SetImageParameters(m_handle, wXres, wYres, dwFlags, param, iLenParam);

	PCO_FN6(error, pcoFn,PCO_SetImageParameters, m_handle, wXres, wYres, dwFlags, param, iLenParam);

	if(error) 
	{ 
		DEB_ALWAYS() << "ERROR: \n" << DEB_VAR2(pcoFn, error);
		throw LIMA_HW_EXC(Error, pcoFn); 
	}

	return fnId;
}



//=================================================================================================
//=================================================================================================
char *Camera::_pco_SetTransferParameter_SetActiveLookupTable(int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	struct stcPcoData _pcoData;
	char msg[ERRMSG_SIZE + 1];
	char *pcoFn;
	bool doLut = false;

	// sizes are only updated AFTER arm, so i will use the last roi settings
	// to get the size. after arm, the size is updated with this value
	WORD wXResActual = m_pcoData->wRoiX1Now - m_pcoData->wRoiX0Now +1;

	//================================================================================================
	// PCO_SetTransferParameter
	//================================================================================================
	// PCO_CL_DATAFORMAT_5x12   0x07     //extract data to 12bit
	// PCO_CL_DATAFORMAT_5x12L  0x09     //extract data to 16Bit
	// PCO_CL_DATAFORMAT_5x12R  0x0A     //without extract

	if (_isInterfaceType(ifCameralinkAll)) 
	{
		char *info = "[none]";

		PCO_FN3(error, pcoFn,PCO_GetTransferParameter, m_handle, &m_pcoData->clTransferParam, sizeof(PCO_SC2_CL_TRANSFER_PARAM));
		PCO_THROW_OR_TRACE(error, pcoFn) ;
	
		memcpy(&_pcoData.clTransferParam, &m_pcoData->clTransferParam,sizeof(PCO_SC2_CL_TRANSFER_PARAM));
	
		m_pcoData->clTransferParam.baudrate = PCO_CL_BAUDRATE_115K2;

		if(_isCameraType(Dimax)){
				//m_pcoData->clTransferParam.Transmit = 1;
				//_pcoData.clTransferParam.Transmit = m_pcoData->clTransferParam.Transmit;
				m_pcoData->clTransferParam.DataFormat=PCO_CL_DATAFORMAT_2x12; //=2
				info = "DIMAX / 2x12 ";
		} else 
		if(_isCameraType(EdgeGL)) {
			m_pcoData->clTransferParam.Transmit = 1;
			m_pcoData->clTransferParam.DataFormat=PCO_CL_DATAFORMAT_5x12 | 
				SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER;
				//SCCMOS_FORMAT_TOP_BOTTOM;
			m_pcoData->wLUT_Identifier = 0; //Switch LUT->off
			doLut = true;
			info = "EDGE GL / 5x12 TOP_CENTER_BOTTOM_CENTER / LUT off";
		} else 
		if(_isCameraType(EdgeHS)) {
			m_pcoData->clTransferParam.Transmit = 1;
			m_pcoData->clTransferParam.DataFormat=PCO_CL_DATAFORMAT_5x16 | 
						SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER;
			m_pcoData->wLUT_Identifier = PCO_EDGE_LUT_NONE; // Switch LUT->off
			doLut = true;
			info = "EDGE HS / 5x16 TOP_CENTER_BOTTOM_CENTER / LUT off";
		} else 
		if(_isCameraType(EdgeRolling)){
			m_pcoData->clTransferParam.Transmit = 1;

			if(m_pcoData->dwPixelRate <= PCO_EDGE_PIXEL_RATE_LOW){
				m_pcoData->clTransferParam.DataFormat=PCO_CL_DATAFORMAT_5x16 | 
					SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER;
				m_pcoData->wLUT_Identifier = PCO_EDGE_LUT_NONE; // Switch LUT->off
				info = "EDGE Rolling / 5x16 TOP_CENTER_BOTTOM_CENTER / LUT off";
			} else 
			if( ((m_pcoData->dwPixelRate >= PCO_EDGE_PIXEL_RATE_HIGH) & 
					(wXResActual > PCO_EDGE_WIDTH_HIGH))) {
				m_pcoData->clTransferParam.DataFormat=PCO_CL_DATAFORMAT_5x12L | 
					SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER;
				m_pcoData->wLUT_Identifier = PCO_EDGE_LUT_SQRT; //Switch LUT->sqrt
				info = "EDGE Rolling / 5x12L TOP_CENTER_BOTTOM_CENTER / LUT SQRT";
			} else {
				m_pcoData->clTransferParam.DataFormat = PCO_CL_DATAFORMAT_5x16 | 
					SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER;
				m_pcoData->wLUT_Identifier = PCO_EDGE_LUT_NONE; // Switch LUT->off
				info = "EDGE Rolling / 5x16 TOP_CENTER_BOTTOM_CENTER / LUT off";
			}
			doLut = true;
		} 

		if((_pcoData.clTransferParam.baudrate != m_pcoData->clTransferParam.baudrate) ||
			(_pcoData.clTransferParam.DataFormat != m_pcoData->clTransferParam.DataFormat) ||
			(_pcoData.clTransferParam.Transmit != m_pcoData->clTransferParam.Transmit)	)
		{
			DEB_ALWAYS() << "PCO_SetTransferParameter (clTransferParam) " << info ;
			PCO_FN3(error, pcoFn,PCO_SetTransferParameter,m_handle, &m_pcoData->clTransferParam, sizeof(m_pcoData->clTransferParam));
			if(error){
				sprintf_s(msg,ERRMSG_SIZE, "PCO_SetTransferParameter - baudrate[%d][%d] dataFormat[x%08x][x%08x] trasmit[%d][%d]",
					_pcoData.clTransferParam.baudrate, m_pcoData->clTransferParam.baudrate,
					_pcoData.clTransferParam.DataFormat, m_pcoData->clTransferParam.DataFormat,
					_pcoData.clTransferParam.Transmit, m_pcoData->clTransferParam.Transmit);
				throw LIMA_HW_EXC(Error, msg); 
			} 
			_armRequired(true);
		} 
		else 
		{
			DEB_ALWAYS() << "PCO_SetTransferParameter (clTransferParam) NOT DONE@" << info ;
			
		}

	} // if cameralink



	//================================================================================================
	// PCO_SetActiveLookupTable
	//================================================================================================

	if(doLut) {
		WORD _wLUT_Identifier, _wLUT_Parameter;

		PCO_FN3(error, pcoFn,PCO_GetActiveLookupTable, m_handle, &_wLUT_Identifier, &_wLUT_Parameter);
	    PCO_THROW_OR_TRACE(error, pcoFn) ;

		if(_wLUT_Identifier != m_pcoData->wLUT_Identifier) {
			PCO_FN3(error, pcoFn,PCO_SetActiveLookupTable, m_handle, &m_pcoData->wLUT_Identifier, &m_pcoData->wLUT_Parameter);
		    PCO_THROW_OR_TRACE(error, pcoFn) ;
			_armRequired(true);

			PCO_FN3(error, pcoFn,PCO_GetActiveLookupTable, m_handle, &m_pcoData->wLUT_Identifier, &m_pcoData->wLUT_Parameter);
		    PCO_THROW_OR_TRACE(error, pcoFn) ;
		}
	}



	return fnId;
}
//=================================================================================================
//=================================================================================================
char * Camera::_pco_SetTriggerMode_SetAcquireMode(int &error){
	DEB_MEMBER_FUNCT();
	char *msg;
	


	DEF_FNID;
	//------------------------------------------------- triggering mode 
	WORD trigmode = m_sync->xlatLimaTrigMode2PcoTrigMode(m_pcoData->bExtTrigEnabled);
    PCO_FN2(error, msg,PCO_SetTriggerMode , m_handle, trigmode);
	if(error) return msg;
	//PCO_THROW_OR_TRACE(error, "PCO_SetTriggerMode") ;
	//DEB_TRACE() << DEB_VAR1(trigmode);

	
	//------------------------------------- acquire mode : ignore or not ext. signal

	WORD acqmode = m_sync->xlatLimaTrigMode2PcoAcqMode();
	PCO_FN2(error, msg,PCO_SetAcquireMode , m_handle, acqmode);
	if(error) return msg;
   //PCO_THROW_OR_TRACE(error, "PCO_SetAcquireMode") ;


	return fnId;
}



//=================================================================================================
// ----------------------------------------- storage mode (recorder + sequence)
// current storage mode
//
// case RecSeq
// case RecRing
// - 0x0000 = [recorder] mode
//		. images are recorded and stored within the internal camera memory (camRAM)
//      . Live View transfers the most recent image to the PC (for viewing / monitoring)
//      . indexed or total image readout after the recording has been stopped
//
// case Fifo
// - 0x0001 = [FIFO buffer] mode
//      . all images taken are transferred to the PC in chronological order
//      . camera memory (camRAM) is used as huge FIFO buffer to bypass short bottlenecks in data transmission
//      . if buffer overflows, the oldest images are overwritten
//      . if Set Recorder = [stop] is sent, recording is stopped and the transfer of the current image to the PC is finished.
//      . Images not read are stored within the segment and can be read with the Read Image From Segment command.
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
//      . recording is stopped by software or disabling acquire signal (<acq enbl>)
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
char * Camera::_pco_SetStorageMode_SetRecorderSubmode(enumPcoStorageMode mode, int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg, *sMode;

	sMode = "invalid";
	switch(mode) {
		case RecSeq:  m_pcoData->storage_mode = 0; m_pcoData->recorder_submode = 0; sMode = "RecSeq" ; break;
		case RecRing: m_pcoData->storage_mode = 0; m_pcoData->recorder_submode = 1; sMode = "RecRing" ;  break;
		case Fifo:    m_pcoData->storage_mode = 1; m_pcoData->recorder_submode = 0;  sMode = "Fifo" ; break;
		default: 
			throw LIMA_HW_EXC(Error,"FATAL - invalid storage mode!" );
	}
    DEB_ALWAYS() << "\n>>> storage/recorder mode: " << DEB_VAR2(sMode, mode) ;

	m_pcoData->traceAcq.sPcoStorageRecorderMode = sMode;
	m_pcoData->traceAcq.iPcoStorageMode = m_pcoData->storage_mode;
	m_pcoData->traceAcq.iPcoRecorderSubmode = m_pcoData->recorder_submode;

    PCO_FN2(error, msg,PCO_SetStorageMode, m_handle, m_pcoData->storage_mode);
	if(error) return msg;
    //PCO_THROW_OR_TRACE(error, msg) ;

    PCO_FN2(error, msg,PCO_SetRecorderSubmode, m_handle, m_pcoData->recorder_submode);
	if(error) return msg;
    //PCO_THROW_OR_TRACE(error, msg) ;

	return fnId;
}

int Camera::_pco_GetStorageMode_GetRecorderSubmode(){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;

	WORD wStorageMode, wRecSubmode;
	int error;

    PCO_FN2(error, msg,PCO_GetStorageMode, m_handle, &wStorageMode);
	if(error){ 
		    PCO_THROW_OR_TRACE(error, msg) ;
	}


    PCO_FN2(error, msg,PCO_GetRecorderSubmode, m_handle, &wRecSubmode);
	if(error) {
		    PCO_THROW_OR_TRACE(error, msg) ;
	}

	m_pcoData->storage_mode = wStorageMode;
	m_pcoData->recorder_submode = wRecSubmode;

	if((wStorageMode == 0) && (wRecSubmode == 0)) { m_pcoData->storage_str= "RecSeq"; return RecSeq; }
	if((wStorageMode == 0) && (wRecSubmode == 1)) { m_pcoData->storage_str= "RecRing"; return RecRing; }
	if((wStorageMode == 1) && (wRecSubmode == 0)) { m_pcoData->storage_str= "Fifo"; return Fifo; }

	m_pcoData->storage_str= "INVALID"; 
	return RecInvalid;
}
//=================================================================================================
//=================================================================================================
void Camera::_pco_GetPixelRate(DWORD &pixRate, DWORD &pixRateNext, int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;

#if 0
		if(!_isCameraType(Edge)) {
			pixRate = 0;
			error = -1;
			return;
		}
#endif

		PCO_FN2(error, msg,PCO_GetPixelRate, m_handle, &m_pcoData->dwPixelRate);
		PCO_THROW_OR_TRACE(error, msg) ;

		pixRate = m_pcoData->dwPixelRate;

		pixRateNext = ((m_pcoData->dwPixelRateRequested != 0) && (pixRate != m_pcoData->dwPixelRateRequested)) ?
			m_pcoData->dwPixelRateRequested : pixRate;
}



//=================================================================================================
//=================================================================================================
void Camera::_pco_GetHWIOSignal(int &errorTot){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	int error;
	errorTot = 0;
	char *msg;

	int i, imax;
	
	if(! _isCapsDesc(capsHWIO) ) {
		errorTot = -1;
		return;
	}

	PCO_FN2(error, msg, PCO_GetHWIOSignalCount, m_handle, &m_pcoData->wNrPcoHWIOSignal0);
	errorTot |= error;

	imax = m_pcoData->wNrPcoHWIOSignal = 
		(m_pcoData->wNrPcoHWIOSignal0 <= SIZEARR_stcPcoHWIOSignal) ? m_pcoData->wNrPcoHWIOSignal0 : SIZEARR_stcPcoHWIOSignal;

	//DEB_ALWAYS()  << "--- size" << DEB_VAR3(imax, m_pcoData->wNrPcoHWIOSignal0 , m_pcoData->wNrPcoHWIOSignal ) ;

	for(i=0; i< imax; i++) {
		//DEB_ALWAYS()  << "---  descriptor" << DEB_VAR2(i, m_pcoData->stcPcoHWIOSignalDesc[i].wSize) ;
		PCO_FN3(error, msg,PCO_GetHWIOSignalDescriptor, m_handle, i, &m_pcoData->stcPcoHWIOSignalDesc[i]);
		errorTot |= error;
		//DEB_ALWAYS()  << "---  signal" << DEB_VAR2(i, m_pcoData->stcPcoHWIOSignal[i].wSize) ;
		PCO_FN3(error, msg,PCO_GetHWIOSignal, m_handle, i, &m_pcoData->stcPcoHWIOSignal[i]);
		errorTot |= error;
	}
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


void Camera::_pco_initHWIOSignal(int mode, int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;

	int  _err, idx;
	error = 0;
	char *name;
	WORD val;


	if(!_isCapsDesc(capsHWIO)  ) {
		error = -1;
		return;
	}

	_pco_GetHWIOSignal(_err); error |= _err;

	//	name[Acquire Enable] idx[0] num[0]
	idx = 0; val = 2;
	name = m_pcoData->stcPcoHWIOSignalDesc[idx].strSignalName[0];
	m_pcoData->stcPcoHWIOSignal[idx].wPolarity = 2;

	_pco_SetHWIOSignal(idx, _err); error |= _err;

	DEB_ALWAYS() << "set PcoHWIOSignal polarity "  << DEB_VAR3(name, idx, val) ;


}



//=================================================================================================
//=================================================================================================
char* Camera::_pco_SetDelayExposureTime(int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;

    DWORD dwExposure, dwDelay;
	WORD wExposure_base, wDelay_base;
    double _exposure, _delay;
    m_sync->getExpTime(_exposure);
    m_sync->getLatTime(_delay);
	double _delay0 = _delay;

	_pco_time2dwbase(_exposure, dwExposure, wExposure_base);
	_pco_time2dwbase(_delay,  dwDelay, wDelay_base);

	PCO_FN5(error, msg,PCO_SetDelayExposureTime, m_handle, dwDelay, dwExposure, wDelay_base, wExposure_base);

	m_pcoData->traceAcq.dLimaExposure = _exposure;
	m_pcoData->traceAcq.dLimaDelay = _delay;

	m_pcoData->traceAcq.iPcoExposure = dwExposure;
	m_pcoData->traceAcq.iPcoDelay = dwDelay;
	m_pcoData->traceAcq.iPcoExposureBase = wExposure_base;
	m_pcoData->traceAcq.iPcoDelayBase = wDelay_base;


	if(error || _getDebug(DBG_EXP)) {
		DEB_ALWAYS() << DEB_VAR3(_exposure, dwExposure, wExposure_base);
		DEB_ALWAYS() << DEB_VAR3(_delay,  dwDelay, wDelay_base);
	}

	if(error) {
		return msg;
	}

	return fnId;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetHWIOSignal(int sigNum, int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;


		if(!_isCapsDesc(capsHWIO)  || 
			(sigNum < 0) || (sigNum >= m_pcoData->wNrPcoHWIOSignal) ) {
			error = -1;
			return;
		}

		PCO_FN3(error, msg,PCO_SetHWIOSignal, m_handle, sigNum, &m_pcoData->stcPcoHWIOSignal[sigNum]);
		
}
//=================================================================================================
//=================================================================================================
char *Camera::_pco_GetTemperatureInfo(int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char msg[MSG_SIZE + 1];
	char *pcoFn;



	// -- Print out current temperatures
	PCO_FN4(error, pcoFn,PCO_GetTemperature, m_handle, &m_pcoData->temperature.wCcd, &m_pcoData->temperature.wCam, &m_pcoData->temperature.wPower);
	if(error) return pcoFn;
	//PCO_THROW_OR_TRACE(error, "PCO_GetTemperature") ;

	sprintf_s(msg, MSG_SIZE, "* temperature: CCD[%.1f]  CAM[%d]  PS[%d]\n", m_pcoData->temperature.wCcd/10., m_pcoData->temperature.wCam, m_pcoData->temperature.wPower);
	//DEB_TRACE() <<   msg;
	m_log.append(msg);


	m_pcoData->temperature.wMinCoolSet = m_pcoData->stcPcoDescription.sMinCoolSetDESC;
	m_pcoData->temperature.wMaxCoolSet = m_pcoData->stcPcoDescription.sMaxCoolSetDESC;

	sprintf_s(msg, MSG_SIZE, "* cooling temperature: MIN [%d]  Max [%d]\n",  m_pcoData->temperature.wMinCoolSet, m_pcoData->temperature.wMaxCoolSet);
	//DEB_TRACE() <<   msg;
	m_log.append(msg);

	// -- Set/Get cooling temperature
	if (m_pcoData->temperature.wSetpoint != -1) {
		if (m_pcoData->temperature.wSetpoint < m_pcoData->temperature.wMinCoolSet)	m_pcoData->temperature.wSetpoint = m_pcoData->temperature.wMinCoolSet;
		if (m_pcoData->temperature.wSetpoint > m_pcoData->temperature.wMaxCoolSet)	m_pcoData->temperature.wSetpoint= m_pcoData->temperature.wMaxCoolSet;
	} else {
		PCO_FN2(error, pcoFn,PCO_GetCoolingSetpointTemperature, m_handle, &m_pcoData->temperature.wSetpoint);
		if(error) return pcoFn;
		//PCO_THROW_OR_TRACE(error, "PCO_GetCoolingSetpointTemperature") ;
	}
	sprintf_s(msg, MSG_SIZE, "* Cooling Setpoint = %d\n", m_pcoData->temperature.wSetpoint);
	//DEB_TRACE() <<   msg;
	m_log.append(msg);


	return fnId;
}


//=================================================================================================
//=================================================================================================

/**************************************************************************************************
	If a set recording status = [stop] command is sent and the current status is already
	[stop]�ped, nothing will happen (only warning, error message). 
	
	If the camera is in
	[run]�ing state, it will last some time (system delay + last image readout), until the
	camera is stopped. The system delay depends on the PC and the image readout
	depends on the image size transferred. The SetRecordingState = [stop] checks for a
	stable stop state by calling GetRecordingState.  --- 165 ms 
	
	Please call PCO_CancelImages to remove pending buffers from the driver.   --- 1.5 s
**************************************************************************************************/

char * Camera::_pco_SetRecordingState(int state, int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;
	LARGE_INTEGER usStart;


	WORD wRecState_new, wRecState_actual;

	wRecState_new = state ? 0x0001 : 0x0000 ; // 0x0001 => START acquisition

	usElapsedTimeSet(usStart);

	PCO_FN2(error, msg,PCO_GetRecordingState, m_handle, &wRecState_actual);
	PCO_PRINT_ERR(error, msg); 	if(error) return msg;

	_setCameraState(CAMSTATE_RECORD_STATE, !!(wRecState_actual));

	m_pcoData->traceAcq.usTicks[8].value = usElapsedTime(usStart);
	m_pcoData->traceAcq.usTicks[8].desc = "PCO_GetRecordingState execTime";
	usElapsedTimeSet(usStart);

	//if(wRecState_new == wRecState_actual) {error = 0; return fnId; }


	// ------------------------------------------ cancel images 
	if(wRecState_new == 0) {
		int count = 1;

		_setCameraState(CAMSTATE_RECORD_STATE, false);

#if 0
		PCO_FN2(error, msg,PCO_GetPendingBuffer, m_handle, &count);
		PCO_PRINT_ERR(error, msg); 	if(error) return msg;
#endif
		if(count) {
			DEB_ALWAYS() << ":  PCO_CancelImages() - CALLING";
			PCO_FN1(error, msg,PCO_CancelImages, m_handle);
			PCO_PRINT_ERR(error, msg); 	if(error) return msg;
		} else {
			DEB_ALWAYS() << ":  PCO_CancelImages() - BYPASSED";
		}
	}


	if(wRecState_new != wRecState_actual) 
	{
		DEB_ALWAYS() << fnId << ": PCO_SetRecordingState " << DEB_VAR1(wRecState_new);
		PCO_FN2(error, msg,PCO_SetRecordingState, m_handle, wRecState_new);
		PCO_PRINT_ERR(error, msg); 	if(error) return msg;
	}

	if(wRecState_new) 
		m_sync->setExposing(pcoAcqRecordStart);

	PCO_FN2(error, msg,PCO_GetRecordingState, m_handle, &wRecState_actual);
	PCO_PRINT_ERR(error, msg); 	if(error) return msg;

	_setCameraState(CAMSTATE_RECORD_STATE, !!(wRecState_actual));

	m_pcoData->traceAcq.usTicks[9].value = usElapsedTime(usStart);
	m_pcoData->traceAcq.usTicks[9].desc = "PCO_SetRecordingState execTime";
	usElapsedTimeSet(usStart);

	_armRequired(true);

	m_pcoData->traceAcq.usTicks[10].value = usElapsedTime(usStart);
	m_pcoData->traceAcq.usTicks[10].desc = "PCO_CancelImages execTime";
	usElapsedTimeSet(usStart);

	//DEB_ALWAYS() << fnId << ": " << DEB_VAR4(error, state, wRecState_actual, wRecState_new);
	return fnId;

}
//=================================================================================================
//=================================================================================================
char *Camera::_pco_SetMetaDataMode(WORD wMetaDataMode, int &error){
		
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;
	WORD _wMetaDataMode, _wMetaDataSize, _wMetaDataVersion ;

	m_pcoData->wMetaDataSize = m_pcoData->wMetaDataVersion = m_pcoData->wMetaDataMode = 0;

	if(!m_pcoData->bMetaDataAllowed) return fnId;

	PCO_FN4(error, msg, PCO_GetMetaDataMode, m_handle, &_wMetaDataMode, &_wMetaDataSize, &_wMetaDataVersion);
	PCO_PRINT_ERR(error, msg); 	if(error) return msg;

	m_pcoData->wMetaDataSize =  _wMetaDataSize;
	m_pcoData->wMetaDataVersion = _wMetaDataVersion;
	m_pcoData->wMetaDataMode = _wMetaDataMode;
	
	// now pco edge also allows metatada (not only dimax)
	//if(_isCameraType(Dimax)) {

	PCO_FN4(error, msg,PCO_SetMetaDataMode, m_handle, wMetaDataMode, &_wMetaDataSize, &_wMetaDataVersion);
	PCO_PRINT_ERR(error, msg); 	if(error) return msg;

	m_pcoData->wMetaDataSize =  _wMetaDataSize;
	m_pcoData->wMetaDataVersion = _wMetaDataVersion;
	m_pcoData->wMetaDataMode = wMetaDataMode;
	
	return fnId;
}
//=================================================================================================
//=================================================================================================
char *Camera::_pco_GetCOCRuntime(int &error){
		
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;

	//====================================== get the coc runtime 
    //---- only valid if it was used PCO_SetDelayExposureTime
	//---- and AFTER armed the cam

	// Get and split the 'camera operation code' runtime into two DWORD. One will hold the longer
	// part, in seconds, and the other will hold the shorter part, in nanoseconds. This function can be
	// used to calculate the FPS. The sum of dwTime_s and dwTime_ns covers the delay, exposure and
	// readout time. If external exposure is active, it returns only the readout time.

	DWORD dwTime_s, dwTime_ns;
    double runTime;

    PCO_FN3(error, msg,PCO_GetCOCRuntime, m_handle, &dwTime_s, &dwTime_ns);
    PCO_PRINT_ERR(error, msg); 	if(error) return msg;

    m_pcoData->cocRunTime = runTime = ((double) dwTime_ns * NANO) + (double) dwTime_s;
    m_pcoData->frameRate = (dwTime_ns | dwTime_s) ? 1.0 / runTime : 0.0;

    DEB_TRACE() << DEB_VAR2(m_pcoData->frameRate, m_pcoData->cocRunTime);

	return fnId;

}
//=================================================================================================
//=================================================================================================
char *Camera::_pco_GetCameraType(int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;

	int errTot = 0;
	bool errTotPcoSdk = false;

	m_pcoData->frames_per_buffer = 1; // for PCO DIMAX

	// --- Get camera type
	{
		
		char *ptr;

		//m_pcoData->stcPcoCamType.wSize= sizeof(m_pcoData->stcPcoCamType);

		// OBSOLETE function after sdk 120
		//error = PCO_GetGigEIPAddress(m_handle, &m_pcoData->ipField[0], &m_pcoData->ipField[1], &m_pcoData->ipField[2], &m_pcoData->ipField[3]);
		//if(error) {m_pcoData->ipField[0] = m_pcoData->ipField[1] = m_pcoData->ipField[2] =m_pcoData->ipField[3]= 0;}
		m_pcoData->ipField[0] = m_pcoData->ipField[1] = m_pcoData->ipField[2] =m_pcoData->ipField[3]= 0;


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

		sprintf_s(m_pcoData->camera_name, CAMERA_NAME_SIZE, "%s (%s) (I/F %s) (SN %d)", 
			_getCameraTypeStr(), 
			_getCameraSubTypeStr(), 
			_getInterfaceTypeStr(), 
			_getCameraSerialNumber());
		DEB_ALWAYS() <<  DEB_VAR2(_getCameraTypeStr(), _getInterfaceTypeStr())
			<< "\n"
			<< "\n====================== CAMERA FOUND ======================"
			<< "\n* "  << m_pcoData->camera_name
			<< "\n==========================================================" 
			<< "\n"
			;

		//if(errTot) return m_pcoData->camera_name;

	}

	// -- Reset to default settings

	PCO_FN2(error, msg,PCO_SetRecordingState, m_handle, 0);
	errTotPcoSdk = errTotPcoSdk || error; //if(error) return msg;


	PCO_FN1(error, msg,PCO_ResetSettingsToDefault, m_handle);
	PCO_PRINT_ERR(error, msg); 	
	errTotPcoSdk = errTotPcoSdk || error; //if(error) return msg;
	

	// callback to update in lima the valid_ranges from the last stcPcoDescription read
	if(m_sync) {
		HwSyncCtrlObj::ValidRangesType valid_ranges;
		m_sync->getValidRanges(valid_ranges);		// from stcPcoDescription
		m_sync->validRangesChanged(valid_ranges);	// callback
		DEB_ALWAYS() << fnId << ": callback - new valid_ranges: " << DEB_VAR1(valid_ranges);
	}
	
    // get the max CAMERA pixel rate (Hz) from the description structure
	m_pcoData->dwPixelRateMax = 0;
	for(int i=0; i<4; i++) {
		if(m_pcoData->dwPixelRateMax < m_pcoData->stcPcoDescription.dwPixelRateDESC[i])
					m_pcoData->dwPixelRateMax = m_pcoData->stcPcoDescription.dwPixelRateDESC[i];
	}	

	DWORD capsDesc1;
	_pco_GetGeneralCapsDESC(capsDesc1, error);

	m_pcoData->bMetaDataAllowed = !!(capsDesc1 & GENERALCAPS1_METADATA) ;



	if(errTotPcoSdk)
	{
		DEB_ALWAYS() <<  "ERRORs in SDK functions!!!";
	}
	
	if(errTot)
	{
		DEB_ALWAYS() <<  "ERRORs in types!!!";
	}


	return fnId;
}
//=================================================================================================
//=================================================================================================
void Camera::_pco_SetTimestampMode(WORD mode, int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;
	int error;
	err = error = 0;

    WORD modeNew, modeOld, modeMax;
    
	DWORD capsDesc1; 
	_pco_GetGeneralCapsDESC(capsDesc1, error);

	if(capsDesc1 & BIT8)
	{
	    DEB_ALWAYS() << "\n   timestampmode not allowed" ;
		err = -1;
		return;
	}
	modeMax = (capsDesc1 & BIT3) ? 3 : 2;
	if(mode > modeMax)
	{
	    DEB_ALWAYS() << "\n  invalid value" << DEB_VAR2(mode, modeMax);
		err = -1;
		return;
	}

	_pco_GetTimestampMode(modeOld, err);
	if(err) return;

	if(mode == modeOld)
	{
	    DEB_ALWAYS()<< "\n   no change " << DEB_VAR2(mode, modeOld); 
		return;
	}

	PCO_FN2(error, msg,PCO_SetTimestampMode,m_handle, mode);
	PCO_PRINT_ERR(error, msg); 	
	err |= error;
	if(err) return;

	_pco_GetTimestampMode(modeNew, err);
	if(err) return;

    DEB_ALWAYS() << "\n   " << DEB_VAR3(mode, modeOld, modeNew) 
        ;

    return;
}

void Camera::_pco_GetTimestampMode(WORD &mode, int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;

	char *msg;
	int error;
	err = error = 0;
    
	PCO_FN2(error, msg,PCO_GetTimestampMode,m_handle, &mode);
	PCO_PRINT_ERR(error, msg); 	
	err |= error;
	if(err) return;

    DEB_ALWAYS() << "\n   " << DEB_VAR1(mode);

    return;
}

//=================================================================================================
//=================================================================================================

void Camera::_pco_GetGeneralCapsDESC(DWORD &capsDesc1, int &err)
{
	err = 0;
	capsDesc1 = m_pcoData->stcPcoDescription.dwGeneralCapsDESC1;
	return;
}

	
//=================================================================================================
//=================================================================================================

void Camera::_pco_GetTransferParameter(void* buffer, int ilen, int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *pcoFn;

	err = 0;
	PCO_FN3(err, pcoFn,PCO_GetTransferParameter, m_handle, buffer, ilen);
	return;
}

//=================================================================================================
//=================================================================================================

//SC2_SDK_FUNC int WINAPI PCO_GetSegmentImageSettings(HANDLE ph, WORD wSegment,
//                                                    WORD* wXRes,
//                                                    WORD* wYRes,
//                                                    WORD* wBinHorz,
//                                                    WORD* wBinVert,
//                                                    WORD* wRoiX0,
//                                                    WORD* wRoiY0,
//                                                    WORD* wRoiX1,
//                                                    WORD* wRoiY1);
// Gets the sizes information for one segment. X0, Y0 start at 1. X1, Y1 end with max. sensor size.
// In: HANDLE ph -> Handle to a previously opened camera.
//     WORD *wXRes -> Pointer to a WORD variable to receive the x resolution of the image in segment
//     WORD *wYRes -> Pointer to a WORD variable to receive the y resolution of the image in segment
//     WORD *wBinHorz -> Pointer to a WORD variable to receive the horizontal binning of the image in segment
//     WORD *wBinVert -> Pointer to a WORD variable to receive the vertical binning of the image in segment
//     WORD *wRoiX0 -> Pointer to a WORD variable to receive the left x offset of the image in segment
//     WORD *wRoiY0 -> Pointer to a WORD variable to receive the upper y offset of the image in segment
//     WORD *wRoiX1 -> Pointer to a WORD variable to receive the right x offset of the image in segment
//     WORD *wRoiY1 -> Pointer to a WORD variable to receive the lower y offset of the image in segment
//      x0,y0----------|
//      |     ROI      |
//      ---------------x1,y1
// Out: int -> Error message.
//SC2_SDK_FUNC int WINAPI PCO_GetNumberOfImagesInSegment(HANDLE ph, 
//                                             WORD wSegment,
//                                             DWORD* dwValidImageCnt,
//                                             DWORD* dwMaxImageCnt);
// Gets the number of images in the addressed segment.
// In: HANDLE ph -> Handle to a previously opened camera.
//     DWORD *dwValidImageCnt -> Pointer to a DWORD varibale to receive the valid image count.
//     DWORD *dwMaxImageCnt -> Pointer to a DWORD varibale to receive the max image count.
// Out: int -> Error message.


void Camera::_pco_GetSegmentInfo(int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	//char *pcoFn;
	int pcoErr;
	err = 0;
	struct stcSegmentInfo *_stc;

	err = 0;
	if(!_isCameraType(Dimax | Pco2k | Pco4k))
	{
		err = 1;
		return;
	}

	for(int iseg = 0; iseg <  PCO_MAXSEGMENTS; iseg++)
	{

		_stc = &m_pcoData->m_stcSegmentInfo[iseg];
		
		_stc->iSegId = iseg+1;
		_stc->wXRes = _stc->wYRes = 
			_stc->wBinHorz = _stc->wBinVert = 
			_stc->wRoiX0 = _stc->wRoiY0  = 
			_stc->wRoiX1 = _stc->wRoiY1 = 0;
			_stc->dwValidImageCnt = _stc->dwMaxImageCnt = 0;
			_stc->iErr = 0;

		pcoErr = PCO_GetSegmentImageSettings(m_handle, iseg,
			&_stc->wXRes, &_stc->wYRes,
			&_stc->wBinHorz, &_stc->wBinVert,
			&_stc->wRoiX0, &_stc->wRoiY0,
			&_stc->wRoiX1, &_stc->wRoiY1);

			_stc->iErr |= pcoErr;
			err |= pcoErr;

		pcoErr = PCO_GetNumberOfImagesInSegment(m_handle, iseg,
			&_stc->dwValidImageCnt, &_stc->dwMaxImageCnt);
	
			_stc->iErr |= pcoErr;
			err |= pcoErr;

	}

	return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetNumberOfImagesInSegment(WORD wSegment, DWORD& dwValidImageCnt, DWORD& dwMaxImageCnt, int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	//char *pcoFn;
	int pcoErr;
	err = 0;

	err = 0;
	if(!_isCameraType(Dimax | Pco2k | Pco4k))
	{
		err = 1;
		return;
	}


		pcoErr = PCO_GetNumberOfImagesInSegment(m_handle, wSegment,
			&dwValidImageCnt, &dwMaxImageCnt);
	
			err |= pcoErr;


	return;
}


//=================================================================================================
//=================================================================================================
//SC2_SDK_FUNC int WINAPI PCO_GetCDIMode(HANDLE ph, WORD *wCDIMode); 
// Gets the correlated double image mode of the camera, if available.
// Only available with a dimax
// In: HANDLE ph -> Handle to a previously opened camera.
//     WORD *wCDIMode -> Pointer to a WORD variable to receive the correlated double image mode.
// Out: int -> Error message.

//SC2_SDK_FUNC int WINAPI PCO_SetCDIMode(HANDLE ph,  WORD wCDIMode); 
// Sets the correlated double image mode of the camera, if available.
// Only available with a dimax
// In: HANDLE ph -> Handle to a previously opened camera.
//     WORD wCDIMode -> WORD variable to set the correlated double image mode.
// Out: int -> Error message.
// CDI mode is available if in the camera descriptor the flag

//Mode parameter is simple: 0 is off (default), 1 is on.
// CDI mode is available if in the camera descriptor the flag
// #define GENERALCAPS1_CDI_MODE                          0x00010000 // Camera has Correlated Double Image Mode
// in GENERALCAPS1 is set.


void Camera::_pco_GetCDIMode(WORD &wCDIMode, int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	//char *pcoFn;
	int pcoErr;
	err = 0;

	if(!_isCapsDesc(capsCDI))
	{
		wCDIMode = 0;
		err = 1;
		return;
	}
	
	err = 0;
	pcoErr = PCO_GetCDIMode(m_handle, &wCDIMode);
	
	err |= pcoErr;

	return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetCDIMode(WORD wCDIMode, int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	//char *pcoFn;
	int pcoErr;
	err = 0;

	if(!_isCapsDesc(capsCDI))
	{
		err = 1;
		return;
	}
	
	if(wCDIMode) 
	{
		wCDIMode = 1;
		_pco_SetDoubleImageMode(0, err); 		// cdi and double image are exclusive
	}

	err = 0;
	pcoErr = PCO_SetCDIMode(m_handle, wCDIMode);
	
	err |= pcoErr;

	return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_GetDoubleImageMode(WORD &wDoubleImage, int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	//char *pcoFn;
	int pcoErr;
	err = 0;

	if(!_isCapsDesc(capsDoubleImage))
	{
		wDoubleImage = 0;
		err = 1;
		return;
	}
	
	err = 0;
	pcoErr = PCO_GetDoubleImageMode(m_handle, &wDoubleImage);
	
	err |= pcoErr;

	return;
}

//=================================================================================================
//=================================================================================================
void Camera::_pco_SetDoubleImageMode(WORD wDoubleImage, int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	//char *pcoFn;
	int pcoErr;
	err = 0;

	if(!_isCapsDesc(capsDoubleImage))
	{
		err = 1;
		return;
	}
	
	if(wDoubleImage) 
	{
		wDoubleImage = 1;
		_pco_SetCDIMode(0, err);		// cdi and double image are exclusive
	}

	err = 0;
	pcoErr = PCO_SetDoubleImageMode(m_handle, wDoubleImage);
	err |= pcoErr;

	return;
}





//=================================================================================================
//=================================================================================================

void Camera::_pco_FillStructures(int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	//char *pcoFn;
	int pcoErr;
	err = 0;

	// PCO_GETGENERAL(hCam, &strGeneral)

	// -- Get General
	m_pcoData->stcPcoGeneral.wSize= sizeof(m_pcoData->stcPcoGeneral);
	m_pcoData->stcPcoGeneral.strCamType.wSize= sizeof(m_pcoData->stcPcoGeneral.strCamType);

	pcoErr = PCO_GetGeneral(m_handle, &m_pcoData->stcPcoGeneral);
	err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetGeneral");

	// PCO_GETCAMERATYPE(hCam, &strCamType)

	m_pcoData->stcPcoCamType.wSize = sizeof(m_pcoData->stcPcoCamType);
	pcoErr = PCO_GetCameraType(m_handle, &m_pcoData->stcPcoCamType);
	err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetCameraType");

	// PCO_GETSENSORSTRUCT(hCam, &strSensor)

	// -- Get Sensor struct
	m_pcoData->stcPcoSensor.wSize= sizeof(m_pcoData->stcPcoSensor);
	m_pcoData->stcPcoSensor.strDescription.wSize= sizeof(m_pcoData->stcPcoSensor.strDescription);
	m_pcoData->stcPcoSensor.strDescription2.wSize= sizeof(m_pcoData->stcPcoSensor.strDescription2);

	pcoErr = PCO_GetSensorStruct(m_handle, &m_pcoData->stcPcoSensor);
	err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetSensorStruct");


	// PCO_GETCAMERADESCRIPTION(hCam, &strDescription)

	// -- Get camera description
	m_pcoData->stcPcoDescription.wSize= sizeof(m_pcoData->stcPcoDescription);

	pcoErr = PCO_GetCameraDescription(m_handle, &m_pcoData->stcPcoDescription);
	err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetCameraDescription"); 	

	// PCO_GETTIMINGSTRUCT(hCam, &strTiming)

	// -- Get timing struct
	m_pcoData->stcPcoTiming.wSize= sizeof(m_pcoData->stcPcoTiming);

	pcoErr = PCO_GetTimingStruct(m_handle, &m_pcoData->stcPcoTiming);
	err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetTimingStruct");

	// PCO_GETRECORDINGSTRUCT(hCam, &strRecording)

	// -- Get recording struct
	m_pcoData->stcPcoRecording.wSize= sizeof(m_pcoData->stcPcoRecording);

	pcoErr = PCO_GetRecordingStruct(m_handle, &m_pcoData->stcPcoRecording);
	err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetRecordingStruct");


	// -- Get storage struct
	m_pcoData->stcPcoStorage.wSize= sizeof(m_pcoData->stcPcoStorage);

	pcoErr = PCO_GetStorageStruct(m_handle, &m_pcoData->stcPcoStorage);
	err |= PCO_CHECK_ERROR(pcoErr, "PCO_GetStorageStruct");

}

//=================================================================================================
//=================================================================================================
void Camera::_pco_CloseCamera(int &err)
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	//char *pcoFn;
	int pcoErr;
	
	//PCO_PRINT_ERR(error, msg); 

	err = 0;
	pcoErr = PCO_CloseCamera(m_handle);
	err |= PCO_CHECK_ERROR(pcoErr, "PCO_CloseCamera");

	m_handle = NULL;

	return;
}
