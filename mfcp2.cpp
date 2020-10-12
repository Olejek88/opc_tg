#define _CRT_SECURE_NO_WARNINGS 1
#define _WIN32_DCOM				// Enables DCOM extensions
#define INITGUID				// Initialize OLE constants

#include <stdio.h>
#include <math.h>				// some mathematical function
#include <time.h>				// some mathematical function
//#include "afxcmn.h"				// MFC function such as CString,....
#include "techgrph.h"			// no comments
#include "unilog.h"				// universal utilites for creating log-files
#include <locale.h>				// set russian codepage
#include "opcda.h"				// basic function for OPC:DA
#include "lightopc.h"			// light OPC library header file
//#include "serialport.h"			// function for work with Serial Port
#define ECL_SID "OPC.TG-exe"	// identificator of OPC server
//---------------------------------------------------------------------------------
static const loVendorInfo vendor = {0,57,34,0,"Technograph OPC Server" };// OPC vendor info (Major/Minor/Build/Reserv)
static void a_server_finished(void*, loService*, loClient*);	// OnServer finish his work
static int OPCstatus=OPC_STATUS_RUNNING;						// status of OPC server
loService *my_service;			// name of light OPC Service
//---------------------------------------------------------------------------------
HANDLE	hCom[10];
DWORD	dwBytesWritten=0,dwBytesRead=0;
//---------------------------------------------------------------------------------
CHAR charset[38]="123456789AbCdEfGhIjKLMNOPQRSTUVWXYZZ";		//	character table
INT tTotal=TECHCOM_NUM_MAX;			// total quantity of tags
INT chNum[MAXPORT_NUMBER][15];		// kanal number on device
INT pNum=0;							// number of ports
INT dNum=0;							// number of devices
UINT qtech=0;

BOOL b_port[MAXPORT_NUMBER];				// true if com-port present
UINT com_num[MAXPORT_NUMBER]={1,2,3};		// COM-port number
UINT speed[MAXPORT_NUMBER]={2400,2400,2400};// COM-port speed
UINT parity[MAXPORT_NUMBER]={0,0,0};		// Parity
UINT databits[MAXPORT_NUMBER]={0,0,0};		// Data bits
CHAR razd;									// 

static Tech *devp[MAXPORT_NUMBER];			// pointer to tag structure
static loTagId ti[TECHCOM_NUM_MAX];		// Tag id
static loTagValue tv[TECHCOM_NUM_MAX];	// Tag value
static CHAR *tn[TECHCOM_NUM_MAX];		// Tag name

CHAR argv0[FILENAME_MAX + 32];	// lenght of command line (file+path (260+32))
unilog *logg=NULL;				// new structure of unilog
UINT ScanBus();					// bus scanned programm
UINT PollDevice(INT portn,INT device);	// polling single device
UINT InitDriver();						// func of initialising port and creating service
UINT DestroyDriver();					// function of detroying driver and service
HRESULT WriteDevice(INT device,const unsigned cmdnum,LPSTR data);	// write tag to device
FILE *CfgFile;					// pointer to .ini file
static CRITICAL_SECTION lk_values;		// protects ti[] from simultaneous access 
static int mymain(HINSTANCE hInstance, INT argc, CHAR *argv[]);
static int show_error(LPCTSTR msg);		// just show messagebox with error
static int show_msg(LPCTSTR msg);		// just show messagebox with message
static void poll_device(VOID);			// function polling device
VOID CheckRegStatus (VOID);				// Check Registry Stop/Run server status
INT RegisterTags (VOID);				// Register tags
VOID dataToTag(INT portn, INT device);	// copy data buffer to tag
CHAR* ReadParam (CHAR *SectionName,CHAR *Value);	// read parametr from .ini file
CRITICAL_SECTION drv_access;
UCHAR DeviceDataBuffer[MAXPORT_NUMBER+1][TECHCOM_ID_MAX][15][40];
BOOL WorkEnable=TRUE;
//---------------------------------------------------------------------------------
// {F610C000-AD12-4508-825A-209A05CC34E0}
DEFINE_GUID(GID_tgOPCserverDll, 
0xf610c000, 0xad12, 0x4508, 0x82, 0x5a, 0x20, 0x9a, 0x5, 0xcc, 0x34, 0xe0);
// {65E6A274-CE6C-4cde-A34F-3DA1AC07CAAC}
DEFINE_GUID(GID_tgOPCserverExe, 
0x65e6a274, 0xce6c, 0x4cde, 0xa3, 0x4f, 0x3d, 0xa1, 0xac, 0x7, 0xca, 0xac);
//---------------------------------------------------------------------------------
VOID timetToFileTime( time_t t, LPFILETIME pft )
{   LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
    pft->dwLowDateTime = (DWORD) ll;
    pft->dwHighDateTime =  (ULONG)(ll >>32);}
CHAR *absPath(CHAR *fileName)					// return abs path of file
{ static char path[sizeof(argv0)]="\0";			// path - massive of comline
  CHAR *cp;
  if(*path=='\0') strcpy(path, argv0);
  if(NULL==(cp=strrchr(path,'\\'))) cp=path; else cp++;
  cp=strcpy(cp,fileName);
  return path;}

inline VOID init_common(VOID)		// create log-file
{ logg = unilog_Create(ECL_SID, absPath(LOG_FNAME), NULL, 0, ll_DEBUG); // level [ll_FATAL...ll_DEBUG] 
  UL_INFO((LOGID, "Start"));}
inline VOID cleanup_common(VOID)	// delete log-file
{
 UL_INFO((LOGID, "Finish"));
 unilog_Delete(logg); logg = NULL;
 UL_INFO((LOGID, "total Finish"));
}

INT show_error(LPCTSTR msg)			// just show messagebox with error
{ ::MessageBox(NULL, msg, ECL_SID, MB_ICONSTOP|MB_OK);
  return 1;}
INT show_msg(LPCTSTR msg)			// just show messagebox with message
{ ::MessageBox(NULL, msg, ECL_SID, MB_OK);
  return 1;}

inline void cleanup_all(DWORD objid)
{ // Informs OLE that a class object, previously registered is no longer available for use  
  if (FAILED(CoRevokeClassObject(objid)))  UL_WARNING((LOGID, "CoRevokeClassObject() failed..."));
  DestroyDriver();					// close port and destroy driver
  CoUninitialize();					// Closes the COM library on the current thread
  cleanup_common();					// delete log-file
  //fclose(CfgFile);				// close .ini file
}
//---------------------------------------------------------------------------------
#include "opc_main.h"	//	main server 
//---------------------------------------------------------------------------------
INT APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{ static char *argv[3] = { "dummy.exe", NULL, NULL };	// defaults arguments
  argv[1] = lpCmdLine;									// comandline - progs keys
  //таблица точек входа   
  return mymain(hInstance, 2, argv);}

INT main(INT argc, CHAR *argv[])
{  return mymain(GetModuleHandle(NULL), argc, argv); }

INT mymain(HINSTANCE hInstance, INT argc, CHAR *argv[]) 
{
const char eClsidName [] = ECL_SID;				// desription 
const char eProgID [] = ECL_SID;				// name
DWORD objid;									// fully qualified path for the specified module
CHAR *cp;
objid=::GetModuleFileName(NULL, argv0, sizeof(argv0));	// function retrieves the fully qualified path for the specified module
if(objid==0 || objid+50 > sizeof(argv0)) return 0;		// not in border

init_common();									// create log-file
if(NULL==(cp = setlocale(LC_ALL, ".1251")))		// sets all categories, returning only the string cp-1251
	{ 
	UL_ERROR((LOGID, "setlocale() - Can't set 1251 code page"));	// in bad case write error in log
	cleanup_common();							// delete log-file
    return 0;
	}
cp = argv[1];		
if(cp)						// check keys of command line 
	{
    int finish = 1;			// flag of comlection
    if (strstr(cp, "/r"))	//	attempt registred server
		{
	     if (loServerRegister(&GID_tgOPCserverExe, eProgID, eClsidName, argv0, 0)) 
			{ show_error("Registration Failed");
			  UL_ERROR((LOGID, "Registration <%s> <%s> Failed", eProgID, argv0));  } 
		 else 
			{ 
			  show_msg("Technograph OPC Registration Ok");
			  UL_INFO((LOGID, "Registration <%s> <%s> Ok", eProgID, argv0));		
			}
		} 
	else 
		if (strstr(cp, "/u")) 
			{
			 if (loServerUnregister(&GID_tgOPCserverExe, eClsidName))
				{ show_error("UnRegistration Failed");
				 UL_ERROR((LOGID, "UnReg <%s> <%s> Failed", eClsidName, argv0)); } 
			 else 
				{ 
				 show_msg("Technograph OPC Server Unregistered");
				 UL_INFO((LOGID, "UnReg <%s> <%s> Ok", eClsidName, argv0));
				}
			} 
		else  // only /r and /u options
			if (strstr(cp, "/?")) 
				 show_msg("Use: \nKey /r to register server.\nKey /u to unregister server.\nKey /? to show this help.");
			else
				{
				 UL_WARNING((LOGID, "Ignore unknown option <%s>", cp));
				 finish = 0;		// nehren delat
				}
		if (finish) {      cleanup_common();      return 0;    } 
	}
if ((CfgFile = fopen(CFG_FILE, "r+")) == NULL )	
	{	
	 show_error("Error open .ini file");
	 UL_ERROR((LOGID, "Error open .ini file"));	// in bad case write error in log
	 return 0;
	}
if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) 
	{	// Initializes the COM library for use by the calling thread
     UL_ERROR((LOGID, "CoInitializeEx() failed. Exiting..."));
     cleanup_common();	// close log-file
     return 0;
	}
UL_INFO((LOGID, "CoInitializeEx() Ok...."));	// write to log
for (UINT i=0;i<MAXPORT_NUMBER;i++) devp[i] = new Tech();		// one structure = one port
if (InitDriver()) {		// open and set com-port
    CoUninitialize();	// Closes the COM library on the current thread
    cleanup_common();	// close log-file
    return 0;
  }
UL_INFO((LOGID, "InitDriver() Ok...."));	// write to log

if (FAILED(CoRegisterClassObject(GID_tgOPCserverExe, &my_CF, 
				   CLSCTX_LOCAL_SERVER|CLSCTX_REMOTE_SERVER|CLSCTX_INPROC_SERVER, 
				   REGCLS_MULTIPLEUSE, &objid)))
    { UL_ERROR((LOGID, "CoRegisterClassObject() failed. Exiting..."));
      cleanup_all(objid);		// close comport and unload all librares
      return 0; }
UL_INFO((LOGID, "CoRegisterClassObject() Ok...."));	// write to log

Sleep(1000);
my_CF.Release();		// avoid locking by CoRegisterClassObject() 

if (OPCstatus!=OPC_STATUS_RUNNING)	// ???? maybe Status changed and OPC not currently running??
	{	while(my_CF.in_use())      Sleep(1000);	// wait
		cleanup_all(objid);
		return 0;	}
while(my_CF.in_use())	// while server created or client connected
    poll_device();		// polling devices else do nothing (and be nothing)
cleanup_all(objid);		// destroy himself
return 0;
}
//----------------------------------------------------------------------------------------------
VOID poll_device(VOID)
{
  FILETIME ft;
  INT devi, i=0, ecode; 
  UINT p=0;
  for (INT cport=0; cport<MAXPORT_NUMBER; cport++) if (b_port[cport])
  for (devi = 0; devi < devp[cport]->idnum; devi++)
	{
	  UL_DEBUG((LOGID, "Driver poll <%d><%d>",cport, devp[cport]->ids[devi]));
	  ecode=PollDevice(cport,devp[cport]->ids[devi]);
	  if (ecode)
		{
		 dataToTag (cport,devp[cport]->ids[devi]);
		 //UL_DEBUG((LOGID, "Copy data to tag success"));
		 time(&devp[cport]->mtime);
		 timetToFileTime(devp[cport]->mtime, &ft);
		}
	  else 
		{
		 GetSystemTimeAsFileTime(&ft);
		 //UL_DEBUG((LOGID, "Copy data to tag not success"));
		}
	 EnterCriticalSection(&lk_values);
	 //UL_DEBUG((LOGID, "device:%d kanals %d",devp->ids[devi],chNum[devp->ids[devi]]));
	 for (INT ci = 0; ci <= chNum[cport][devp[cport]->ids[devi]]; ci++, i++) 
		{
	      WCHAR buf[64];
		  LCID lcid = MAKELCID(0x0409, SORT_DEFAULT); // This macro creates a locale identifier from a language identifier. Specifies how dates, times, and currencies are formatted
 	 	  MultiByteToWideChar(CP_ACP,	 // ANSI code page
									  0, // flags
					devp[cport]->cv[ci], // points to the character string to be converted
		  strlen(devp[cport]->cv[ci])+1, // size in bytes of the string pointed to 
									buf, // Points to a buffer that receives the translated string
			  sizeof(buf)/sizeof(buf[0])); // function maps a character string to a wide-character (Unicode) string		  
		  UL_DEBUG((LOGID, "devp[cport]->cv_status[%d]=%d",ci,devp[cport]->cv_status[ci]));
		  if (devp[cport]->cv_status[ci]) 
			{				  
			  VARTYPE tvVt = tv[i].tvValue.vt;
			  VariantClear(&tv[i].tvValue);			  
			  switch (tvVt)
					{
					  case VT_R8:   // float vr4;									
									// vr4= (float) atof(devp->cv[ci]);
									DOUBLE vr4;
									//vr4= atof(devp->cv[ci]);
									for (p=0;p<strlen(devp[cport]->cv[ci]);p++)
										 if (*(devp[cport]->cv[ci]+p)=='.')
											*(devp[cport]->cv[ci]+p)=razd;
									vr4= atof(devp[cport]->cv[ci]);
									//sscanf(devp->cv[ci], "%f", &vr4);
									UL_DEBUG((LOGID, "[%d][%d] ffff:%f",i,ci,vr4));
									tv[i].tvState.tsQuality = OPC_QUALITY_GOOD;
									V_R8(&tv[i].tvValue) = vr4;
									tv[i].tvState.tsTime = ft;
									break;
					  case VT_DATE: DATE date;
									VarDateFromStr(buf, lcid, 0, &date);
									V_DATE(&tv[i].tvValue) = date;
									break;
					  case VT_BSTR:
					  default:
								V_BSTR(&tv[i].tvValue) = SysAllocString(buf);
					}
			  V_VT(&tv[i].tvValue) = tvVt;
			}		    
			else
				if (ecode==1) tv[i].tvState.tsQuality = OPC_QUALITY_UNCERTAIN;
			if (ecode == 0)
				 tv[i].tvState.tsQuality = OPC_QUALITY_UNCERTAIN;
			if (ecode == 2)
				 tv[i].tvState.tsQuality = OPC_QUALITY_DEVICE_FAILURE;
			tv[i].tvState.tsTime = ft;
		}
//	 tv[7].tvState.tsQuality=OPC_QUALITY_GOOD;
//     UL_DEBUG((LOGID, "tv[%d].tvValue=%f(%f) [0x%x]",1,tv[1].tvValue,&tv[1].tvValue,&tv[1].tvState.tsQuality));
//     UL_DEBUG((LOGID, "tv[%d].tvValue=%f(%f) [0x%x]",2,tv[2].tvValue,&tv[2].tvValue,&tv[2].tvState.tsQuality));
//     UL_DEBUG((LOGID, "tv[%d].tvValue=%f(%f) [0x%x]",3,tv[3].tvValue,&tv[3].tvValue,&tv[3].tvState.tsQuality));
//     UL_DEBUG((LOGID, "tv[%d].tvValue=%f(%f) [0x%x]",6,tv[6].tvValue,&tv[6].tvValue,&tv[6].tvState.tsQuality));
//     UL_DEBUG((LOGID, "tv[%d].tvValue=%f(%f) [0x%x]",7,tv[7].tvValue,&tv[7].tvValue,&tv[7].tvState.tsQuality));
//     UL_DEBUG((LOGID, "tv[%d].tvValue=%f(%f) [0x%x]",8,tv[8].tvValue,&tv[8].tvValue,&tv[8].tvState.tsQuality));
	 loCacheUpdate(my_service, tTotal, tv, 0);
	}
 LeaveCriticalSection(&lk_values);
 Sleep(1000);
}
//-------------------------------------------------------------------
loTrid ReadTags(const loCaller *, unsigned  count, loTagPair taglist[],
		VARIANT   values[],	WORD      qualities[],	FILETIME  stamps[],
		HRESULT   errs[],	HRESULT  *master_err,	HRESULT  *master_qual,
		const VARTYPE vtype[],	LCID lcid)
{  return loDR_STORED; }
int WriteTags(const loCaller *ca,
              unsigned count, loTagPair taglist[],
              VARIANT values[], HRESULT error[], HRESULT *master, LCID lcid)
{  
 return loDW_TOCACHE; 
}
VOID activation_monitor(const loCaller *ca, int count, loTagPair *til){}
//-------------------------------------------------------------------
VOID dataToTag(INT portn, INT device)
{
INT	max_com=0;
UINT j;	
CHAR buf[50];
CHAR *pbuf=buf;  *pbuf = '\0';
//UL_DEBUG((LOGID, "total channals %d",chNum[device]));
for (INT i=0; i<chNum[portn][device]+2; i++)
	{
	 if (i<chNum[portn][device]) 	 
		for (j=0; j<6; j++)
		 buf[j]=DeviceDataBuffer[portn][device][i][j];
	 if (i==chNum[portn][device])
 		for (j=0; j<5; j++)
		 buf[j]=DeviceDataBuffer[portn][device][i][j];	 
	 if (i==chNum[portn][device]+1)
 		for (j=0; j<8; j++)
		 buf[j]=DeviceDataBuffer[portn][device][i][j];	 
	 buf[j] = '\0';	
	 strcpy (devp[portn]->cv[i+1],buf);
	 UL_DEBUG((LOGID, "device:%d kanal %d data %s",device,i,devp[portn]->cv[i]));
	}
}
//-----------------------------------------------------------------------------------
HRESULT WriteDevice(int device,const unsigned cmdnum,LPSTR data)
{
  return S_OK;
}
//-----------------------------------------------------------------------------------
UINT PollDevice(INT portn, INT device)
{
 INT cnt=0,c0m=0, chff=0, num_bytes=0, startid=0, cnt_false=0, i=0, y=0;
 const int sBuf[] = {0x3f,0x30,0x30};	// ?00
 UCHAR sBuf1[500],DId[80],ks=0,sBuf2[100]; 
 CHAR Out[15], *Outt = Out;
 UCHAR *DeviceId = DId,*Int = sBuf1;
 for (i=0;i<=2;i++) 	Out[i] = (CHAR) sBuf[i]; 
 Out[1] = 48+device/10; Out[2] = 48+device%10; Out[3]=0;
 UL_DEBUG((LOGID, "command request ?%d%d",Out[1],Out[2]));
 for (UINT po=0;po<strlen (Out);po++)	WriteFile(hCom[portn], Out+po, 1, &dwBytesWritten, NULL);
 Sleep(700);
 for (cnt=0;cnt<300;cnt++) sBuf1[cnt]=0;
 BOOL bSuccess = ReadFile(hCom[portn], sBuf1, 100, &dwBytesRead, NULL);
 UINT dwbr1 = dwBytesRead;
 while (dwBytesRead!=0 && bSuccess)
	{			 
	 bSuccess = ReadFile(hCom[portn], sBuf2, sizeof(sBuf2), &dwBytesRead, NULL);
	 for (UINT i=0;i<dwBytesRead;i++) sBuf1[dwbr1+i]=sBuf2[i];
	 dwbr1 = dwbr1+dwBytesRead;
	}
 if (!bSuccess || !dwbr1) { for (i=0;i<15;i++) devp[portn]->cv_status[i]=FALSE; UL_DEBUG((LOGID, "status false")); return 2;}
 BOOL bcFF_OK = TRUE, bcK_OK = FALSE;
 //for (i=0;i<cnt-10;i++) if (sBuf1[i]<32) bcFF_OK=FALSE;
 if (bcFF_OK)
 for (i=0;i<(INT)dwbr1;i++)
	{
	 UL_DEBUG((LOGID, "byte in %d",sBuf1[i]));
	 if (sBuf1[i]==75) 
		{				 				 
		 i=i+2;
		 for (INT rt=0; rt<20; rt++)
			 if (charset[rt]==sBuf1[i-1])
				chff=rt;
	 	 UL_DEBUG((LOGID, "device:%d kanal %d",device,chff));
		 INT r=0;
		 while (sBuf1[i]!=75 && i<cnt && r<6)
			{
			 if (sBuf1[i]>32 && sBuf1[i]<80)
				{
				 DeviceDataBuffer[portn][device][chff][r] = sBuf1[i];
				 bcK_OK=TRUE;
				 UL_DEBUG((LOGID, "device:%d kanal %d sym: %d = %d",device,chff,r,DeviceDataBuffer[portn][device][chff][r]));
				 r++; 
				}
			 i++;
			}
		 i--;
		 //UL_DEBUG((LOGID, "status true %d",chff+1));
		 devp[portn]->cv_status[chff+1]=TRUE;
		}
	 chff=chNum[portn][device];
	 for (y=0;y<5;y++)	DeviceDataBuffer[portn][device][chff][y]=sBuf1[1+y];
	 for (y=0;y<8;y++)	DeviceDataBuffer[portn][device][chff+1][y]=sBuf1[7+y];
	 devp[portn]->cv_status[chff]=TRUE;
	 devp[portn]->cv_status[chff+1]=TRUE;
	}
 if (bcK_OK) return 1;	else  return 2;
}
//-------------------------------------------------------------------
UINT DestroyDriver()
{
  if (my_service)		
    {
      int ecode = loServiceDestroy(my_service);
      UL_INFO((LOGID, "%!e loServiceDestroy(%p) = ", ecode));	// destroy derver
      DeleteCriticalSection(&lk_values);						// destroy CS
      my_service = 0;		
    }
  for (UINT i=0;i<MAXPORT_NUMBER;i++) CloseHandle (hCom[i]);		// one structure = one port
  UL_INFO((LOGID, "Close COM-port"));						// write in log
  return	1;
}
//-------------------------------------------------------------------
UINT InitDriver()
{
 loDriver ld;		// structure of driver description
 LONG ecode;		// error code 
 if (my_service) {	
      UL_ERROR((LOGID, "Driver already initialized!"));
      return 0;
  }
 memset(&ld, 0, sizeof(ld));   
 ld.ldRefreshRate =5000;				// polling time 
 ld.ldRefreshRate_min = 4000;			// minimum polling time
 ld.ldWriteTags = WriteTags;			// pointer to function write tag
 ld.ldReadTags = ReadTags;				// pointer to function read tag
 ld.ldSubscribe = activation_monitor;	// callback of tag activity
 ld.ldFlags = loDF_IGNCASE;				// ignore case
 ld.ldBranchSep = '/';					// hierarchial branch separator
 ecode = loServiceCreate(&my_service, &ld, tTotal);		//	creating loService 
 UL_TRACE((LOGID, "%!e loServiceCreate()=", ecode));	// write to log returning code
 if (ecode) return 1;									// error to create service	
 InitializeCriticalSection(&lk_values);
// COMMTIMEOUTS timeouts;
 CHAR buf[50]; CHAR *pbuf=buf;
 CHAR portname[8];
 pbuf = ReadParam ("Server","Separator");
 razd = *pbuf;
 pbuf = ReadParam ("Server","Technographs");
 qtech = atoi (pbuf);
 for (UINT pp=0; pp<MAXPORT_NUMBER; pp++)
	{
	 sprintf (portname,"Port%d",pp+1);
	 pbuf = ReadParam (portname,"COM"); 
	 if (strcmp(pbuf,"error")) 
		{
		 com_num[pp] = atoi(pbuf);
		 pbuf = ReadParam (portname,"Speed"); if (strcmp(pbuf,"error")) speed[pp] = atoi(pbuf);
		 pbuf = ReadParam (portname,"Databits"); if (strcmp(pbuf,"error")) databits[pp] = atoi(pbuf);
		 pbuf = ReadParam (portname,"Parity");
		 if (!strcmp(pbuf,"EvenParity")) parity[pp]=0;
		 if (!strcmp(pbuf,"MarkParity")) parity[pp]=1;
		 if (!strcmp(pbuf,"NoParity")) parity[pp]=2;
		 if (!strcmp(pbuf,"OddParity")) parity[pp]=3;
		 if (!strcmp(pbuf,"SpaceParity")) parity[pp]=4;
		 UL_INFO((LOGID, "Opening port COM%d on speed %d with parity %d and databits %d",com_num[pp],speed[pp], parity[pp], databits[pp]));	 
		 DCB dcb = {0};								// com port settings struct 
		 DWORD dwStoredFlags;
		 COMSTAT comstat;
		 dwStoredFlags = EV_RXCHAR | EV_TXEMPTY | EV_RXFLAG;	
		 COMMTIMEOUTS timeouts;
		 timeouts.ReadIntervalTimeout = 5;			//20
		 timeouts.ReadTotalTimeoutMultiplier = 0;
		 timeouts.ReadTotalTimeoutConstant = 100;	//190
		 timeouts.WriteTotalTimeoutMultiplier = 0; 
		 timeouts.WriteTotalTimeoutConstant = 25;	//25
		 CHAR sPort[100];
		 sprintf (sPort,"\\\\.\\COM%d", com_num[pp]);
		 hCom[pp] = CreateFile(sPort,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
		 //UL_INFO((LOGID, "E (%s) %d + %d * %d",sPort,hCom[pp],INVALID_HANDLE_VALUE,databits[pp]));
		 if (hCom[pp] != INVALID_HANDLE_VALUE)
		 if (databits[pp])
			{
			 pNum++; b_port[pp]=TRUE;
			 GetCommState(hCom[pp], &dcb);
			 dcb.BaudRate = speed[pp];
			 dcb.Parity = NOPARITY;
			 dcb.StopBits = TWOSTOPBITS;
			 dcb.ByteSize = 8;
			 dcb.fDsrSensitivity = FALSE;
			 dcb.fOutxCtsFlow = FALSE;
			 dcb.fOutxDsrFlow = FALSE;
			 dcb.fOutX = FALSE;
			 dcb.fInX = FALSE;
			 SetCommState(hCom[pp], &dcb);
			 SetCommTimeouts(hCom[pp], &timeouts);
			 UL_INFO((LOGID, "Opening port success [%d]",pp));	 
			 DWORD dwErrors;
			 if (!ClearCommError(hCom[pp], &dwErrors, &comstat))
				UL_ERROR((LOGID,"Failed in call to ClearCommError"));
			}
		 else UL_ERROR((LOGID,"Open com-port: error! %d",GetLastError ()));
		}
	}
 UL_INFO((LOGID, "Scan bus"));		// write in log 
 if (ScanBus()) 
	{ 	 
	 UL_INFO((LOGID, "Total %d devices found",dNum));
	 tTotal=tTotal+dNum*14+1;
	 if (RegisterTags()) return 1;
		else return 0;
	}
  else		
	{ UL_ERROR((LOGID, "No devices found")); return 1; }
}
//----------------------------------------------------------------------------------------
UINT ScanBus()
{
const int sBuf[] = {0x3f,0x30,0x30};	// ?00
CHAR buf[50]; CHAR *pbuf=buf;
CHAR Out[50],*Outt=Out,tec[50];
UINT addr=0;
INT	 chff=0, cport=0;
DWORD dwStoredFlags = EV_RXCHAR | EV_TXEMPTY | EV_RXFLAG;
for (UINT ie=0;ie<MAXPORT_NUMBER;ie++)  devp[ie]->idnum = 0;
//------------------------------------------------------------------------------------
UL_INFO((LOGID, "b_port[%d]=%d",cport,b_port[cport]));	// write in log
for (cport=0; cport<MAXPORT_NUMBER; cport++) if (b_port[cport])
	for (INT adr=1;adr<=TECHCOM_ID_MAX;adr++)
		{
		 UL_INFO((LOGID, "%d %d",cport,adr));	// write in log
		 sprintf (buf,"Port%d",cport+1);
		 sprintf (tec,"Tech%d",adr);
		 Outt = ReadParam (buf,tec);
		 addr = atoi (Outt);
		 if (addr>0 && addr<15)
			{
			 devp[cport]->ids[devp[cport]->idnum] = addr; devp[cport]->idnum++;
			 chNum[cport][adr]=12;
			 dNum++;
			 UL_INFO((LOGID, "Device found on port %d address %d, device has %d channals",cport,addr,chNum[cport][adr]));	// write in log
			}
//	 	 port[cport].SetMask (dwStoredFlags);
//		 port[cport].GetStatus (comstat);
//		 for (cnt=0;cnt<20;cnt++) sBuf1[cnt]=0;
//		 for (INT i=0;i<=2;i++) 	Out[i] = (CHAR) sBuf[i]; // 5 первых ff + стандарт команды
//		 Out[1] = 48+adr/10; Out[2] = 48+adr%10; Out[3]=0;
//		 for (UINT po=0;po<strlen (Out);po++)	WriteFile(hCom[cport], Out+po, 1, &dwBytesWritten, NULL);
//		 Sleep (300);
//		 BOOL bSuccess = ReadFile(hCom[cport], sBuf1, 100, &dwBytesRead, NULL);
//		 UINT dwbr1 = dwBytesRead;
//		 while (dwBytesRead!=0 && bSuccess)
//			{			 
//			 bSuccess = ReadFile(hCom[cport], sBuf2, sizeof(sBuf2), &dwBytesRead, NULL);
//			 for (UINT i=0;i<dwBytesRead;i++) sBuf1[dwbr1+i]=sBuf2[i];
//			 dwbr1 = dwbr1+dwBytesRead;
//			}
//		 for (i=0;i<=2;i++)
//			{	
//			 port[cport].Write(Outt+i, 1);	port[cport].WaitEvent (dwStoredFlags);
//			 UL_DEBUG((LOGID, "byte out %d",*(Outt+i)));
//			}
//		 Int = sBuf1;	cnt_false=0; 		
//		 for (cnt=0;cnt<200;cnt++)
//			{
//			 if (port[cport].Read(Int+cnt, 1) == FALSE)
//				{ cnt_false++; if (cnt_false>1) break;}		//!!! (4)
//			 else cnt_false=0;
//			 //UL_DEBUG((LOGID, "byte in %d",*(Int+cnt)));
//			 port[cport].GetStatus (comstat);
//			}
//		 if (sBuf1[2]==0x3a||sBuf1[3]==0x3a||sBuf1[4]==0x3a||cnt>50)
//			{	
//			 devp[cport]->ids[devp[cport]->idnum] = adr; devp[cport]->idnum++;
//			 chNum[cport][adr]=0;
//			 for (INT r=0;r<cnt;r++)
//				if (sBuf1[r]==75)
//					chNum[cport][adr]++;
//			 chNum[cport][adr]=12;
//			 dNum++;
//			 UL_INFO((LOGID, "Device found on port %d address %d, device has %d channals",cport,adr,chNum[cport][adr]));	// write in log
//			}
		}	 
return dNum;
}
//-----------------------------------------------------------------------------------
CHAR* ReadParam (CHAR *SectionName,CHAR *Value)
{
CHAR buf[150]; 
CHAR string1[50];
CHAR *pbuf=buf;
UINT s_ok=0;
sprintf(string1,"[%s]",SectionName);
rewind (CfgFile);
	  while(!feof(CfgFile))
		 if(fgets(buf,50,CfgFile)!=NULL)
			if (strstr(buf,string1))
				{ s_ok=1; break; }
if (s_ok)
	{
	 while(!feof(CfgFile))
		{
		 if(fgets(buf,100,CfgFile)!=NULL&&strstr(buf,"[")==NULL&&strstr(buf,"]")==NULL)
			{
			 for (s_ok=0;s_ok<strlen(buf)-1;s_ok++)
				 if (buf[s_ok]==';') buf[s_ok+1]='\0';
			 if (strstr(buf,Value))
				{
				 for (s_ok=0;s_ok<strlen(buf)-1;s_ok++)
					if (s_ok>strlen(Value)) buf[s_ok-strlen(Value)-1]=buf[s_ok];
						 buf[s_ok-strlen(Value)-1]='\0';
				// UL_INFO((LOGID, "Section name %s, value %s, che %s",SectionName,Value,buf));	// write in log
				 return pbuf;
				}
			}
		}	 	
 	 if (SectionName=="Port")	{ buf[0]='1'; buf[1]='\0';}		// default
	 return pbuf;
	}
else{
	 sprintf(buf, "error");			// if something go wrong return error
	 return pbuf;
	}	
}
//------------------------------------------------------------------------------------
INT RegisterTags (VOID)
{
  FILETIME ft;	//  64-bit value representing the number of 100-ns intervals since January 1,1601
  UINT rights=0;	// tag type (read/write)
  INT ecode,devi,i;
  GetSystemTimeAsFileTime(&ft);	// retrieves the current system date and time
  EnterCriticalSection(&lk_values);  
  //UL_TRACE((LOGID, "tTotal=%d",tTotal));
  for (i=0; i < tTotal; i++)    
	  tn[i] = new char[TECHCOM_MAX_NAMEL];	// reserve memory for massive  
  i=0;
  for (INT cport=0; cport<MAXPORT_NUMBER; cport++) if (b_port[cport])
  for (devi = 0; devi < devp[cport]->idnum; devi++)
	{
  	 sprintf(tn[i], "empty %d%d",cport,devi);
	 VariantInit(&tv[i].tvValue);
	 V_BSTR(&tv[i].tvValue) = SysAllocString(L"");
	 V_VT(&tv[i].tvValue) = VT_BSTR;
	 ecode = loAddRealTag(my_service, &ti[i], (loRealTag)(i+1), tn[i], 0, rights, &tv[i].tvValue, 0, 0);
	 tv[i].tvTi = ti[i];
	 tv[i].tvState.tsTime = ft;
	 tv[i].tvState.tsError = S_OK;
	 tv[i].tvState.tsQuality = OPC_QUALITY_NOT_CONNECTED;
	 UL_TRACE((LOGID, "%!e loAddRealTag(%s) = %u", ecode, tn[i], ti[i]));
	 i++;
	 
     for (INT ci=0;ci<12; ci++, i++) 
		{
		 INT cmdid = devp[cport]->cv_cmdid[ci];
		 sprintf(tn[i], "com%d/id%0.2d/channel%d",com_num[cport], devp[cport]->ids[devi], ci+1);
		 rights=OPC_READABLE;
		 VariantInit(&tv[i].tvValue);
		 V_R8(&tv[i].tvValue) = 0.0;
		 V_VT(&tv[i].tvValue) = VT_R8;
		 ecode = loAddRealTag_a(my_service, &ti[i], (loRealTag)(i+1),tn[i], 0, rights, &tv[i].tvValue, -99999.0, 99999.0);
	  	 //V_BSTR(&tv[i].tvValue) = SysAllocString(L"");
		 //V_VT(&tv[i].tvValue) = VT_BSTR;
		 //ecode = loAddRealTag(my_service, &ti[i], (loRealTag)(i+1), tn[i], 0, rights, &tv[i].tvValue, 0, 0);
		 tv[i].tvTi = ti[i];
		 tv[i].tvState.tsTime = ft;
		 tv[i].tvState.tsError = S_OK;
		 tv[i].tvState.tsQuality = OPC_QUALITY_NOT_CONNECTED;
		 UL_TRACE((LOGID, "%!e loAddRealTag(%s) = %u",ecode, tn[i], ti[i]));
		}
 	 UL_TRACE((LOGID, "cport=%d devp[cport]->idnum=%d devp[cport]->ids[0]=%d devp[cport]->ids[1]=%d cnNum=%d",cport,devp[cport]->idnum,devp[cport]->ids[0],devp[cport]->ids[1],chNum[cport][devp[cport]->ids[devi]]));
/*	 sprintf(tn[i], "com%d/id%0.2d/Time",com_num[cport], devp[cport]->ids[devi]);
	 VariantInit(&tv[i].tvValue);
	 V_BSTR(&tv[i].tvValue) = SysAllocString(L"");
	 V_VT(&tv[i].tvValue) = VT_BSTR;
	 ecode = loAddRealTag(my_service, &ti[i], (loRealTag)(i+1), tn[i], 0, rights, &tv[i].tvValue, 0, 0);
	 tv[i].tvTi = ti[i];
	 tv[i].tvState.tsTime = ft;
	 tv[i].tvState.tsError = S_OK;
	 tv[i].tvState.tsQuality = OPC_QUALITY_NOT_CONNECTED;
	 UL_TRACE((LOGID, "%!e loAddRealTag(%s) = %u", ecode, tn[i], ti[i]));
	 i++;
	 sprintf(tn[i], "com%d/id%0.2d/Data",com_num[cport], devp[cport]->ids[devi]);
	 VariantInit(&tv[i].tvValue);
	 V_BSTR(&tv[i].tvValue) = SysAllocString(L"");
	 V_VT(&tv[i].tvValue) = VT_BSTR;
	 ecode = loAddRealTag(my_service, &ti[i], (loRealTag)(i+1), tn[i], 0, rights, &tv[i].tvValue, 0, 0);
	 tv[i].tvTi = ti[i];
	 tv[i].tvState.tsTime = ft;
	 tv[i].tvState.tsError = S_OK;
	 tv[i].tvState.tsQuality = OPC_QUALITY_NOT_CONNECTED;
	 UL_TRACE((LOGID, "%!e loAddRealTag(%s) = %u", ecode, tn[i], ti[i]));
	 i++;*/
	}
  LeaveCriticalSection(&lk_values);
  if(ecode) 
  {
    UL_ERROR((LOGID, "%!e driver_init()=", ecode));
    return -1;
  }
  return 0;
}
//--------------------------------------------------------------------------------------------
