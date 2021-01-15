/******************************************************

******************************************************/

#include <clib.h>
#include <stdio.h>
#include <mem.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <time.h>


/* Only SystemCORP Generic is supported in the protcol */
#define SUPPORTED_PROTOCOL		IEC61850_SYSTEMCORP_GENERIC
/* Include IEC 61850 API */
#include "IEC61850API.h"

//#define DEBUG_SV61850 1
/* Maximum Object types */
#define OBJECT_TYPES  2
/* Total Objects in each object type */
#define OBJECTS 		 8
/* SC143 Input Output Address Location */
#define IO_ADDR       0xC00
/* Input Output Handler Task Priority */
#define IOHANDLER_PRIO                    120
/* Input Output Handler Task Stack Size */
#define TASK_STACKSIZE 							1024

/******************************************************************************
* Constants
******************************************************************************/
#define VERBOSE
#define NTP_PORT             123          // Well known SNTP port
#define OFFSET_JAN_1_1970    2208988800UL // Difference between Jan 1st 1900
                                          // 0:00:00 (NTP's base) and Jan 1st
                                          // 1970 0:00:00 (Unix Timestamp's base)
                                          // in seconds

#define BOOL   unsigned char
#ifndef FALSE
  #define FALSE  0
  #define TRUE   1
#endif

/* Object Types */
enum
{
	DIGITAL_INPUT 		= 1,	// Digital Input 	(DIP Switch )
   DIGITAL_OUTPUT 	= 2,  // Digital Output (LED )
}eObjectTypes;

enum
{
	DIGINPUT_INDEX 	= 0,	// Digital Input
   DIGOUTPUT_INDEX 	= 1,  // Digital Output
}eObjectIndex;

/* Object Information Index */
enum
{
	VALUE_INDEX 		= 1,		// Value Index
   QUALITY_INDEX 		= 2,     // Quality Index
   TIME_STAMP_INDEX	= 3,     // Time Stamp Index
}eObjectInfoIndex;

/* NTP Time Stamp */
typedef	 struct		tag_NTPTimeStamp
{
   unsigned long int		uliSeconds;						/* Number of Seconds */
   unsigned long int		uliFractionsOfSecond;		/* Fraction of a second as a binary fraction. */
}tNTPTimeStamp;


/* Objects */
typedef struct tag_DK61Object
{
	unsigned char 			ucObjectNo;     				/* Object Number */
   unsigned char 			ucObjectType;              /* Object Type */
   unsigned char 			ucObjectValue;             /* Object Value */
   unsigned short int   usiObjectQuality;          /* Object Quality */
   tNTPTimeStamp			tObjectTime;          		/* Obect Time */
}tDK61Object;

/* Full Quality */
typedef struct tag_FullQuality
{
   unsigned char				ucvalidity;				 	/* validity  */
   unsigned char				ucdetailQual;			 	/* detail quality */
   unsigned char				ucsource;					/* source */
   unsigned char				uctest;						/* testing ? */
   unsigned char				ucoperatorBlocked;		/* operator blocked */
}tFullQuality;



/* Local Database */
tDK61Object atObj[OBJECT_TYPES][OBJECTS];


/* IO Handler Task Stack */
static unsigned int IOHandler_stack[TASK_STACKSIZE];
/* IO Handler Function */
void huge IOHandler(void);
/* Read Callback function */
int MyReadFunction(IEC61850_ObjectID * ptObjectID, IEC61850_ObjectData * ptReturnedValue);
/* Write Callback function */
int MyWriteFunction(IEC61850_ObjectID * ptObjectID, const IEC61850_ObjectData * ptNewValue);
/* Create Local Database */
void CreateLocalDatabase(void);
/* Function to convert time to 61850 Time */
void ConvertTo61850Time(tNTPTimeStamp *ptObjectTime);
/* Function to get NTP Time */
BOOL getTime(struct date *date,
              struct time *time,     // Destination for date and time (DOS format)
              char *remoteIp,        // IP address of the NTP server
              signed int timezone );  // Addend for timezone (e.g. +1 for MET)


/* IO Handler Task Definition Block */
static TaskDefBlock  IOHandlerTask =
{
  IOHandler,
  {'I','O','H','L'},            			// a name: 4 chars
  &IOHandler_stack[TASK_STACKSIZE],  	// top of stack
  TASK_STACKSIZE*sizeof(int),   			// size of stack
  0,                            			// attributes, not supported now
  IOHANDLER_PRIO,                  		// priority 20(high) ... 127(low)
  0,                            			// time slice (if any), not supported now
  0,0,0,0                       			// mailbox depth, not supported now
};

/* IEC61850 Server */
IEC61850	myServer							= 0;



/******************************************************************************
* Type definitions
******************************************************************************/
#ifdef SC243
#pragma pack(1)
#endif
typedef struct
{
  unsigned char li_vn_mode;
  unsigned char stratum;
  unsigned char poll;
  unsigned char precision;
  unsigned long rootDelay;
  unsigned long rootDispersion;
  unsigned long referenceIdentifier;
  unsigned long reference_Timestamp[2];
  unsigned long originateTimestamp[2];
  unsigned long receiveTimestamp[2];
  unsigned long transmitTimestamp[2];
} TSntpHeader;
#ifdef SC243
#pragma pack()
#endif



/* Main Function */
int main(int argc, char *argv[])
{

	IEC61850_Parameters	tServerParam	= {0};							// Server Parameters
	int						error				= IEC61850_ERROR_NONE;     // Error
   int						taskID 			= -1;								// Task ID
   TimeDate_Structure	tTimeDate      = {0};
     char *month[] = { "---", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                    "Aug", "Sep", "Oct", "Nov", "Dec" };
  struct date date;
  struct time time;




   /* Enable Programmable Chip Select for SC143 */
	pfe_enable_pcs( 6 );

   /* ICD file as parameter */
   if(argc < 2)
   {
   	printf("\r\n Usage : SV61850 <FILENAME.ICD>");
      return 0;
   }


   // Set timezone environment variable to UTC
   putenv("TZ=UTC0");
   tzset();


    // Get NTP time
    if( !getTime( &date, &time, 0, 0 ) )
    {
    	/* If NTP Time failed Than Start Time is 1/1/2020 00:00:00 Hours */
      printf( "\r\nError: Could not query current date and time!" );
      tTimeDate.yr = 10;
      tTimeDate.mn = 01;
      tTimeDate.dy = 01;
      tTimeDate.hr = 00;
      tTimeDate.mn = 00;
      tTimeDate.sec = 00;
      RTX_Set_TimeDate(&tTimeDate);
    }
    else
    {
    	/* NTP time get successful Set System Clock */
      printf( "\r\nDate: %s %u. %u", month[date.da_mon], date.da_day, date.da_year );
      printf( "\r\nTime: %u:%02u:%02u", time.ti_hour, time.ti_min, time.ti_sec);
      // Set system clock
      printf( "\r\nSetting system clock..." );
      setdate( &date );
      settime( &time );
    }




	/* Set all outputs to Off */
	outportb(IO_ADDR, 0);

   do
   {

      tServerParam.ClientServerFlag = IEC61850_SERVER; // This is a Server
      tServerParam.ptReadCallback = MyReadFunction;      // Assign Read Callback function
      tServerParam.ptWriteCallback = MyWriteFunction;	 // Assign Write Callback function
      tServerParam.ptUpdateCallback = NULL;            // No Update callback for Server

      printf("\r\n Server Create");

      myServer = IEC61850_Create(&tServerParam,&error);  //Create a server
      if(myServer == NULL)
      {
         printf(" Failed : %i", error);
         break;
      }

      printf("\r\n Server Load SCL");

      error = IEC61850_LoadSCLFile(myServer,argv[1]); // Load in ICD file
      if(error != IEC61850_ERROR_NONE)
      {
         printf(" Failed : %i",error);
         break;
      }

      printf("\r\n Server Start");
      error = IEC61850_Start(myServer); // Starts myServer
      if(error != IEC61850_ERROR_NONE)
      {
         printf(" Failed : %i",error);
         break;
      }
   }while(0); /* Dummy while loop to avoid nested If's */

   /* Create Local Database */
   CreateLocalDatabase();

   /* Check for Any Error */
   if(error == IEC61850_ERROR_NONE)
   {
   	/* Create and Start IO Handler Task */
     error = RTX_Create_Task(&taskID , &IOHandlerTask);
     if(error != 0)
     {
     		printf("\r\n IO Handler task Create Failed : %i", error);
     }

     /* Do not Exit */
     while(1)
     {
     		RTX_Sleep_Time(30000);
     }

	}
   else
   {
   	/* If any errors in Create and Starting the Server */
      /* Stop the Server */
      printf("\r\n  Server Stop");
      error = IEC61850_Stop(myServer);
      if(error != IEC61850_ERROR_NONE)
      {
         printf(" Failed : %i",error);
      }

      /* Free all Memory */
      printf("\r\n  Server Free");
      IEC61850_Free(myServer);
   }


   return 0;
}

/* Function to Create Local Database */
void CreateLocalDatabase(void)
{
	unsigned char 			ucObjTypeCnt;
   unsigned char 			ucObjects;
	IEC61850_ObjectData 	UpdateValue		= {0};	// Value to send on Change
	IEC61850_ObjectID		Object			= {0};	// ID of the Object


   /* For all Object Types */
   for(ucObjTypeCnt = 0; ucObjTypeCnt < OBJECT_TYPES; ucObjTypeCnt++)
   {
       Object.uiField2 = ucObjTypeCnt + 1;  // Object Type
   	/* Each Object within Object Type */
   	for(ucObjects = 0; ucObjects < OBJECTS;  ucObjects++)
      {
      	/* Assign Object Number */
       	atObj[ucObjTypeCnt][ucObjects].ucObjectNo 	= ucObjects + 1;
         /* Assign Object Type DIP Switch : 1 , LED : 2 */
			atObj[ucObjTypeCnt][ucObjects].ucObjectType 	= ucObjTypeCnt + 1;

        	Object.uiField1 = ucObjects + 1; // Object Number


         /* Initialise Value to 0 */
			atObj[ucObjTypeCnt][ucObjects].ucObjectValue = 0;
        	Object.uiField3 = VALUE_INDEX;
         UpdateValue.pvData = &atObj[ucObjTypeCnt][ucObjects].ucObjectValue;
         UpdateValue.ucType = IEC61850_DATATYPE_BOOLEAN;
         UpdateValue.uiBitLength = 8;
        /* Send Update for Value */
        IEC61850_Update(myServer, &Object, &UpdateValue);

        	if(Object.uiField2 == DIGITAL_INPUT)
         {
            /* Initialise Quality */
            atObj[ucObjTypeCnt][ucObjects].usiObjectQuality = (IEC61850_QUALITY_FAILURE | IEC61850_QUALITY_INVALID |
            																  IEC61850_QUALITY_OLDDATA | IEC61850_QUALITY_QUESTIONABLE );

           	Object.uiField3 = QUALITY_INDEX;
            UpdateValue.pvData = &atObj[ucObjTypeCnt][ucObjects].usiObjectQuality;
            UpdateValue.ucType = IEC61850_DATATYPE_QUALITY;
            UpdateValue.uiBitLength = IEC61850_QUALITY_BITSIZE;
            /* Send Update for Quality */
        		IEC61850_Update(myServer, &Object, &UpdateValue);
            /* Initialise Time */
            ConvertTo61850Time(&atObj[ucObjTypeCnt][ucObjects].tObjectTime);
           	Object.uiField3 = TIME_STAMP_INDEX;
            UpdateValue.pvData = &atObj[ucObjTypeCnt][ucObjects].tObjectTime;
            UpdateValue.ucType = IEC61850_DATATYPE_TIMESTAMP;
            UpdateValue.uiBitLength = IEC61850_TIMESTAMP_BITSIZE;
            /* Send Update for Time Stamp */
        		IEC61850_Update(myServer, &Object, &UpdateValue);
         }
      }
   }

}

/* Read Callback Function */
int MyReadFunction(IEC61850_ObjectID * ptObjectID, IEC61850_ObjectData * ptReturnedValue)
{
   unsigned char ucObjects		= 0;
   unsigned char ucFound 			= 0;

  	/* Each Object within Object type */
   for(ucObjects = 0; ucObjects < OBJECTS;  ucObjects++)
   {
      /* Check if the Field matches */
      if((ptObjectID->uiField1 == atObj[DIGINPUT_INDEX][ucObjects].ucObjectNo) &&
         ((ptObjectID->uiField2 == atObj[DIGINPUT_INDEX][ucObjects].ucObjectType)))
      {

      	if(ptObjectID->uiField3 == VALUE_INDEX)
         {
            /* Return Value */
            memcpy(ptReturnedValue->pvData, &atObj[DIGINPUT_INDEX][ucObjects].ucObjectValue,(ptReturnedValue->uiBitLength/8));
            ucFound = 1;
         }

         if(ucFound) break;


      	if(ptObjectID->uiField3 == QUALITY_INDEX)
         {
            /* Return Value */
            memcpy(ptReturnedValue->pvData, &atObj[DIGINPUT_INDEX][ucObjects].usiObjectQuality,2);
            ucFound = 1;
         }

         if(ucFound) break;

        	if(ptObjectID->uiField3 == TIME_STAMP_INDEX)
         {
            /* Return Value */
              memcpy(ptReturnedValue->pvData, &atObj[DIGINPUT_INDEX][ucObjects].tObjectTime,(IEC61850_TIMESTAMP_BITSIZE/8));
            ucFound = 1;
         }

        	if(ucFound) break;


      }
      else
      {
      	  /* Check if the Field matches */
         if((ptObjectID->uiField1 == atObj[DIGOUTPUT_INDEX][ucObjects].ucObjectNo) &&
            ((ptObjectID->uiField2 == atObj[DIGOUTPUT_INDEX][ucObjects].ucObjectType)))
         {
            if(ptObjectID->uiField3 == VALUE_INDEX)
            {
	            memcpy(ptReturnedValue->pvData, &atObj[DIGOUTPUT_INDEX][ucObjects].ucObjectValue,(ptReturnedValue->uiBitLength/8));
               ucFound = 1;
            }

				if(ucFound) break;
         }

         if(ucFound) break;
      }





   }
	return (0);
}



/* Write Function */
int MyWriteFunction(IEC61850_ObjectID * ptObjectID, const IEC61850_ObjectData * ptNewValue)
{
//	static unsigned char ucPrevLED = 0;
   unsigned char 			ucLED 	 		= 0;
   unsigned char 			ucObjects		= 0;
   unsigned char			blLEDChange 	= 0;
   unsigned char			ucFound        = 0;


	  /* Each Object within Object type */
   for(ucObjects = 0; ucObjects < OBJECTS;  ucObjects++)
   {
      /* Check if the Field matches */
      if((ptObjectID->uiField1 == atObj[DIGOUTPUT_INDEX][ucObjects].ucObjectNo) &&
         ((ptObjectID->uiField2 == atObj[DIGOUTPUT_INDEX][ucObjects].ucObjectType)))
      {

      	if(ptObjectID->uiField3 == VALUE_INDEX)
         {
            /* Get the Value of the */
            memcpy(&ucLED, ptNewValue->pvData, sizeof(unsigned char));
            if(ucLED != 0)
            {
            	ucLED = 1;
            }
            /* Check if the LED has changed */
            if(atObj[DIGOUTPUT_INDEX][ucObjects].ucObjectValue != ucLED)
            {
               /* Set the Value */
               atObj[DIGOUTPUT_INDEX][ucObjects].ucObjectValue = ucLED;
               blLEDChange = 1;
               ucFound = 1;
            }
         }
      }

      if(ucFound) break;
   }

	/* LED Changed */
	if(blLEDChange)
   {
   	ucLED = 0;
      /* Get all the values of the LED */
      for(ucObjects = 0; ucObjects < OBJECTS;  ucObjects++)
      {
      	/* Form Byte to Output */
    		ucLED = (ucLED | (atObj[DIGOUTPUT_INDEX][ucObjects].ucObjectValue << ucObjects));
      }
#ifdef DEBUG_SV61850
      printf("\r\n Write LED : %X ", ucLED);
#endif
      /* Output the LED */
      outportb(IO_ADDR,ucLED);
   }

	return(0);
}

/* IO Handler Task */
void huge IOHandler(void)
{
  	unsigned char 			ucDIPValue		= 0;		// DIP Switch Value
   unsigned char			ucPrevDIPValue = 0;     // Previous DIP Swithc Value
   unsigned char			nucObjects 		= 0;		// Total Objects
  	IEC61850_ObjectData 	UpdateValue		= {0};	// Value to send on Change
	IEC61850_ObjectID		Object			= {0};	// ID of the Object
   unsigned char 			ucObjectVal 	= 0;		// Local Object Value
   unsigned short	int   usiQuality 		= 0;     // Local Quality
   tNTPTimeStamp			tNTPTime 		= {0};   // Local Time Stamp


   while(1)	// Indefinite Loop
   {
   		/* Read DIP Switch Value */
         ucDIPValue = inportb(IO_ADDR);
         /* If previous value does not match current value */
         if(ucPrevDIPValue != ucDIPValue)
         {
#ifdef DEBUG_SV61850
            printf("\r\n DIP Value : %X", ucDIPValue);
#endif

            /* Check which swithc has changed */
            for(nucObjects = 0; nucObjects < OBJECTS; nucObjects++)
            {
            	/* Get the Values which have changed */
              ucObjectVal = ((ucDIPValue & (1 << nucObjects)) >> nucObjects);
              if(ucObjectVal != atObj[0][nucObjects].ucObjectValue)
              {
#ifdef DEBUG_SV61850
                 printf("\r\n  %u : %X",  atObj[0][nucObjects].ucObjectNo, ucObjectVal);
#endif
					  /* Common Field to all Index */
                 Object.uiField1 = nucObjects + 1; // Object Number
                 Object.uiField2 = DIGITAL_INPUT;  // Object Type

                 /* Object Value */
                 UpdateValue.pvData = &ucObjectVal;
                 UpdateValue.ucType = IEC61850_DATATYPE_BOOLEAN;
                 UpdateValue.uiBitLength = 8;
                 /* Object Value Index */
                 Object.uiField3 = VALUE_INDEX;
                 /* Update Value in the database */
                 atObj[0][nucObjects].ucObjectValue 		 = ucObjectVal;
                 /* Send Update for Value */
                 IEC61850_Update(myServer, &Object, &UpdateValue);

                 usiQuality = 0;
                 if(atObj[0][nucObjects].usiObjectQuality != usiQuality)
                 {
                    /* Object Quality  */
                    /* No way to determine if DIP Switch failed so */
                    UpdateValue.pvData = &usiQuality;
                    UpdateValue.ucType = IEC61850_DATATYPE_QUALITY;
                    UpdateValue.uiBitLength = IEC61850_QUALITY_BITSIZE;
                    Object.uiField3 = QUALITY_INDEX;
                    /* Update Quality in the database */
                    atObj[0][nucObjects].usiObjectQuality    = usiQuality;
                    /* Send Update for Quality */
                    IEC61850_Update(myServer, &Object, &UpdateValue);
                 }

                 /* Send Time */
                 /* Convert to 61850 Time */
                 ConvertTo61850Time(&tNTPTime);
                 UpdateValue.pvData = &tNTPTime;
                 UpdateValue.ucType = IEC61850_DATATYPE_TIMESTAMP;
				 	  UpdateValue.uiBitLength = IEC61850_TIMESTAMP_BITSIZE;
                 Object.uiField3 = TIME_STAMP_INDEX;
                 /* Update Time in the database */
                 memcpy(&atObj[0][nucObjects].tObjectTime, &tNTPTime, sizeof(tNTPTimeStamp));
                 /* Send Update for Time Stamp */
                 IEC61850_Update(myServer, &Object, &UpdateValue);
              }
            }
            ucPrevDIPValue =   ucDIPValue;
         }
         RTX_Sleep_Time(1000);
      }
}

/* Convert to 61850 Time */
void ConvertTo61850Time(tNTPTimeStamp *ptObjectTime)
{
	TimeDate_Structure tTimeDate 	= {0};
  	struct time	tTime 				= {0};
	struct date	tDate 				= {0};
   unsigned long int	uliDigit 	= 0;
   unsigned long int uliMicrosec = 0;

	RTX_Get_TimeDate (&tTimeDate);

   tTime.ti_hour = tTimeDate.hr;
   tTime.ti_min = tTimeDate.min;
   tTime.ti_sec = tTimeDate.sec;

   tDate.da_day = tTimeDate.dy;
   tDate.da_mon = tTimeDate.mn;
   tDate.da_year = tTimeDate.yr + 2000;

   //Load Seconds since 1 Jan 1900
	ptObjectTime->uliSeconds = dostounix(&tDate, &tTime); // Calculate Number of second

   ptObjectTime->uliFractionsOfSecond = 0;

   for(uliDigit = 0x80000000L; uliDigit > 0; uliDigit = uliDigit /2)	// Loop down throught 2^-1 to 2^-16
   {
      uliMicrosec = uliMicrosec * 2;	// Check for a muilt of fraction
      if(uliMicrosec >= 1000000L)
      {
         ptObjectTime->uliFractionsOfSecond  = ptObjectTime->uliFractionsOfSecond | uliDigit; // Set bit to 1
         uliMicrosec = uliMicrosec - 1000000L; // Remove 1000000
      }
   }

   ptObjectTime->uliFractionsOfSecond = (ptObjectTime->uliFractionsOfSecond & 0xFFFFFF00L) | IEC61850_TIMEQUALITY_ACCURACY_UNSPECIFIED | IEC61850_TIMEQUALITY_LEAP_SECOND_KNOWN; // Set Time accurcy and Leap Second Known
}

/******************* NTP Time Start Section ************************************************/

/******************************************************************************
* headerGetLi()
*
* Gets the LI field from the given TSntpHeader-structure.
*
* Returns the value of the LI field or 0xFF if no valid TSntpHeader-structure
* is given.
******************************************************************************/
unsigned char headerGetLi(
                           TSntpHeader *sntpHeader )  // TSntpHeader-structure to
                                                      // get field from
{
  if( sntpHeader )
    return ( sntpHeader->li_vn_mode & 0xC0 ) >> 6;
  else return 0xFF;
}



/******************************************************************************
* headerSetLi()
*
* Sets the LI field in the given TSntpHeader-structure.
******************************************************************************/
void headerSetLi(
                  unsigned char li,          // Value of the LI field
                  TSntpHeader *sntpHeader )  // TSntpHeader-structure to set field in
{
  if( sntpHeader &&
      li <= 3 )
    sntpHeader->li_vn_mode |= ( li & 0x03 ) << 6;
}



/******************************************************************************
* headerSetVn()
*
* Sets the VN field in the given TSntpHeader-structure.
******************************************************************************/
void headerSetVn(
                  unsigned char vn,          // Value of the VN field
                  TSntpHeader *sntpHeader )  // TSntpHeader-structure to set field in
{
  if( sntpHeader &&
      vn <= 7 )
    sntpHeader->li_vn_mode |= ( vn & 0x07 ) << 3;
}



/******************************************************************************
* headerSetMode()
*
* Sets the Mode field in the given TSntpHeader-structure.
******************************************************************************/
void headerSetMode(
                    unsigned char mode,        // Value of the Mode field
                    TSntpHeader *sntpHeader )  // TSntpHeader-structure to set field in
{
  if( sntpHeader &&
      mode <= 7 )
    sntpHeader->li_vn_mode |= ( mode & 0x07 );
}




/******************************************************************************
* getTime()
******************************************************************************/
BOOL getTime(
              struct date *date,
              struct time *time,     // Destination for date and time (DOS format)
              char *remoteIp,        // IP address of the NTP server
              signed int timezone )  // Addend for timezone (e.g. +1 for MET)
{
  static TSntpHeader sntpHeader;  // SNTP header for request/ reply
  struct sockaddr_in address;     // Address structure
  int socket,                     // Socket for connection to server
      error;                      // Result of TCP/IP-API calls
  unsigned long unixTimestamp;    // time in Unix format


  // Check addend for timezone
  if( abs( timezone ) > 12 )
  {
    #ifdef VERBOSE
    printf( "\r\nError: The addend for the timezone is out of range!" );
    #endif
    return FALSE;
  }


  // Initialize header according tp RFC 1769 \A75
  memset( &sntpHeader, 0, sizeof( TSntpHeader ) );
  headerSetLi( 0, &sntpHeader );  // No warning
  headerSetVn( 3, &sntpHeader );  // NTP Version
  headerSetMode( 3, &sntpHeader );  // Client, unicast


  // Open socket
  if( ( socket = opensocket( SOCK_DGRAM, &error ) ) == API_ERROR )
  {
    #ifdef VERBOSE
    printf( "\r\nError: Could not open socket! (%i)", error );
    #endif
    return FALSE;
  }


  // Prepare and send request
  address.sin_family = AF_INET;
  address.sin_port = htons( NTP_PORT );
  inet_addr( remoteIp, &address.sin_addr.s_addr );
  if( sendto( socket, (char *)&sntpHeader, sizeof( TSntpHeader ), MSG_BLOCKING,
             (const struct sockaddr *)&address, &error ) == API_ERROR )
  {
    #ifdef VERBOSE
    printf( "\r\nError: Could not send request! (%i)", error );
    #endif
    closesocket( socket, &error );
    return FALSE;
  }


  // Receive reply
  memset( &sntpHeader, 0, sizeof( TSntpHeader ) );   // Clear SNTP-header
  memset( &address, 0, sizeof( struct sockaddr ) );  // Clear address
  address.sin_family = AF_INET;
  if( recvfrom( socket, (char *)&sntpHeader, sizeof( TSntpHeader ), MSG_TIMEOUT, 30000L,
                (struct sockaddr *)&address, &error ) < sizeof( TSntpHeader ) )
  {
    #ifdef VERBOSE
    printf( "\r\nError: Could not receive reply! (%i)", error );
    #endif
    closesocket( socket, &error );
    return FALSE;
  }
  closesocket( socket, &error );


  // Check reply
  if( headerGetLi( &sntpHeader ) == 3 ||
      sntpHeader.stratum  < 1 || sntpHeader.stratum > 15 ||
      sntpHeader.transmitTimestamp[0] == 0 ||
      sntpHeader.transmitTimestamp[1] == 0 )
    printf( "\r\nWarning: Clock of NTP server is not synchronized!" );


  // Convert time to Unix format
  sntpHeader.transmitTimestamp[0] = hostToLE32(sntpHeader.transmitTimestamp[0]);
  unixTimestamp = ( sntpHeader.transmitTimestamp[0] & 0xFF000000L ) >> 24;
  unixTimestamp |= ( sntpHeader.transmitTimestamp[0] & 0x00FF0000L ) >> 8;
  unixTimestamp |= ( sntpHeader.transmitTimestamp[0] & 0x0000FF00L ) << 8;
  unixTimestamp |= ( sntpHeader.transmitTimestamp[0] & 0x000000FFL ) << 24;
                   // Correct byte order and discard decimal places
  unixTimestamp -= OFFSET_JAN_1_1970;  // NTP delivers the number of seconds since
                                       // January 1st 1900. Unix format is number
                                       // of seconds since January 1st 1970.

  // Correct time according to timezone
  unixTimestamp += timezone * 3600;

  // Convert time to DOS format
  unixtodos( unixTimestamp, date, time );


  return TRUE;
}
/********************* NTP Time End Section **********************************/
