
#pragma once

#include <CL\opencl.h>

cl_context createContext(bool bCPU = 0);
cl_command_queue createCommandQueue(cl_context context, cl_device_id *device, bool bProfiling);
cl_program createProgram(cl_context context, cl_device_id device, const char* fileName);
