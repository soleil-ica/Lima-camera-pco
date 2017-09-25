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

#define PCO_ERRT_H_CREATE_OBJECT
#define BYPASS

#define TOUT_MIN_DIMAX 500
#define ERROR_MSG_LINE 128

//#define BUFF_INFO_SIZE 5000


#include <cstdlib>
#include <process.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <time.h>

#include "lima/Exceptions.h"
#include "lima/HwSyncCtrlObj.h"

#include "PcoCamera.h"
#include "PcoSyncCtrlObj.h"
#include "PcoBufferCtrlObj.h"
#include "PcoCameraUtils.h"


using namespace lima;
using namespace lima::Pco;

static char *timebaseUnits[] = {"ns", "us", "ms"};

//char *_checkLogFiles();
void _pco_acq_thread_dimax(void *argin);
void _pco_acq_thread_dimax_trig_single(void *argin);

void _pco_acq_thread_dimax_live(void *argin);
void _pco_acq_thread_ringBuffer(void *argin);

void _pco_acq_thread_edge(void *argin);
void _pco_shutter_thread_edge(void *argin);

const char * _timestamp_pcosyncctrlobj();
const char * _timestamp_pcointerface();
const char * _timestamp_pcobufferctrlobj();
const char * _timestamp_pcodetinfoctrlobj();
const char * _timestamp_pcocamerautils();
const char * _timestamp_pcoroictrlobj();

char *_split_date(const char *s);

char *str_trim_left(char *s);
char *str_trim_right(char *s);
char *str_trim(char *s) ;
char *str_toupper(char *s);




//=========================================================================================================
//=========================================================================================================
const char* _timestamp_pcocamera() {return ID_FILE_TIMESTAMP ;}


#ifdef WITH_GIT_VERSION
#include "PcoGitVersion.h"

#ifndef PCO_SDK_VERSION
#define PCO_SDK_VERSION "sdkVersion - not set"
#endif

char * _timestamp_gitversion(char *buffVersion, int len)
{
	sprintf_s(buffVersion, len, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n", 
				 PCO_GIT_VERSION,
				 PCO_SDK_VERSION,
				 PROCLIB_GIT_VERSION,
				 LIBCONFIG_GIT_VERSION,
				 LIMA_GIT_VERSION,
				 TANGO_GIT_VERSION,
				 SPEC_GIT_VERSION
				 );
	return buffVersion;
}
#endif


//=========================================================================================================

//=========================================================================================================
//=========================================================================================================
const char *Camera::xlatCode2Str(int code, struct stcXlatCode2Str *stc) {

	const char *type;

	while( (type = stc->str) != NULL) {
		if(stc->code == code) return type;
		stc++;
	}

	return NULL;

}

//=========================================================================================================
//=========================================================================================================


const char * Camera::_xlatPcoCode2Str(int code, enumTblXlatCode2Str table, int &err) {
	DEB_CONSTRUCTOR();
	struct stcXlatCode2Str modelType[] = {
		{CAMERATYPE_PCO1200HS, "PCO 1200 HS"},
		{CAMERATYPE_PCO1300, "PCO 1300"},
		{CAMERATYPE_PCO1400, "PCO 1400"},
		{CAMERATYPE_PCO1600, "PCO 1600"},
		{CAMERATYPE_PCO2000, "PCO 2000"},
		{CAMERATYPE_PCO4000, "PCO 4000"},
		{CAMERATYPE_PCO_DIMAX_STD, "PCO DIMAX STD"},
		{CAMERATYPE_PCO_DIMAX_TV, "PCO DIMAX TV"},
		{CAMERATYPE_PCO_DIMAX_AUTOMOTIVE, "PCO DIMAX AUTOMOTIVE"},
		{CAMERATYPE_PCO_EDGE, "PCO EDGE 5.5 RS"},
		{CAMERATYPE_PCO_EDGE_42, "PCO EDGE 4.2 RS"},
		{CAMERATYPE_PCO_EDGE_GL, "PCO EDGE 5.5 GL"},
		{CAMERATYPE_PCO_EDGE_USB3, "PCO EDGE USB3"},
		{CAMERATYPE_PCO_EDGE_HS, "PCO EDGE hs"},
		{CAMERATYPE_PCO_EDGE, "PCO EDGE"},
		{CAMERATYPE_PCO_EDGE_GL, "PCO EDGE GL"},
		{0, "NO_modelType"},
		{0, NULL}
	};


	struct stcXlatCode2Str modelSubType[] = {
		{CAMERATYPE_PCO1200HS, "PCO 1200 HS"},
		{CAMERASUBTYPE_PCO_DIMAX_Weisscam, "DIMAX_Weisscam"},
		{CAMERASUBTYPE_PCO_DIMAX_HD, "DIMAX_HD"},
		{CAMERASUBTYPE_PCO_DIMAX_HD_plus, "DIMAX_HD_plus"},
		{CAMERASUBTYPE_PCO_DIMAX_X35, "DIMAX_X35"},
		{CAMERASUBTYPE_PCO_DIMAX_HS1, "DIMAX_HS1"},
		{CAMERASUBTYPE_PCO_DIMAX_HS2, "DIMAX_HS2"},
		{CAMERASUBTYPE_PCO_DIMAX_HS4, "DIMAX_HS4"},

		{CAMERASUBTYPE_PCO_EDGE_SPRINGFIELD, "EDGE_SPRINGFIELD"},
		{CAMERASUBTYPE_PCO_EDGE_31, "EDGE_31"},
		{CAMERASUBTYPE_PCO_EDGE_42, "EDGE_42"},
		{CAMERASUBTYPE_PCO_EDGE_55, "EDGE_55"},
		{CAMERASUBTYPE_PCO_EDGE_DEVELOPMENT, "EDGE_DEVELOPMENT"},
		{CAMERASUBTYPE_PCO_EDGE_X2, "EDGE_X2"},
		{CAMERASUBTYPE_PCO_EDGE_RESOLFT, "EDGE_RESOLFT"},
		{CAMERASUBTYPE_PCO_EDGE_GOLD, "EDGE_GOLD"},
		{CAMERASUBTYPE_PCO_EDGE_DUAL_CLOCK, "DUAL_CLOCK"},
		{CAMERASUBTYPE_PCO_EDGE_DICAM, "DICAM"},
		{CAMERASUBTYPE_PCO_EDGE_42_LT, "EDGE_42_LT"},
		{0, "NO_subType"},
		{0, NULL}
	};

	struct stcXlatCode2Str interfaceType[] = {
		{INTERFACE_FIREWIRE, "FIREWIRE"},
		{INTERFACE_CAMERALINK, "CAMERALINK"},
		{INTERFACE_USB, "USB"},
		{INTERFACE_ETHERNET, "ETHERNET"},
		{INTERFACE_SERIAL, "SERIAL"},
		{INTERFACE_USB3, "USB3"},
		{INTERFACE_CAMERALINKHS, "CAMERALINK_HS"},
		{INTERFACE_COAXPRESS, "COAXPRESS"},
		{0, "NO_interfaceType"},
		{0, NULL}
	};

  struct stcXlatCode2Str *stc;
	const char *ptr;
	static char buff[BUFF_XLAT_SIZE+1];
	const char *errTable ;

  switch(table) {
	case ModelType: stc = modelType; errTable = "modelType" ; break;
	case ModelSubType: stc = modelSubType;  errTable = "modelSubType" ; break;
	case InterfaceType: stc = interfaceType; errTable = "interfaceType" ;  break;

    default:
  		sprintf_s(buff, BUFF_XLAT_SIZE, "UNKNOWN XLAT TABLE [%d]", table);
  		err = 1;
	  	return buff;
  }

	if((ptr = xlatCode2Str(code, stc)) != NULL) {
		err = 0;
		return ptr;
	} 

	if((stc == modelSubType) && _isCameraType(Dimax))
	{
		switch(code)
		{
		  case   0:  ptr = "STD";      break;
		  case 100:  ptr = "OEM Variant 100";         break;
		  case 200:  ptr = "OEM Variant 200";         break;
		  default:
			switch(code / 0x100)
			{
			  case 0x01:  ptr = "S1";       break;
			  case 0x02:  ptr = "S2";       break;
			  case 0x04:  ptr = "S4";       break;
	 
			  case 0x80:  ptr = "HD";       break;
			  case 0xC0:  ptr = "HD+";      break;
	 
			  case 0x20:  ptr = "HS1";      break;
			  case 0x21:  ptr = "HS2";      break;
			  case 0x23:  ptr = "HS4";      break;
			  
			  default: ptr = NULL;
			} // switch(camtype.wCamSubType / 0x100) ...
		} // switch(camtype.wCamSubType) ...

		if(ptr)
		{
			sprintf_s(buff, BUFF_XLAT_SIZE, "subType DIMAX %s code [0x%04x]", ptr, code);
			err = 0;
			return buff;
	
		}
	}

	
	{
		sprintf_s(buff, BUFF_XLAT_SIZE, "UNKNOWN %s code [0x%04x]", errTable, code);
		err = 1;
		return buff;
	}
}



//=========================================================================================================
//=========================================================================================================

#define BUFFER_LEN 256
#define BUFFVERSION_LEN ( MSG8K )
stcPcoData::stcPcoData(){

	char *ptr, *ptrMax;
	int i;
	char buff[BUFFER_LEN+1];
	
	memset(this, 0, sizeof(struct stcPcoData));

	version = new char[BUFFVERSION_LEN];
	if(!version)
	{
		throw LIMA_HW_EXC(Error, "version > creation error");
	}

	ptr = version; *ptr = 0;
	ptrMax = ptr + BUFFVERSION_LEN - 1;

	ptr += sprintf_s(ptr, ptrMax - ptr, "\n");
	ptr += sprintf_s(ptr, ptrMax - ptr, "%s\n", _split_date(_timestamp_pcocamera()));
	ptr += sprintf_s(ptr, ptrMax - ptr, "%s\n", _split_date(_timestamp_pcosyncctrlobj()));
	ptr += sprintf_s(ptr, ptrMax - ptr, "%s\n", _split_date(_timestamp_pcointerface()));
	ptr += sprintf_s(ptr, ptrMax - ptr, "%s\n", _split_date(_timestamp_pcobufferctrlobj()));
	ptr += sprintf_s(ptr, ptrMax - ptr, "%s\n", _split_date(_timestamp_pcodetinfoctrlobj()));
	ptr += sprintf_s(ptr, ptrMax - ptr, "%s\n", _split_date(_timestamp_pcocamerautils()));
	ptr += sprintf_s(ptr, ptrMax - ptr, "%s\n", _split_date(_timestamp_pcoroictrlobj()));

#ifdef WITH_GIT_VERSION
	char buffVersion[BUFFVERSION_LEN+1];
	ptr += sprintf_s(ptr, ptrMax - ptr, "%s\n", _timestamp_gitversion(buffVersion, BUFFVERSION_LEN));
#endif

	ptr += sprintf_s(ptr, ptrMax - ptr, "       timestamp: %s\n", getTimestamp(Iso));
	ptr += sprintf_s(ptr, ptrMax - ptr, "   computer name: %s\n", _getComputerName(buff, BUFFER_LEN));
	ptr += sprintf_s(ptr, ptrMax - ptr, "       user name: %s\n", _getUserName(buff, BUFFER_LEN));
	ptr += sprintf_s(ptr, ptrMax - ptr, "VS configuration: %s\n", _getVSconfiguration(buff, BUFFER_LEN));
	
#ifndef __linux__
	ptr += sprintf_s(ptr, ptrMax - ptr, " PCO SDK version: %s\n", _getPcoSdkVersion(buff, BUFFER_LEN, (char *) "sc2_cam.dll"));
	ptr += sprintf_s(ptr, ptrMax - ptr, "                  %s\n", _getPcoSdkVersion(buff, BUFFER_LEN, (char *) "sc2_cl_me4.dll"));
	ptr += sprintf_s(ptr, ptrMax - ptr, "                  %s\n", _getPcoSdkVersion(buff, BUFFER_LEN, (char *) "sc2_clhs.dll"));
	ptr += sprintf_s(ptr, ptrMax - ptr, "    lima pco dll: %s\n", _getDllPath(FILE_PCO_DLL, buff, BUFFER_LEN));
#else
	ptr += sprintf_s(ptr, ptrMax - ptr, " PCO SDK version: %s\n", _getPcoSdkVersion(buff, BUFFER_LEN, (char *) "sc2_cam.dll"));
#endif


	stcPcoGeneral.wSize = sizeof(stcPcoGeneral);
	stcPcoGeneral.strCamType.wSize = sizeof(stcPcoGeneral.strCamType);
	stcPcoCamType.wSize = sizeof(stcPcoCamType);
	stcPcoSensor.wSize = sizeof(stcPcoSensor);
	stcPcoSensor.strDescription.wSize = sizeof(stcPcoSensor.strDescription);
	stcPcoSensor.strDescription2.wSize = sizeof(stcPcoSensor.strDescription2);
	stcPcoDescription.wSize = sizeof(stcPcoDescription);
	stcPcoTiming.wSize = sizeof(stcPcoTiming);
	stcPcoStorage.wSize = sizeof(stcPcoStorage);
	stcPcoRecording.wSize = sizeof(stcPcoRecording);

	for(i=0; i < SIZEARR_stcPcoHWIOSignal; i++) {
		stcPcoHWIOSignal[i].wSize = sizeof(stcPcoHWIOSignal[i]);
		stcPcoHWIOSignalDesc[i].wSize = sizeof(stcPcoHWIOSignalDesc[i]);
	}

	bAllocatedBufferDone = 
		false;
	
	msAcqRecTimestamp = msAcqXferTimestamp =
			getTimestamp();

}

//=========================================================================================================
//=========================================================================================================
bool Camera::paramsGet(const char *key, char *&value) {
	//DEF_FNID;
	DEB_CONSTRUCTOR();
	bool ret;

	for(int i = 0; i < m_pcoData->params.nr; i++) {
		if(_stricmp(key, m_pcoData->params.ptrKey[i]) == 0) {
			ret = true;
			value = m_pcoData->params.ptrValue[i];
			DEB_TRACE() << DEB_VAR3(key, ret, value);
			return ret;
		}
	}
	ret = false;
	value = (char *) ""; 
	DEB_TRACE() << DEB_VAR3(key, ret, value);
	return ret;
}


//=========================================================================================================
//=========================================================================================================

// params string:
//      "key1 = value1 ; key2 = value2 ; key3 ; ...."

void Camera::paramsInit(const char *str) 
{
	DEF_FNID;
	DEB_CONSTRUCTOR();

	DEB_TRACE() << _sprintComment(false, fnId, "[ENTRY]")
		<< DEB_VAR1(str);

	int i;
	char *tokNext = NULL;
	char *buff = m_pcoData->params.buff;
	int &nrList = m_pcoData->params.nr;
	int nr;
	
	// --- split of params string
	strcpy_s(buff, PARAMS_LEN_BUFF , str);
	char *ptr = buff;
	
	for(nr = i = 0; i < PARAMS_NR; i++) {
		if( (m_pcoData->params.ptrKey[i] = strtok_s(ptr, ";", &tokNext)) == NULL) break;
		ptr = NULL;
		nr = i+1;
	}

	nrList = 0;

	for(i = 0; i < nr; i++) {
		char *key, *value;
		bool found;

		ptr = str_trim(m_pcoData->params.ptrKey[i]);
		key = strtok_s(ptr, "=", &tokNext);
		value = strtok_s(NULL, "=", &tokNext);
		str_toupper(key);	
		m_pcoData->params.ptrKey[i] = key = str_trim(key);	
		value = str_trim(value);	
		if(value == NULL) value = (char *) "";
		m_pcoData->params.ptrValue[i] = value;	
		
		found = false;
		for(int j = 0; j < nrList; j++) {
			if(_stricmp(m_pcoData->params.ptrKey[j], m_pcoData->params.ptrKey[i]) == 0){
				m_pcoData->params.ptrValue[j] = m_pcoData->params.ptrValue[i];
				found = true;
				break;
			}
		}
		if(!found) {
			key = m_pcoData->params.ptrKey[nrList] = m_pcoData->params.ptrKey[i]; 
			value = m_pcoData->params.ptrValue[nrList] = m_pcoData->params.ptrValue[i] ;
			nrList++;
		}
	}

	for(int j = 0; j < nrList; j++) {
		char *key, *value;
		key = m_pcoData->params.ptrKey[j];
		value = m_pcoData->params.ptrValue[j];
		DEB_TRACE() << DEB_VAR2(key, value);
	}
	DEB_TRACE() << _sprintComment(false, fnId, "[EXIT]");
};

//=========================================================================================================
//=========================================================================================================

Camera::Camera(const char *params) :
	m_cam_connected(false),
	m_acq_frame_nb(1),
	m_sync(NULL),
	m_buffer(NULL),
	m_handle(NULL)
{
	DEF_FNID;
	DEB_CONSTRUCTOR();

	int error=0;
	m_config = TRUE;
	DebParams::checkInit();

	
	m_msgLog = new ringLog(300) ;
	m_tmpLog = new ringLog(300) ;
	if(m_msgLog == NULL)
		throw LIMA_HW_EXC(Error, "m_msgLog > creation error");
	if(m_tmpLog == NULL)
		throw LIMA_HW_EXC(Error, "m_tmpLog > creation error");

	m_pcoData =new stcPcoData();
	if(m_pcoData == NULL)
		throw LIMA_HW_EXC(Error, "m_pcoData > creation error");



	// properties: params 
	paramsInit(params);

	char *value;
	const char  *key;
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
	if(ret) {m_pcoData->testCmdMode = _atoi64(value);}

	key = "acqTimeoutRetry";
	ret = paramsGet(key, value);
	iValue = ret ? atoi(value) : 3;
	m_pcoData->acqTimeoutRetry = (iValue < 0 ) ? 0 : iValue;

	// patch in xMinSize meanwhile firmware for CLHS 1.19 is fixed
	key = "xMinSize";
	m_pcoData->params_xMinSize = !!paramsGet(key, value);

	// ignore the nbMaxImages calculated for dimax HS
	key = "ignoreMaxImages";
	m_pcoData->params_ignoreMaxImages = !!paramsGet(key, value);


	DEB_ALWAYS()
		<< ALWAYS_NL << DEB_VAR1(m_pcoData->version) 
		<< ALWAYS_NL << _checkLogFiles(true);

	//m_bin.changed = Invalid;
	
	_init();
	m_config = FALSE;
	_setActionTimestamp(tsConstructor);
}


//=========================================================================================================
//=========================================================================================================
void Camera::_init(){
	DEB_CONSTRUCTOR();
	DEF_FNID;

	DEB_ALWAYS() << _sprintComment(false, fnId, "[ENTRY]");

	char msg[MSG4K + 1];
	//char *pMsg;
	int error=0;

	_armRequired(true);

	m_log.clear();
	sprintf_s(msg, sizeof(msg), "*** Pco log %s\n", getTimestamp(Iso));
	m_log.append(msg);

	DEB_TRACE() <<_getDllPath(FILE_PCO_DLL, msg, sizeof(msg));


	const char *key = "sn";
	char *value;
	bool bRet = paramsGet(key, value);
	DWORD dwSn = (DWORD) (bRet ? atoi(value) : 0);


		// --- Open Camera - close before if it is open
	if(m_handle) {
		DEB_TRACE() << fnId << " [closing opened camera]";
		_pco_CloseCamera(error);
		PCO_THROW_OR_TRACE(error, "_init(): PCO_CloseCamera - closing opened cam") ;
		m_handle = 0;
	}

	int retrySleep =15;
	int retryMax = 0;
	int retry = retryMax;
	while(true) {
		//_pco_OpenCamera(error);
		_pco_OpenCameraSn(dwSn, error);
		if(error)
		{
			if(retry--<=0) break;
			DEB_ALWAYS() << "\n... ERROR - PCO_OpenCamera / retry ... " << DEB_VAR2(retry, retrySleep);
			::Sleep(retrySleep * 1000);
		} else {break;}
	} 

	if(error)
	{ 
		DEB_ALWAYS() << "\n... ERROR - PCO_OpenCamera / abort" ; 
		THROW_HW_ERROR(Error) << "ERROR - PCO_OpenCamera";
	}
		
	DEB_TRACE() << "_init(): PCO_OpenCamera" ;
	
	_pco_GetCameraType(error);
	PCO_THROW_OR_TRACE(error, "_pco_GetCameraType") ;


	DEB_ALWAYS() 
			<< "\n"
			<< "\n====================== CAMERA FOUND ======================"
			<< "\n* "  << _getCameraIdn()
			<< "\n==========================================================" 
			<< "\n"
			;



	DEB_TRACE() << fnId << " [camera opened] " << DEB_VAR1(m_handle);

	// -- Initialise ADC
	//-------------------------------------------------------------------------------------------------
	// PCO_SetADCOperation
    // Set analog-digital-converter (ADC) operation for reading the image sensor data. Pixel data can be
    // read out using one ADC (better linearity) or in parallel using two ADCs (faster). This option is
    // only available for some camera models. If the user sets 2ADCs he must center and adapt the ROI
    // to symmetrical values, e.g. pco.1600: x1,y1,x2,y2=701,1,900,500 (100,1,200,500 is not possible).
    //
	// DIMAX -> 1 adc
	//-------------------------------------------------------------------------------------------------
	
	// set ADC = 1 for better linearity (when it is configurable ...)
	int adc_working;
	_pco_SetADCOperation(1, adc_working);
	m_pcoData->wNowADC= (WORD) adc_working;

	// set alignment to [LSB aligned]; all raw image data will be aligned to the LSB.

	_pco_SetBitAlignment(1);

		// -- Initialise size, bin, roi
	unsigned int maxWidth, maxHeight,maxwidth_step, maxheight_step; 
	getMaxWidthHeight(maxWidth, maxHeight);
	getXYsteps(maxwidth_step, maxheight_step);



	WORD bitsPerPix;
	getBitsPerPixel(bitsPerPix);

	sprintf_s(msg, MSG_SIZE, "* CCD Size = X[%d] * Y[%d] (%d bits)\n", maxWidth, maxHeight, bitsPerPix);
	DEB_TRACE() <<   msg;
	m_log.append(msg);
	
	sprintf_s(msg, MSG_SIZE, "* ROI Steps = x:%d, y:%d\n", maxwidth_step, maxheight_step);
	DEB_TRACE() <<   msg;
	m_log.append(msg);

	_pco_GetTemperatureInfo(error);
	PCO_THROW_OR_TRACE(error, "_pco_GetTemperatureInfo") ;

	_pco_SetRecordingState(0, error);
	
	if(_isCameraType(Dimax)) _init_dimax();
	else if(_isCameraType(Pco2k)) _init_dimax();
	else if(_isCameraType(Pco4k)) _init_dimax();
	else if(_isCameraType(Edge)) _init_edge();
	else {
		char msg[MSG_SIZE+1];
		sprintf_s(msg, MSG_SIZE, "Camera type not supported! [x%04x]", _getCameraType());
		DEB_ALWAYS() <<  msg;

		throw LIMA_HW_EXC(Error, msg);
	}


	m_cam_connected = true;
	error = 0;

	if(!m_cam_connected)
		throw LIMA_HW_EXC(Error, "Camera not found!");

	_pco_initHWIOSignal(0, 0x4, error);

	_pco_SetCameraToCurrentTime(error);


	DEB_TRACE() << m_log;
	DEB_TRACE() << "END OF CAMERA";
	DEB_ALWAYS() << _sprintComment(false, fnId, "[EXIT]");

}

//=========================================================================================================
//=========================================================================================================
void  Camera::_init_dimax() {

	DEB_CONSTRUCTOR();
	char msg[MSG_SIZE + 1];
	const char *pcoFn;

	int error=0;
	DWORD _dwValidImageCnt, _dwMaxImageCnt;

	
	// block #1 -- Get RAM size
	{
		int segmentPco, segmentArr;

		DWORD ramSize;
		WORD pageSize;

		WORD bitsPerPix;
		getBitsPerPixel(bitsPerPix);

		_pco_GetCameraRamSize(ramSize, pageSize, error);
		PCO_THROW_OR_TRACE(error, "_pco_GetCameraRamSize") ;

		m_pcoData->dwRamSize = ramSize;     // nr of pages of the ram
		m_pcoData->wPixPerPage = pageSize;    // nr of pixels of the page

		sprintf_s(msg, MSG_SIZE, "* ramPages[%d] pixPerPage[%d] bitsPerPix[%d]\n",  
				m_pcoData->dwRamSize, m_pcoData->wPixPerPage, bitsPerPix);
		DEB_TRACE() <<   msg;
		m_log.append(msg);
		
		double nrBytes = (double) m_pcoData->dwRamSize  * (double) m_pcoData->wPixPerPage * 
			(double)bitsPerPix / 9.; // 8 bits data + 1 bit CRC -> 9
		
		sprintf_s(msg, MSG_SIZE, "* camMemorySize [%lld B] [%g GB]\n",  
				(long long int) nrBytes, nrBytes/GIGABYTE);
		DEB_TRACE() <<   msg;
		m_log.append(msg);

		// ----------------- get initial seg Size - images & print

		// ---- get the size in pages of each of the 4 segments

		DWORD   segSize[4];
		_pco_GetCameraRamSegmentSize(segSize, error);
		PCO_THROW_OR_TRACE(error, "_pco_GetCameraRamSegmentSize") ;

		for(segmentArr=0; segmentArr < PCO_MAXSEGMENTS ; segmentArr++) {
			segmentPco = segmentArr +1;		// PCO segment (1 ... 4)
			m_pcoData->dwSegmentSize[segmentArr] = segSize[segmentArr];

			sprintf_s(msg, MSG_SIZE, "* segment[%d] number of pages[%d]\n", segmentPco,m_pcoData->dwSegmentSize[segmentArr]);
			DEB_TRACE() <<   msg;
			m_log.append(msg);

		}

		//---- get nr de images in each segment & nr max of img on each segmente
		for(segmentArr=0;  segmentArr< PCO_MAXSEGMENTS ; segmentArr++) {
			segmentPco = segmentArr +1;

			_pco_GetNumberOfImagesInSegment(segmentPco, _dwValidImageCnt, _dwMaxImageCnt, error);
			PCO_THROW_OR_TRACE(error, pcoFn) ;

			m_pcoData->dwValidImageCnt[segmentArr] = _dwValidImageCnt;
			m_pcoData->dwMaxImageCnt[segmentArr] = _dwMaxImageCnt;

			sprintf_s(msg, MSG_SIZE, "* segment[%d] nr images [%d]  max imag [%d]\n", segmentPco, _dwValidImageCnt,  _dwMaxImageCnt);
			DEB_TRACE() <<   msg;
			m_log.append(msg);

		} // for	


		// set the first segment to the max ram size, the others = 0
		// This function will result in all segments being cleared. 
		// All previously recorded images will be lost!

		//m_pcoData->dwSegmentSize[0] = m_pcoData->dwRamSize;
		for(segmentArr=1;  segmentArr < PCO_MAXSEGMENTS ; segmentArr++) {
			m_pcoData->dwSegmentSize[0] += m_pcoData->dwSegmentSize[segmentArr];
			m_pcoData->dwSegmentSize[segmentArr] = 0;
		}
		sprintf_s(msg, MSG_SIZE, "* m_pcoData->dwSegmentSize0 [%d]  m_pcoData->dwRamSize [%d]\n", m_pcoData->dwSegmentSize[0], m_pcoData->dwRamSize);
		DEB_TRACE() <<   msg;
		m_log.append(msg);


		_pco_SetCameraRamSegmentSize(&m_pcoData->dwSegmentSize[0], error);
		PCO_THROW_OR_TRACE(error, pcoFn) ;
	}  // block #1 

	DEB_TRACE() <<  "end block 1 / get initial seg Size - images";

	{
		int segmentPco, segmentArr;
		
		unsigned int maxWidth, maxHeight; 
		getMaxWidthHeight(maxWidth, maxHeight);

		
		DWORD pages_per_image = maxWidth * maxHeight / m_pcoData->wPixPerPage;

		///------------------------------------------------------------------------TODO ?????
		for(segmentArr=0; segmentArr < PCO_MAXSEGMENTS ; segmentArr++) {
			segmentPco = segmentArr +1;
			if(m_pcoData->dwMaxImageCnt[segmentArr] == 0){
				m_pcoData->dwMaxImageCnt[segmentArr] = m_pcoData->dwSegmentSize[segmentArr] / pages_per_image;
				if(m_pcoData->dwMaxImageCnt[segmentArr] > 4) m_pcoData->dwMaxImageCnt[segmentArr] -= 2;
			}
		}	
	} // block


	// -- Get Active RAM segment 

		WORD wActSeg; _pco_GetActiveRamSegment(wActSeg, error);

		_pco_GetNumberOfImagesInSegment(m_pcoData->wActiveRamSegment, _dwValidImageCnt, _dwMaxImageCnt, error);
		PCO_THROW_OR_TRACE(error, pcoFn) ;



	DEB_TRACE() <<  "original DONE";


}


//=========================================================================================================
//=========================================================================================================
void Camera::_init_edge() {

	m_pcoData->fTransferRateMHzMax = 550.;



}



//=========================================================================================================
//=========================================================================================================
Camera::~Camera()
{
	DEB_DESTRUCTOR();
	DEB_TRACE() << "DESTRUCTOR ...................." ;

	m_cam_connected = false;

	reset(RESET_CLOSE_INTERFACE);
}


//=================================================================================================
//=================================================================================================
void Camera::prepareAcq()
{
    DEB_MEMBER_FUNCT();
	DEF_FNID;

	DEB_ALWAYS() << _sprintComment(false, fnId, "[ENTRY]") << _checkLogFiles();

	int error;

    int iRequestedFrames;
			// live video requested frames = 0
    m_sync->getNbFrames(iRequestedFrames);

    TrigMode trig_mode;
    m_sync->getTrigMode(trig_mode);


 	//------------------------------------------------- check bin
	// info only, hw already set
	Bin binActual;

	_pco_GetBinning(binActual, error);
	
	m_pcoData->traceAcq.iPcoBinHorz = binActual.getX();
	m_pcoData->traceAcq.iPcoBinVert = binActual.getY();
	
	DEB_TRACE() << "_pco_GetBinning "<< DEB_VAR1(binActual);
    //------------------------------------------------- 

    //------------------------------------------------- check roi
	// info only, hw already set

	unsigned int x0, x1, y0, y1;\
	Roi roiNow;

	_pco_GetROI(roiNow, error);
	
	_xlatRoi_lima2pco(roiNow, x0, x1, y0, y1);

	m_pcoData->traceAcq.iPcoRoiX0 = m_pcoData->wRoiX0Now = x0;
	m_pcoData->traceAcq.iPcoRoiX1 = m_pcoData->wRoiX1Now = x1;
	m_pcoData->traceAcq.iPcoRoiY0 = m_pcoData->wRoiY0Now = y0;
	m_pcoData->traceAcq.iPcoRoiY1 = m_pcoData->wRoiY1Now = y1;

	DEB_TRACE() 
			<< "\n   PCO_GetROI> " << DEB_VAR5(x0, x1, y0, y1, roiNow);

	_armRequired(true);

    //------------------------------------------------- 

	//------------------------------------------------- set CDI if needed
	{
		WORD cdi;
		int err;
		_pco_GetCDIMode(cdi, err);
	}

    //------------------------------------------------- 


	//------------------------------------------------- triggering mode 
    //------------------------------------- acquire mode : ignore or not ext. signal
	_pco_SetTriggerMode_SetAcquireMode(error);
    PCO_THROW_OR_TRACE(error, "_pco_SetTriggerMode_SetAcquireMode") ;

    //------------------------------------------------- 

    // ----------------------------------------- storage mode (recorder + sequence)
    if(_isCameraType(Dimax | Pco4k | Pco2k)) {

               enumPcoStorageMode mode;

               if((trig_mode  == ExtTrigSingle) && (iRequestedFrames > 0)) {
                       mode = RecRing;
               } else {
                        // live video requested frames = 0
                       mode = (iRequestedFrames > 0) ? RecSeq : Fifo;
               }

               DEB_TRACE() << "\n>>> set storage/recorder mode - DIMAX 2K 4K: " << DEB_VAR1(mode);

		_pco_SetStorageMode_SetRecorderSubmode(mode, error);
		PCO_THROW_OR_TRACE(error, "_pco_SetStorageMode_SetRecorderSubmode") ;
	}

#if 0
	if(_isCameraType(Pco4k | Pco2k)) {
			// live video requested frames = 0
		enumPcoStorageMode mode = Fifo;
		DEB_TRACE() << "PcoStorageMode mode - PCO2K / 4K: " << DEB_VAR1(mode);

		_pco_SetStorageMode_SetRecorderSubmode(mode, error);
		PCO_THROW_OR_TRACE(error, "_pco_SetStorageMode_SetRecorderSubmode") ;
	}
#endif

    //------------------------------------------------- 

	//----------------------------------- set exposure time & delay time
	_pco_SetDelayExposureTime(error);   // initial set of delay (phase = 0)
	PCO_THROW_OR_TRACE(error, "_pco_SetDelayExposureTime") ;
    //------------------------------------------------- 


    //------------------------------------------------- check recording state
    WORD state;


	state = _pco_GetRecordingState(error);
    PCO_THROW_OR_TRACE(error, "_pco_GetRecordingState") ;

    if (state>0) {
        DEB_TRACE() << "Force recording state to 0x0000" ;

		_pco_SetRecordingState(0, error);
        PCO_THROW_OR_TRACE(error, "PCO_SetRecordingState") ;
	}

    //------------------------------------------------- 



	// ----------------------------------------- set Record Stop Event (used for dimax for ExtTrigSingle)
	if(_isCameraType(Dimax)) {
		WORD wRecordStopEventMode;
		DWORD dwRecordStopDelayImages;

		if((trig_mode  == ExtTrigSingle) && (iRequestedFrames > 0)) {
			wRecordStopEventMode = 0x0002;    // record stop by edge at the <acq. enbl.>
			dwRecordStopDelayImages = iRequestedFrames;
			DEB_TRACE() << "..... PCO_SetRecordStopEvent";
			_pco_SetRecordStopEvent(wRecordStopEventMode, dwRecordStopDelayImages, error);
			PCO_THROW_OR_TRACE(error, "PCO_SetRecordStopEvent") ;
		}
	}


    //------------------------------------------------- 



//-----------------------------------------------------------------------------------------------
//	5. Arm the camera.
//	6. Get the sizes and allocate a buffer:
//		PCO_GETSIZES(hCam, &actualsizex, &actualsizey, &ccdsizex, &ccdsizey)
//		PCO_ALLOCATEBUFFER(hCam, &bufferNr, actualsizex * actualsizey * sizeof(WORD), &data, &hEvent)
//		In case of CamLink and GigE interface: PCO_CamLinkSetImageParameters(actualsizex, actualsizey)
//		PCO_ArmCamera(hCam)
//-----------------------------------------------------------------------------------------------
	
	//--------------------------- metadata
	_pco_SetMetaDataMode(0, error); 
	PCO_THROW_OR_TRACE(error, "_pco_SetMetaDataMode") ;
 
	//--------------------------- PREPARE / pixel rate - ARM required 
	_pco_SetPixelRate(error); 
	PCO_THROW_OR_TRACE(error, "_pco_SetPixelRate") ;
    //------------------------------------------------- 
		
	//--------------------------- PREPARE / clXferParam, LUT - ARM required 
	_pco_SetTransferParameter_SetActiveLookupTable(error); 
	PCO_THROW_OR_TRACE(error, "_pco_SetTransferParameter_SetActiveLookupTable") ;
    //------------------------------------------------- 

#ifndef __linux__
	//--------------------------- PREPARE / ARM  
	DEB_TRACE() << "\n   ARM the camera / PCO_ArmCamera (1)";
	_pco_ArmCamera(error); 
	PCO_THROW_OR_TRACE(error, "_pco_ArmCamera") ;

    //------------------------------------------------- 


	//--------------------------- PREPARE / getSizes (valid after ARM) alloc buffer
	
	//_pco_GetSizes(&m_pcoData->wXResActual, &m_pcoData->wYResActual, &m_pcoData->wXResMax, &m_pcoData->wYResMax, error);
	//PCO_THROW_OR_TRACE(error, "_pco_GetSizes") ;

	m_buffer->_pcoAllocBuffers(false);
    //------------------------------------------------- 

	//--------------------------- PREPARE / img parameters
	_pco_SetImageParameters(error); 
	PCO_THROW_OR_TRACE(error, "_pco_SetImageParameters") ;
    //------------------------------------------------- 

#endif

	//--------------------------- PREPARE / cocruntime (valid after PCO_SetDelayExposureTime and ARM)
	_pco_GetCOCRuntime(error); 
	PCO_THROW_OR_TRACE(error, "_pco_GetCOCRuntime") ;

    //------------------------------------------------- 

#ifndef __linux__
    //------------------------------------------------- checking nr of frames for cams with memory

#if 0
	if(_isCameraType(Dimax)){
        unsigned long framesMax;
        framesMax = _pco_GetNumberOfImagesInSegment_MaxCalc(m_pcoData->wActiveRamSegment);

        if ((((unsigned long) iRequestedFrames) > framesMax)) {
            throw LIMA_HW_EXC(Error, "frames OUT OF RANGE");
        }
    } 
#endif

#if 0
	unsigned long ulFramesMaxInSegment = _pco_GetNumberOfImagesInSegment_MaxCalc(m_pcoData->wActiveRamSegment);
	unsigned long ulRequestedFrames = (unsigned long) iRequestedFrames;

	
	if(ulFramesMaxInSegment > 0)
	{
		WORD wDoubleImage;
		int err;

		// Double Image -> requested images will be the total nr of images (basic + primary)
		//      must be even and twice of the nr of images for pco
		_pco_GetDoubleImageMode(wDoubleImage, err);

		bool bOutOfRange = false;

		if(wDoubleImage) 
		{
			if ( ((ulRequestedFrames % 2) != 0) || (ulRequestedFrames/2 > ulFramesMaxInSegment) ) bOutOfRange = true;
		}
		else
		{
			if ( ulRequestedFrames > ulFramesMaxInSegment ) bOutOfRange = true;
		}

		if(bOutOfRange)
		{

			DEB_ALWAYS() << "\nERROR frames OUT OF RANGE " << DEB_VAR3(ulRequestedFrames, ulFramesMaxInSegment, wDoubleImage);
			{
				Event *ev = new Event(Hardware,Event::Error,Event::Camera,Event::CamNoMemory, "ERROR frames OUT OF RANGE");
				_getPcoHwEventCtrlObj()->reportEvent(ev);
			}
				m_sync->setStarted(false);
				return;
		}
	}
#endif




    //------------------------------------------------- checking nr of frames for cams with memory
#endif

}
//=================================================================================================
//=================================================================================================
void Camera::startAcq()
{
    DEB_MEMBER_FUNCT();

	m_acq_frame_nb = -1;
	m_pcoData->pcoError = 0;
	m_pcoData->pcoErrorMsg[0] = 0;

	m_pcoData->traceAcqClean();

	TIME_USEC tStart;
	msElapsedTimeSet(tStart);


//=====================================================================
	DEF_FNID;
    HANDLE hEvent= NULL;

	DEB_ALWAYS() << _sprintComment(false, fnId, "[ENTRY]") << _checkLogFiles();

	int error;


	//------------------------------------------------- start acquisition

	m_pcoData->traceAcq.msStartAcqStart = msElapsedTime(tStart);

	m_sync->setStarted(true);
	//m_sync->setExposing(pcoAcqRecordStart);
	m_sync->setExposing(pcoAcqStart);
	
    int iRequestedFrames;
    m_sync->getNbFrames(iRequestedFrames);

	unsigned long ulFramesMaxInSegment = _pco_GetNumberOfImagesInSegment_MaxCalc(m_pcoData->wActiveRamSegment);
	unsigned long ulRequestedFrames = (unsigned long) iRequestedFrames;

	if(ulRequestedFrames > 0)
	{
		WORD wDoubleImage;
		int err;

		// Double Image -> requested images will be the total nr of images (basic + primary)
		//      must be even and twice of the nr of images for pco
		_pco_GetDoubleImageMode(wDoubleImage, err);

		bool bOutOfRange = false;

		if( (wDoubleImage) && ((ulRequestedFrames % 2) != 0) ) bOutOfRange = true;
		if((ulFramesMaxInSegment >0) && (ulRequestedFrames > ulFramesMaxInSegment)) bOutOfRange = true;


		if(bOutOfRange)
		{

			DEB_ALWAYS() << "\nERROR frames OUT OF RANGE " << DEB_VAR3(ulRequestedFrames, ulFramesMaxInSegment, wDoubleImage);
			{
				Event *ev = new Event(Hardware,Event::Error,Event::Camera,Event::CamNoMemory, "ERROR frames OUT OF RANGE");
				_getPcoHwEventCtrlObj()->reportEvent(ev);
			}
				m_sync->setStarted(false);
				m_sync->setExposing(pcoAcqError);
				return;
		}
	}

	if(!_isRunAfterAssign())
	{
		DEB_TRACE() << "========================= recordingState 1 - BEFORE ASSIGN (startAcq)";
		_pco_SetRecordingState(1, error);
	}

	if(_isCameraType(Edge)){

		_beginthread( _pco_acq_thread_edge, 0, (void*) this);

#if 0
		AutoMutex lock(m_cond.mutex());

		bool resWait;
		int retry = 3;
		int val, val0; val0 = pcoAcqRecordStart;

		while( ((val =  m_sync->getExposing()) != val0) && retry--)
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

#if 0
	if(_isCameraType(Pco2k | Pco4k)){
		_beginthread( _pco_acq_thread_ringBuffer, 0, (void*) this);
		m_pcoData->traceAcq.msStartAcqEnd = msElapsedTime(tStart);
		return;
	}
#endif

	if(_isCameraType(Dimax | Pco2k | Pco4k)){
	    int iRequestedFrames;
		m_sync->getNbFrames(iRequestedFrames);

	    TrigMode trig_mode;
		m_sync->getTrigMode(trig_mode);
		_pco_SetRecordingState(1, error);
		if(iRequestedFrames > 0 ) {
			if((trig_mode  == ExtTrigSingle) ) {
				_beginthread( _pco_acq_thread_dimax_trig_single, 0, (void*) this);
			} else {
				_beginthread( _pco_acq_thread_dimax, 0, (void*) this);	// normal mode
			}
		} else {
			_beginthread( _pco_acq_thread_dimax_live, 0, (void*) this);
		}
		m_pcoData->traceAcq.msStartAcqEnd = msElapsedTime(tStart);
		return;
	}

	throw LIMA_HW_EXC(Error, "unkown camera type");
	return;
}


//==========================================================================================================
//==========================================================================================================

long msElapsedTime(TIME_USEC &t0) {
	TIME_USEC tNow;
	_ftime64_s(&tNow);

	return (long)((tNow.time - t0.time)*1000) + (tNow.millitm - t0.millitm);
}

void msElapsedTimeSet(TIME_USEC &t0) {
	_ftime64_s(&t0);
}


double usElapsedTimeTicsPerSec() {
	LARGE_INTEGER ticksPerSecond;

	QueryPerformanceFrequency(&ticksPerSecond); 
	return (double) ticksPerSecond.QuadPart;

}

//==========================================================================================================
//==========================================================================================================
void _pco_acq_thread_dimax(void *argin) {
	DEF_FNID;

	static char msgErr[LEN_ERROR_MSG+1];

	int error;
	int _nrStop;
	DWORD _dwPcoValidImageCnt, _dwPcoMaxImageCnt;

	Camera* m_cam = (Camera *) argin;
	SyncCtrlObj* m_sync = m_cam->_getSyncCtrlObj();
	//BufferCtrlObj* m_buffer = m_sync->_getBufferCtrlObj();
	BufferCtrlObj* m_buffer = m_cam->_getBufferCtrlObj();

	char _msg[LEN_MSG + 1];
    sprintf_s(_msg, LEN_MSG, "%s> [ENTRY]", fnId);
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

	WORD wSegment; m_cam->_pco_GetActiveRamSegment(wSegment, error ); 
	double msPerFrame = (m_cam->pcoGetCocRunTime() * 1000.);
	m_pcoData->traceAcq.msImgCoc = msPerFrame;

	//DWORD dwMsSleepOneFrame = (DWORD) (msPerFrame + 0.5);	// 4/5 rounding
	DWORD dwMsSleepOneFrame = (DWORD) (msPerFrame/5.0);	// 4/5 rounding
	if(dwMsSleepOneFrame == 0) dwMsSleepOneFrame = 1;		// min sleep

	WORD wDoubleImage;
	int err;
	m_cam->_pco_GetDoubleImageMode(wDoubleImage, err);


	bool nb_frames_fixed = false;

	int iLimaRequestedFrames; 	m_sync->getNbFrames(iLimaRequestedFrames);
	m_pcoData->traceAcq.nrImgRequested0 = iLimaRequestedFrames;
	WORD dwPcoRequestedFrames = wDoubleImage ? iLimaRequestedFrames/2 : iLimaRequestedFrames;
	

	m_sync->setAcqFrames(0);

	timeout = timeout0 = (long) (msPerFrame * (iLimaRequestedFrames * 1.3));	// 30% guard
	if(timeout < TOUT_MIN_DIMAX) timeout = TOUT_MIN_DIMAX;
    
	m_pcoData->traceAcq.msTout = m_pcoData->msAcqTout = timeout;
	_dwPcoValidImageCnt = 0;

	m_sync->getExpTime(m_pcoData->traceAcq.sExposure);
	m_sync->getLatTime(m_pcoData->traceAcq.sDelay);

	m_sync->setExposing(pcoAcqRecordStart);

	while(true) {
		m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwPcoValidImageCnt, _dwPcoMaxImageCnt, error);
		if(error) {
			printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, "_pco_GetNumberOfImagesInSegment");
			throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
		}

		m_pcoData->dwValidImageCnt[wSegment-1] = 
			m_pcoData->traceAcq.nrImgRecorded = _dwPcoValidImageCnt;
		m_pcoData->dwMaxImageCnt[wSegment-1] =
			m_pcoData->traceAcq.maxImgCount = _dwPcoMaxImageCnt;

		m_pcoData->msAcqTnow = msNowRecordLoop = msElapsedTime(tStart);
		m_pcoData->traceAcq.msRecordLoop = msNowRecordLoop;
		
		if( (dwPcoRequestedFrames > _dwPcoMaxImageCnt) ){
			nb_frames_fixed = true;
			
			sprintf_s(msgErr,LEN_ERROR_MSG, 
				"=== %s [%d]> ERROR INVALID NR FRAMES fixed nb_frames[%d] _dwPcoMaxImageCnt[%d]", 
				fnId, __LINE__, dwPcoRequestedFrames, _dwPcoMaxImageCnt);
			printf("%s\n", msgErr);

			m_sync->setExposing(pcoAcqError);
			break;
		}

		if(  (_dwPcoValidImageCnt >= dwPcoRequestedFrames)) break;

		if((timeout < msNowRecordLoop) && !m_pcoData->bExtTrigEnabled) { 
			//m_sync->setExposing(pcoAcqRecordTimeout);
			//m_sync->stopAcq();
			m_sync->setExposing(pcoAcqStop);
			printf("=== %s [%d]> TIMEOUT!!! tout[(%ld) 0(%ld)] recLoopTime[%ld ms] lastImgRecorded[%ld] dwPcoRequestedFrames[%d]\n", 
				fnId, __LINE__, timeout, timeout0, msNowRecordLoop, _dwPcoValidImageCnt, dwPcoRequestedFrames);
			break;
		}
	
		if((requestStop = m_sync->_getRequestStop(_nrStop))  == stopRequest) {
			m_sync->_setRequestStop(stopNone);
		
			char msg[LEN_TRACEACQ_MSG+1];
				//m_buffer->_setRequestStop(stopProcessing);
				//m_sync->setExposing(pcoAcqStop);
				
			snprintf(msg,LEN_TRACEACQ_MSG, "=== %s> STOP REQ (recording). lastImgRec[%d]\n", fnId, _dwPcoValidImageCnt);
				printf(msg);
				m_pcoData->traceMsg(msg);
				break;
		}
		Sleep(dwMsSleepOneFrame);	// sleep 1 frame
	} // while(true)

	m_pcoData->msAcqTnow = msNowRecordLoop = msElapsedTime(tStart);
	m_pcoData->traceAcq.msRecordLoop = msNowRecordLoop;

	msg = m_cam->_pco_SetRecordingState(0, error);
	if(error) {
		printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
		throw LIMA_HW_EXC(Error, "_pco_SetRecordingState");
	}

	if( (requestStop != stopRequest) && (!nb_frames_fixed)) {
		if(m_sync->getExposing() == pcoAcqRecordStart) m_sync->setExposing(pcoAcqRecordEnd);

		m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwPcoValidImageCnt, _dwPcoMaxImageCnt, error);
		if(error) {
			printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
			throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
		}

		m_pcoData->dwValidImageCnt[wSegment-1] = 
			m_pcoData->traceAcq.nrImgRecorded = _dwPcoValidImageCnt;

		dwPcoAcquiredFrames = (_dwPcoValidImageCnt < dwPcoRequestedFrames) ? _dwPcoValidImageCnt : dwPcoRequestedFrames;
		iLimaAcquiredFrames = wDoubleImage ? dwPcoAcquiredFrames*2 : dwPcoAcquiredFrames;
		//m_sync->setAcqFrames(nb_acq_frames);

		// dimax recording time
		m_pcoData->msAcqRec = msRecord = msElapsedTime(tStart);
		m_pcoData->traceAcq.msRecord = msRecord;    // loop & stop record
		
		m_pcoData->traceAcq.endRecordTimestamp = m_pcoData->msAcqRecTimestamp = getTimestamp();
		
		m_pcoData->traceAcq.nrImgAcquired = iLimaAcquiredFrames;
		m_pcoData->traceAcq.nrImgRequested = iLimaRequestedFrames;

		msElapsedTimeSet(tStart);  // reset for xfer

		if(iLimaAcquiredFrames < iLimaRequestedFrames) m_sync->setNbFrames(iLimaAcquiredFrames);

//		if(m_buffer->_getRequestStop()) {
//			m_sync->setExposing(pcoAcqStop);
//		} else 
		
		// --- in case of stop request during the record phase, the transfer
		// --- is made to avoid lose the image recorded
		{
			pcoAcqStatus status;



			if(m_cam->_isCameraType(Pco2k | Pco4k)){
				if(m_pcoData->testCmdMode & TESTCMDMODE_DIMAX_XFERMULTI) {
					status = (pcoAcqStatus) m_buffer->_xferImag();
				} else {
					if(wDoubleImage)
					{ 
						status = (pcoAcqStatus) m_buffer->_xferImagMultDoubleImage();  //  <------------- default pco2k/4k NO waitobj
					}
					else
					{
						status = (pcoAcqStatus) m_buffer->_xferImagMult();  //  <------------- default pco2k/4k NO waitobj
					}
				}
			}else{
				if(m_pcoData->testCmdMode & TESTCMDMODE_DIMAX_XFERMULTI) {
					status = (pcoAcqStatus) m_buffer->_xferImagMult();
				} else {
					if(wDoubleImage)
					{ 
						status = (pcoAcqStatus) m_buffer->_xferImagDoubleImage(); //  <------------- default dimax YES waitobj
					}
					else
					{
						status = (pcoAcqStatus) m_buffer->_xferImag(); //  <------------- default dimax YES waitobj
					}
				}

			}
			
			if(nb_frames_fixed) status = pcoAcqError;
			m_sync->setExposing(status);

		}

	} // if nb_frames_fixed && no stopRequested
	

	//m_sync->setExposing(status);
	m_pcoData->dwMaxImageCnt[wSegment-1] =
			m_pcoData->traceAcq.maxImgCount = _dwPcoMaxImageCnt;

	// traceAcq info - dimax xfer time
	m_pcoData->msAcqXfer = msXfer = msElapsedTime(tStart);
	m_pcoData->traceAcq.msXfer = msXfer;

	m_pcoData->msAcqAll = msTotal = msElapsedTime(tStart0);
	m_pcoData->traceAcq.msTotal= msTotal;

	m_pcoData->traceAcq.endXferTimestamp = m_pcoData->msAcqXferTimestamp = getTimestamp();


	sprintf_s(_msg, LEN_MSG, "%s [%d]> [EXIT] imgRecorded[%d] coc[%g] recLoopTime[%ld] "
			"tout[(%ld) 0(%ld)] rec[%ld] xfer[%ld] all[%ld](ms)\n", 
			fnId, __LINE__, _dwPcoValidImageCnt, msPerFrame, msNowRecordLoop, timeout, timeout0, msRecord, msXfer, msTotal);
	m_cam->_traceMsg(_msg);

	// included in 34a8fb6723594919f08cf66759fe5dbd6dc4287e only for dimax (to check for others)
	m_sync->setStarted(false);


#if 0
	if(requestStop == stopRequest) 
	{
		Event *ev = new Event(Hardware,Event::Error,Event::Camera,Event::CamFault, errMsg);
		m_cam->_getPcoHwEventCtrlObj()->reportEvent(ev);
	}

#endif



	_endthread();
}

//==========================================================================================================
//==========================================================================================================

void _pco_acq_thread_dimax_trig_single(void *argin) {
	DEF_FNID;
	printf("=== %s [%d]> %s ENTRY\n",  fnId, __LINE__,getTimestamp(Iso));

	static char msgErr[LEN_ERROR_MSG+1];

	int error;
	int _nrStop;
	DWORD _dwValidImageCnt, _dwMaxImageCnt;

	Camera* m_cam = (Camera *) argin;
	SyncCtrlObj* m_sync = m_cam->_getSyncCtrlObj();
	//BufferCtrlObj* m_buffer = m_sync->_getBufferCtrlObj();
	BufferCtrlObj* m_buffer = m_cam->_getBufferCtrlObj();

	struct stcPcoData *m_pcoData = m_cam->_getPcoData();
	m_pcoData->traceAcq.fnId = fnId;

	m_cam->_sprintComment(true, fnId, "[ENTRY]");

	const char *msg;
	TIME_USEC tStart, tStart0;
	msElapsedTimeSet(tStart);
	tStart0 = tStart;

	long timeout, timeout0, msNowRecordLoop, msRecord, msXfer, msTotal;
	int nb_acq_frames;
	int requestStop = stopNone;

	HANDLE m_handle = m_cam->getHandle();

	WORD wSegment;  m_cam->_pco_GetActiveRamSegment(wSegment, error); 
	double msPerFrame = (m_cam->pcoGetCocRunTime() * 1000.);
	m_pcoData->traceAcq.msImgCoc = msPerFrame;

	//DWORD dwMsSleepOneFrame = (DWORD) (msPerFrame + 0.5);	// 4/5 rounding
	DWORD dwMsSleepOneFrame = (DWORD) (msPerFrame/5.0);	// 4/5 rounding
	if(dwMsSleepOneFrame == 0) dwMsSleepOneFrame = 1;		// min sleep

	bool nb_frames_fixed = false;
	int nb_frames; 	m_sync->getNbFrames(nb_frames);
	//m_pcoData->traceAcq.nrImgRequested = nb_frames;
	m_pcoData->traceAcq.nrImgRequested0 = nb_frames;

	m_sync->setAcqFrames(0);

	timeout = timeout0 = (long) (msPerFrame * (nb_frames * 1.3));	// 30% guard
	if(timeout < TOUT_MIN_DIMAX) timeout = TOUT_MIN_DIMAX;
    
	m_pcoData->traceAcq.msTout = m_pcoData->msAcqTout = timeout;
	_dwValidImageCnt = 0;

	m_sync->getExpTime(m_pcoData->traceAcq.sExposure);
	m_sync->getLatTime(m_pcoData->traceAcq.sDelay);

	m_sync->setExposing(pcoAcqRecordStart);

		m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwValidImageCnt, _dwMaxImageCnt, error);
		if(error) {
			printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, "_pco_GetNumberOfImagesInSegment");
			throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
		}
		m_pcoData->dwValidImageCnt[wSegment-1] = 
			m_pcoData->traceAcq.nrImgRecorded = _dwValidImageCnt;
	 	m_pcoData->dwMaxImageCnt[wSegment-1] =
			m_pcoData->traceAcq.maxImgCount = _dwMaxImageCnt;

		bool doWhile =true;

		if( ((DWORD) nb_frames > _dwMaxImageCnt) ){
			nb_frames_fixed = true;
			
			sprintf_s(msgErr,LEN_ERROR_MSG, 
				"=== %s [%d]> ERROR INVALID NR FRAMES fixed nb_frames[%d] _dwMaxImageCnt[%d]", 
				fnId, __LINE__, nb_frames, _dwMaxImageCnt);
			printf("%s\n", msgErr);

			m_sync->setExposing(pcoAcqError);
			doWhile = false;
		}




	while(doWhile) {
		WORD wRecState_actual;

		m_pcoData->msAcqTnow = msNowRecordLoop = msElapsedTime(tStart);
		m_pcoData->traceAcq.msRecordLoop = msNowRecordLoop;
		

		wRecState_actual = m_cam->_pco_GetRecordingState(error);
		if(error) {
			printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, "_pco_GetRecordingState");
			throw LIMA_HW_EXC(Error, "PCO_GetRecordingState");
		}
		
		if(wRecState_actual == 0) break;

		if((requestStop = m_sync->_getRequestStop(_nrStop))  == stopRequest) {
			m_sync->_setRequestStop(stopNone);
		
			char msg[LEN_TRACEACQ_MSG+1];
				//m_buffer->_setRequestStop(stopProcessing);
				//m_sync->setExposing(pcoAcqStop);
				
			snprintf(msg,LEN_TRACEACQ_MSG, "=== %s> STOP REQ (recording). lastImgRec[%d]\n", fnId, _dwValidImageCnt);
				printf(msg);
				m_pcoData->traceMsg(msg);
				break;
		}
		Sleep(dwMsSleepOneFrame);	// sleep 1 frame
	} // while(true)

	m_pcoData->msAcqTnow = msNowRecordLoop = msElapsedTime(tStart);
	m_pcoData->traceAcq.msRecordLoop = msNowRecordLoop;

	msg = m_cam->_pco_SetRecordingState(0, error);
	if(error) {
		printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
		throw LIMA_HW_EXC(Error, "_pco_SetRecordingState");
	}

	if( (requestStop != stopRequest) && (!nb_frames_fixed)) {
		if(m_sync->getExposing() == pcoAcqRecordStart) m_sync->setExposing(pcoAcqRecordEnd);

		m_cam->_pco_GetNumberOfImagesInSegment(wSegment, _dwValidImageCnt, _dwMaxImageCnt, error);
		if(error) {
			printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
			throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
		}

		m_pcoData->dwValidImageCnt[wSegment-1] = 
			m_pcoData->traceAcq.nrImgRecorded = _dwValidImageCnt;

		nb_acq_frames = (_dwValidImageCnt < (DWORD) nb_frames) ? _dwValidImageCnt : nb_frames;
		//m_sync->setAcqFrames(nb_acq_frames);

		// dimax recording time
		m_pcoData->msAcqRec = msRecord = msElapsedTime(tStart);
		m_pcoData->traceAcq.msRecord = msRecord;    // loop & stop record
		
		m_pcoData->traceAcq.endRecordTimestamp = m_pcoData->msAcqRecTimestamp = getTimestamp();
		
		m_pcoData->traceAcq.nrImgAcquired = nb_acq_frames;
		m_pcoData->traceAcq.nrImgRequested = nb_frames;

		msElapsedTimeSet(tStart);  // reset for xfer


		if(nb_acq_frames < nb_frames) m_sync->setNbFrames(nb_acq_frames);

//		if(m_buffer->_getRequestStop()) {
//			m_sync->setExposing(pcoAcqStop);
//		} else 
		
		// --- in case of stop request during the record phase, the transfer
		// --- is made to avoid lose the image recorded
		{
			pcoAcqStatus status;

			status = (pcoAcqStatus) m_buffer->_xferImag_getImage();

			if(nb_frames_fixed) status = pcoAcqError;
			m_sync->setExposing(status);

		}

	} // if nb_frames_fixed & no stopped
	
	
	
	//m_sync->setExposing(status);
	m_pcoData->dwMaxImageCnt[wSegment-1] =
			m_pcoData->traceAcq.maxImgCount = _dwMaxImageCnt;

	// traceAcq info - dimax xfer time
	m_pcoData->msAcqXfer = msXfer = msElapsedTime(tStart);
	m_pcoData->traceAcq.msXfer = msXfer;

	m_pcoData->msAcqAll = msTotal = msElapsedTime(tStart0);
	m_pcoData->traceAcq.msTotal= msTotal;

	m_pcoData->traceAcq.endXferTimestamp = m_pcoData->msAcqXferTimestamp = getTimestamp();


	printf("=== %s [%d]> EXIT nb_frames_requested[%d] _dwValidImageCnt[%d] _dwMaxImageCnt[%d] coc[%g] recLoopTime[%ld] "
			"tout[(%ld) 0(%ld)] rec[%ld] xfer[%ld] all[%ld](ms)\n", 
			fnId, __LINE__, nb_frames, _dwValidImageCnt, _dwMaxImageCnt, msPerFrame, msNowRecordLoop, 
				timeout, timeout0, msRecord, msXfer, msTotal);

	// included in 34a8fb6723594919f08cf66759fe5dbd6dc4287e only for dimax (to check for others)
	m_sync->setStarted(false);



#if 0
	if(requestStop == stopRequest) 
	{
		Event *ev = new Event(Hardware,Event::Error,Event::Camera,Event::CamFault, errMsg);
		m_cam->_getPcoHwEventCtrlObj()->reportEvent(ev);
	}

#endif

	_endthread();
}

//==========================================================================================================
//==========================================================================================================

//==========================================================================================================
//==========================================================================================================

const char *sPcoAcqStatus[] ={
	"pcoAcqIdle", 
	"pcoAcqStart", 
	"pcoAcqRecordStart", 
	"pcoAcqRecordEnd",  
	"pcoAcqTransferStart", 
	"pcoAcqTransferEnd", 
	"pcoAcqStop", 
	"pcoAcqTransferStop", 
	"pcoAcqRecordTimeout",
	"pcoAcqWaitTimeout",
	"pcoAcqWaitError",
	"pcoAcqError",
	"pcoAcqPcoError",
};



//=====================================================================
//=====================================================================

void _pco_acq_thread_edge(void *argin) {
	DEF_FNID;

	Camera* m_cam = (Camera *) argin;
	SyncCtrlObj* m_sync = m_cam->_getSyncCtrlObj();
	//BufferCtrlObj* m_buffer = m_sync->_getBufferCtrlObj();
	BufferCtrlObj* m_buffer = m_cam->_getBufferCtrlObj();

	char _msg[LEN_MSG + 1];
	sprintf_s(_msg, LEN_MSG, "%s> [ENTRY]", fnId);
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


	if(m_pcoData->testCmdMode & TESTCMDMODE_EDGE_XFER) {
		status = (pcoAcqStatus) m_buffer->_xferImag_getImage_edge();
	} else {
		status = (pcoAcqStatus) m_buffer->_xferImag();  // original
	}

	m_sync->setExposing(status);
	//m_sync->stopAcq();
	const char *msg = m_cam->_pco_SetRecordingState(0, error);
	if(error) {
		printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
		//throw LIMA_HW_EXC(Error, "_pco_SetRecordingState");
	}

	m_pcoData->traceAcq.fnId = fnId;

	m_sync->getExpTime(m_pcoData->traceAcq.sExposure);
	m_sync->getLatTime(m_pcoData->traceAcq.sDelay);


	m_pcoData->msAcqXfer = msXfer = msElapsedTime(tStart);
	sprintf_s(_msg, LEN_MSG, "%s> [EXIT] xfer[%ld] (ms) status[%s]\n", 
			fnId, msXfer, sPcoAcqStatus[status]);
	m_cam->_traceMsg(_msg);


	
	m_sync->setStarted(false); // updated

	char *errMsg = NULL;
	switch(status) 
	{
		case pcoAcqRecordTimeout: errMsg = "pcoAcqRecordTimeout" ; break;
		case pcoAcqWaitTimeout:   errMsg = "pcoAcqWaitTimeout" ; break;
		case pcoAcqWaitError:     errMsg = "pcoAcqWaitError" ; break;
		case pcoAcqError:         errMsg = "pcoAcqError" ; break;
		case pcoAcqPcoError:      errMsg = "pcoAcqPcoError" ; break;
	}

	if(errMsg) 
	{
		Event *ev = new Event(Hardware,Event::Error,Event::Camera,Event::CamFault, errMsg);
		m_cam->_getPcoHwEventCtrlObj()->reportEvent(ev);
	}

	_endthread();
}

//=====================================================================
//=====================================================================

void _pco_acq_thread_dimax_live(void *argin) {
	DEF_FNID;

	Camera* m_cam = (Camera *) argin;
	SyncCtrlObj* m_sync = m_cam->_getSyncCtrlObj();
	//BufferCtrlObj* m_buffer = m_sync->_getBufferCtrlObj();
	BufferCtrlObj* m_buffer = m_cam->_getBufferCtrlObj();

	char _msg[LEN_MSG + 1];
    sprintf_s(_msg, LEN_MSG, "%s> [ENTRY]", fnId);
	m_cam->_traceMsg(_msg);

	struct stcPcoData *m_pcoData = m_cam->_getPcoData();

	TIME_USEC tStart;
	msElapsedTimeSet(tStart);
	int error;
	long msXfer;
	int requestStop = stopNone;

	m_cam->_sprintComment(true, fnId, "[ENTRY]");

	HANDLE m_handle = m_cam->getHandle();

	m_sync->setAcqFrames(0);

	// dimax recording time -> live NO record
	m_pcoData->msAcqRec  = 0;
	m_pcoData->msAcqRecTimestamp = getTimestamp();


	pcoAcqStatus status = (pcoAcqStatus) m_buffer->_xferImag();
	m_sync->setExposing(status);
	//m_sync->stopAcq();
	const char *msg = m_cam->_pco_SetRecordingState(0, error);
	if(error) {
		printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__, msg);
		//throw LIMA_HW_EXC(Error, "_pco_SetRecordingState");
	}

	// dimax xfer time
	m_pcoData->msAcqXfer = msXfer = msElapsedTime(tStart);
	m_pcoData->msAcqXferTimestamp = getTimestamp();
	sprintf_s(_msg, LEN_MSG, "%s> [EXIT] xfer[%ld] (ms) status[%s]\n", 
			fnId, msXfer, sPcoAcqStatus[status]);
	m_cam->_traceMsg(_msg);

	m_sync->setStarted(false); // to test

	_endthread();
}

//=====================================================================
//=====================================================================
void _pco_acq_thread_ringBuffer(void *argin) {
	DEF_FNID;

	Camera* m_cam = (Camera *) argin;
	SyncCtrlObj* m_sync = m_cam->_getSyncCtrlObj();
	BufferCtrlObj* m_buffer = m_cam->_getBufferCtrlObj();

	char _msg[LEN_MSG + 1];
	sprintf_s(_msg, LEN_MSG, "%s> [ENTRY]", fnId);
	m_cam->_traceMsg(_msg);

	m_cam->_sprintComment(true, fnId, "[ENTRY]");

	struct stcPcoData *m_pcoData = m_cam->_getPcoData();

	TIME_USEC tStart;
	msElapsedTimeSet(tStart);

	LARGE_INTEGER usStart;
	usElapsedTimeSet(usStart);

	int error;
	long msXfer;
	int requestStop = stopNone;
	pcoAcqStatus status;

	HANDLE m_handle = m_cam->getHandle();

	m_sync->setAcqFrames(0);

	// traceAcq
	m_pcoData->traceAcq.fnId = fnId;
	double msPerFrame = (m_cam->pcoGetCocRunTime() * 1000.);
	m_pcoData->traceAcq.msImgCoc = msPerFrame;
	m_sync->getExpTime(m_pcoData->traceAcq.sExposure);
	m_sync->getLatTime(m_pcoData->traceAcq.sDelay);


	m_pcoData->msAcqRec  = 0;
	m_pcoData->msAcqRecTimestamp = getTimestamp();



	m_pcoData->traceAcq.usTicks[0].value = usElapsedTime(usStart);
	m_pcoData->traceAcq.usTicks[0].desc = "before xferImag execTime";
	
	usElapsedTimeSet(usStart);

	if(m_pcoData->testCmdMode & TESTCMDMODE_PCO2K_XFER_WAITOBJ) {
		status = (pcoAcqStatus) m_buffer->_xferImag();      //  <------------- uses WAITOBJ
	} else {
		status = (pcoAcqStatus) m_buffer->_xferImagMult();  //  <------------- USES PCO_GetImageEx (NO waitobj)   0x20
	}

	m_pcoData->traceAcq.usTicks[1].value = usElapsedTime(usStart);
	m_pcoData->traceAcq.usTicks[1].desc = "xferImag execTime";
	usElapsedTimeSet(usStart);

	
	m_sync->setExposing(status);

	m_pcoData->traceAcq.usTicks[2].value = usElapsedTime(usStart);
	m_pcoData->traceAcq.usTicks[2].desc = "sync->setExposing(status) execTime";
	usElapsedTimeSet(usStart);

	//m_sync->stopAcq();

	m_pcoData->traceAcq.usTicks[3].value = usElapsedTime(usStart);
	m_pcoData->traceAcq.usTicks[3].desc = "sync->stopAcq execTime";
	usElapsedTimeSet(usStart);

	const char *msg = m_cam->_pco_SetRecordingState(0, error);
	m_pcoData->traceAcq.usTicks[4].value = usElapsedTime(usStart);
	m_pcoData->traceAcq.usTicks[4].desc = "_pco_SetRecordingState execTime";
	usElapsedTimeSet(usStart);

	if(error) {
		sprintf_s(_msg, LEN_MSG, "%s> [%d]> ERROR %s", fnId, __LINE__, msg);
		m_cam->_traceMsg(_msg);
		//throw LIMA_HW_EXC(Error, "_pco_SetRecordingState");
	}

	// xfer time
	m_pcoData->msAcqXfer =
		m_pcoData->traceAcq.msXfer = 
		m_pcoData->traceAcq.msTotal = 
		msXfer =
		msElapsedTime(tStart);

	m_pcoData->traceAcq.endXferTimestamp =
		m_pcoData->msAcqXferTimestamp = 
		getTimestamp();

	sprintf_s(_msg, LEN_MSG, "%s> EXIT xfer[%ld] (ms) status[%s]", 
			fnId, msXfer, sPcoAcqStatus[status]);
	m_cam->_traceMsg(_msg);


	m_pcoData->traceAcq.usTicks[5].desc = "up to _endtrhead execTime";
	m_pcoData->traceAcq.usTicks[5].value = usElapsedTime(usStart);

	m_sync->setStarted(false); // to test

	_endthread();
}

//=====================================================================
//=====================================================================


//=====================================================================
//=====================================================================
void Camera::reset(int reset_level)
{
	DEB_MEMBER_FUNCT();
	int error;


	switch(reset_level) 
	{
	case RESET_CLOSE_INTERFACE: 
		DEB_TRACE() << "\n... RESET - freeBuff, closeCam, resetLib  " << DEB_VAR1(reset_level) ;

		m_buffer->_pcoAllocBuffersFree();

		_pco_CloseCamera(error);
		PCO_PRINT_ERR(error, "_pco_CloseCamera"); 
		m_handle = 0;

		_pco_ResetLib(error);
		PCO_PRINT_ERR(error,  "_pco_ResetLib"); 
		break;

	default:
		DEB_TRACE() << "\n... RESET -  " << DEB_VAR1(reset_level);
		//_init();
		break;
	}

}

//=========================================================================================================
//=========================================================================================================
#define LEN_TMP_MSG 256
int Camera::PcoCheckError(int line, const char *file, int err, const char *fn, const char *comments) 
{
	DEB_MEMBER_FUNCT();
	DEF_FNID;


	static char lastErrorMsg[500];
	static char tmpMsg[LEN_TMP_MSG+1];
	static char tmpMsg1[LEN_TMP_MSG+1];
	char *msg;
	size_t lg;

	sprintf_s(tmpMsg,LEN_TMP_MSG,"PCOfn[%s] file[%s] line[%d]", fn, file,line);
	if (err != 0) {
		sprintf_s(tmpMsg1,LEN_TMP_MSG,"ERROR - PCOfn[%s]", fn);
		msgLog(tmpMsg);
		DWORD dwErr = err;
		m_pcoData->pcoError = err;
		msg = m_pcoData->pcoErrorMsg;

		PCO_GetErrorText(dwErr, msg, ERR_SIZE-14);
        
		lg = strlen(msg);
		sprintf_s(msg+lg,ERR_SIZE - lg, " [%s][%d]", file, line);

		if(err & PCO_ERROR_IS_WARNING) {
			DEB_WARNING() << fnId << ": --- WARNING - IGNORED --- " << DEB_VAR1(m_pcoData->pcoErrorMsg);
			//DEB_TRACE() << fnId << ": --- WARNING - IGNORED --- " << DEB_VAR1(m_pcoData->pcoErrorMsg);
			return 0;
		}
		DEB_TRACE() << fnId << ":\n   " << msg
			<< "\n    " << tmpMsg;
		return (err);
	}
	return (err);
}


//=========================================================================================================
//=========================================================================================================
char* Camera::_PcoCheckError(int line, const char *file, int err, int &error, const char *fn) {
	static char lastErrorMsg[ERR_SIZE];
	static char tmpMsg[LEN_TMP_MSG+1];
	char *msg;
	size_t lg;


	error = m_pcoData->pcoError = err;
	msg = m_pcoData->pcoErrorMsg;

	if (err != 0) {
		sprintf_s(tmpMsg,LEN_TMP_MSG,"ERROR %s (%d)", fn, line);
		
		PCO_GetErrorText(err, lastErrorMsg, ERR_SIZE-14);
		strncpy_s(msg, ERR_SIZE, lastErrorMsg, _TRUNCATE); 

		lg = strlen(msg);
		sprintf_s(msg+lg,ERR_SIZE - lg, " [%s][%d]", file, line);

		return lastErrorMsg;
	}
	return NULL;
}








//=================================================================================================
//=================================================================================================
int Camera::dumpRecordedImages(int &nrImages, int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;

	WORD wSegment; _pco_GetActiveRamSegment(wSegment, error); 
	DWORD _dwValidImageCnt, _dwMaxImageCnt;


	WORD wRecState_actual;

	nrImages = -1;

	if(!_isCameraType(Dimax)) return -2;

	wRecState_actual = _pco_GetRecordingState(error);
	PCO_PRINT_ERR(error, "_pco_GetRecordingState"); 	
	
	if (error) return -100;
	if(wRecState_actual != 0) return -1;


	_pco_GetNumberOfImagesInSegment(wSegment, _dwValidImageCnt, _dwMaxImageCnt, error);
	if(error) {
		printf("=== %s [%d]> ERROR %s\n", fnId, __LINE__,"_pco_GetNumberOfImagesInSegment");
		throw LIMA_HW_EXC(Error, "PCO_GetNumberOfImagesInSegment");
	}

	nrImages = _dwValidImageCnt;

	return 0;

}









//=================================================================================================
//=================================================================================================
bool Camera::_isValid_pixelRate(DWORD dwPixelRate){
		
	DEB_MEMBER_FUNCT();

	// pixelrate 1     (long word; frequency in Hz)
	// pixelrate 2,3,4 (long word; frequency in Hz; if not available, then value = 0)

	if(dwPixelRate > 0) 
		for(int i = 0; i < 4; i++) {			
			if(dwPixelRate == m_pcoData->stcPcoDescription.dwPixelRateDESC[i]) return TRUE;
		}

	return FALSE;
}



//=========================================================================================================
//=========================================================================================================
void Camera::_get_ImageType(ImageType& image_type)
{
  unsigned int pixbytes;
  getBytesPerPixel(pixbytes);
  image_type = (pixbytes == 2) ? Bpp16 : Bpp8;
}

//=================================================================================================
//=================================================================================================

// 31/10/2013 PCO Support Team <support@pco.de>
// Pixelsize is not implemented in the complete SW- and HW-stack.

void Camera::_get_PixelSize(double& x_size,double &y_size)
{  

	// pixel size in micrometer 

	if( _isCameraType(Pco2k)) {
		x_size = y_size = 7.4;	// um / BR_pco_2000_105.pdf	
		return;
	}

	if( _isCameraType(Pco4k)) {
		x_size = y_size = 9.0;	// um / BR_pco_4000_105.pdf	
		return;
	}

	if( _isCameraType(Edge)) {
		x_size = y_size = 6.5;	// um / pco.edge User Manual V1.01, page 34	
		return;
	}

	if( _isCameraType(Dimax)) {
		x_size = y_size = 11;	// um / pco.dimax User's Manual V1.01	
		return;
	}

	x_size = y_size = -1.;		

}

//=================================================================================================
//=================================================================================================
void Camera::_set_ImageType(ImageType curr_image_type)
{
    // ---- DONE
	// only check if it valid, BUT don't set it ????
  switch(curr_image_type)
    {
    case Bpp16:
    case Bpp8:
      break;

    default:
      throw LIMA_HW_EXC(InvalidValue,"This image type is not Managed");
    }

}
//=========================================================================================================
//=========================================================================================================
void Camera::_get_DetectorType(std::string& det_type)
{
    // ---- DONE
   det_type = "Pco";
}

//=========================================================================================================
//=========================================================================================================
void Camera::_get_MaxImageSize(Size& max_image_size)
{

  unsigned  width,height;

  getMaxWidthHeight(width,height);
  max_image_size = Size(width,height);

}

//=================================================================================================
//=================================================================================================
bool Camera::_isCameraType(unsigned long long tp){
		
	DEB_MEMBER_FUNCT();
	DEF_FNID;

	WORD wCameraType = _getCameraType();
	WORD wCameraSubtype = _getCameraSubType();


	DEB_TRACE() << "[entry] " << DEB_VAR3(tp, wCameraType, wCameraSubtype);

	switch(wCameraType) {
		case CAMERATYPE_PCO_DIMAX_STD: 
		case CAMERATYPE_PCO_DIMAX_TV:
		case CAMERATYPE_PCO_DIMAX_CS:
			if(tp & Dimax) {DEB_TRACE() << "Dimax [exit] "; return TRUE;}
			switch(wCameraSubtype >> 8)
			{
				case 0x20:
					if(tp & (DimaxHS1 | DimaxHS)) {DEB_TRACE() << "DimaxHS1 / HS [exit] "; return TRUE;}
					break;
				case 0x21:
					if(tp & (DimaxHS2 | DimaxHS)) {DEB_TRACE() << "DimaxHS2 / HS [exit] "; return TRUE;}
					break;
				case 0x23:
					if(tp & (DimaxHS4 | DimaxHS)) {DEB_TRACE() << "DimaxHS4 / HS [exit] "; return TRUE;}
					break;
				default:
					break;
			}
			DEB_TRACE() << "Dimax SUBTYPE NONE FALSE [exit] ";
			return FALSE;

		case CAMERATYPE_PCO_EDGE_GL:
			return !!(tp & (EdgeGL | Edge));

		case CAMERATYPE_PCO_EDGE_USB3:
			return !!(tp & (EdgeUSB | EdgeRolling | Edge));

		case CAMERATYPE_PCO_EDGE_HS:
			return !!(tp & (EdgeHS | EdgeRolling | Edge));

		case CAMERATYPE_PCO_EDGE_42:
		case CAMERATYPE_PCO_EDGE:
			return !!(tp & (EdgeRolling | Edge));

		case CAMERATYPE_PCO2000:
			return !!(tp & Pco2k) ;

		case CAMERATYPE_PCO4000:
			return !!(tp & Pco4k) ;

		default:
			break;
	}
		

	return FALSE;
}


//=================================================================================================
//=================================================================================================
bool Camera::_isInterfaceType(int tp)
{
		
	DEB_MEMBER_FUNCT();
	DEF_FNID;

	switch(_getInterfaceType()) {
		case INTERFACE_FIREWIRE: 
			return !!(tp & ifFirewire) ;
		
		case INTERFACE_CAMERALINK:
			return !!(tp & (ifCameralink | ifCameralinkAll));

		case INTERFACE_CAMERALINKHS:
			return !!(tp & (ifCameralinkHS | ifCameralinkAll));

		case INTERFACE_USB:
			return !!(tp & (ifUsb));

		case INTERFACE_USB3:
			return !!(tp & (ifUsb3));

		case INTERFACE_ETHERNET:
			return !!(tp & (ifEth));

		case INTERFACE_SERIAL:
			return !!(tp & (ifSerial));

		case INTERFACE_COAXPRESS:
			return !!(tp & (ifCoaxpress));

		default:
			return FALSE;

	}
		
}


//=================================================================================================
//=================================================================================================
void Camera::_get_XYsteps(Point &xy_steps)
{
	DEB_MEMBER_FUNCT();

		unsigned int xSteps, ySteps;

		getXYsteps(xSteps, ySteps);

		xy_steps.x = xSteps;
		xy_steps.y = ySteps;
}

//=================================================================================================
//=================================================================================================
void Camera::_presetPixelRate(DWORD &pixRate, int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;

	if(! (_isCameraType(Edge) || _isCameraType(Pco2k) || _isCameraType(Pco4k)) ) {
		DEB_TRACE() << "WARNING - this camera doesn't allows setPixelRate"  ;
		pixRate = 0;
		error = -1;
		return;
	}

	if(!_isValid_pixelRate(pixRate)) {
		DEB_ALWAYS() << "INVALID requested pixel Rate" << DEB_VAR1(pixRate)  ;
		pixRate = 0;
		error = -1;
		return;
	}

	m_pcoData->dwPixelRateRequested = pixRate;
	error = 0;
}


//=================================================================================================
//=================================================================================================
void Camera::msgLog(const char *s) {
	m_msgLog->add(s);
}



//=================================================================================================
//=================================================================================================

DWORD Camera::_getCameraSerialNumber()
{
	DEB_MEMBER_FUNCT();
	return m_pcoData->stcPcoCamType.dwSerialNumber;
}

WORD Camera::_getInterfaceType()
{
	DEB_MEMBER_FUNCT();
	return m_pcoData->stcPcoCamType.wInterfaceType;
}

const char *Camera::_getInterfaceTypeStr()
{
	DEB_MEMBER_FUNCT();
	return m_pcoData->iface;
}


const char *Camera::_getCameraIdn()
{
	DEB_MEMBER_FUNCT();
	return m_pcoData->camera_name;
}

//=================================================================================================
//=================================================================================================
WORD Camera::_getCameraType()
{
	DEB_MEMBER_FUNCT();
	return m_pcoData->stcPcoCamType.wCamType;
}

const char *Camera::_getCameraTypeStr()
{
	DEB_MEMBER_FUNCT();
	return m_pcoData->model;
}



//=================================================================================================
//=================================================================================================
WORD Camera::_getCameraSubType()
{
	DEB_MEMBER_FUNCT();
	return m_pcoData->stcPcoCamType.wCamSubType;
}

const char *Camera::_getCameraSubTypeStr()
{
	DEB_MEMBER_FUNCT();
	return m_pcoData->modelSubType;
}





//=================================================================================================
//=================================================================================================


//=================================================================================================
//=================================================================================================

void Camera::_checkImgNrInit(bool &checkImgNr, int &imgNrDiff, int &alignmentShift){
	DEB_MEMBER_FUNCT();
	DEF_FNID;

	checkImgNr = false;
	imgNrDiff = 1;
	alignmentShift = 0;
	int err;

	WORD wTimeStampMode;

	_pco_GetTimestampMode(wTimeStampMode, err);

	if(wTimeStampMode == 0) return;
	checkImgNr = true;

	int alignment;

	_pco_GetBitAlignment(alignment);

	if(alignment == 0)
		alignmentShift = (16 - m_pcoData->stcPcoDescription.wDynResDESC);
	else
		alignmentShift = 0;

	
	return;

}
//=================================================================================================
//=================================================================================================

bool Camera::_getCameraState(long long flag)
{
	return !!(m_state & flag);
}



//=================================================================================================
//=================================================================================================

void Camera::_setCameraState(long long flag, bool val)
{
	if(val) 
	{
		m_state |= flag;
	} 
	else
	{
		m_state |= flag;
		m_state ^= flag;
	}
	return;
}



//=================================================================================================
//=================================================================================================

bool Camera::_isRunAfterAssign()
{
	return (_isCameraType(Edge) && (_isInterfaceType(ifCameralinkAll))  );
}

//=================================================================================================
//=================================================================================================

time_t Camera::_getActionTimestamp(int action)
{
	if((action < 0) || (action >= DIM_ACTION_TIMESTAMP)) return 0;
	time_t ts = m_pcoData->action_timestamp.ts[action];
	return ts ? ts : 1;
}

void Camera::_setActionTimestamp(int action)
{
	if((action >= 0) && (action < DIM_ACTION_TIMESTAMP)) 
	{
		m_pcoData->action_timestamp.ts[action] = time(NULL);
	}
}



//=================================================================================================
// to merge
//=================================================================================================

//=================================================================================================
//=================================================================================================
void Camera::getStatus(Camera::Status& status)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    status = m_status;
    DEB_RETURN() << DEB_VAR1(DEB_HEX(status));
}

//=================================================================================================
//=================================================================================================
void Camera::_setStatus(Camera::Status status,bool force)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    if(force || m_status != Camera::Fault)
        m_status = status;
    m_cond.broadcast();
    aLock.unlock();
}

//=================================================================================================
//=================================================================================================
//================================= ROLLING SHUTTER ===============================================
//=================================================================================================
//=================================================================================================
void Camera::_get_shutter_rolling_edge(DWORD &dwRolling, int &error){
	DEB_MEMBER_FUNCT();
	DEF_FNID;

	DWORD m_dwSetup[10];
	WORD m_wLen = 10;
	WORD m_wType;

	m_wType = 0;
    _pco_GetCameraSetup(m_wType, m_dwSetup[0], m_wLen, error);
    PCO_PRINT_ERR(error, "_pco_GetCameraSetup"); 	
	
	dwRolling =  error ? 0 :  m_dwSetup[0];

	return;

}
//=================================================================================================
//=================================================================================================
void Camera::_set_shutter_rolling_edge(DWORD dwRolling, int &error)
{
		
	DEB_MEMBER_FUNCT();
	error = 0;

	if(!_isValid_rollingShutter(dwRolling)) 
	{
		DEB_ALWAYS() << "ERROR requested Rolling Shutter not allowed " << DEB_VAR1(dwRolling);
		error = -1;
		return;
	}

	m_pcoData->dwRollingShutter = dwRolling;

	DEB_TRACE() << "requested Rolling Shutter OK " << DEB_VAR1(dwRolling);

	_beginthread( _pco_shutter_thread_edge, 0, (void*) this);

	return;

}

//=================================================================================================
//=================================================================================================
void Camera::_pco_set_shutter_rolling_edge(int &error){
		
	DEB_MEMBER_FUNCT();
	DEF_FNID;
	char *msg;
	const char  *cmsg ;
	char msgBuff[MSG_SIZE+1];

	DWORD dwRollingShRequested, dwRollingShNow;
	DWORD m_dwSetup[10];
	WORD m_wLen = 10;
	WORD m_wType = 0;

	// PCO recommended timing values
	int ts[3] = {2000, 3000, 250}; // command, image, channel timeout
	DWORD sleepMs = 10000;  // sleep time after reboot

	if(!_isCameraType(Edge)) {
		return ;
	}

	DEB_TRACE() << fnId << " [entry - edge] ";

	m_config = TRUE;

	// DWORD m_dwSetup[10];
	// WORD m_wLen = 10;
	// WORD m_wType;
	// int ts[3] = { 2000, 3000, 250}; // command, image, channel timeout
	// PCO_OpenCamera(&m_hCam,0);
	// PCO_GetCameraSetup(m_hCam, &m_wType, &m_dwSetup[0], &m_wLen);

	// m_dwSetup[0] = PCO_EDGE_SETUP_GLOBAL_SHUTTER;
	// PCO_SetTimeouts(m_hCam, &ts[0], sizeof(ts));
	// PCO_SetCameraSetup(m_hCam, m_wType, &m_dwSetup[0], m_wLen);
	// PCO_RebootCamera(m_hCam);
	// PCO_CloseCamera(m_hCam);
	// Camera setup parameter for pco.edge:
	// #define PCO_EDGE_SETUP_ROLLING_SHUTTER 0x00000001         // rolling shutter
	// #define PCO_EDGE_SETUP_GLOBAL_SHUTTER  0x00000002         // global shutter

	
	dwRollingShRequested = m_pcoData->dwRollingShutter ;

	m_wType = 0;
	_pco_GetCameraSetup(m_wType, m_dwSetup[0], m_wLen, error);
    PCO_PRINT_ERR(error, "_pco_GetCameraSetup"); 	
	if(error)
	{
		DEB_ALWAYS() << fnId << " [ERROR PCO_GetCameraSetup] " << msg;
		m_config = FALSE;
		return;
	}


	dwRollingShNow = m_dwSetup[0];

	
	if(m_dwSetup[0] == dwRollingShRequested) { 
		DEB_TRACE() << "exit NO Change in ROLLING SHUTTER " << DEB_VAR2(dwRollingShNow, dwRollingShRequested);
		m_config = FALSE;
		return;
	}

	msg = msgBuff;
	sprintf_s(msg, MSG_SIZE, "[Change ROLLING SHUTTER from [%d] to [%d]]", 
		m_dwSetup[0]==PCO_EDGE_SETUP_ROLLING_SHUTTER, dwRollingShRequested==PCO_EDGE_SETUP_ROLLING_SHUTTER);

	DEB_TRACE() << "Change in ROLLING SHUTTER " << DEB_VAR2(dwRollingShNow, dwRollingShRequested);

	m_dwSetup[0] = dwRollingShRequested;

    _pco_SetTimeouts(&ts[0], sizeof(ts), error);
    PCO_PRINT_ERR(error, msg); 	if(error) return;

	cmsg = "[PCO_SetCameraSetup]";
	DEB_TRACE() << fnId << " " << cmsg;
    _pco_SetCameraSetup(m_wType, m_dwSetup[0], m_wLen, error);
    PCO_PRINT_ERR(error, cmsg); 	
	if(error)
	{
		DEB_ALWAYS() << fnId << " [ERROR PCO_SetCameraSetup] " << cmsg;
		m_config = FALSE;
		return;
	}

	cmsg = "[PCO_RebootCamera]";
	DEB_TRACE() << fnId << " " << cmsg;
    _pco_RebootCamera(error);
    PCO_PRINT_ERR(error, cmsg); 	if(error) return;

	//m_sync->_getBufferCtrlObj()->_pcoAllocBuffersFree();
	m_buffer->_pcoAllocBuffersFree();

	cmsg = "[PCO_CloseCamera]";
	DEB_TRACE() << fnId << " " << cmsg;
    _pco_CloseCamera(error);
    PCO_PRINT_ERR(error, cmsg); 	if(error) return;
	m_handle = 0;
	
	msg = msgBuff;
	sprintf_s(msg, MSG_SIZE, "[Sleep %d ms]", sleepMs);
	DEB_TRACE() << fnId << " " << msg;
	::Sleep(sleepMs);

	_init();

	DEB_TRACE() << fnId << " [exit] ";

	m_config = FALSE;
	return;

}
//=====================================================================
//=====================================================================
void _pco_shutter_thread_edge(void *argin) {
	DEF_FNID;
	int error;


	Camera* m_cam = (Camera *) argin;
	//SyncCtrlObj* m_sync = m_cam->_getSyncCtrlObj();

	char _msg[LEN_MSG + 1];
	sprintf_s(_msg, LEN_MSG, "%s> [ENTRY]", fnId);
	m_cam->_traceMsg(_msg);

	m_cam->_sprintComment(true, fnId, "[ENTRY]");

	m_cam->_pco_set_shutter_rolling_edge(error);


	sprintf_s(_msg, LEN_MSG, "%s> [EXIT]", fnId);
	m_cam->_traceMsg(_msg);

	//m_sync->setStarted(false); // to test

	_endthread();
}

//=================================================================================================
//=================================================================================================
bool Camera::_isValid_rollingShutter(DWORD dwRolling)
{

	switch(dwRolling) 
	{
		case PCO_EDGE_SETUP_ROLLING_SHUTTER: return _isCapsDesc(capsRollingShutter);   // 1
		case PCO_EDGE_SETUP_GLOBAL_SHUTTER: return _isCapsDesc(capsGlobalShutter) ;    // 2
		case PCO_EDGE_SETUP_GLOBAL_RESET: return _isCapsDesc(capsGlobalResetShutter) ; //4
		default: return FALSE;
	}

}
//================================================================================================
//================================= ROLLING SHUTTER / end ========================================
//================================================================================================
