
//
// Helper functions to setup OpenCL and load kernels
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <locale>
#include <exception>
#include "setup_cl.h"


using namespace std;


// Helper function to report available platforms and devices and create and return an OpenCL context
cl_context createContext(bool bCPU) {

	cl_int				clerr;
	cl_uint				numPlatforms;
	cl_platform_id		*platformArray;
	locale				loc;
	cl_context			context = nullptr;
	const				string platformToFind = string("NVIDIA");
	int					platformIndex = bCPU;

	try {

		// Query the number of platforms
		clerr = clGetPlatformIDs(0, nullptr, &numPlatforms);

		if (clerr != CL_SUCCESS || numPlatforms == 0)
			throw exception("No OpenCL platforms found");

		// Create the array of available platforms
		platformArray = (cl_platform_id*)malloc(numPlatforms * sizeof(cl_platform_id));

		if (!platformArray)
			throw exception("Unable to create platform array");

		// Get platform list - for this application we'll use only the first platform found
		clerr = clGetPlatformIDs(numPlatforms, platformArray, nullptr);

		// Validate returned platform
		if (clerr != CL_SUCCESS || numPlatforms == 0)
			throw exception("Unable to obtain platform information");

		// Cycle through each platform and display information
		char* value;
		size_t valueSize;
		cl_device_id* devices;
		cl_uint deviceCount;
		for (cl_uint i = 0; i<numPlatforms; ++i) {

			size_t resultSize; // Used to store size of buffer for clGetPlatformInfo to put results of queries

			// Example - query and report the PROFILE property for each platform...

			clerr = clGetPlatformInfo(platformArray[i], CL_PLATFORM_PROFILE, 0, nullptr, &resultSize);
			char *platformProfile = (char*)malloc(resultSize);
			clerr = clGetPlatformInfo(platformArray[i], CL_PLATFORM_PROFILE, resultSize, platformProfile, nullptr);

			// get all devices
			clGetDeviceIDs(platformArray[i], CL_DEVICE_TYPE_ALL, 0, NULL, &deviceCount);
			devices = (cl_device_id*)malloc(sizeof(cl_device_id) * deviceCount);
			clGetDeviceIDs(platformArray[i], CL_DEVICE_TYPE_ALL, deviceCount, devices, NULL);

			// for each device print critical attributes
			for (int j = 0; j < deviceCount; j++) {

				// print device name
				clGetDeviceInfo(devices[j], CL_DEVICE_NAME, 0, NULL, &valueSize);
				value = (char*)malloc(valueSize);
				clGetDeviceInfo(devices[j], CL_DEVICE_NAME, valueSize, value, NULL);
				printf("Platform %i. Device: %s \n", i, value);
				free(value);		
			}
		}

		// --------------------------

		// Create OpenCL context based on the first available nVidia platform
		cl_context_properties contextProperties[] = {

			CL_CONTEXT_PLATFORM,
			(cl_context_properties)platformArray[platformIndex],
			0
		};

		context = clCreateContextFromType(contextProperties, 
			bCPU == 1 ? CL_DEVICE_TYPE_CPU:CL_DEVICE_TYPE_GPU,
			nullptr, nullptr, nullptr);

		if (clerr != CL_SUCCESS || !context)
			throw exception("Unable to create a valid GPU context");

		return context;
	}
	catch (exception& err)
	{
		cout << err.what() << endl;
		return nullptr;
	}
}


// Helper function to create and return an OpenCL command queue for the first available device.  The queue that is created passes commands to be processed 'in-order' - that is they are processed in the same order they are placed in the queue.  The device used is returned in *device.
cl_command_queue createCommandQueue(cl_context context, cl_device_id *device, bool bProfiling) {

	size_t deviceBufferSize;

	cl_int errNum = clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, nullptr, &deviceBufferSize);

	// Check clGetContextInfo returned okay
	if (errNum != CL_SUCCESS) {

		cout << "clGetContextInfo failed to get device info\n";
		return nullptr;
	}

	// If no devices returned then simply return null
	if (deviceBufferSize == 0) {

		cout << "No devices available\n";
		return nullptr;
	}

	cl_device_id *devices = new cl_device_id[deviceBufferSize / sizeof(cl_device_id)];

	errNum = clGetContextInfo(context, CL_CONTEXT_DEVICES, deviceBufferSize, devices, nullptr);

	// Check clGetContextInfo returned okay
	if (errNum != CL_SUCCESS) {

		cout << "clGetContextInfo failed to get device info\n";
		return nullptr;
	}

	// Create command queue for first device
	cl_command_queue commandQueue = clCreateCommandQueue(context, devices[0], 
		bProfiling == 1 ? CL_QUEUE_PROFILING_ENABLE : 0
		, nullptr);

	if (commandQueue == nullptr) {

		cout << "Could not create the command queue\n";
		return nullptr;
	}


	// Return first device
	*device = devices[0];

	// Clean-up local resources
	delete[] devices;

	return commandQueue;
}



// Helper function to load the kernel source code from a suitable (text) file and setup an OpenCL program object
cl_program createProgram(cl_context context, cl_device_id device, const char* fileName) {

	// Open the kernel file - the extension can be anything, including .txt since it's just a text file
	ifstream kernelFile(fileName, ios::in);

	if (!kernelFile.is_open()) {

		cout << "Cannot open source file " << fileName << endl;
		return nullptr;
	}

	// Store the file contents in a stringstream object
	ostringstream oss;
	oss << kernelFile.rdbuf();

	// Extract a std::string from the stringstream object
	string srcString = oss.str();

	// Obtain the pointer to the contained C string
	const char* src = srcString.c_str();

	cl_int err;

	cl_program program = clCreateProgramWithSource(context, 1, (const char**)&src, 0, &err);

	if (!program) {

		cout << "Cannot create program from source file " << fileName << endl;
		return nullptr;
	}

	// Attempt to build program object
	err = clBuildProgram(program, 0, 0, 0, 0, 0);

	if (err != CL_SUCCESS) {

		size_t logSize;
		clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);

		char *buildLog = (char *)calloc(logSize + 1, 1);

		clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, buildLog, nullptr);

		cout << "Error in kernel:\n\n";
		cout << buildLog;

		// Clean-up
		clReleaseProgram(program);

		return nullptr;
	}

	return program;
}
