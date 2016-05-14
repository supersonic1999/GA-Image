#pragma once
#include "stdafx.h"
#include "GeneticAlgorithm.h"

using namespace std;

struct GeneticAlgorithm::popStruct
{
	double fitness;
	unsigned int *DNA;
}; 

void GeneticAlgorithm::drawImage(unsigned int *result, const char* fileName) {
	FreeImage_Initialise();
	FIBITMAP *bitmap = FreeImage_Allocate(250, 250, 24);
	for (int y = 0; y < 250; y++) {
		for (int x = 0; x < 250; x++) {
			RGBQUAD color;
			color.rgbRed = result[((x * 3) + ((y * 3) * 250)) + 0];
			color.rgbGreen = result[((x * 3) + ((y * 3) * 250)) + 1];
			color.rgbBlue = result[((x * 3) + ((y * 3) * 250)) + 2];
			FreeImage_SetPixelColor(bitmap, x, 250-y, &color);
		}
	}
	FreeImage_Save(FIF_PNG, bitmap, fileName, 0);
	FreeImage_DeInitialise();
}

void GeneticAlgorithm::Evaulate()
{
	cl_event fitnessEvent[1];
	cl_event finalEvent;
	size_t globalWorkSize[1];
	size_t localWorkSize[1];

	////////////////////////////////////////////////////
	// drawRect Kernel
	////////////////////////////////////////////////////
	globalWorkSize[0] = popSize * shapePerMember;
	localWorkSize[0] = shapePerMember;

	// Copy DNA into one contiguous array block
	for (int i = 0; i < popSize; i++)
		memcpy(dnaArray + (i * genesPerShape * shapePerMember), population[i].DNA, genesPerShape * shapePerMember * sizeof(int));

	// Write DNA into buffer
	cl_int err = clEnqueueWriteBuffer(commandQueue, dnaBuffer, CL_FALSE, 0, genesPerShape * shapePerMember * popSize * sizeof(int), dnaArray, 0, 0, &resetEvents[0]);
	if (err != CL_SUCCESS) {
		cout << "Error writing data\n";
		return;
	}

	// Enqueue drawing kernel
	if (generation == 0) // Only 1 event is needed on 1st run
		err = clEnqueueNDRangeKernel(commandQueue, drawKernel, 1, 0, globalWorkSize, localWorkSize, 1, resetEvents, fitnessEvent);
	else 
		err = clEnqueueNDRangeKernel(commandQueue, drawKernel, 1, 0, globalWorkSize, localWorkSize, 3, resetEvents, fitnessEvent);
	if (err != CL_SUCCESS) {
		cout << "Error enqueing data\n";
		return;
	}
	
	////////////////////////////////////////////////////
	// calcFitness Kernel
	////////////////////////////////////////////////////
	globalWorkSize[0] = popSize * imageSize * imageSize;
	localWorkSize[0] = 50;

	// Enqueue fitness kernel
	err = clEnqueueNDRangeKernel(commandQueue, fitnessKernel, 1, 0, globalWorkSize, localWorkSize, 1, fitnessEvent, &finalEvent);
	if (err != CL_SUCCESS) {
		cout << "Error enqueing data\n";
		return;
	}

	// Read calculated fitnesses from buffer
	err = clEnqueueReadBuffer(commandQueue, fitnessBuffer, CL_TRUE, 0, popSize * sizeof(int), fitnessArray, 1, &finalEvent, 0);
	if (err != CL_SUCCESS) {
		cout << "Error reading data\n";
		return;
	}
	////////////////////////////////////////////////////

	double bestFitness = -1;
	int bestIndex = -1;
	for (int i = 0; i < popSize; i++) {
		population[i].fitness = 1.0f - double(fitnessArray[i]) / (imageSize * imageSize * 3 * 256.0f); // Sum Diff
		if (population[i].fitness > bestFitness) {
			bestFitness = population[i].fitness;
			bestIndex = i;
		}
		fitnessArray[i] = 0; // Reset fitness values
	}

	// Display new image
	if (bestFitness > topFitness) {
		topFitness = bestFitness;

		// Get best result from device
		err = clEnqueueReadBuffer(commandQueue, resultBuffer, CL_TRUE, bestIndex * 3 * imageSize * imageSize * sizeof(int), 3 * imageSize * imageSize * sizeof(int), topImage, 0, 0, 0);
		if (err != CL_SUCCESS) {
			cout << "Error reading data\n";
			return;
		}
	}

	////////////////////////////////////////////////////

	// Reset result buffer
	err = clEnqueueWriteBuffer(commandQueue, resultBuffer, CL_FALSE, 0, popSize * 3 * imageSize * imageSize * sizeof(int), resetImg, 0, 0, &resetEvents[1]);
	if (err != CL_SUCCESS) {
		cout << "Error writing data\n";
		return;
	}

	// Reset fitness buffer
	err = clEnqueueWriteBuffer(commandQueue, fitnessBuffer, CL_FALSE, 0, popSize * sizeof(int), fitnessArray, 0, 0, &resetEvents[2]);
	if (err != CL_SUCCESS) {
		cout << "Error writing data\n";
		return;
	}
}

void GeneticAlgorithm::Selection(int type)
{
	// Bubble Sort
	unsigned int* tempDNA = new unsigned int[genesPerShape * shapePerMember];
	double tempFitness;
	for (int i = 0; i < popSize; i++) {
		bool sorted = false;
		for (int j = 0; j < popSize - 1 - i; j++) {

			if (population[j].fitness < population[j + 1].fitness) {
				// Make temp copy
				memcpy(tempDNA, population[j].DNA, (genesPerShape * shapePerMember * sizeof(int)));
				tempFitness = population[j].fitness;

				// Copy into slot 0
				memcpy(population[j].DNA, population[j + 1].DNA, (genesPerShape * shapePerMember * sizeof(int)));
				population[j].fitness = population[j + 1].fitness;

				// Copy temp into slot 1
				memcpy(population[j + 1].DNA, tempDNA, (genesPerShape * shapePerMember * sizeof(int)));
				population[j + 1].fitness = tempFitness;
				sorted = true;
			}
		}
		if (!sorted)
			break; // Break if no sorts were executed
	}
	delete[] tempDNA; // Cleanup

	switch (type) {	
	case RANK: {
		double maxRank = 0;
		for (int i = 1; i <= popSize; i++)
			maxRank += i;

		for (int i = 0; i < popSize*selectionCutoff; i++) {
			double num = (rand() % RAND_MAX) / double(RAND_MAX); // Float rand
			double curRank = 0;
			for (int j = 0; j < popSize; j++) {
				curRank += (popSize-j) / maxRank;

				if (curRank > num || j == popSize - 1) {
					bool bValid = true;
					for (int k = 0; k < i; k++)
					if (selectedPopulation[k].DNA == population[j].DNA)
						bValid = false;

					if (bValid) {
						memcpy(selectedPopulation[i].DNA, population[j].DNA, (genesPerShape * shapePerMember * sizeof(int)));
						selectedPopulation[i].fitness = population[j].fitness;
						break;
					}
				}
			}
		}
		break;
	}
	case ROULETTE_WHEEL: {
		// Count total fitness
		double totalFitness = 0;
		for (int i = 0; i < popSize; i++)
			totalFitness += population[i].fitness;

		for (int i = 0; i < popSize*selectionCutoff; i++) {
			double num = (rand() % RAND_MAX) / double(RAND_MAX); // Float rand
			double curFitness = 0;
			for (int j = 0; j < popSize; j++) {
				curFitness += population[j].fitness / totalFitness;

				if (curFitness > num || j == popSize - 1) {
					bool bValid = true;
					for (int k = 0; k < i; k++)
					if (selectedPopulation[k].DNA == population[j].DNA)
						bValid = false;

					if (bValid) {
						selectedPopulation[i] = population[j];
						break;
					}
				}
			}
		}
		break;
	}
	case TOURNAMENT:
		int tournamentSize = 15;

		for (int i = 0; i < popSize*selectionCutoff; i++) {
			std::vector<int> selectedPopIndex;

			// Select tournament members
			while (int(selectedPopIndex.size()) < tournamentSize) {
				int num = rand() % popSize;

				if (i > 0 && selectedPopulation[0].DNA == population[num].DNA)
					continue;

				// Validate if num has been previously picked
				bool bValid = true;
				for (std::vector<int>::iterator it = selectedPopIndex.begin(); it != selectedPopIndex.end(); ++it) {
					if (*it == num) {
						bValid = false;
						break;
					}
				}

				if (bValid)
					selectedPopIndex.push_back(num);
			}

			double bestFitness = 0;
			int bestIndex = 0;
			for (int j = 0; j < tournamentSize; j++) {
				if (population[selectedPopIndex[j]].fitness > bestFitness) {
					bestFitness = population[selectedPopIndex[j]].fitness;
					bestIndex = selectedPopIndex[j];
				}
			}

			// Select best in tournament
			selectedPopulation[i] = population[bestIndex];
		}
		break;
	}
}

void GeneticAlgorithm::Crossover(int type)
{
	// Keep fittest (Elitism)
	double fitnessValue = 0.0f;
	int fitnessIndex = 0;
	for(int i = 0; i < popSize; i++) {
		if (population[i].fitness > fitnessValue) {
			fitnessValue = population[i].fitness;
			fitnessIndex = i;
		}
	}
	memcpy(population[0].DNA, population[fitnessIndex].DNA, (genesPerShape * shapePerMember * sizeof(int)));

	int count = 1;
	switch (type) {
	case ONE_POINT: // Single Point Crossover
		for (int i = 0; i < popSize*selectionCutoff; i++) {
			for (int j = 0; j < 1 / selectionCutoff; j++) {
				if (count >= popSize )
					break;

				int num = rand() % (genesPerShape * shapePerMember);
				int randInd = i;
				while (randInd == i)
					randInd = rand() % int(popSize*selectionCutoff);

				memcpy(population[count].DNA, selectedPopulation[i].DNA, (genesPerShape * shapePerMember * sizeof(int)));
				memcpy(population[count].DNA + num, population[randInd].DNA + num,
					((genesPerShape * shapePerMember) - num)* sizeof(int)); // Copy second half of second member
				count++;
			}
		}
		break;
	case UNIFORM: // Uniform Crossover
		for (int i = 1; i < popSize; i++) {
			for (int j = 0; j < shapePerMember; j++) {
				for (int k = 0; k < 7; k++) {
					int num = rand() % 2;
					int randInd = i;
					while (randInd == i)
						randInd = rand() % int(popSize*selectionCutoff);

					if (num)
						memcpy(population[i].DNA + ((genesPerShape * j) + k), selectedPopulation[int(count*selectionCutoff)].DNA + ((genesPerShape * j) + k), genesPerShape);
					else 
						memcpy(population[i].DNA + ((genesPerShape * j) + k), selectedPopulation[randInd].DNA + ((genesPerShape * j) + k), genesPerShape);
				}
			}
			count++;
		}
		break;
	}
}

void GeneticAlgorithm::Mutation(int type)
{
	switch (type) {
	case M_UNIFORM: // Uniform
		for (int i = 1; i < popSize; i++) {
			for (int j = 0; j < shapePerMember; j++) {
				for (int k = 0; k < genesPerShape; k++) {
					if (((rand() % RAND_MAX) / double(RAND_MAX) * 100.0f) <= mutationRate) {
						population[i].DNA[(genesPerShape * j) + k] += int((rand() % 255) * 0.3 * 2 - (255 * 0.3));//rand() % 255;
						if (population[i].DNA[(genesPerShape * j) + k] > 255)
							population[i].DNA[(genesPerShape * j) + k] = 255;
						else if (population[i].DNA[(genesPerShape * j) + k] < 0)
							population[i].DNA[(genesPerShape * j) + k] = 0;
					}
				}
			}
		}
		break;
	case NON_UNIFORM: // Non Uniform
		for (int i = 1; i < popSize; i++) {
			for (int j = 0; j < shapePerMember; j++) {
				for (int k = 0; k < genesPerShape; k++) {
					if (((rand() % RAND_MAX) / double(RAND_MAX) * 100.0f) <= mutationRate*(1.0f - topFitness)) {
						population[i].DNA[(genesPerShape * j) + k] += int((rand() % 255) * 0.3 * 2 - (255 * 0.3));//rand() % 255;
						if (population[i].DNA[(genesPerShape * j) + k] > 255)
							population[i].DNA[(genesPerShape * j) + k] = 255;
						else if (population[i].DNA[(genesPerShape * j) + k] < 0)
							population[i].DNA[(genesPerShape * j) + k] = 0;
					}
				}
			}
		}
		break;
	}
}

DWORD GeneticAlgorithm::run(LPVOID thread_obj)
{
	GeneticAlgorithm* thread = (GeneticAlgorithm*)thread_obj; // Ref

	// Init population
	srand(int(time(NULL)));
	for (int i = 0; i < thread->popSize; i++) {
		thread->population[i].DNA = new unsigned int[genesPerShape * thread->shapePerMember];
		for (int j = 0; j < thread->shapePerMember; j++) {
			thread->population[i].DNA[(j * genesPerShape) + 0] = rand() % 255;
			thread->population[i].DNA[(j * genesPerShape) + 1] = rand() % 255;
			thread->population[i].DNA[(j * genesPerShape) + 2] = 59;
			thread->population[i].DNA[(j * genesPerShape) + 3] = rand() % 255;
			thread->population[i].DNA[(j * genesPerShape) + 4] = rand() % 255;
			thread->population[i].DNA[(j * genesPerShape) + 5] = rand() % 255;
			thread->population[i].DNA[(j * genesPerShape) + 6] = rand() % 40;
		}
		thread->population[i].fitness = 0;
	}

	//Init selected DNA
	for (int i = 0; i < thread->popSize*thread->selectionCutoff; i++)
		thread->selectedPopulation[i].DNA = new unsigned int[genesPerShape * thread->shapePerMember];
	
	while (TRUE) // GA Main Loop
	{
		if (thread->bPaused)
			continue;
		thread->Evaulate();
		thread->Selection(thread->selectionType);
		thread->Crossover(thread->crossoverType);
		thread->Mutation(thread->mutationType);

		// Output image milestones
		if (thread->generation == 200)
			thread->drawImage(thread->topImage, "gen-200.png");
		else if (thread->generation == 1000)
			thread->drawImage(thread->topImage, "gen-1000.png");
		else if (thread->generation == 10000)
			thread->drawImage(thread->topImage, "gen-10000.png");
		else if (thread->generation == 50000)
			thread->drawImage(thread->topImage, "gen-50000.png");
		else if (thread->generation == 250000)
			thread->drawImage(thread->topImage, "gen-250000.png");
		else if (thread->generation == 1000000)
			thread->drawImage(thread->topImage, "gen-1000000.png");

		thread->generation++;
	}
	return 0;
}

GeneticAlgorithm::~GeneticAlgorithm()
{
	if (GAThread)
		CloseHandle(GAThread);

	delete[] population;
	delete[] selectedPopulation;
	delete[] srcImage;
	delete[] fitnessArray;
	delete[] resetImg;
	delete[] dnaArray;
}

GeneticAlgorithm::GeneticAlgorithm(int pSize, int sCount, float mRate, sType selectionType, cType crossoverType, mType mutType, char* imagePath)
{
	this->imageSize		 = 250;
	this->selectionCutoff = 0.2f;
	this->generation = 0;
	this->bPaused = false;
	this->shapePerMember = sCount;
	this->popSize = pSize;
	this->mutationRate = mRate;	
	this->selectionType = selectionType;
	this->crossoverType = crossoverType;
	this->mutationType = mutType;
	this->population = new popStruct[this->popSize]();
	this->selectedPopulation = new popStruct[int(this->popSize * this->selectionCutoff)]();
	this->resetImg = new unsigned int[popSize * 3 * imageSize * imageSize]();
	this->dnaArray = new unsigned int[genesPerShape * shapePerMember * popSize]();
	this->fitnessArray = new unsigned int[popSize]();
	this->topImage = new unsigned int[popSize * 3 * imageSize * imageSize]();

	// Load Image
	fipImage input;
	input.load(imagePath);
	RGBQUAD pixel;
	srcImage = new unsigned int[3 * imageSize * imageSize];
	for (int i = 0; i < imageSize; i++) {
		for (int j = 0; j < imageSize; j++) {
			input.getPixelColor(i, (imageSize-1)-j, &pixel);
			srcImage[3 * (i + (j * imageSize)) + 0] = pixel.rgbRed;
			srcImage[3 * (i + (j * imageSize)) + 1] = pixel.rgbGreen;
			srcImage[3 * (i + (j * imageSize)) + 2] = pixel.rgbBlue;
		}
	}

	// Setup OpenCL
	context = createContext(false);
	if (!context) {
		cout << "OpenCL context not created\n";
		return;
	}

	cl_device_id		device = nullptr;
	commandQueue = createCommandQueue(context, &device, false);
	if (!commandQueue) {
		cout << "Command queue not created\n";
		return;
	}

	cl_program program = createProgram(context, device, "Resources\\Kernels\\drawKernel.cl");
	drawKernel = clCreateKernel(program, "drawRect", nullptr);
	if (!drawKernel) {
		cout << "Could not create kernel\n";
		return;
	}

	fitnessKernel = clCreateKernel(program, "calcFitness", nullptr);
	if (!fitnessKernel) {
		cout << "Could not create kernel\n";
		return;
	}

	// draw Kernel
	dnaBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, genesPerShape * popSize * shapePerMember * sizeof(int), dnaArray, 0);
	resultBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, popSize * 3 * imageSize * imageSize * sizeof(int), 0, 0);
	clSetKernelArg(drawKernel, 0, sizeof(cl_mem), &dnaBuffer);
	clSetKernelArg(drawKernel, 1, sizeof(cl_mem), &resultBuffer);

	// fitness Kernel
	srcImgBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 3 * imageSize * imageSize * sizeof(int), srcImage, 0);
	fitnessBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, popSize * imageSize * imageSize * sizeof(int), 0, 0);
	clSetKernelArg(fitnessKernel, 0, sizeof(cl_mem), &srcImgBuffer);
	clSetKernelArg(fitnessKernel, 1, sizeof(cl_mem), &resultBuffer);
	clSetKernelArg(fitnessKernel, 2, sizeof(cl_mem), &fitnessBuffer);

	// Start separate thread
	GAThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GeneticAlgorithm::run, this, 0, 0);
}