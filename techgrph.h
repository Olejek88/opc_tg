#include <opcda.h>
#include <windows.h>
#include "unilog.h"

#define LOGID logg,0
#define LOG_FNAME "techgrph.log"
#define CFG_FILE "tg160.ini"
#define MAX_PORTS 32
#define PORTNAME_LEN 15
#define TECHCOM_ID_MAX	15		// devices
#define TECHCOM_NUM_MAX	300		// tags = values
#define TECHCOM_MAX_NAMEL 100       	// tags name lenght
#define TECHCOM_DATALEN_MAX 50
#define	MAXPORT_NUMBER	3

typedef struct _OPCcfg OPCcfg;

struct _OPCcfg {
  char ports[MAX_PORTS][PORTNAME_LEN+1];
  int speed;  
  LONG read(LPSTR path);
};

typedef struct _Tech Tech;

struct _Tech {
  time_t mtime; 	// measurement time
  INT cv_size;      // size (quantity scanned commands)
  LPSTR *cv;		// pointer to value
  INT *cv_cmdid;    // command identitificator
  BOOL *cv_status;	// status
  INT ids[TECHCOM_ID_MAX+1];
  INT idnum;

_Tech(): mtime(0), idnum(0)
  {
    INT i;
    cv_size = 20; // maximum
    cv = new LPSTR[TECHCOM_ID_MAX];	// ???
    cv_status = new BOOL[TECHCOM_ID_MAX]; // massive status
    cv_cmdid = new int[TECHCOM_ID_MAX];   // massive id
    int cv_i;				
    for (i = 0, cv_i = 0; i<20; i++) 
	{
	 cv[cv_i] = new char[TECHCOM_DATALEN_MAX+1];	// init tag	
	 cv_status[cv_i] = FALSE;			// init status
	 cv_cmdid[cv_i] = i;							// init id
	 cv_i++;
    	}
  }

~_Tech()						// destructor
  {
    int i;
    for (i =0; i < cv_size; i++)
      delete[] cv[i];           // free memory

    delete[] cv_status;
    delete[] cv_cmdid;
    delete[] cv;
  }
};
