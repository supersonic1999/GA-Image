#pragma once
#include "stdafx.h"
#include <time.h>
#include <Windows.h>
#include <math.h>
#include <sstream>
#include "setup_cl.h"
#include <vector>
#include "FreeImage\FreeImagePlus.h"

class GeneticAlgorithm 
{
private:
#define 	genesPerShape  7 // X, Y, Z, R, G, B, A

	//////Settings/////
	float mutationRate;
	float selectionCutoff;
	int popSize;
	int shapePerMember;
	int imageSize;
	///////////////////
	
	struct popStruct;

	cl_command_queue commandQueue;
	cl_context context;
	cl_kernel drawKernel;
	cl_kernel fitnessKernel;
	cl_event resetEvents[3];
	
	unsigned int *srcImage;
	unsigned int *fitnessArray;
	unsigned int *resetImg;
	unsigned int *dnaArray;

	// Object Buffers
	cl_mem resultBuffer;
	cl_mem valsBuffer;
	cl_mem dnaBuffer;
	cl_mem srcImgBuffer;
	cl_mem fitnessBuffer;

	int selectionType;
	int crossoverType;
	int mutationType;

	popStruct *population;
	popStruct *selectedPopulation;
	HANDLE GAThread;

	void drawImage(unsigned int *result, const char* fileName);
	void Evaulate();
	void Selection(int type);
	void Crossover(int type);
	void Mutation(int type);

	// Main Genetic Algorithm thread
	static DWORD run(LPVOID thread_obj);
public:
	int generation;
	unsigned int *topImage;
	double topFitness;
	bool bPaused;

	enum sType { ROULETTE_WHEEL, TOURNAMENT, RANK };
	enum cType{ ONE_POINT, UNIFORM };
	enum mType { M_UNIFORM, NON_UNIFORM };

	~GeneticAlgorithm();

	GeneticAlgorithm(int pSize, int sCount, float mRate, sType selectionType, cType crossoverType, mType mutType, char* imagePath);
};