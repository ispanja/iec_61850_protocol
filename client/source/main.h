//---------------------------------------------------------------------------

#ifndef MainH
#define MainH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "About.h"
#include "IPSelection.h"
#include "IEC61850API.h"
#include "TntSysUtils.hpp"
#include "TntForms.hpp"
#include "TntMenus.hpp"
#include "TntComCtrls.hpp"
#include "TntStdCtrls.hpp"
#include "TntIniFiles.hpp"
#include "TntFileCtrl.hpp"
#include "TntClasses.hpp"
#include <ComCtrls.hpp>
#include <time.h>
#include "VrControls.hpp"
#include "VrLeds.hpp"
#include "VrMatrix.hpp"
#include "JvExControls.hpp"
#include "JvSwitch.hpp"
#include "VrBanner.hpp"
#include "HTMLCredit.hpp"
#include <ExtCtrls.hpp>
#include <Graphics.hpp>
#define REPORT_INPUTS_FIELD2 1
#define REPORT_OUTPUTS_FIELD2 2
#define SPCSO_COUNT		8
#define DK61_IND1_V 1
#define DK61_IND2_V 2
#define DK61_IND3_V 3
#define DK61_IND4_V 4
#define DK61_ALM1_V 5
#define DK61_ALM2_V 6
#define DK61_ALM3_V 7
#define DK61_ALM4_V 8


//---------------------------------------------------------------------------
class TForm_Main : public TTntForm
{
__published:	// IDE-managed Components
	TTntStatusBar *SB_Status;
	TPanel *Panel2;
	TPanel *Panel3;
	TTntButton *BT_Connect;
	TPanel *Panel4;
	TTntGroupBox *TntGroupBox3;
	TVrUserLed *VrUserLed1;
	TTntLabel *TntLabel1;
	TVrUserLed *VrUserLed2;
	TVrUserLed *VrUserLed3;
	TVrUserLed *VrUserLed4;
	TVrUserLed *VrUserLed5;
	TVrUserLed *VrUserLed6;
	TVrUserLed *VrUserLed7;
	TVrUserLed *VrUserLed8;
	TTntLabel *TntLabel2;
	TTntLabel *TntLabel3;
	TTntLabel *TntLabel4;
	TTntLabel *TntLabel5;
	TTntLabel *TntLabel6;
	TTntLabel *TntLabel7;
	TTntLabel *TntLabel8;
	TPanel *Panel1;
	TVrMatrix *VrMatrix2;
	TImage *Image2;
	TVrMatrix *VrMatrix4;
	TVrBanner *VrBanner1;
	THTMLCredit *HTMLCredit1;
	TImage *Image1;
	TPanel *Panel5;
	TTntGroupBox *TntGroupBox1;
	TVrMatrix *VrMatrix1;
	TJvSwitch *JvSwitch1;
	TJvSwitch *JvSwitch2;
	TJvSwitch *JvSwitch3;
	TJvSwitch *JvSwitch4;
	TTntGroupBox *TntGroupBox2;
	TVrMatrix *VrMatrix3;
	TJvSwitch *JvSwitch5;
	TJvSwitch *JvSwitch6;
	TJvSwitch *JvSwitch7;
	TJvSwitch *JvSwitch8;
	void __fastcall BT_ConnectClick(TObject *Sender);
	void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
	void __fastcall VrUserLed1Click(TObject *Sender);
private:	// User declarations
		IEC61850			myClient;    //private member for 61850 client
		IEC61850_Parameters	tClientParam; //61850 Client parameters
		WideString ProgramDir;            //program directory
		void __fastcall GetSwitchStatus(TVrUserLed *led);
		TTntLabel *SPCSO_Label[SPCSO_COUNT]; //array of pointers for the LED control lable.
		TVrUserLed *SPCSO[SPCSO_COUNT];      //array of pointers for the LED control.
public:		// User declarations
	__fastcall TForm_Main(TComponent* Owner);
	static void IEC61850_UpdateCallback(IEC61850_ObjectID * ptObjectID, const IEC61850_ObjectData * ptNewValue);

};
//---------------------------------------------------------------------------
extern PACKAGE TForm_Main *Form_Main;
//---------------------------------------------------------------------------
#endif
