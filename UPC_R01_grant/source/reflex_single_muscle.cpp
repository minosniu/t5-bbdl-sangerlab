
using namespace std;

//------------------------------------------------------------------------
// DESTester.CPP
//
// This is the C++ source file for the FPGA-based DES encryptor/decryptor.
// This source calls the Opal Kelly FrontPanel C++ API to perform all
// communication with the device including:
//  - PLL Configuration
//  - Spartan-3 configuration file download
//  - Data transfer for the DES block.
//
// The general operation is as follows:
// 1. The device PLL is configured to generate appropriate clocks for
//    the hardware we have implemented.
// 2. A Xilinx configuration file is downloaded to the FPGA to configure
//    it with our hardware.  This file was generated using the Xilinx
//    tools.
// 3. Hardware setup is performed:
//    a. A reset signal is sent using a WireIn.
//    b. The DES key (64-bits) is set using 8 separate WireIns.
//    c. Another WireIn is set to control the DES 'decrypt' input.
//    d. The input and output files are opened.
//    e. The transfer block RAM address pointer is reset using a TriggerIn.
// 4. The DES algorithm is continually run on data from the input file
//    until the input is exhausted.  Results are stored in the output file:
//    a. A block of 2048 bytes is read from the input.
//    b. This block is sent to the FPGA using okCFrontPanel::WriteToPipeIn(...)
//    c. A TriggerIn is activated to start the DES state machine.
//    d. A block of 2048 bytes is read from the FPGA using the method
//       okCXEM::ReadFromPipeOut(...)
//    e. The block is written to the output file.
//    f. This sequence is repeated until the input file is exhausted.
//
//
// Copyright (c) 2004-2009
// Opal Kelly Incorporated
// $Rev: 1010 $ $Date: 2011-10-08 17:49:29 -0700 (Sat, 08 Oct 2011) $
//------------------------------------------------------------------------

#include <iostream>
#include <conio.h>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "okFrontPanelDLL.h"
/*
*	Name:			reflex_single_muscle

*	Author:			C. Minos Niu

*	Email:			minos.niu AT sangerlab.net

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
double gAuxvar [NUM_AUXVAR];
pthread_t gThreads[NUM_THREADS];
pthread_mutex_t gMutexPosition;
TaskHandle gEnableHandle, gForceReadTaskHandle, gAOTaskHandle, gEncoderHandle;
void ledIndicator ( float w, float h );
float64 gLenOrig, gLenScale, gMuscleLce;
bool gIsWindingUp,bEnableMotors,gIsRecording=false;
LARGE_INTEGER gInitTick, gCurrentTick, gClkFrequency;
FILE *gDataFile;
float64 gMotorCmd[NUM_MOTOR]={0.0};

OGLGraph* gMyGraph;

void init ( GLvoid )     // Create Some Everyday Functions
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.f);				// Black Background
    //glClearDepth(1.0f);								// Depth Buffer Setup
    gMyGraph = OGLGraph::Instance();
    gMyGraph->setup( 500, 100, 10, 20, 2, 2, 1, 200 );
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

    
    gMyGraph->update( 10.0 * gAuxvar[0] );
    //printf("%.4lf \n", g_force[0]);
    gMyGraph->draw();
    
    
    if(!bEnableMotors)
        glColor3f(1.0f,0.0f,0.0f);
    else
        glColor3f(0.0f,1.0f,0.0f);
    ledIndicator ( 10.0f,80.0f );

    if(gIsWindingUp)
        glColor3f(1.0f,0.0f,0.0f);
    else
        glColor3f(0.0f,1.0f,0.0f);
    ledIndicator ( 30.0f,80.0f );

    if(!gIsRecording)
        glColor3f(1.0f,0.0f,0.0f);
    else
        glColor3f(0.0f,1.0f,0.0f);
    ledIndicator ( 50.0f,80.0f );
    glColor3f(1.0f,1.0f,1.0f);
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
}


void keyboard ( unsigned char key, int x, int y )  // Create Keyboard Function
{
    switch ( key ) 
    {
    case 27:        // When Escape Is Pressed...

        exit(0);   // Exit The Program
        break;        
    case 32:        // SpaceBar 
        DisableMotors(&gEnableHandle);
        bEnableMotors=false;
        break;  
    case 'R':       //Winding up
    case 'r':
        if(!gIsRecording)
            gIsRecording=true;
        else
            gIsRecording=false;
        break;case 'W':       //Winding up
    case 'w':
        if(!gIsWindingUp)
            gIsWindingUp=true;
        else
            gIsWindingUp=false;
        break;
    case 'z':       //Rezero
    case 'Z':
        gLenOrig=gAuxvar[2];
        break;
    case 69:        // E 
    case 101:        // e
        EnableMotors(&gEnableHandle);
        bEnableMotors=true;
        break;  
    default:        
        break;
    }
}

void idle(void)
{
    glutPostRedisplay();
}

// This Fucntion performs the Experimental Protocol

#define		DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

void* control_loop(void*)
{
    
	int32       error=0;
	char        errBuff[2048]={'\0'};

    while (1)
    {
        int i=0;

		//printf("\n\t%f",dataEncoder[0]); 

        
        //printf("f1 %0.4lf :: f2 %0.4lf :::: p1 %0.4lf :: p2 %0.4lf \n", 
        //    gAuxvar[0], gAuxvar[1], gAuxvar[2], gAuxvar[3]);
        if(_kbhit())
        {
            break;

        }
    } 
	return 0;
}

void exitProgram() 
{
    DisableMotors(&gEnableHandle);
    StopPositionRead(&gEncoderHandle);
    StopSignalLoop(gForceReadTaskHandle);
}

void initProgram()
{
    //WARNING: DON'T CHANGE THE SEQUENCE BELOW
    StartPositionRead(&gEncoderHandle);
    StartSignalLoop(gForceReadTaskHandle);
    EnableMotors(&gEnableHandle);
    bEnableMotors=true;

}

void main ( int argc, char** argv )   // Create Main Function For Bringing It All Together
{
    gLenOrig=0.0;
    gLenScale=0.0001;
    glutInit( &argc, argv ); // Erm Just Write It =)
    init();

    glutInitDisplayMode( GLUT_RGB | GLUT_DOUBLE ); // Display Mode
    glutInitWindowSize( 500, 250 ); // If glutFullScreen wasn't called this is the window size
    glutCreateWindow( "OpenGL Graph Component" ); // Window Title (argv[0] for current directory as title)
    glutDisplayFunc( display );  // Matching Earlier Functions To Their Counterparts
    glutReshapeFunc( reshape );
    glutKeyboardFunc( keyboard );
    glutIdleFunc(idle);
  
    initProgram();
    atexit( exitProgram );

    // gAuxvar = {current force 0, current force 1, current pos 0, current pos 1};
    int ctrl_handle = pthread_create(&gThreads[0], NULL, control_loop,	(void *)gAuxvar);
   
    glutMainLoop( );          // Initialize The Main Loop  

    return;

}

