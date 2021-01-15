
 */
/*****************************************************************************/

/******************************************************************************
*	Includes
******************************************************************************/

#include <stdio.h>

#include "syscapps.h"
#include "sysclibr.h"

#define SUPPORTED_PROTOCOL		IEC61850_SYSTEMCORP_GENERIC
#include "IEC61850API.h"

#include "protmain.h"
/* Windows Headers */
#include <windows.h>



/******************************************************************************
*	Defines
******************************************************************************/

/******************************************************************************
*	Global Variables
******************************************************************************/
//Global coordinates accessing by the main and the callback function
COORD SwitchDescriptionPos[8]={{0,11},{10,11},{20,11},{30,11},{40,11},{50,11},{60,11},{70,11}};
COORD SwitchPos[8]={{0,10},{10,10},{20,10},{30,10},{40,10},{50,10},{60,10},{70,10}};
COORD LEDDescriptionPos[8]={{0,21},{10,21},{20,21},{30,21},{40,21},{50,21},{60,21},{70,21}};
COORD LEDPos[8]={{0,20},{10,20},{20,20},{30,20},{40,20},{50,20},{60,20},{70,20}};
COORD InputPos={0,23};
COORD ErrorPos={0,24};
COORD Position;
HANDLE hOut;


/******************************************************************************
*	Prototypes
*****************************************************************************/
void UpdateFunction(IEC61850_ObjectID * ptObjectID,const IEC61850_ObjectData * ptNewValue);

/*****************************************************************************/
/*****************************************************************************/                  
unsigned char GetOutputStatus(IEC61850 myClient,int index)
{
	IEC61850_ObjectData 	Value;
	IEC61850_ObjectID	Object;
	int iError;
	unsigned char bState=0;
	Object.uiField1=index;
	Object.uiField2=2;
	Object.uiField3=1;
	Object.uiField4=0;
	Object.uiField5=0;

	if (myClient == NULL) //error if myClient is not initialized
	{
		bState=0;
	}
	else {
		//set the value
		Value.ucType =  IEC61850_DATATYPE_BOOLEAN;
		Value.uiBitLength = sizeof(bState)*8;
		Value.pvData = &bState;
		//read from the client
		iError = IEC61850_Read(myClient, &Object, &Value);
		if(iError != IEC61850_ERROR_NONE)
		{
				SetConsoleCursorPosition(hOut,ErrorPos);
				printf("Failed to read from DO:SPCSO%d",Object.uiField1);
				bState = 0;//default is false
		}
		else {
			unsigned char *bReadStatus;
			bReadStatus = (unsigned char *)Value.pvData;
			bState =*bReadStatus;
		}
	}
	return bState;
}


/*****************************************************************************/
/*****************************************************************************/                  
int main(void)
{
    //HANDLE hIn;
    //HANDLE hError;
	
	tErrorCode 		tErrCode 			= -1;
	tErrorValue		tErrValue			= 0;
	tErrorMessage		tErrMess			= {0};
	IEC61850			myClient = NULL;
	IEC61850_Parameters	tClientParam;
	int 					iError = 0;
	unsigned char OutputStatus[8];	
	DWORD Written;
	char InputMsg[]="Enter (1 to 8) to control the LEDs (x to exit):";
	
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	Position.X=0;
	Position.Y=0;
	FillConsoleOutputCharacter(hOut,' ',2000,Position,&Written);
	SetConsoleCursorPosition(hOut,Position);
	printf("\r\n ----------------------------------------------------------------------------");
	printf("\r\n SystemCORP Pty Ltd");
	printf("\r\n Suite 4/12 Brodie Hall Drive, Bentley, Technology Park,Western Australia,6102");
	printf("\r\n email: info@systemcorp.com.au  Telephone:+61 (0) 8 9472 8500");
	printf("\r\n http://www.systemcorp.com.au");	
	printf("\r\n ----------------------------------------------------------------------------");
	
	tErrMess.tProdID = WEBCAN_PRODUCT_ID;
	tErrMess.tAppNo = IEC61850_CLIENT_APP;

	do
	{
		//Print out the console user interface
		for (iError=0;iError<4;iError++){
			SetConsoleCursorPosition(hOut,SwitchDescriptionPos[iError]);
			printf("DO:Ind%d",iError+1);
		}
		for (iError=4;iError<8;iError++){
			SetConsoleCursorPosition(hOut,SwitchDescriptionPos[iError]);
			printf("DO:Alm%d",iError+1);
		}
		for (iError=0;iError<8;iError++){
			SetConsoleCursorPosition(hOut,LEDDescriptionPos[iError]);
			printf("DO:SPCSO%d",iError+1);
		}
		Position.X=10;
		Position.Y=SwitchPos[0].Y-2;
		SetConsoleCursorPosition(hOut,Position);
		printf("Report Control Blocks");
		Position.Y=SwitchPos[0].Y-1;
		SetConsoleCursorPosition(hOut,Position);
		printf("LN:DIPS_GGIO1");

		Position.X=50;
		Position.Y=SwitchPos[0].Y-2;
		SetConsoleCursorPosition(hOut,Position);
		printf("GOOSE Control Blocks");
		Position.Y=SwitchPos[0].Y-1;
		SetConsoleCursorPosition(hOut,Position);
		printf("LN:DIPS_GGIO2");

		Position.X=25;
		Position.Y=LEDPos[0].Y-2;
		SetConsoleCursorPosition(hOut,Position);
		printf("Single point control outputs");
		Position.Y=LEDPos[0].Y-1;
		SetConsoleCursorPosition(hOut,Position);
		printf("LN:LEDO_GGIO3");

		Position.X=strlen(InputMsg);
		Position.Y=InputPos.Y;

		//set the client parameters
		tClientParam.ClientServerFlag = IEC61850_CLIENT;
		tClientParam.ptReadCallback = NULL;
		tClientParam.ptWriteCallback = NULL;
		tClientParam.ptUpdateCallback = UpdateFunction;
		//Create the 61850 Client application
		myClient = IEC61850_Create(&tClientParam, &iError);  //Create a client
		if(myClient == NULL)
		{
			printf("Client Failed to create:%i", iError);
		}
		//Load the Client ICD file
		iError = IEC61850_LoadSCLFile(myClient,"DK61Client.ICD");
		if(iError != IEC61850_ERROR_NONE)
		{
			printf("Loading error has occured: %i", iError);
			break;
		}
		//Start the 61850 Client
		iError = IEC61850_Start(myClient);
		if(iError != IEC61850_ERROR_NONE)
		{
			printf("Failed to start client: %i", iError);
			break;
		}
		for (iError=0;iError<8;iError++){
			OutputStatus[iError]=GetOutputStatus(myClient,iError+1);
			SetConsoleCursorPosition(hOut,LEDPos[iError]);
			if (OutputStatus[iError])
				printf("ON ");
			else 
				printf("OFF");
		}
		SetConsoleCursorPosition(hOut,InputPos);
		printf(InputMsg);
		//main processing loop
		while(1) 
		{
			char c;
			// Read in a value
			IEC61850_ObjectData Value;
			IEC61850_ObjectID	Object;
			unsigned char		u8Val = 0;
			
			c = getchar();
			if ((c >= '1') && (c <= '8')){ //control output command
				Object.uiField1 = c-'0';
				Object.uiField2=2; //Field 2 is set to 2, please refer to the private field in the ICD file
				Object.uiField3=1;
				Object.uiField4=0;
				Object.uiField5=0;
				Value.ucType =  IEC61850_DATATYPE_BOOLEAN;
				Value.uiBitLength = sizeof(u8Val)*8;
				u8Val=!OutputStatus[Object.uiField1-1];
				Value.pvData = &u8Val;
				//write to the control
				iError = IEC61850_Write(myClient, &Object, &Value);
				if(iError != IEC61850_ERROR_NONE)
				{
					SetConsoleCursorPosition(hOut,ErrorPos);
					printf("Failed to write to DO:SPCSO%d",Object.uiField1);
				}
				else {
					//read back the result
					iError = IEC61850_Read(myClient, &Object, &Value);					
					if(iError != IEC61850_ERROR_NONE)
					{
						SetConsoleCursorPosition(hOut,ErrorPos);
						printf("Failed to Read from DO:SPCSO%d",Object.uiField1);
					}
					else {
						unsigned char *bReadStatus;
						bReadStatus = (unsigned char *)Value.pvData;
						SetConsoleCursorPosition(hOut,LEDPos[Object.uiField1-1]);
						OutputStatus[Object.uiField1-1]=*bReadStatus;
						if (*bReadStatus) {
							printf("ON ");
						}
						else {
							printf("OFF");
						}
						SetConsoleCursorPosition(hOut,Position);
					}
				}

			}
			else { 
				if ((c=='x')||(c=='X')){//exit
					break;
				}
			}
		}

		// Shutting down client 
		iError = IEC61850_Stop(myClient);
		if(iError != IEC61850_ERROR_NONE)
		{
			SetConsoleCursorPosition(hOut,ErrorPos);
			printf("Failed to stop client: %i", iError);
		}
	} while(0);

	if( myClient != NULL )
	{
		// End of program
		IEC61850_Free(myClient);
	}

	if(tErrCode != 0)
	{
		tErrMess.tErrCode = tErrCode;
		tErrMess.tErrValue = tErrValue;
	}
	SetConsoleCursorPosition(hOut,ErrorPos);
	printf("\r\n Application \'%s\' has terminated.", PROTOCOL_APPNAME);
	return(0);
}   // End main()


/*****************************************************************************/
/*****************************************************************************/                  
void UpdateFunction(IEC61850_ObjectID * ptObjectID, const IEC61850_ObjectData * ptNewValue)
{
	if(ptNewValue->ucType == IEC61850_DATATYPE_BOOLEAN)
	{
		if (ptObjectID->uiField2==1){ //Field 2 is set to 1 for input, please refer to the private field in the ICD file			
			unsigned char *bReadStatus;			
			bReadStatus = (unsigned char *)ptNewValue->pvData;
			SetConsoleCursorPosition(hOut,SwitchPos[ptObjectID->uiField1-1]);
			if (*bReadStatus){
				printf("ON ");
			}
			else {
				printf("OFF ");
			}
			//reset the cursor position
			SetConsoleCursorPosition(hOut,Position);
		}
	}
}

// End of file
