// Compiling
// g++ -Wall v2.cpp gl_frontEnd.cpp -lm -lGL -lglut -pthread -o prog
// ./prog 16 16 3 4


//
//  main.c
//  Final Project CSC412
//
//  Created by Jean-Yves Hervé on 2019-12-12
//	This is public domain code.  By all means appropriate it and change is to your
//	heart's content.
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <string.h>
#include <unistd.h>
//
#include "gl_frontEnd.h"

using namespace std;

//==================================================================================
//	Function prototypes
//==================================================================================
void displayGridPane(void);
void displayStatePane(void);
void initializeApplication(void);
void cleanupGridAndLists(void);

//==================================================================================
//	Application-level global variables
//==================================================================================

//	Don't touch
/** Ints used for the grid and state pane
 * 
 */
extern int	GRID_PANE, STATE_PANE;
/** Ints used for the grid and state pane
 * 
 */
extern int 	GRID_PANE_WIDTH, GRID_PANE_HEIGHT;
/** Ints used for the grid and state pane
 * 
 */
extern int	gMainWindow, gSubwindow[2];

//	Don't rename any of these variables
//-------------------------------------
//	The state grid and its dimensions (arguments to the program)
/** The grid array
 * 
 */
int** grid;
/** The number of rows
 * 
 */
int numRows = -1;	//	height of the grid
/** The number of columns
 * 
 */
int numCols = -1;	//	width
/** The number of boxes
 * 
 */
int numBoxes = -1;	//	also the number of robots
/** The number of doors
 * 
 */
int numDoors = -1;	//	The number of doors.

/** The number of robots
 * 
 */
int numLiveThreads = 0;		//	the number of live robot threads

//	robot sleep time between moves (in microseconds)
/**	robot sleep time between moves (in microseconds)
 * 
 */
const int MIN_SLEEP_TIME = 1000;
/**	robot sleep time between moves (in microseconds)
 * 
 */
int robotSleepTime = 100000;

//	An array of C-string where you can store things you want displayed
//	in the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
/** max number of messages
 * 
 */
const int MAX_NUM_MESSAGES = 8;
/** max length of messages
 * 
 */
const int MAX_LENGTH_MESSAGE = 32;
/** array to hold the message
 * 
 */
char** message;

/** used to help with randomization/start time
 * 
 */
time_t startTime;

//==================================================================================
//	These are the functions that tie the simulation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

/** Mutex lock for the file
 * 
 */
pthread_mutex_t fileLock;


/**Mutex lock for the grid
 * 
 */
pthread_mutex_t gridLock;


/** Struct to hold the robots data
 * 
 */
typedef struct Robot {
	pthread_t 	threadID;
	int			x; //it's current location
	int			y; //current loc
	int			robotnum;
	int			doornum;
	bool		rAlive = true;
} Robot;

/** Stuct to hold the doors data
 * 
 */
typedef struct Door {
	int			x; //it's current location
	int			y; //current loc
	int			doornum;
} Door;

/** Struct to hold the boxes data
 * 
 */
typedef struct Box {
	int			x; //it's current location
	int			y; //current loc
	int			boxnum;
} Box;

/** Vector list for the robots
 * 
 */
std::vector<Robot*> robotList;
/** Vector list for the doors
 * 
 */
std::vector<Door> doorList; 
/** Vector list for the boxes
 * 
 */
std::vector<Box*> boxList; 


/*Holdovr for v2 and v3
 typedef struct Robot {
    pthread_t   threadID;
    int         index;
    int         numberToCountTo;
    int         detachYourself;
    int         sleepTime;      // in microseconds
	int			x; //it's current location
	int			y; //current loc
	int			doorX; //it's targetdoor x 
	int			doorY; //target door y
	int			boxX; //box x
	int			boxY;  //box y
} Robot;
	*/



// Grid Lock is here
/** Function to display the grid
 * 
 */
void displayGridPane(void)
{
	//	This is OpenGL/glut magic.  Don't touch
	glutSetWindow(gSubwindow[GRID_PANE]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(0, GRID_PANE_HEIGHT, 0);
	glScalef(1.f, -1.f, 1.f);


	//given a list


	for (int i=0; i<numBoxes; i++)
	{
//jyh
//	You need to synchronize this.  Otherwise, the drawing (main) thread could be reading the Robot info
//	while it's being updated.
//	And what I wrote in the comment below was not just for decoration.
//	Because you ignored it,
//	this is why your robots still show up on the grid way after they have terminated

		if(robotList[i]->rAlive == false){// not alive anymore
			// Don't draw
		}
		else{
			//	here I would test if the robot thread is still live
			//						row				column			row			column
			drawRobotAndBox(i, robotList[i]->y, robotList[i]->x,boxList[i]->y, boxList[i]->x, robotList[i]->doornum);
		}

	}


	for (int i=0; i<numDoors; i++)
	{
		
		//	here I would test if the robot thread is still alive
		//				row				column	
		drawDoor(i, doorList[i].y, doorList[i].x); 

	}

	

	//	This call does nothing important. It only draws lines
	//	There is nothing to synchronize here
	drawGrid();

	//	This is OpenGL/glut magic.  Don't touch
	glutSwapBuffers();
	
	glutSetWindow(gMainWindow);
}

/** Function to display the state
 * 
 */
void displayStatePane(void)
{
	//	This is OpenGL/glut magic.  Don't touch
	glutSetWindow(gSubwindow[STATE_PANE]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//	Here I hard-code a few messages that I want to see displayed
	//	in my state pane.  The number of live robot threads will
	//	always get displayed.  No need to pass a message about it.
	time_t currentTime = time(NULL);
	double deltaT = difftime(currentTime, startTime);

	int numMessages = 3;
	sprintf(message[0], "We have %d doors", numDoors);
	sprintf(message[1], "I like cheese");
	sprintf(message[2], "Run time is %4.0f", deltaT);

	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//
	//	You *must* synchronize this call.
	//
	//---------------------------------------------------------
	drawState(numMessages, message);
	
	
	//	This is OpenGL/glut magic.  Don't touch
	glutSwapBuffers();
	
	glutSetWindow(gMainWindow);
}

//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------
/** Function to speed up the robots
 * 
 */
void speedupRobots(void)
{
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * robotSleepTime) / 10;
	
	if (newSleepTime > MIN_SLEEP_TIME)
	{
		robotSleepTime = newSleepTime;
	}
}

/** Function to slow down the robots
 * 
 */
void slowdownRobots(void)
{
	//	increase sleep time by 20%
	robotSleepTime = (12 * robotSleepTime) / 10;
}




//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function besides
//	the initialization of numRows, numCos, numDoors, numBoxes.
//------------------------------------------------------------------------
int main(int argc, char** argv)
{
	//	We know that the arguments  of the program  are going
	//	to be the width (number of columns) and height (number of rows) of the
	//	grid, the number of boxes (and robots), and the number of doors.
	//	You are going to have to extract these.  For the time being,
	//	I hard code-some values
	numRows = stoi(argv[1]);
	numCols = stoi(argv[2]);
	numDoors = stoi(argv[3]);
	numBoxes = stoi(argv[4]);

	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv, displayGridPane, displayStatePane);
	
	//	Now we can do application-level initialization
	initializeApplication();

	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that 
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();
	
	cleanupGridAndLists();
	
//jyh
// completely unnecessary, because these are all local to initializeApp
//	// Free heap variables
//	free(threadID);
//	free(temp);
//	free(temp2);


	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}

//---------------------------------------------------------------------
//	Free allocated resource before leaving (not absolutely needed, but
//	just nicer.  Also, if you crash there, you know something is wrong
//	in your code.
//---------------------------------------------------------------------
/** Function to clean up memory
 * 
 */
void cleanupGridAndLists(void)
{
	for (int i=0; i< numRows; i++)
		free(grid[i]);
	free(grid);
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		free(message[k]);
	free(message);
}


//==================================================================================
//
//	This is a part that you have to edit and add to.
//
//==================================================================================


/** Function to write the beginning of the file
 * 
 */
void writeBeginning(void){
    ofstream myfile;
      myfile.open ("robotSimulOut.txt", std::ofstream::out | std::ofstream::app);

    myfile << "NumRows = " << numRows << ", " << "NumCols = " << numCols << ", " << "NumBoxes = " << numBoxes << ", " <<  "NumDoors = " << numDoors << std::endl;

    myfile << std::endl;

    // door
    for(unsigned int i = 0; i < (doorList.size()); i++){
        myfile << "DoorPosX = " << doorList[i].x << " " << "DoorPosY = " << doorList[i].y << std::endl;
    }

    myfile << std::endl;

    // box
    for(unsigned int i = 0; i < (boxList.size()); i++){
        myfile << "BoxPosX = " << boxList[i]->x << " " << "BoxPosY = " << boxList[i]->y << std::endl;
    }

	myfile << std::endl;

    // robot
    for(unsigned int i = 0; i < (robotList.size()); i++){
        myfile << "RobotPosX = " << robotList[i]->x << " " << "RobotPosY = " << robotList[i]->y << std::endl;
    }

    myfile << std::endl;


    myfile.close();
}

/** Function to write output to the file
 * @param roboNum is the number of the robot
 * @param flag is the indicator used to determine whether the robot is moving or pushing
 * @param dir is the character passed that indicates direction I.E. N, S, E, W
 */
void writeToFile(int roboNum, int flag, char dir){

    string MovPush;

    ofstream myfile;
      myfile.open ("robotSimulOut.txt", std::ofstream::out | std::ofstream::app);

    // Flag is used to deternine Moving 0 or Pushing 1 or Ending 2
    if(flag == 0)
	{
        MovPush = "move";
    }
    else if(flag == 1){
        MovPush = "push";
    }
    else
    {
        MovPush = "End"; 
    }

      myfile << "robot " << roboNum << " " << MovPush << " " << dir << "\n";

     myfile.close();

}

// Grid Lock is here
// File Lock is here
/** Function to move the robot to the target
 * @param r is the reference to the robots data
 * @param tx is the targets x
 * @param ty is the targets y
 */
void movetotarget(Robot &r, int tx, int ty)
{
	//calc vert
	int vertshift = r.y - ty; 
	int horzshift = tx-r.x; 

	//move vertical
	while (vertshift != 0)
	{
		//move up
		if (vertshift>0)
		{

			r.y--;
//jyh
usleep(100000);


			vertshift--;
			pthread_mutex_lock(&fileLock);
			//lockFile(); 
            writeToFile(r.robotnum,0,'N');
			//unlockFile(); 
			pthread_mutex_unlock(&fileLock);

		}
		//movedown
		if (vertshift<0)
		{

			r.y++;
//jyh
usleep(100000);

			vertshift++; 
			pthread_mutex_lock(&fileLock);
			
            writeToFile(r.robotnum,0,'S');
			
			pthread_mutex_unlock(&fileLock);
		} 
	}
	//move horizontal
	while (horzshift != 0)
	{
		//move up
		if (horzshift>0)
		{
			

			r.x++;
//jyh
usleep(100000);

			

			horzshift--;
			pthread_mutex_lock(&fileLock);
			
            writeToFile(r.robotnum,0,'R');
			
			pthread_mutex_unlock(&fileLock);
		}
		//movedown
		if (horzshift<0)
		{
			

			r.x--;
//jyh
usleep(100000);


			horzshift++; 
			pthread_mutex_lock(&fileLock);
			
            writeToFile(r.robotnum,0,'L');
			 
			pthread_mutex_unlock(&fileLock);
		} 
	}
}

// Grid Lock is here
// File Lock is here
/** Function to push vertically
 * @param r is the refernece to the robots data
 * @param vertshift is the amount to vertically shift
 */
void pushboxvert(Robot &r, int vertshift)
{
	//pushbox

	//move vertical
	while (vertshift != 0)
	{
		//move up
		if (vertshift>0)
		{

			r.y--;
//jyh
usleep(100000);


			boxList[r.robotnum]->y--; 
			vertshift--; 

			pthread_mutex_lock(&fileLock);
			
            writeToFile(r.robotnum,1,'N');
			
			pthread_mutex_unlock(&fileLock);
		}
		//movedown
		if (vertshift<0)
		{
			
			r.y++;

			boxList[r.robotnum]->y++; 
			vertshift++; 

			pthread_mutex_lock(&fileLock);
			
            writeToFile(r.robotnum,1,'S');
			
			pthread_mutex_unlock(&fileLock);
		}
	}
}

// Grid Lock is here
// File Lock is here
/** Function to push horizontally
 * @param r is the reference to the robots data
 * @param horizshift is the amount tp horizontally shift
 */
void pushbothorz(Robot &r, int horizshift)
{
	//pushbox

	//move vertical
	while (horizshift != 0)
	{
		//move right
		if (horizshift<0)
		{

			r.x--;
//jyh
usleep(100000);

			boxList[r.robotnum]->x--; 
			horizshift++; 

			pthread_mutex_lock(&fileLock);
			
            writeToFile(r.robotnum,1,'L');
			
			pthread_mutex_unlock(&fileLock);
		}
		//movedown
		if (horizshift>0)
		{

			r.x++;
//jyh
usleep(100000);

			boxList[r.robotnum]->x++; 
			horizshift--; 

			pthread_mutex_lock(&fileLock);
	
            writeToFile(r.robotnum,1,'R');
			
			pthread_mutex_unlock(&fileLock);
		}
	}
}

// File Lock is here
/** Function to calculate the target for the bot to move to
 * @param d is the door 
 * @param b is a reference to the box data
 * @param r is a reference to the robots data
 */
void movebot(Door d, Box &b, Robot &r)
{

	//std::cout << "Inside movebot" << std::endl;

	//calculate box to door
	int doorvert = b.y-d.y; 

	//determine which angle to approach box from and move to that spot
	//vert
	int targety;
	int targetx; 
	if (doorvert < 0)
	{
		//set target at spot above box to execute push
		targety=b.y-1; 
		targetx=b.x; 
	}
	else if (doorvert >0)
	{
		targety=b.y+1; 
		targetx=b.x; 
	}
	else 
	{
		//if doorvert =0 there will be no vert shift and so robot doesn't move. 
		targetx = r.x; 
		targety = r.y; 
	}

	//move to target vertical and horizontal
	movetotarget(r,targetx,targety);
	//push boxvert


	pushboxvert(r,doorvert); 
    

    int horizshift=d.x-b.x; 

    //left = neg
    if (horizshift < 0)
    {
        //get on right side of current box position 
        targetx=b.x+1; 
        targety=b.y; 
    }
    if (horizshift>0)
    {
        targetx=b.x-1; 
        targety=b.y; 
    }
    else 
    {
        targetx=r.x; 
        targety=r.y; 
    }

    //right = pos


	movetotarget(r,targetx,targety);
	pushbothorz(r,horizshift); 


	  //end in the file
	  pthread_mutex_lock(&fileLock);
	  
        ofstream myfile;
      	myfile.open ("robotSimulOut.txt", std::ofstream::out | std::ofstream::app);

      myfile << "robot " << r.robotnum << " End" <<std::endl; 

     myfile.close();
	 
	 pthread_mutex_unlock(&fileLock);
		

}


/** This is the thread function to give the threads all the work
 * @param argument is the robots number
 */
void* threadFunc(void* argument)
{

	Robot *info = (Robot*) argument;

	int i = info->robotnum; 

	//determine door to call
		int d = robotList[i]->doornum; 

	
	// Detatch itself
	int detatchErr = pthread_detach(pthread_self());
	if(detatchErr != 0){
		std::cout << "Could not detatch thread" << std::endl;
	}

	
		movebot(doorList[d],*boxList[i],*robotList[i]);

	// Kill drawing of the Robots and Boxes
	info->rAlive = false;

	pthread_exit(0);
}


// WORK IN HERE
/** Function to initialize the application
 * 
 */
void initializeApplication(void)
{
	//	Allocate the grid
	grid = (int**) malloc(numRows * sizeof(int*));
	for (int i=0; i<numRows; i++)
		grid[i] = (int*) malloc(numCols * sizeof(int));
	
	message = (char**) malloc(MAX_NUM_MESSAGES*sizeof(char*));
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = (char*) malloc((MAX_LENGTH_MESSAGE+1)*sizeof(char));



	//	seed the pseudo-random generator
	startTime = time(NULL);
	srand((unsigned int) startTime);

	//create the robots and boxes and put them in global vectors
	for (int i = 0; i<numBoxes; ++i)
	{
		//jyh
		//	Obviously, temp and temp2, storing pointers, can (and should) be local variables
		// Made Temp on the Heap 
		Robot* temp = new Robot;
		temp->x= rand() % numCols; 
		temp->y = rand() % numRows; 
		temp->robotnum = i; 
		temp->doornum = rand() %numDoors; 

		robotList.push_back(temp); 

		// Made Temp2 on the Heap
		//Box temp2; 
		Box* temp2 = new Box;
		temp2->x = rand() %numCols; 
		temp2->y = rand() %numRows;

		//validate
		if (temp2->x==0) 
		{
			temp2->x=temp2->x+1;
		}
		if (temp2->x ==numCols-1)
		{	
			temp2->x=temp2->x-1; 
		}
		if (temp2->y==0)
		{
			temp2->y=temp2->y+1; 
		}
		if (temp2->y==numRows-1)
		{
			temp2->y= temp2->y -1; 
		}
		temp2->boxnum = i; 
		boxList.push_back(temp2); 
	}

	//create doors

	for (int i =0; i<numDoors; ++i)
	{
		Door temp; 
		temp.x =rand()%numCols; 
		temp.y = rand()%numRows; 
		temp.doornum = i; 
		doorList.push_back(temp); 
	}


	pthread_mutex_init(&fileLock,NULL); 
	pthread_mutex_init(&gridLock,NULL);

	writeBeginning(); 
	//move/push. 

	// create the threads
	for (int i = 0; i < numBoxes; i++) {
		//printf("About to create Thread %d\n", i);

	//								pointer to				pointer to
	//								a pthread_t	 parameters  thread func  pointer to data
//																				robotList[i] is a Robot*
		int errCode = pthread_create (&(robotList[i]->threadID), NULL, threadFunc, robotList[i]);

		if (errCode != 0)
		{
			printf ("could not pthread_create thread %d. %d/%s\n",
					 i, errCode, strerror(errCode));
			exit (EXIT_FAILURE);
		}
	}
	
}


