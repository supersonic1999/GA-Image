////////////////////////////
// CREDIT: DR PAUL ANGEL
////////////////////////////
#include "stdafx.h"
#include <glew\glew.h>
#include <glew\wglew.h>
#include <freeglut\freeglut.h>
#include <CoreStructures\CoreStructures.h>
#include <CGImport\CGModel\CGModel.h>
#include <CGImport\Importers\CGImporters.h>
#include "ShaderLoader.h"
#include "TextureLoader.h"
#include <random>
#include <ctime>
#include <new>

#include "Quad.h"
#include "GeneticAlgorithm.h"

using namespace std;
using namespace CoreStructures;

#pragma region Scene variables and resources

// Variables needed to track where the mouse pointer is so we can determine which direction it's moving in
int								mouse_x, mouse_y;

GUClock							*mainClock = nullptr;

Quad *renderedQuad = nullptr;
GeneticAlgorithm *GA = nullptr;

//
// Main scene resources
//
GUPivotCamera					*mainCamera = nullptr;

#pragma endregion

#pragma region Function Prototypes

void init(void); // Main scene initialisation function
void update(void); // Main scene update function
void display(void); // Main scene render function

// Event handling functions
void mouseButtonDown(int button_id, int state, int x, int y);
void mouseMove(int x, int y);
void mouseWheel(int wheel, int direction, int x, int y);
void closeWindow(void);

#pragma endregion

int _tmain(int argc, char* argv[]) {

	srand(time(NULL));
	rand(); // Error if not called (wierd bug)

	glutInit(&argc, argv);
	CoInitialize(NULL);

	init();
	mainClock = new GUClock();

	// Main event / render loop
	// Actors to be rendered to scene.
	GA = new GeneticAlgorithm(20, 100, 1.0f, 
		GeneticAlgorithm::sType::RANK, 
		GeneticAlgorithm::cType::ONE_POINT, 
		GeneticAlgorithm::mType::NON_UNIFORM, 
		"Resources/Images/tom.jpg");
	renderedQuad = new Quad();
	renderedQuad->GA = GA;
	glutMainLoop();

	// Stop clock and report final timing data
	if (mainClock) {
		mainClock->stop();
		mainClock->reportTimingData();
		
		mainClock->release();
	}
	
	// Shut down COM and exit
	CoUninitialize();
	return 0;
}

#pragma region Initialisation, Update and Render

void init(void) {
	glutInitContextVersion(4, 0);
	glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);

	// Setup OpenGL Display mode - include MSAA x4
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE | GLUT_MULTISAMPLE);
	glutSetOption(GLUT_MULTISAMPLE, 4);

	// Setup window
	int windowWidth = 800;
	int windowHeight = 800;
	glutInitWindowSize(windowWidth, windowHeight);
	glutInitWindowPosition(64, 64);
	glutCreateWindow("Real-Time Rendering Techniques");
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	// Register callback functions
	glutIdleFunc(update); // Main scene update function
	glutDisplayFunc(display); // Main render function
	glutMouseFunc(mouseButtonDown); // Mouse button handler
	glutMotionFunc(mouseMove); // Mouse move handler
	glutMouseWheelFunc(mouseWheel); // Mouse wheel event handler
	glutCloseFunc(closeWindow); // Main resource cleanup handler

	// Initialise GLEW library
	GLenum err = glewInit();

	// Ensure GLEW was initialised successfully before proceeding
	if (err == GLEW_OK) {
		cout << "GLEW initialised okay\n";
	} else {
		cout << "GLEW could not be initialised\n";
		throw;
	}	

	// Initialise OpenGL
	wglSwapIntervalEXT(0);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Setup main camera
	float viewportAspect = (float)windowWidth / (float)windowHeight;
	mainCamera = new GUPivotCamera(0.0f, 0.0f, 105.0f, 55.0f, viewportAspect, 0.1f);
}

// Main scene update function (called by FreeGLUT's main event loop every frame) 
void update(void) {

	// Update clock
	mainClock->tick();

	// Redraw the screen
	display();

	// Update the window title to show current frames-per-second and seconds-per-frame data
	char timingString[256];
	sprintf_s(timingString, 256, "12046280 Final Year Project. Average fps: %.0f; Generation: %i; Best Fitness: %.7f", mainClock->averageFPS(), GA->generation, GA->topFitness);
	glutSetWindowTitle(timingString);
}

// Main scene rendering function
void display(void) {
	int time = glutGet(GLUT_ELAPSED_TIME);

	// Clear the screen
	glClearColor(255.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Set viewport to the client area of the current window
	glViewport(0, 0, glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
	
	// Get view-projection transform as a GUMatrix4
	GUMatrix4 viewProjectionMatrix;
	viewProjectionMatrix = mainCamera->projectionTransform() * mainCamera->viewTransform();
		
	// Render Actors
	renderedQuad->display(viewProjectionMatrix);
		
	glutSwapBuffers();
}
#pragma endregion

#pragma region Event handling functions

void mouseButtonDown(int button_id, int state, int x, int y) {

	if (button_id == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) {
			mouse_x = x;
			mouse_y = y;		
		}
	}
}

void mouseMove(int x, int y) {
	int dx = x - mouse_x;
	int dy = y - mouse_y;

	if (mainCamera)
		mainCamera->transformCamera((float)-dy, (float)-dx, 0.0f);
		
	mouse_x = x;
	mouse_y = y;
}

void mouseWheel(int wheel, int direction, int x, int y) {
	if (mainCamera) {
		if (direction<0)
			mainCamera->scaleCameraRadius(1.1f);
		else if (direction>0)
			mainCamera->scaleCameraRadius(0.9f);
	}
}

void closeWindow(void) {
	if (mainCamera) //Clean up
		mainCamera->release();
}
#pragma endregion
