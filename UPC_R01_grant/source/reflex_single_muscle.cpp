using namespace std;

#include <iostream>
#include <conio.h>
#include <fstream>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <process.h>
#include <AntTweakBar.h>
#include "okFrontPanelDLL.h"
/*
    Name:			reflex_single_muscle    
    Author:			C. Minos Niu    
    Email:			minos.niu AT sangerlab.net
*/

#include	<math.h>
#include	<pthread.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<time.h>
#include	"NIDAQmx.h"
#include	"PxiDAQ.h"
#include	"Utilities.h"
#include	"glut.h"   // The GL Utility Toolkit (Glut) Header
#include	"OGLGraph.h"


// *** Global variables
float                   gAuxvar [NUM_AUXVAR*NUM_MOTOR];
pthread_t               gThreads[NUM_THREADS];
pthread_mutex_t         gMutex;
TaskHandle              gEnableHandle, gForceReadTaskHandle, gAOTaskHandle, gEncoderHandle[NUM_MOTOR];
float                   gLenOrig[NUM_MOTOR], gLenScale[NUM_MOTOR], gMuscleLce[NUM_MOTOR], gMuscleVel[NUM_MOTOR];
bool                    gResetSim=false,gIsRecording=false, gResetGlobal=false;
LARGE_INTEGER           gInitTick, gCurrentTick, gClkFrequency;
FILE                    *gDataFile, *gConfigFile;
int                     gCurrMotorState = MOTOR_STATE_INIT;
double                  gEncoderCount[NUM_MOTOR];
float64                 gMotorCmd[NUM_MOTOR]={0.0, 0.0};
okCFrontPanel           *gFpgaBiceps, *gFpgaTriceps;
float                   gCtrlFromFPGA[NUM_FPGA];
int                     gMuscleEMG[NUM_FPGA], gMNCount[NUM_FPGA];

OGLGraph*               gMyGraph;
char                    gLceLabel1[60];
char                    gLceLabel2[60];
char                    gTimeStamp[20];
char                    gStateLabel[5][30] = { "MOTOR_STATE_INIT",
                                               "MOTOR_STATE_WINDING_UP",
                                               "MOTOR_STATE_OPEN_LOOP",
                                               "MOTOR_STATE_CLOSED_LOOP",                            
                                               "MOTOR_STATE_SHUTTING_DOWN"};
//IPP
Ipp32f* taps0;
Ipp32f* taps1;
Ipp32f* dly0;
Ipp32f* dly1;
//IppsFIRState_32f *pFIRState0, *pFIRState1;
IppsIIRState_32f *pIIRState0, *pIIRState1;

// Sinewave
int gWave[1024];

//Descending command
int gM1Voluntary = 0;
int gM1Dystonia = 0;

//AntTweakBar
TwBar *gBar; // Pointer to the tweak bar

void ledIndicator ( float w, float h );

void init ( GLvoid )     // Create Some Everyday Functions
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.f);				// Black Background
    //glClearDepth(1.0f);								// Depth Buffer Setup
    gMyGraph = OGLGraph::Instance();
    gMyGraph->setup( 500, 100, 10, 20, 2, 2, 1, 200 );
}
void outputText(int x, int y, char *string)
{
  int len, i;
  glRasterPos2f(x, y);
  len = (int) strlen(string);
  for (i = 0; i < len; i++)
  {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, string[i]);
  }
}
void display ( void )   // Create The Display Function
{

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (float) 800 / (float) 600, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // This is a dummy function. Replace with custom input/data
    float time = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
    float value;
    value = 5*sin( 5*time ) + 10.f;

    
    //gMyGraph->update( 10.0 * gAuxvar[0] );
    gMyGraph->update( gMuscleVel[0] );

    gMyGraph->draw();
    
    
    if(MOTOR_STATE_WINDING_UP)
        glColor3f(1.0f,0.0f,0.0f);
    else
        glColor3f(0.0f,1.0f,0.0f);
    ledIndicator( 10.0f,80.0f );


    if(!gIsRecording)
        glColor3f(1.0f,0.0f,0.0f);
    else
        glColor3f(0.0f,1.0f,0.0f);
    ledIndicator( 50.0f,80.0f );
    glColor3f(1.0f,1.0f,1.0f);

    // Draw tweak bars
    TwDraw();
//    sprintf_s(gLceLabel1,"%f    %.2f   %.2f", gMuscleLce[0], gMuscleVel[0], gCtrlFromFPGA[0]);
    sprintf_s(gLceLabel1,"%f    %d   %.2f", gMuscleLce[0], gM1Voluntary, gCtrlFromFPGA[0]);
    outputText(10,95,gLceLabel1);
//    sprintf_s(gLceLabel2,"%f    %.2f   %.2f", gMuscleLce[1], gMuscleVel[1],gCtrlFromFPGA[NUM_FPGA - 1]);
    sprintf_s(gLceLabel2,"%f    %d   %.2f", gMuscleLce[1], gM1Dystonia, gCtrlFromFPGA[NUM_FPGA - 1]);
    outputText(10,85,gLceLabel2);
    //printf("\n\t%f\t%f", gMuscleVel[0], gMuscleVel[1]);
    
    //sprintf_s(gStateLabel,"%.2f    %.2f   %f",gAuxvar[0], gMuscleLce, gCtrlFromFPGA[0]);
    outputText(300,95,gStateLabel[gCurrMotorState]);

    

    glutSwapBuffers ( );
}

void ledIndicator ( float w, float h )   // Create The Reshape Function (the viewport)
{
    glBegin(GL_TRIANGLE_FAN);
        glVertex3f(w,h,0.0f);
        glVertex3f(w,h+10.0f,0.0f);
        glVertex3f(w+10.0,h+10.0f,0.0f);
        glVertex3f(w+10.0f,h,0.0f);
    glEnd();
}
void reshape ( int w, int h )   // Create The Reshape Function (the viewport)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (float) w / (float) h, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glViewport(0, 0, w, h);

    // Send the new window size to AntTweakBar
    TwWindowSize(w, h);
}

int SendButton(okCFrontPanel *xem, int buttonValue, char *evt)
{
    if (0 == strcmp(evt, "BUTTON_RESET_SIM"))
    {
        if (buttonValue) 
            xem -> SetWireInValue(0x00, 0xff, 0x02);
        else 
            xem -> SetWireInValue(0x00, 0x00, 0x02);
        xem -> UpdateWireIns();
    }
    else if (0 == strcmp(evt, "BUTTON_RESET_GLOBAL"))
    {
        if (buttonValue) 
            xem -> SetWireInValue(0x00, 0xff, 0x01);
        else 
            xem -> SetWireInValue(0x00, 0x00, 0x01);
        xem -> UpdateWireIns();
    }
    return 0;
}
int ProceedFSM(int *state);
int InitMotor(int *state);

int ProceedFSM(int *state)
{
    switch(*state)
    {
        case MOTOR_STATE_INIT:
            EnableMotors(&gEnableHandle);
            *state = MOTOR_STATE_WINDING_UP;
            break;
        case MOTOR_STATE_WINDING_UP:
            *state = MOTOR_STATE_OPEN_LOOP;
            break;
        case MOTOR_STATE_OPEN_LOOP:
            *state = MOTOR_STATE_CLOSED_LOOP;
            break;
        case MOTOR_STATE_CLOSED_LOOP:
            DisableMotors(&gEnableHandle);
            *state = MOTOR_STATE_SHUTTING_DOWN;
            break;
        case MOTOR_STATE_SHUTTING_DOWN:
            *state = MOTOR_STATE_SHUTTING_DOWN;
            break;
        default:
            *state = MOTOR_STATE_INIT;
    }
    return 0;
}

int InitMotor(int *state)
{
    DisableMotors(&gEnableHandle);
    *state = MOTOR_STATE_INIT;
    return 0;
}

int ShutdownMotor(int *state);
int ShutdownMotor(int *state)
{
    DisableMotors(&gEnableHandle);
    *state = MOTOR_STATE_SHUTTING_DOWN;
    return 0;
}

void keyboard ( unsigned char key, int x, int y )  // Create Keyboard Function
{
    switch ( key ) 
    {
    case 27:        // When Escape Is Pressed...

        exit(0);   // Exit The Program
        break;        
    //case 32:        // SpaceBar 
    //    //ShutdownMotor(&gCurrMotorState);
    //    break;  
    case 'G':       //Proceed in FSM
    case 'g':
        ProceedFSM(&gCurrMotorState);
        break;
    case 'R':       //Winding up
    case 'r':
        if(!gIsRecording)
        {
            gIsRecording=true;
        }
        else
            gIsRecording=false;
        break;
    case '0':       //Reset SIM
        if(!gResetSim)
            gResetSim=true;
        else
            gResetSim=false;
        SendButton(gFpgaBiceps, (int) gResetSim, "BUTTON_RESET_SIM");
        SendButton(gFpgaTriceps, (int) gResetSim, "BUTTON_RESET_SIM");
        break;
    case '9':       //Reset GLOBAL
        if(!gResetGlobal)
            gResetGlobal=true;
        else
            gResetGlobal=false;
        SendButton(gFpgaBiceps, (int) gResetGlobal, "BUTTON_RESET_GLOBAL");
        SendButton(gFpgaTriceps, (int) gResetGlobal, "BUTTON_RESET_GLOBAL");
        break;
    case 'z':       //Rezero
    case 'Z':
        gLenOrig[0]=gAuxvar[2] + gEncoderCount[0];
        gLenOrig[1]=gAuxvar[2+NUM_AUXVAR] + gEncoderCount[1];
        break;
    case 'E':         
    case 'e':     
        InitMotor(&gCurrMotorState);
        break;  
    default:        
        break;
    }
    TwEventKeyboardGLUT(NULL,NULL,NULL);
}

void idle(void)
{
    glutPostRedisplay();
}



int ReadFPGA(okCFrontPanel *xem, BYTE getAddr, char *type, float32 *outVal)
{
    xem -> UpdateWireOuts();
    // Read 18-bit integer from FPGA
    if (0 == strcmp(type, "int18"))
    {
        //intValLo = self.xem.GetWireOutValue(getAddr) & 0xffff # length = 16-bit
        //intValHi = self.xem.GetWireOutValue(getAddr + 0x01) & 0x0003 # length = 2-bit
        //intVal = ((intValHi << 16) + intValLo) & 0xFFFFFFFF
        //if intVal > 0x1FFFF:
        //    intVal = -(0x3FFFF - intVal + 0x1)
        //outVal = float(intVal) # in mV De-Scaling factor = 0xFFFF
    }
    // Read 32-bit float
    else if (0 == strcmp(type, "float32")) 
    {
        int32 outValLo = xem -> GetWireOutValue(getAddr) & 0xffff;
        int32 outValHi = xem -> GetWireOutValue(getAddr + 0x01) & 0xffff;
        int32 outValInt = ((outValHi << 16) + outValLo) & 0xFFFFFFFF;
        memcpy(outVal, &outValInt, sizeof(*outVal));
        //outVal = ConvertType(outVal, 'I', 'f')
        //#print outVal
    }

    return 0;
}


int ReadFPGA(okCFrontPanel *xem, BYTE getAddr, char *type, int *outVal)
{
    xem -> UpdateWireOuts();
    // Read 18-bit integer from FPGA
    if (0 == strcmp(type, "int18"))
    {
        //intValLo = self.xem.GetWireOutValue(getAddr) & 0xffff # length = 16-bit
        //intValHi = self.xem.GetWireOutValue(getAddr + 0x01) & 0x0003 # length = 2-bit
        //intVal = ((intValHi << 16) + intValLo) & 0xFFFFFFFF
        //if intVal > 0x1FFFF:
        //    intVal = -(0x3FFFF - intVal + 0x1)
        //outVal = float(intVal) # in mV De-Scaling factor = 0xFFFF
    }
    // Read 32-bit signed integer from FPGA
    else if (0 == strcmp(type, "int32"))
    {
        int outValLo = xem -> GetWireOutValue(getAddr) & 0xffff;
        int outValHi = xem -> GetWireOutValue(getAddr + 0x01) & 0xffff;
        int outValInt = ((outValHi << 16) + outValLo) & 0xFFFFFFFF;
        *outVal = outValInt;
    //elif type == "int32" :
    //    intValLo = self.xem.GetWireOutValue(getAddr) & 0xffff # length = 16-bit
    //    intValHi = self.xem.GetWireOutValue(getAddr + 0x01) & 0xffff # length = 16-bit
    //    intVal = ((intValHi << 16) + intValLo) & 0xFFFFFFFF
    //    outVal = ConvertType(intVal, 'I',  'i')  # in mV De-Scaling factor = 128  #????
    }

    return 0;
}

int ReInterpret(float32 in, int32 *out)
{
    memcpy(out, &in, sizeof(int32));
    return 0;
}

int ReInterpret(int32 in, int32 *out)
{
    memcpy(out, &in, sizeof(int32));
    return 0;
}

int WriteFPGA(okCFrontPanel *xem, int32 bitVal, int32 trigEvent)
{
    //bitVal = 0;

    int32 bitValLo = bitVal & 0xffff;
    int32 bitValHi = (bitVal >> 16) & 0xffff;

    
    //xem -> SetWireInValue(0x01, bitValLo, 0xffff);
    if (okCFrontPanel::NoError != xem -> SetWireInValue(0x01, bitValLo, 0xffff)) {
		printf("SetWireInLo failed.\n");
		//delete xem;
		return -1;
	}
    if (okCFrontPanel::NoError != xem -> SetWireInValue(0x02, bitValHi, 0xffff)) {
		printf("SetWireInHi failed.\n");
		//delete xem;
		return -1;
	}
   
    xem -> UpdateWireIns();
    xem -> ActivateTriggerIn(0x50, trigEvent)   ;
    
    return 0;
}

int WriteFpgaLceVel(okCFrontPanel *xem, int32 bitValLce, int32 bitValVel, int32 bitValM1Voluntary, int32 bitValM1Dystonia, int32 trigEvent)
{
    //bitVal = 0;

    int32 bitValLo = bitValLce & 0xffff;
    int32 bitValHi = (bitValLce >> 16) & 0xffff;

    
    //Send muscle lce to fpga
    if (okCFrontPanel::NoError != xem -> SetWireInValue(0x01, bitValLo, 0xffff)) {
		printf("SetWireInLo failed.\n");
		//delete xem;
		return -1;
	}
    if (okCFrontPanel::NoError != xem -> SetWireInValue(0x02, bitValHi, 0xffff)) {
		printf("SetWireInHi failed.\n");
		//delete xem;
		return -1;
	}


    bitValLo = bitValVel & 0xffff;
    bitValHi = (bitValVel >> 16) & 0xffff;    
    //send muscle velocity to Fpga
    if (okCFrontPanel::NoError != xem -> SetWireInValue(0x03, bitValLo, 0xffff)) {
		printf("SetWireInLo failed.\n");
		//delete xem;
		return -1;
	}
    if (okCFrontPanel::NoError != xem -> SetWireInValue(0x04, bitValHi, 0xffff)) {
		printf("SetWireInHi failed.\n");
		//delete xem;
		return -1;
	}

    

    bitValLo = bitValM1Voluntary & 0xffff;
    bitValHi = (bitValM1Voluntary >> 16) & 0xffff;    
    //send muscle velocity to Fpga
    if (okCFrontPanel::NoError != xem -> SetWireInValue(0x05, bitValLo, 0xffff)) {
		printf("SetWireInLo failed.\n");
		//delete xem;
		return -1;
	}
    if (okCFrontPanel::NoError != xem -> SetWireInValue(0x06, bitValHi, 0xffff)) {
		printf("SetWireInHi failed.\n");
		//delete xem;
		return -1;
    }  

    bitValLo = bitValM1Dystonia & 0xffff;
    bitValHi = (bitValM1Dystonia >> 16) & 0xffff;    
    //send muscle velocity to Fpga
    if (okCFrontPanel::NoError != xem -> SetWireInValue(0x07, bitValLo, 0xffff)) {
		printf("SetWireInLo failed.\n");
		//delete xem;
		return -1;
	}
    if (okCFrontPanel::NoError != xem -> SetWireInValue(0x08, bitValHi, 0xffff)) {
		printf("SetWireInHi failed.\n");
		//delete xem;
		return -1;
	}

    xem -> UpdateWireIns();
    xem -> ActivateTriggerIn(0x50, trigEvent)   ;
    
    return 0;
}


void* ControlLoop(void*)
{
    
	int32       error=0;
	char        errBuff[2048]={'\0'};
        
    int32 IEEE_30;
    ReInterpret((float32)(30.0), &IEEE_30);
    WriteFPGA(gFpgaBiceps, IEEE_30, 1);
    WriteFPGA(gFpgaTriceps, IEEE_30, 1);    

    //Gamma_dyn = 40 (low)
    int32 IEEE_40;
    ReInterpret((float32)(40.0), &IEEE_40);
    WriteFPGA(gFpgaBiceps, IEEE_40, 4);
    WriteFPGA(gFpgaTriceps, IEEE_40, 4);
    
    //Set i_gain_syn_SN_to_CN
    WriteFPGA(gFpgaBiceps, 1, 12); //working 1
    WriteFPGA(gFpgaTriceps, 1, 12);

    //Set i_gain_syn_SN_to_MN
    WriteFPGA(gFpgaBiceps, 1, 6); // working with 2
    WriteFPGA(gFpgaTriceps, 1, 6);


    //int32 bitValM1Dystonia;
    //ReInterpret((int32)(5000), &bitValM1Dystonia);
    //WriteFPGA(gFpgaBiceps, bitValM1Dystonia, DATA_EVT_M1_DYS);
    //WriteFPGA(gFpgaTriceps, bitValM1Dystonia, DATA_EVT_M1_DYS);

    long iLoop = 0;
    while (1)
    {
        if(GetAsyncKeyState(VK_SPACE))
        {
            ShutdownMotor(&gCurrMotorState);

        }

        if ((MOTOR_STATE_CLOSED_LOOP != gCurrMotorState) && (MOTOR_STATE_OPEN_LOOP != gCurrMotorState)) continue;
		//printf("\n\t%f",dataEncoder[0]); 
        iLoop++;
/*        if (10000 <= iLoop)
        {
            gM1Dystonia = 5000;
            gM1Voluntary = gWave[iLoop % 1024];
        }    */    

        float32 tGainBic = 0.11; // working = 0.141
        float32 tGainTri = 0.11; // working = 0.141
        float32 forceBiasBic = 10.0f;
        float32 forceBiasTri = 10.0f;
        float   coef_damp = 0.04; // working = 0.3
        float   muslceDamp = 0.04;

        float rawCtrlBic, rawCtrlTri;
        int muscleEMG = 0;
        //ReadFPGA(gFpgaBiceps, 0x30, "float32", &rawCtrlBic);
        ReadFPGA(gFpgaBiceps, 0x26, "int32", &gMNCount[0]);
        //ReadFPGA(gFpgaBiceps, 0x32, "int32", &muscleEMG);
        gMuscleEMG[0] = muscleEMG;
        

        // Read FPGA1
        //ReadFPGA(gFpgaTriceps, 0x30, "float32", &rawCtrlTri);
        ReadFPGA(gFpgaTriceps, 0x26, "int32", &gMNCount[1]);
        //ReadFPGA(gFpgaTriceps, 0x32, "int32", &muscleEMG);
        gMuscleEMG[NUM_FPGA-1] = muscleEMG;

        /*
        gCtrlFromFPGA[0] = adjustedForceBic;
        gCtrlFromFPGA[1] = adjustedForceTri;*/
        //PthreadMutexUnlock(&gMutex);

        //printf("%.4f\t", gCtrlFromFPGA[0]);
        //    gAuxvar[0], gAuxvar[1], gAuxvar[2], gAuxvar[3]);

        int32 bitValLce, bitValVel;
        int32   bitM1VoluntaryBic = 0, 
                bitM1VoluntaryTri = 0, 
                bitM1DystoniaBic = 000, 
                bitM1DystoniaTri = 000;

        ReInterpret((float32)(gMuscleLce[0]), &bitValLce);
        ReInterpret((float32)(muslceDamp * gMuscleVel[0]), &bitValVel);
        // M1 drive        
        //ReInterpret((int32)(max(0, gM1Voluntary)), &bitM1VoluntaryBic);
        //ReInterpret((int32)(gM1Dystonia), &bitM1DystoniaBic);
        WriteFpgaLceVel(gFpgaBiceps, bitValLce, bitValVel, bitM1VoluntaryBic, bitM1DystoniaBic, DATA_EVT_LCEVEL);
        
        ReInterpret((float32)(gMuscleLce[1]), &bitValLce);
        ReInterpret((float32)(muslceDamp * gMuscleVel[1]), &bitValVel);
        //ReInterpret((int32)(max(0, -gM1Voluntary)), &bitM1VoluntaryTri);
        //ReInterpret((int32)(gM1Dystonia), &bitM1DystoniaTri);
        WriteFpgaLceVel(gFpgaTriceps, bitValLce, bitValVel, bitM1VoluntaryTri, bitM1DystoniaTri, DATA_EVT_LCEVEL);


        //printf("%.2f\t", gWave[1020]);

        Sleep(1);
        if(_kbhit()) break;
    } 
	return 0;
}

void SaveConfigCache()
{
    FILE *ConfigCacheFile;
    ConfigCacheFile= fopen("ConfigPXI.cache","w");
    fprintf(ConfigCacheFile,"%lf\t%lf",gLenScale[0],gLenScale[1]);
    fclose(ConfigCacheFile);
}

int InitFpga(okCFrontPanel *xem0)
{   
	if (okCFrontPanel::NoError != xem0->OpenBySerial()) {
		delete xem0;
		printf("Device could not be opened.  Is one connected?\n");
		return(NULL);
	}
	
	printf("Found a device: %s\n", xem0->GetBoardModelString(xem0->GetBoardModel()).c_str());

    // Configure the PLL using default config
	xem0->LoadDefaultPLLConfiguration();

	// Get some general information about the XEM.
	printf("Device firmware version: %d.%d\n", xem0->GetDeviceMajorVersion(), xem0->GetDeviceMinorVersion());
	printf("Device serial number: %s\n", xem0->GetSerialNumber().c_str());
	printf("Device ID string: %s\n", xem0->GetDeviceID().c_str());


    okCPLL22393 *pll;
    pll = new okCPLL22393;
    pll -> SetReference(48);        //base clock frequency
    const int HIGHEST_CLK_FREQ = 48 * 4; // in MHz
    int baseRate = HIGHEST_CLK_FREQ; //in MHz
    pll -> SetPLLParameters(0, baseRate, 48,  true);
    pll -> SetOutputSource(0, okCPLL22393::ClkSrc_PLL0_0);
    int clkRate = HIGHEST_CLK_FREQ; //mhz; 200 is fastest
    //pll -> SetOutputDivider(0, 1) ;
    pll -> SetOutputEnable(0, true);
    xem0 -> SetPLL22393Configuration(*pll);

    // Download the configuration file.
	if (okCFrontPanel::NoError != xem0->ConfigureFPGA(FPGA_BIT_FILENAME)) {
		printf("FPGA configuration failed.\n");
		delete xem0;
		return(-1);
	}


    //int newHalfCnt = 1 * 200 * (10 **6) / SAMPLING_RATE / NUM_NEURON / (value*4) / 2 / 2;
    //const int32 X_REAL_TIME = 1;
    const int32 X_REAL_TENTH_TIME = 10;

    //int32 newHalfCnt = HIGHEST_CLK_FREQ * (int32)(1e6) / 1024 / 128 / X_REAL_TIME / 2 / 2;
    
    int32 newHalfCnt = HIGHEST_CLK_FREQ * (int32)(1e6) * 10 / 1024 / 128 / X_REAL_TENTH_TIME / 2 / 2;
    printf("newHalfCnt = %d \n", newHalfCnt);
//    WriteFPGA(xem0, 197, DATA_EVT_CLKRATE);
    WriteFPGA(xem0, newHalfCnt, DATA_EVT_CLKRATE);


	// Check for FrontPanel support in the FPGA configuration.
	if (false == xem0->IsFrontPanelEnabled()) {
		printf("FrontPanel support is not enabled.\n");
		delete xem0;
		return(-1);
	}
    
    SendButton(xem0, (int) true, "BUTTON_RESET_SIM");
    SendButton(xem0, (int) false, "BUTTON_RESET_SIM");
	printf("FrontPanel support is enabled.\n");


	return 0;
}

FileContainer *gSwapFiles;

void InitProgram()
{
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf_s(gTimeStamp,"%4d%02d%02d%02d%02d.txt",timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min);

    // Load Fpga DLLs
    char dll_date[32], dll_time[32];

	printf("---- Opal Kelly ---- FPGA-DES Application v1.0 ----\n");
	if (FALSE == okFrontPanelDLL_LoadLib(NULL)) {
		printf("FrontPanel DLL could not be loaded.\n");
		return;
	}
	okFrontPanelDLL_GetVersion(dll_date, dll_time);
	printf("FrontPanel DLL loaded.  Built: %s  %s\n", dll_date, dll_time);

    // Two muscles, each with one Fpga handle
    gFpgaBiceps = new okCFrontPanel;
    gFpgaTriceps = new okCFrontPanel;
        
    if (0 != InitFpga(gFpgaBiceps)) {
		printf("FPGA could not be initialized.\n");
		return;
	};

    if (0 != InitFpga(gFpgaTriceps)) {
		printf("FPGA could not be initialized.\n");
		return;
	};   

    gSwapFiles = new FileContainer;


    
    //IPP
    taps0 = ippsMalloc_32f(lenFilter);
    taps1 = ippsMalloc_32f(lenFilter);
    dly0  = ippsMalloc_32f(lenFilter);
    dly1  = ippsMalloc_32f(lenFilter);

    //taps0[0] =  0.0078; // for Lowpass filter velocity
    //taps0[1] =  0.0156;
    //taps0[2] =  0.0078;
    //taps0[3] =  1.0000;
    //taps0[4] = -1.7347;
    //taps0[5] =  0.7660;

    //taps1[0] =  0.0078;
    //taps1[1] =  0.0156;
    //taps1[2] =  0.0078;
    //taps1[3] =  1.0000;
    //taps1[4] = -1.7347;
    //taps1[5] =  0.7660;
    float   P   =   1.0;
    float   e   =   2.7183;
    float   T   =   0.001;
    float   tau   =   0.090; // rising time of muscle twitch in seconds
    float   a   =   exp(-T / tau);
    float   pefat = P * e * T * a / tau;
    

    taps0[0] =   0.0000000 * pefat;
    taps0[1] =   1.00 * pefat;
    taps0[2] =   0.0000000 * pefat;
    taps0[3] =   1.00 * pefat;
    taps0[4] =  -2 * a * pefat;
    taps0[5] =   a * a * pefat;
                          
    taps1[0] =   0.0000000 * pefat;
    taps1[1] =   1.00 * pefat;
    taps1[2] =   0.0000000 * pefat;
    taps1[3] =   1.00 * pefat;
    taps1[4] =  -2 * a * pefat;
    taps1[5] =   a * a * pefat;


    ippsZero_32f(dly0,lenFilter);
    ippsZero_32f(dly1,lenFilter);
        
    //ippsFIRInitAlloc_32f( &pFIRState0, taps0, lenFilter, dly0 );
    //ippsFIRInitAlloc_32f( &pFIRState1, taps1, lenFilter, dly1 );
    ippsIIRInitAlloc_32f( &pIIRState0, taps0, lenFilter, dly0 );
    ippsIIRInitAlloc_32f( &pIIRState1, taps1, lenFilter, dly1 );

    // *** Get a sine wave
    SineGen(gWave);
    //{
    //    float F = 1.0f; // in Hz
    //    float BIAS = 20000.0f;
    //    float AMP = 10000.0f;
    //    float PHASE = 0.0f;
    //    float SAMPLING_RATE = 1024.0f;
    //    float dt = 1.0f / SAMPLING_RATE; // Sampling interval in seconds
    //    float periods = 1.0;

    //    auto w = F * 2 * PI * dt;
    //    int max_n = floor(periods * SAMPLING_RATE / F);
    //    printf("max_n = %d\n", max_n);
    //    for (int i = 0; i < max_n; i++)
    //    {
    //        gWave[i] = (int) (AMP * sin(w * i + PHASE) + BIAS);
    //    }
    //}
 
    //WARNING: DON'T CHANGE THE SEQUENCE BELOW
    StartReadPos(&gEncoderHandle[0], &gEncoderHandle[1]);
    StartSignalLoop(&gAOTaskHandle, &gForceReadTaskHandle); 
    InitMotor(&gCurrMotorState);
}

void ExitProgram() 
{
    //pthread_join(gThreads[0], NULL);
    SaveConfigCache();
    DisableMotors(&gEnableHandle);    
    StopSignalLoop(&gAOTaskHandle, &gForceReadTaskHandle);
    StopPositionRead(&gEncoderHandle[0],&gEncoderHandle[1]);


    //IPP
    //ippsFIRFree_32f(pFIRState0);
    //ippsFIRFree_32f(pFIRState1);
    
    ippsFree(taps0);
    ippsFree(taps1);
    ippsFree(dly0);
    ippsFree(dly1);


    ippsIIRFree_32f(pIIRState0);
    ippsIIRFree_32f(pIIRState1);
    
    TwDeleteBar(gBar);

    TwTerminate();    
    delete gSwapFiles;

    delete gFpgaBiceps;
    delete gFpgaTriceps;
}

inline void LogData( void)
{   
    // Approximately 100 Hz Recording
    double actualTime;
    QueryPerformanceCounter(&gCurrentTick);
    actualTime = gCurrentTick.QuadPart - gInitTick.QuadPart;
    actualTime /= gClkFrequency.QuadPart;
    if (gIsRecording)
    {   
        fprintf(gDataFile,"%.3lf\t",actualTime );																	
        //fprintf(gDataFile,"%f\t%f\t%f\t%d\t", gMuscleLce[0],gMuscleVel[0],gCtrlFromFPGA[0],gMuscleEMG[0]);			
        //fprintf(gDataFile,"%f\t%f\t%f\t%d\t", gMuscleLce[1],gMuscleVel[1],gCtrlFromFPGA[1],gMuscleEMG[1]);			
        fprintf(gDataFile,"%f\t%f\t%f\t%d\t", gMuscleLce[0],gMuscleVel[0],gCtrlFromFPGA[0],gMNCount[0]);			
        fprintf(gDataFile,"%f\t%f\t%f\t%d\t", gMuscleLce[1],gMuscleVel[1],gCtrlFromFPGA[1],gMNCount[1]);			
        fprintf(gDataFile,"\n");
    }
}


void TimerCB (int iTimer)
{
    LogData();

	// Set The Timer For This Function Again
	glutTimerFunc (3, TimerCB, 1);
}

void* NoTimerCB (void *)
{
    while (1)
    {
        LogData();
        Sleep(1);
    }
}

int main ( int argc, char** argv )   // Create Main Function For Bringing It All Together
{
    gLenOrig[0]=0.0;
    gLenOrig[1]=0.0;
    
    gM1Voluntary= 0.0;
    gM1Dystonia= 0.0;
    //gLenScale=0.0001;
    
    FILE *ConfigFile;
    ConfigFile= fopen("ConfigPXI.txt","r");

    fscanf(ConfigFile,"%f\t%f",&gLenScale[0],&gLenScale[1]);

    fclose(ConfigFile);
    glutInit( &argc, argv ); // Erm Just Write It =)
    init();

    glutInitDisplayMode( GLUT_RGB | GLUT_DOUBLE ); // Display Mode
    glutInitWindowSize( 800, 600); // If glutFullScreen wasn't called this is the window size
    glutCreateWindow( "OpenGL Graph Component" ); // Window Title (argv[0] for current directory as title)
    glutDisplayFunc( display );  // Matching Earlier Functions To Their Counterparts
    glutReshapeFunc( reshape );

    TwInit(TW_OPENGL, NULL);

    glutMouseFunc((GLUTmousebuttonfun)TwEventMouseButtonGLUT);
    glutMotionFunc((GLUTmousemotionfun)TwEventMouseMotionGLUT);
    glutPassiveMotionFunc((GLUTmousemotionfun)TwEventMouseMotionGLUT); // same as MouseMotion
    glutKeyboardFunc((GLUTkeyboardfun)TwEventKeyboardGLUT);
    glutSpecialFunc((GLUTspecialfun)TwEventSpecialGLUT);
    // TODO: Code the TimerCB() to log data
    //printf("\n\t%f",gMuscleLce); 
    //***** RELOCATE THIS -->   
    //glutTimerFunc(3, TimerCB, 1);

    // send the ''glutGetModifers'' function pointer to AntTweakBar
    TwGLUTModifiersFunc(glutGetModifiers);

    glutKeyboardFunc( keyboard );
    glutIdleFunc(idle);




    // Make sure to pair InitProgram() with ExitProgram()
    // Resources need to be released  
    InitProgram();
   
    atexit(ExitProgram);


    // gAuxvar = {current force 0, current force 1, current pos 0, current pos 1};

    pthread_mutex_init (&gMutex, NULL);
    int ctrl_handle = pthread_create(&gThreads[0], NULL, ControlLoop,	(void *)gAuxvar);
    int logdata_handle = pthread_create(&gThreads[1], NULL, NoTimerCB,	NULL);
   
    gBar = TwNewBar("TweakBar");
    TwDefine(" GLOBAL help='This is our interface for the T5 Project BBDL-SangerLab.' "); // Message added to the help bar.
    TwDefine(" TweakBar size='400 200' color='96 216 224' "); // change default tweak bar size and color

    // Add 'g_Zoom' to 'bar': this is a modifable (RW) variable of type TW_TYPE_FLOAT. Its key shortcuts are [z] and [Z].
    TwAddVarRW(gBar, "Gain", TW_TYPE_FLOAT, &gLenScale, 
               " min=0.0000 max=0.0002 step=0.000001 keyIncr=l keyDecr=L help='Scale the object (1=original size).' ");
    //TwAddVarRW(gBar, "M1Voluntary", TW_TYPE_INT32, &gM1Voluntary, 
    //           " min=0.00 max=500000.00 step=40000 ");
    TwAddVarRW(gBar, "M1Dystonia", TW_TYPE_INT32, &gM1Dystonia, 
               " min=0.00 max=500000.00 step=10000 ");

    glutMainLoop( );          // Initialize The Main Loop  

    return 0;

}

