#include <stdio.h>
#include <windows.h>
#include <math.h>

#include "lodepng.h"
#include "CL\cl.h"

#define L_INPUT_IMAGE_NAME "im0.png"
#define R_INPUT_IMAGE_NAME "im1.png"

#define MAX_DISP 65
#define MIN_DISP 0
#define WIN_SIZE_DISP 20
#define THRESHOLD 8

#define WIN_WIDTH_OF 32
#define WIN_HEIGHT_OF 32

cl_kernel clOneKernelPlease(cl_context context, cl_device_id device_id, const char* file_name, const char* kernel_name);
void clPrintInfo(cl_platform_id platform_id, cl_device_id device_id);
int clCheckStatus(cl_int status_code);

int main()
{
	// Tunable parameters
	cl_int max_disp = MAX_DISP;
	cl_int max_disp_neg = -MAX_DISP;
	cl_int min_disp = MIN_DISP;
	cl_int win_size_disp = WIN_SIZE_DISP; // Disparity window
	cl_int threshold = THRESHOLD; // Cross-check threshold
	cl_int win_width_of = WIN_WIDTH_OF; // Occlusion fill window
	cl_int win_height_of = WIN_HEIGHT_OF; // Occlusion fill window

										  // OpenCL errors go to this
	cl_int status;

	// Timer
	LARGE_INTEGER frequency;
	LARGE_INTEGER t1;
	LARGE_INTEGER t2;
	double elapsed_time;


	// Gets platform and device
	cl_platform_id platform_id;
	cl_device_id device_id;
	cl_uint platform_count;
	cl_uint device_count;

	clGetPlatformIDs(1, &platform_id, &platform_count);
	clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &device_count);

	clPrintInfo(platform_id, device_id);


	// Creates context
	printf("\nCreating context...\n");
	cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &status);
	clCheckStatus(status);


	// Decodes images
	printf("Decoding images...\n");
	unsigned char* rgba_L;
	unsigned char* rgba_R;

	int w_in;
	int h_in;

	lodepng_decode32_file(&rgba_L, &w_in, &h_in, L_INPUT_IMAGE_NAME);
	lodepng_decode32_file(&rgba_R, &w_in, &h_in, R_INPUT_IMAGE_NAME);

	int w_out = w_in / 4;
	int h_out = h_in / 4;


	// Creates OpenCL image objects
	printf("Creating image objects...\n");
	cl_image_format image_format;
	image_format.image_channel_order = CL_RGBA;
	image_format.image_channel_data_type = CL_UNSIGNED_INT8;

	cl_image_desc image_desc;
	image_desc.image_type = CL_MEM_OBJECT_IMAGE2D;
	image_desc.image_width = w_in;
	image_desc.image_height = h_in;
	image_desc.image_depth = 8;
	image_desc.image_row_pitch = w_in * 4;
	image_desc.image_slice_pitch = 0;
	image_desc.num_mip_levels = 0;
	image_desc.num_samples = 0;
	image_desc.buffer = NULL;

	cl_mem img_L = clCreateImage(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, &image_format, &image_desc, rgba_L, &status);
	clCheckStatus(status);
	cl_mem img_R = clCreateImage(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, &image_format, &image_desc, rgba_R, &status);
	clCheckStatus(status);


	// Buffers for images
	printf("Creating image buffers...\n");
	cl_mem buff_L = clCreateBuffer(context, CL_MEM_READ_WRITE, w_out*h_out, 0, &status);
	clCheckStatus(status);
	cl_mem buff_R = clCreateBuffer(context, CL_MEM_READ_WRITE, w_out*h_out, 0, &status);
	clCheckStatus(status);
	cl_mem buff_disp_LR = clCreateBuffer(context, CL_MEM_READ_WRITE, w_out*h_out, 0, &status);
	clCheckStatus(status);
	cl_mem buff_disp_RL = clCreateBuffer(context, CL_MEM_READ_WRITE, w_out*h_out, 0, &status);
	clCheckStatus(status);
	cl_mem buff_disp_CC = clCreateBuffer(context, CL_MEM_READ_WRITE, w_out*h_out, 0, &status);
	clCheckStatus(status);
	cl_mem buff_disp_CC_OF = clCreateBuffer(context, CL_MEM_READ_WRITE, w_out*h_out, 0, &status);
	clCheckStatus(status);

	// Creates sampler
	printf("Creating sampler...\n");
	cl_sampler sampler = clCreateSampler(context, CL_FALSE, CL_ADDRESS_CLAMP_TO_EDGE, CL_FILTER_NEAREST, &status);
	clCheckStatus(status);


	// Create kernels
	cl_kernel resize_greyscale_kernel = clOneKernelPlease(context, device_id, "resize_greyscale.cl", "resize_greyscale");
	cl_kernel zncc_kernel = clOneKernelPlease(context, device_id, "zncc.cl", "zncc");
	cl_kernel crosscheck_kernel = clOneKernelPlease(context, device_id, "crosscheck.cl", "crosscheck");
	cl_kernel occlusionfill_kernel = clOneKernelPlease(context, device_id, "occlusionfill.cl", "occlusionfill");
	cl_kernel normalization_kernel = clOneKernelPlease(context, device_id, "normalization.cl", "normalization");


	// OpenCL Events for benchmarking kernels
	cl_event event_0; // Resize and greyscale
	cl_event event_1; // ZNCC left-right
	cl_event event_2; // ZNCC right-left
	cl_event event_3; // Cross-checking
	cl_event event_4; // Occlusion filling
	cl_event event_5; // Normalization

	cl_ulong t1_oc;
	cl_ulong t2_oc;
	double elapsed_time_oc;
	cl_double total_time_oc = 0;

	// Creates command queue
	printf("Creating command queue...\n");
	cl_command_queue command_queue = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &status);
	clCheckStatus(status);

	clFinish(command_queue);

	// Starting the timer

	QueryPerformanceFrequency(&frequency); // Get ticks per second
	QueryPerformanceCounter(&t1);

	// EXECUTE!
	size_t localWorkSize[2] = { 3, 21 };
	size_t globalWorkSize[2] = { w_out, h_out };
	size_t localWorkSize1D[1] = { localWorkSize[0] * localWorkSize[1] };
	size_t globalWorkSize1D[1] = { globalWorkSize[0] * globalWorkSize[1] };

	// Resize and greyscale
	printf("\nPerforming resize and greyscale...\n");
	clSetKernelArg(resize_greyscale_kernel, 0, sizeof(cl_mem), &img_L);
	clSetKernelArg(resize_greyscale_kernel, 1, sizeof(cl_mem), &img_R);
	clSetKernelArg(resize_greyscale_kernel, 2, sizeof(cl_mem), &buff_L);
	clSetKernelArg(resize_greyscale_kernel, 3, sizeof(cl_mem), &buff_R);
	clSetKernelArg(resize_greyscale_kernel, 4, sizeof(cl_sampler), &sampler);
	clSetKernelArg(resize_greyscale_kernel, 5, sizeof(cl_int), &w_out);
	clSetKernelArg(resize_greyscale_kernel, 6, sizeof(cl_int), &h_out);
	status = clEnqueueNDRangeKernel(command_queue, resize_greyscale_kernel, 2, NULL, globalWorkSize, NULL, 0, NULL, &event_0);
	clCheckStatus(status);

	// ZNCC
	// Left-right
	printf("Performing ZNCC for left-right...\n");
	clSetKernelArg(zncc_kernel, 0, sizeof(cl_mem), &buff_L);
	clSetKernelArg(zncc_kernel, 1, sizeof(cl_mem), &buff_R);
	clSetKernelArg(zncc_kernel, 2, sizeof(cl_mem), &buff_disp_LR);
	clSetKernelArg(zncc_kernel, 3, sizeof(cl_int), &w_out);
	clSetKernelArg(zncc_kernel, 4, sizeof(cl_int), &h_out);
	clSetKernelArg(zncc_kernel, 5, sizeof(cl_int), &win_size_disp);
	clSetKernelArg(zncc_kernel, 6, sizeof(cl_int), &min_disp);
	clSetKernelArg(zncc_kernel, 7, sizeof(cl_int), &max_disp);
	status = clEnqueueNDRangeKernel(command_queue, zncc_kernel, 2, NULL, globalWorkSize, NULL, 0, NULL, &event_1);
	clCheckStatus(status);
	// Right-left - some variables stay the same
	printf("Performing ZNCC for right-left...\n");
	clSetKernelArg(zncc_kernel, 0, sizeof(cl_mem), &buff_R);
	clSetKernelArg(zncc_kernel, 1, sizeof(cl_mem), &buff_L);
	clSetKernelArg(zncc_kernel, 2, sizeof(cl_mem), &buff_disp_RL);
	clSetKernelArg(zncc_kernel, 6, sizeof(cl_int), &max_disp_neg);
	clSetKernelArg(zncc_kernel, 7, sizeof(cl_int), &min_disp);
	status = clEnqueueNDRangeKernel(command_queue, zncc_kernel, 2, NULL, globalWorkSize, NULL, 0, NULL, &event_2);
	clCheckStatus(status);

	// Cross-checking
	printf("Performing cross-check...\n");
	clSetKernelArg(crosscheck_kernel, 0, sizeof(cl_mem), &buff_disp_LR);
	clSetKernelArg(crosscheck_kernel, 1, sizeof(cl_mem), &buff_disp_RL);
	clSetKernelArg(crosscheck_kernel, 2, sizeof(cl_mem), &buff_disp_CC);
	clSetKernelArg(crosscheck_kernel, 3, sizeof(cl_int), &threshold);
	status = clEnqueueNDRangeKernel(command_queue, crosscheck_kernel, 1, NULL, globalWorkSize1D, localWorkSize1D, 0, NULL, &event_3);
	clCheckStatus(status);

	// Occlusion filling
	printf("Performing occlusion filling...\n");
	clSetKernelArg(occlusionfill_kernel, 0, sizeof(cl_mem), &buff_disp_CC);
	clSetKernelArg(occlusionfill_kernel, 1, sizeof(cl_mem), &buff_disp_CC_OF);
	clSetKernelArg(occlusionfill_kernel, 2, sizeof(cl_int), &w_out);
	clSetKernelArg(occlusionfill_kernel, 3, sizeof(cl_int), &h_out);
	clSetKernelArg(occlusionfill_kernel, 4, sizeof(cl_int), &win_width_of);
	clSetKernelArg(occlusionfill_kernel, 5, sizeof(cl_int), &win_height_of);
	status = clEnqueueNDRangeKernel(command_queue, occlusionfill_kernel, 2, NULL, globalWorkSize, NULL, 0, NULL, &event_4);
	clCheckStatus(status);

	// Normalization
	printf("Performing normalization...\n");
	clSetKernelArg(normalization_kernel, 0, sizeof(cl_mem), &buff_disp_CC_OF);
	clSetKernelArg(normalization_kernel, 1, sizeof(cl_int), &w_out);
	clSetKernelArg(normalization_kernel, 2, sizeof(cl_int), &h_out);
	clSetKernelArg(normalization_kernel, 3, sizeof(cl_int), &max_disp);
	status = clEnqueueNDRangeKernel(command_queue, normalization_kernel, 1, NULL, globalWorkSize1D, localWorkSize1D, 0, NULL, &event_5);
	clCheckStatus(status);





	// Kernel profiling
	clFinish(command_queue);
	clGetEventProfilingInfo(event_0, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &t1_oc, NULL);
	clGetEventProfilingInfo(event_0, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &t2_oc, NULL);
	elapsed_time_oc = (t2_oc - t1_oc)*1.0e-9f;
	total_time_oc += elapsed_time_oc;
	printf("\nResize and greyscale kernel: %lf seconds\n", elapsed_time_oc);
	clGetEventProfilingInfo(event_1, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &t1_oc, NULL);
	clGetEventProfilingInfo(event_1, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &t2_oc, NULL);
	elapsed_time_oc = (t2_oc - t1_oc)*1.0e-9f;
	total_time_oc += elapsed_time_oc;
	printf("ZNCC left-right kernel:      %lf seconds\n", elapsed_time_oc);
	clGetEventProfilingInfo(event_2, CL_PROFILING_COMMAND_START , sizeof(cl_ulong), &t1_oc, NULL);
	clGetEventProfilingInfo(event_2, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &t2_oc, NULL);
	elapsed_time_oc = (t2_oc - t1_oc)*1.0e-9f;
	total_time_oc += elapsed_time_oc;
	printf("ZNCC right-left kernel:      %lf seconds\n", elapsed_time_oc);
	clGetEventProfilingInfo(event_3, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &t1_oc, NULL);
	clGetEventProfilingInfo(event_3, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &t2_oc, NULL);
	elapsed_time_oc = (t2_oc - t1_oc)*1.0e-9f;
	total_time_oc += elapsed_time_oc;
	printf("Cross-check kernel:          %lf seconds\n", elapsed_time_oc);
	clGetEventProfilingInfo(event_4, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &t1_oc, NULL);
	clGetEventProfilingInfo(event_4, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &t2_oc, NULL);
	elapsed_time_oc = (t2_oc - t1_oc)*1.0e-9f;
	total_time_oc += elapsed_time_oc;
	printf("Occlusion fill kernel:       %lf seconds\n", elapsed_time_oc);
	clGetEventProfilingInfo(event_5, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &t1_oc, NULL);
	clGetEventProfilingInfo(event_5, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &t2_oc, NULL);
	elapsed_time_oc = (t2_oc - t1_oc)*1.0e-9f;
	total_time_oc += elapsed_time_oc;
	printf("Normalization kernel:        %lf seconds\n", elapsed_time_oc);

	// Stopping the timer
	QueryPerformanceCounter(&t2);
	elapsed_time = (float)(t2.QuadPart - t1.QuadPart) / frequency.QuadPart; // Time in seconds
																			// Print host and kernel times
	printf("Total Kernel time:	%lf seconds\n", total_time_oc);
	printf("Total execution time:	%lf seconds\n", elapsed_time);
	printf("Host side time: %lf seconds\n", elapsed_time - total_time_oc);


	// Read buffers for results
	printf("Reading and saving results...\n");
	size_t region[3] = { w_out, h_out, 0 };
	// Grey left
	unsigned char* grey_L = (unsigned char*)malloc(w_out * h_out);
	status = clEnqueueReadBuffer(command_queue, buff_L, CL_TRUE, 0, w_out*h_out, grey_L, 0, NULL, NULL);
	clCheckStatus(status);
	// Grey right
	unsigned char* grey_R = (unsigned char*)malloc(w_out * h_out);
	status = clEnqueueReadBuffer(command_queue, buff_R, CL_TRUE, 0, w_out*h_out, grey_R, 0, NULL, NULL);
	clCheckStatus(status);
	// Disparity left-right
	unsigned char* disp_LR = (unsigned char*)malloc(w_out * h_out);
	status = clEnqueueReadBuffer(command_queue, buff_disp_LR, CL_TRUE, 0, w_out*h_out, disp_LR, 0, NULL, NULL);
	clCheckStatus(status);
	// Disparity right-left
	unsigned char* disp_RL = (unsigned char*)malloc(w_out * h_out);
	status = clEnqueueReadBuffer(command_queue, buff_disp_RL, CL_TRUE, 0, w_out*h_out, disp_RL, 0, NULL, NULL);
	clCheckStatus(status);
	// Cross-checked
	unsigned char* disp_CC = (unsigned char*)malloc(w_out * h_out);
	status = clEnqueueReadBuffer(command_queue, buff_disp_CC, CL_TRUE, 0, w_out*h_out, disp_CC, 0, NULL, NULL);
	clCheckStatus(status);
	// Occlusion filled (and normalized)
	unsigned char* disp_CC_OF = (unsigned char*)malloc(w_out * h_out);
	status = clEnqueueReadBuffer(command_queue, buff_disp_CC_OF, CL_TRUE, 0, w_out*h_out, disp_CC_OF, 0, NULL, NULL);
	clCheckStatus(status);


	// Encode and save results
	lodepng_encode_file("output/_grey_L_GPU.png", grey_L, w_out, h_out, LCT_GREY, 8);
	lodepng_encode_file("output/_grey_R_GPU.png", grey_R, w_out, h_out, LCT_GREY, 8);
	lodepng_encode_file("output/_disp_LR_GPU.png", disp_LR, w_out, h_out, LCT_GREY, 8);
	lodepng_encode_file("output/_disp_RL_GPU.png", disp_RL, w_out, h_out, LCT_GREY, 8);
	lodepng_encode_file("output/_disp_CC_GPU.png", disp_CC, w_out, h_out, LCT_GREY, 8);
	lodepng_encode_file("output/_disp_CC_OF_norm_GPU.png", disp_CC_OF, w_out, h_out, LCT_GREY, 8);

	return 0;
}

cl_kernel clOneKernelPlease(cl_context context, cl_device_id device_id, const char* file_name, const char* kernel_name)
{
	//	This function simplifies creation of multiple kernels,
	//	It wraps filereading, program building and kernel creation.

	cl_int status;

	// Load the kernel source code into source_str
	FILE *fp;
	char *source_str;
	size_t source_size;

	fopen_s(&fp, file_name, "r");
	source_str = (char*)malloc(0x100000);
	source_size = fread(source_str, 1, 0x100000, fp);
	fclose(fp);

	// Create and build a program
	cl_program program = clCreateProgramWithSource(context, 1, (const char**)&source_str, (const size_t *)&source_size, &status);
	clCheckStatus(status);
	//char options[] = "-cl-unsafe-math-optimizations -cl-mad-enable";
	status = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	if (status != CL_SUCCESS)
	{
		printf("clBuildProgram error... %d\n", status);

		if (status != CL_SUCCESS) {
			char *buff_erro;
			cl_int errcode;
			size_t build_log_len;
			errcode = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &build_log_len);
			if (errcode) {
				printf("clGetProgramBuildInfo failed at line %d\n", __LINE__);
				exit(-1);
			}

			buff_erro = malloc(build_log_len);
			if (!buff_erro) {
				printf("malloc failed at line %d\n", __LINE__);
				exit(-2);
			}

			errcode = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, build_log_len, buff_erro, NULL);
			if (errcode) {
				printf("clGetProgramBuildInfo failed at line %d\n", __LINE__);
				exit(-3);
			}

			fprintf(stderr, "Build log: \n%s\n", buff_erro); //Be careful with  the fprint
			free(buff_erro);
			fprintf(stderr, "clBuildProgram failed\n");
			exit(EXIT_FAILURE);
		}
	}

	// Create kernel
	cl_kernel kernel = clCreateKernel(program, kernel_name, &status);
	clCheckStatus(status);

	return kernel;
}

void clPrintInfo(cl_platform_id platform_id, cl_device_id device_id)
{
	cl_char string[10240] = { 0 };
	cl_uint num;
	cl_ulong mem_size;
	cl_device_local_mem_type mem_type;
	size_t size;
	size_t dims[3];

	printf("------ PLATFORM ------\n");

	clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, sizeof(string), &string, NULL);
	printf("Name:			%s\n", string);

	clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, sizeof(string), &string, NULL);
	printf("Version:		%s\n", string);

	printf("\n------- DEVICE -------\n");

	clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(string), &string, NULL);
	printf("Name:			%s\n", string);

	clGetDeviceInfo(device_id, CL_DEVICE_ADDRESS_BITS, sizeof(cl_uint), &num, NULL);
	printf("Address bits:	%d\n", num);

	clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &num, NULL);
	printf("Max compute units:	%d\n", num);

	clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &mem_size, NULL);
	printf("Local memory size:	%llu KB\n", mem_size / 1024);

	clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_TYPE, sizeof(cl_device_local_mem_type), &mem_type, NULL);
	printf("Local memory type:	0x%u\n", mem_type);

	clGetDeviceInfo(device_id, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_uint), &num, NULL);
	printf("Max clock frequency:	%d MHz\n", num);

	clGetDeviceInfo(device_id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &mem_size, NULL);
	printf("Max buffer size:	%llu KB\n", mem_size / 1024);

	clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &size, NULL);
	printf("Max work group size:	%zu B\n", size);

	clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(dims), &dims, NULL);
	printf("Max work item sizes:	%ld B * %ld B * %ld B\n", dims[0], dims[1], dims[2]);
}

int clCheckStatus(cl_int status_code)
{
	if (status_code != CL_SUCCESS)
	{
		printf("OpenCL error... %d\n", status_code);
		return 1;
	}
}
