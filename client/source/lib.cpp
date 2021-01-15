//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "main.h"
#include "internal\IEC61850.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "TntStdCtrls"
#pragma link "TntComCtrls"
#pragma link "VrControls"
#pragma link "VrLeds"
#pragma link "VrMatrix"
#pragma link "JvExControls"
#pragma link "JvSwitch"
#pragma link "VrBanner"
#pragma link "HTMLCredit"
#pragma resource "*.dfm"
TForm_Main *Form_Main;
//---------------------------------------------------------------------------
__fastcall TForm_Main::TForm_Main(TComponent* Owner)
	: TTntForm(Owner)
{
	//Get the program directory
	ProgramDir=TntApplication->ExeName.SubString(0,WideLastDelimiter("\\",TntApplication->ExeName));
	//Assign array of Output controls
	SPCSO_Label[0]=TntLabel1;
	SPCSO_Label[1]=TntLabel2;
	SPCSO_Label[2]=TntLabel3;
	SPCSO_Label[3]=TntLabel4;
	SPCSO_Label[4]=TntLabel5;
	SPCSO_Label[5]=TntLabel6;
	SPCSO_Label[6]=TntLabel7;
	SPCSO_Label[7]=TntLabel8;
	SPCSO[0]=VrUserLed1;
	SPCSO[1]=VrUserLed2;
	SPCSO[2]=VrUserLed3;
	SPCSO[3]=VrUserLed4;
	SPCSO[4]=VrUserLed5;
	SPCSO[5]=VrUserLed6;
	SPCSO[6]=VrUserLed7;
	SPCSO[7]=VrUserLed8;
}
//---------------------------------------------------------------------------
//get the output status of the given LED
void __fastcall TForm_Main::GetSwitchStatus(TVrUserLed *led)
{
	IEC61850_ObjectData 	Value;
	IEC61850_ObjectID	Object;
	Object.uiField1=led->Tag;  //field 1 value is stored in the led->Tag
	Object.uiField2=REPORT_OUTPUTS_FIELD2; //value is 2
	Object.uiField3=1;
	Object.uiField4=0;
	Object.uiField5=0;
	::Boolean bState;
	 //call IEC61850_Read to get the value
		Value.ucType =  IEC61850_DATATYPE_BOOLEAN;
		Value.uiBitLength = sizeof(bState)*8;
		Value.pvData = &bState;
		int error = IEC61850_Read(myClient, &Object, &Value);
		if(error != IEC61850_ERROR_NONE)
		{
			SB_Status->Panels->Items[0]->Text=WideString("Failed to read to "+SPCSO_Label[led->Tag-1]->Caption+": Error code: ")+IntToStr(error);
			led->Active = false;//default is false
		}
		else {
			//received the value, set the value to the LED
			::Boolean *bReadStatus;
			bReadStatus = (::Boolean *)Value.pvData;
			led->Active =*bReadStatus;
		}



}

//When user press connect
void __fastcall TForm_Main::BT_ConnectClick(TObject *Sender)
{
	int error;
	TCursor oldCursor = Screen->Cursor;
	Screen->Cursor = crHourGlass;

	if (!BT_Connect->Tag) { //not yet connected
		//initialize the client
		tClientParam.ClientServerFlag = IEC61850_CLIENT;
		tClientParam.ptReadCallback = NULL;
		tClientParam.ptWriteCallback = NULL;
		tClientParam.ptUpdateCallback = IEC61850_UpdateCallback;
		myClient = IEC61850_Create(&tClientParam,&error);  //Create a client
		//error creating the client
		if(myClient == NULL)
		{
			SB_Status->Panels->Items[0]->Text=WideString("Error creating IEC61850 Client. Error code: ")+IntToStr(error);
		}
		else {
			error = IEC61850_LoadSCLFile(myClient,AnsiString(ProgramDir+"\\DK61Client.ICD").c_str());
			if(error != IEC61850_ERROR_NONE)
			{
				SB_Status->Panels->Items[0]->Text=WideString("Error loading DK61Client.ICD. Error code: ")+IntToStr(error);
			}
			else {
				error = IEC61850_Start(myClient); //start the 61850 Client
				if(error != IEC61850_ERROR_NONE)
				{
					SB_Status->Panels->Items[0]->Text=WideString("Failed to start IEC61850 Client. Error code: ")+IntToStr(error);
				}
				else {
					//get the status of all the LEDs
				   for (int i = 0; i < SPCSO_COUNT; i++) {
						GetSwitchStatus(SPCSO[i]);
				   }
				   SB_Status->Panels->Items[0]->Text=WideString("IEC61850 Client is successfully connected.");
				   BT_Connect->Tag=true;
				   BT_Connect->Caption="Disconnect & Exit";
				}
			}
		}
	}
	else {//it was connected, let's disconnect
		int error = IEC61850_Stop(myClient);
		BT_Connect->Enabled=false;
		if(error != IEC61850_ERROR_NONE)
		{
			SB_Status->Panels->Items[0]->Text=WideString("Error stopping IEC61850 Client. Error code: ")+IntToStr(error);
		}
		try {
			// End of program
			IEC61850_Free(myClient);
			BT_Connect->Tag=false;
			BT_Connect->Caption="Connect";
			SB_Status->Panels->Items[0]->Text=WideString("IEC61850 Client is disconnected successfully.");
		} catch (...) {
		}
		Close(); //exit the program
	}
	//disable the LEDs
	TntGroupBox3->Enabled=BT_Connect->Tag;
	Screen->Cursor = oldCursor;
}
//---------------------------------------------------------------------------
//61850 Update callback function
void TForm_Main::IEC61850_UpdateCallback(IEC61850_ObjectID * ptObjectID, const IEC61850_ObjectData * ptNewValue)
{
	time_t t;
	::Boolean *bStatus;
	char *int8;
	short *int16;
	int *int32;
	unsigned char *uint8;
	unsigned short *uint16;
	unsigned int *uint32;
	float *f32;
	double *f64;
	bool StateOn=false;
	t = time(NULL);
	AnsiString msgtime=AnsiString(ctime(&t));
	msgtime=msgtime.SubString(0,msgtime.Length()-1);
	switch (ptNewValue->ucType) {
		//in this example, we only interested in Boolean type
		case IEC61850_DATATYPE_BOOLEAN:
			bStatus=(::Boolean *)ptNewValue->pvData;
			if (*bStatus) {
				StateOn=true;
			}
			if (ptObjectID->uiField2==REPORT_INPUTS_FIELD2) {
				switch (ptObjectID->uiField1) {
					case DK61_IND1_V:
						Form_Main->JvSwitch1->StateOn=StateOn;
					break;
					case DK61_IND2_V:
						Form_Main->JvSwitch2->StateOn=StateOn;
					break;
					case DK61_IND3_V:
						Form_Main->JvSwitch3->StateOn=StateOn;
					break;
					case DK61_IND4_V:
						Form_Main->JvSwitch4->StateOn=StateOn;
					break;
					case DK61_ALM1_V:
						Form_Main->JvSwitch5->StateOn=StateOn;
					break;
					case DK61_ALM2_V:
						Form_Main->JvSwitch6->StateOn=StateOn;
					break;
					case DK61_ALM3_V:
						Form_Main->JvSwitch7->StateOn=StateOn;
					break;
					case DK61_ALM4_V:
						Form_Main->JvSwitch8->StateOn=StateOn;
					break;
				default:
					;
				}
			}
		break;
		//the rest of the datatypes are ignored
		case IEC61850_DATATYPE_INT8:
		int8 = (char *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_INT16:
		int16 = (short *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_INT32:
		int32 = (int *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_INT8U:
		uint8 = (unsigned char *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_INT16U:
		uint16 = (unsigned short *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_INT32U:
		uint32 = (unsigned int *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_FLOAT32:
		f32 = (float *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_FLOAT64:
		f64 = (double *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_ENUMERATED:
		break;
		case IEC61850_DATATYPE_CODED_ENUM:
		uint8 = (unsigned char *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_OCTEL_STRING:
		int8 = (char *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_VISIBLE_STRING:
		int8 = (char *)ptNewValue->pvData;
		break;
		case IEC61850_DATATYPE_UNICODE_STRING:
		break;
		case IEC61850_DATATYPE_TIMESTAMP:
		break;
		case IEC61850_DATATYPE_QUALITY:
		break;
	default:
		;
	}
}
//when the page is closed, disconnect it if it is still connected
void __fastcall TForm_Main::FormClose(TObject *Sender, TCloseAction &Action)
{
	if (BT_Connect->Tag) { //it is connected
		BT_ConnectClick(NULL); //disconnect it
	}
}
//---------------------------------------------------------------------------
//when user click on the LED, call IEC61850_Write to control the LED
void __fastcall TForm_Main::VrUserLed1Click(TObject *Sender)
{
	TVrUserLed *led = (TVrUserLed*)Sender;
	IEC61850_ObjectData 	Value;
	IEC61850_ObjectID	Object;
	Object.uiField1=led->Tag;
	Object.uiField2=REPORT_OUTPUTS_FIELD2;
	Object.uiField3=1;
	Object.uiField4=0;
	Object.uiField5=0;
	::Boolean bState;
	TCursor oldCursor = Screen->Cursor;
	Screen->Cursor = crHourGlass;
		//use the state of the LED as data value
		bState = !led->Active;
		Value.ucType =  IEC61850_DATATYPE_BOOLEAN;
		Value.uiBitLength = sizeof(bState)*8;
		Value.pvData = &bState;
		//write the data through mms
		int error = IEC61850_Write(myClient, &Object, &Value);
		if(error != IEC61850_ERROR_NONE)
		{
			SB_Status->Panels->Items[0]->Text=WideString("Failed to write to "+SPCSO_Label[led->Tag-1]->Caption+": Error code: ")+IntToStr(error);
		}
		else {
			//write the successful, let's read the value back
			error = IEC61850_Read(myClient, &Object, &Value);
			led->Active=!led->Active; //change to other state
			if(error != IEC61850_ERROR_NONE)
			{
				SB_Status->Panels->Items[0]->Text=WideString("Failed to read from "+SPCSO_Label[led->Tag-1]->Caption+": Error code: ")+IntToStr(error);
			}
			else {
				//reading of the value is successful, display the message
				::Boolean *bReadStatus;
				bReadStatus = (::Boolean *)Value.pvData;
				WideString strON;
				if (*bReadStatus) {
					strON=L"ON";
				}
				else {
					strON=L"OFF";
				}
				SB_Status->Panels->Items[1]->Text=WideString(SPCSO_Label[led->Tag-1]->Caption+" is "+strON);
			}
		}
		Screen->Cursor = oldCursor;
}
//---------------------------------------------------------------------------



