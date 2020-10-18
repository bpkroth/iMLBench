// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include "backprop.h"

#ifdef NV //NVIDIA
	#include <oclUtils.h>
#else 
	#include <CL/cl.h>
#endif


////////////////////////////////////////////////////////////////////////////////

// local variables
static cl_context	    context;
static cl_command_queue cmd_queue;
static cl_device_type   device_type;
static cl_device_id   * device_list;
static cl_int           num_devices;

#define THREADS 256
#define WIDTH 16  
#define HEIGHT 16 
#define ETA 0.3f       
#define MOMENTUM 0.3f  

#define WM(i, j)   weight_matrix[(j) + (i) * WIDTH]
/*

	clSetKernelArgSVMPointer(kernel1, 0, (void*) input_ocl);
	clSetKernelArgSVMPointer(kernel1, 1, (void*) output_hidden_ocl);
	clSetKernelArgSVMPointer(kernel1, 2, (void*) input_hidden_ocl);
	clSetKernelArgSVMPointer(kernel1, 3, (void*) hidden_partial_sum );
	clSetKernelArg(kernel1, 4, sizeof(float) *  HEIGHT, (void*)NULL );
	clSetKernelArg(kernel1, 5, sizeof(float ) *  HEIGHT * WIDTH, (void*)NULL );
	clSetKernelArg(kernel1, 6, sizeof(cl_int), (void*) &in);
	clSetKernelArg(kernel1, 7, sizeof(cl_int), (void*) &hid);
	size_t global_work[3] = { BLOCK_SIZE, BLOCK_SIZE * num_blocks, 1 }; 
	size_t local_work[3] = { BLOCK_SIZE, BLOCK_SIZE, 1 };
	*/
/*
void bpnn_layerforward_omp(float *input_cuda,
	                   float *output_hidden_cuda,
					   float *input_hidden_cuda,
					   float *hidden_partial_sum,
					   float *input_node,
					   float *weight_matrix,
					   int in,
					   int hid) 
{

   int by = get_group_id(1);
   int tx = get_local_id(0);
   int ty = get_local_id(1);

   int index =  ( hid + 1 ) * HEIGHT * by + ( hid + 1 ) * ty + tx + 1 + ( hid + 1 ) ;  

   int index_in = HEIGHT * by + ty + 1;
   
	if ( tx == 0 )
		input_node[ty] = input_cuda[index_in] ;
		barrier(CLK_LOCAL_MEM_FENCE);

		weight_matrix[ty * WIDTH + tx] =  input_hidden_cuda[index];
		barrier(CLK_LOCAL_MEM_FENCE);
   
		weight_matrix[ty * WIDTH + tx]= weight_matrix[ty * WIDTH + tx] * input_node[ty];
		barrier(CLK_LOCAL_MEM_FENCE);
   
		for ( int i = 1 ; i <= HEIGHT ; i=i*2){
	        int power_two = i; 
	        if( ty % power_two == 0 )
		    	weight_matrix[ty * WIDTH + tx]= weight_matrix[ty * WIDTH + tx] + weight_matrix[(ty + power_two/2)* WIDTH + tx];
		    barrier(CLK_LOCAL_MEM_FENCE);
    	}
    // change
    input_hidden_cuda[index] =  weight_matrix[ty * WIDTH + tx];
   
	barrier(CLK_LOCAL_MEM_FENCE);

    if ( tx == 0 ) {
    	// change
	    hidden_partial_sum[by * hid + ty] = weight_matrix[tx* WIDTH + ty];
    }

}



__kernel void  bpnn_adjust_weights_ocl( __global float * delta,   
										 int hid,         
										__global float * ly,      
										 int in,          
										__global float * w,       
										__global float * oldw)  									
{
   
   int by = get_group_id(1);
   int tx = get_local_id(0);
   int ty = get_local_id(1);
	
   int index =  ( hid + 1 ) * HEIGHT * by + ( hid + 1 ) * ty + tx + 1 + ( hid + 1 ) ;  
   int index_y = HEIGHT * by + ty + 1;
   int index_x = tx + 1;

   w[index] += ((ETA * delta[index_x] * ly[index_y]) + (MOMENTUM * oldw[index]));
   oldw[index] = ((ETA * delta[index_x] * ly[index_y]) + (MOMENTUM * oldw[index]));

   barrier(CLK_LOCAL_MEM_FENCE);

   if (ty == 0 && by ==0){
	w[index_x] += ((ETA * delta[index_x]) + (MOMENTUM * oldw[index_x]));
	oldw[index_x] = ((ETA * delta[index_x]) + (MOMENTUM * oldw[index_x]));
   }

}
*/

static int initialize(int use_gpu)
{
	cl_int result;
	size_t size;

	// create OpenCL context
	cl_platform_id platform_id;
	if (clGetPlatformIDs(1, &platform_id, NULL) != CL_SUCCESS) { printf("ERROR: clGetPlatformIDs(1,*,0) failed\n"); return -1; }
	cl_context_properties ctxprop[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platform_id, 0};
	device_type = use_gpu ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU;
	context = clCreateContextFromType( ctxprop, device_type, NULL, NULL, NULL );
	if( !context ) { printf("ERROR: clCreateContextFromType(%s) failed\n", use_gpu ? "GPU" : "CPU"); return -1; }

	// get the list of GPUs
	result = clGetContextInfo( context, CL_CONTEXT_DEVICES, 0, NULL, &size );
	num_devices = (int) (size / sizeof(cl_device_id));
	printf("num_devices = %d\n", num_devices);
	
	if( result != CL_SUCCESS || num_devices < 1 ) { printf("ERROR: clGetContextInfo() failed\n"); return -1; }
	device_list = new cl_device_id[num_devices];
	//device_list = (cl_device_id *)malloc(sizeof(cl_device_id)*num_devices);
	if( !device_list ) { printf("ERROR: new cl_device_id[] failed\n"); return -1; }
	result = clGetContextInfo( context, CL_CONTEXT_DEVICES, size, device_list, NULL );
	if( result != CL_SUCCESS ) { printf("ERROR: clGetContextInfo() failed\n"); return -1; }

	// create command queue for the first device
	cmd_queue = clCreateCommandQueue( context, device_list[0], 0, NULL );
	if( !cmd_queue ) { printf("ERROR: clCreateCommandQueue() failed\n"); return -1; }
	return 0;
}

static int shutdown()
{
	// release resources
	if( cmd_queue ) clReleaseCommandQueue( cmd_queue );
	if( context ) clReleaseContext( context );
	if( device_list ) delete[] device_list;

	// reset all variables
	cmd_queue = 0;
	context = 0;
	device_list = 0;
	num_devices = 0;
	device_type = 0;

	return 0;
}

double gettime() {
  struct timeval t;
  gettimeofday(&t,NULL);
  return t.tv_sec+t.tv_usec*1e-6;
}

unsigned int num_threads = 0;
unsigned int num_blocks = 0;

////////////////////////////////////////////////////////////////////////////////
// Program main
////////////////////////////////////////////////////////////////////////////////
int
main( int argc, char** argv) 
{
	setup(argc, argv);
	return 0;
}



int bpnn_train_kernel(BPNN *net, float *eo, float *eh)
{
	int in, hid, out;
	float out_err, hid_err;
  
	in = net->input_n;
	hid = net->hidden_n;
	out = net->output_n;   
   
	int sourcesize = 1024*1024;
	char * source = (char *)calloc(sourcesize, sizeof(char)); 
	if(!source) { printf("ERROR: calloc(%d) failed\n", sourcesize); return -1; }

	// read the kernel core source
	char * kernel_bp1  = "bpnn_layerforward_ocl";
	char * kernel_bp2  = "bpnn_adjust_weights_ocl";
	char * tempchar = "./backprop_kernel.cl";
	FILE * fp = fopen(tempchar, "rb"); 
	if(!fp) { printf("ERROR: unable to open '%s'\n", tempchar); return -1; }
	fread(source + strlen(source), sourcesize, 1, fp);
	fclose(fp);
	
	int use_gpu = 1;
	if(initialize(use_gpu)) return -1;
	
	// compile kernel
	cl_int err = 0;
	const char * slist[2] = { source, 0 };
	cl_program prog = clCreateProgramWithSource(context, 1, slist, NULL, &err);
	if(err != CL_SUCCESS) { printf("ERROR: clCreateProgramWithSource() => %d\n", err); return -1; }
	err = clBuildProgram(prog, 0, NULL, NULL, NULL, NULL);
	{ // show warnings/errors
		//static char log[65536]; memset(log, 0, sizeof(log));
		//cl_device_id device_id = 0;
		//err = clGetContextInfo(context, CL_CONTEXT_DEVICES, sizeof(device_id), &device_id, NULL);
		//clGetProgramBuildInfo(prog, device_id, CL_PROGRAM_BUILD_LOG, sizeof(log)-1, log, NULL);
		//if(err || strstr(log,"warning:") || strstr(log, "error:")) printf("<<<<\n%s\n>>>>\n", log);
	}
	if(err != CL_SUCCESS) { printf("ERROR: clBuildProgram() => %d\n", err); return -1; }
    	
	cl_kernel kernel1;
	cl_kernel kernel2;
	kernel1 = clCreateKernel(prog, kernel_bp1, &err);  
	kernel2 = clCreateKernel(prog, kernel_bp2, &err);  
	if(err != CL_SUCCESS) { printf("ERROR: clCreateKernel() 0 => %d\n", err); return -1; }
	clReleaseProgram(prog);
	
	float *input_weights_one_dim;
    float *input_weights_prev_one_dim;
	//float * partial_sum;
	float sum;
	float num_blocks = in / BLOCK_SIZE;

	input_weights_one_dim = (float *)clSVMAlloc(context, CL_MEM_READ_WRITE, (in + 1) * (hid + 1) * sizeof(float), 0);
	if(input_weights_one_dim == NULL) { printf("ERROR: clSVMAlloc input_weights_one_dim\n"); return -1;}
	input_weights_prev_one_dim = (float *) clSVMAlloc(context, CL_MEM_READ_WRITE, (in + 1)* (hid + 1) * sizeof(float), 0);
	if(input_weights_prev_one_dim == NULL) { printf("ERROR: clSVMAlloc input_weights_prev_one_dim\n"); return -1;}

	//partial_sum = (float *) malloc(num_blocks * WIDTH * sizeof(float));
	
	// set global and local workitems
	size_t global_work[3] = { BLOCK_SIZE, BLOCK_SIZE * num_blocks, 1 }; 
	size_t local_work[3] = { BLOCK_SIZE, BLOCK_SIZE, 1 };
	
	// this preprocessing stage is temporarily added to correct the bug of wrong memcopy using two-dimensional net->inputweights
	// todo: fix mem allocation
	int m = 0;
	for (int k = 0; k <= in; k++) {	
		for (int j = 0; j <= hid; j++) {
		input_weights_one_dim[m] = net->input_weights[k][j];
		input_weights_prev_one_dim[m] = net-> input_prev_weights[k][j];
	    m++;
		}
	}
	
	void * input_hidden_ocl;
	void * input_ocl;
	void * output_hidden_ocl;
	float * hidden_partial_sum;
	void * hidden_delta_ocl;
	void * input_prev_weights_ocl;
/*
	input_ocl = clCreateBuffer(context, CL_MEM_READ_WRITE, (in + 1) * sizeof(float), NULL, &err );
	if(err != CL_SUCCESS) { printf("ERROR: clCreateBuffer input_ocl\n"); return -1;}
	input_hidden_ocl = clCreateBuffer(context, CL_MEM_READ_WRITE, (in + 1) * (hid + 1) * sizeof(float), NULL, &err );
	if(err != CL_SUCCESS) { printf("ERROR: clCreateBuffer input_hidden_ocl\n"); return -1;}
	output_hidden_ocl = clCreateBuffer(context, CL_MEM_READ_WRITE, (hid + 1) * sizeof(float), NULL, &err );
	if(err != CL_SUCCESS) { printf("ERROR: clCreateBuffer output_hidden_ocl\n"); return -1;}
	hidden_partial_sum = clCreateBuffer(context, CL_MEM_READ_WRITE, num_blocks * WIDTH * sizeof(float), NULL, &err );
	if(err != CL_SUCCESS) { printf("ERROR: clCreateBuffer hidden_partial_sum\n"); return -1;}
	hidden_delta_ocl = clCreateBuffer(context, CL_MEM_READ_WRITE, (hid + 1) * sizeof(float), NULL, &err );
	if(err != CL_SUCCESS) { printf("ERROR: clCreateBuffer hidden_delta_ocl\n"); return -1;}
	input_prev_weights_ocl = clCreateBuffer(context, CL_MEM_READ_WRITE, (in + 1) * (hid + 1) * sizeof(float), NULL, &err );
	if(err != CL_SUCCESS) { printf("ERROR: clCreateBuffer input_prev_weights_ocl\n"); return -1;}

	input_hidden_ocl = clSVMAlloc(context, CL_MEM_READ_WRITE, (in + 1) * (hid + 1) * sizeof(float), 0);
	if(input_hidden_ocl == NULL) { printf("ERROR: clSVMAlloc input_hidden_ocl\n"); return -1;}
	output_hidden_ocl = clSVMAlloc(context, CL_MEM_READ_WRITE, (hid + 1) * sizeof(float), 0);
	if(output_hidden_ocl == NULL) { printf("ERROR: clSVMAlloc output_hidden_ocl\n"); return -1;}
	memcpy(input_ocl, net->input_units, (in + 1) * sizeof(float));
	memcpy(input_hidden_ocl, input_weights_one_dim, (in + 1) * (hid + 1) * sizeof(float));
	*/
	input_hidden_ocl = input_weights_one_dim;

	input_ocl = clSVMAlloc(context, CL_MEM_READ_WRITE, (in + 1) * sizeof(float), 0);
	if(input_ocl == NULL) { printf("ERROR: clSVMAlloc input_ocl\n"); return -1;}
    output_hidden_ocl = clSVMAlloc(context, CL_MEM_READ_WRITE, (hid + 1) * sizeof(float), 0);
	if(output_hidden_ocl == NULL) { printf("ERROR: clSVMAlloc output_hidden_ocl\n"); return -1;}
	hidden_partial_sum = (float *)clSVMAlloc(context, CL_MEM_READ_WRITE, num_blocks * WIDTH * sizeof(float), 0);
	if(hidden_partial_sum == NULL) { printf("ERROR: clSVMAlloc hidden_partial_sum\n"); return -1;}
	hidden_delta_ocl = clSVMAlloc(context, CL_MEM_READ_WRITE, (hid + 1) * sizeof(float), 0);
	if(hidden_delta_ocl == NULL) { printf("ERROR: clSVMAlloc hidden_delta_ocl\n"); return -1;}

	input_prev_weights_ocl = input_weights_prev_one_dim;

	memcpy(input_ocl, net->input_units, (in + 1) * sizeof(float));

	printf("Performing GPU computation\n");
	
	//write buffers
	/*
	err = clEnqueueWriteBuffer(cmd_queue, input_ocl, 1, 0, (in + 1) * sizeof(float), net->input_units, 0, 0, 0);
	if(err != CL_SUCCESS) { printf("ERROR: clEnqueueWriteBuffer input_ocl\n"); return -1; }
	err = clEnqueueWriteBuffer(cmd_queue, input_hidden_ocl, 1, 0, (in + 1) * (hid + 1) * sizeof(float), input_weights_one_dim, 0, 0, 0);
	if(err != CL_SUCCESS) { printf("ERROR: clEnqueueWriteBuffer input_hidden_ocl\n"); return -1; }
 */

	clSetKernelArgSVMPointer(kernel1, 0, (void*) input_ocl);
	clSetKernelArgSVMPointer(kernel1, 1, (void*) output_hidden_ocl);
	clSetKernelArgSVMPointer(kernel1, 2, (void*) input_hidden_ocl);
	clSetKernelArgSVMPointer(kernel1, 3, (void*) hidden_partial_sum );
	clSetKernelArg(kernel1, 4, sizeof(float) *  HEIGHT, (void*)NULL );
	clSetKernelArg(kernel1, 5, sizeof(float ) *  HEIGHT * WIDTH, (void*)NULL );
	clSetKernelArg(kernel1, 6, sizeof(cl_int), (void*) &in);
	clSetKernelArg(kernel1, 7, sizeof(cl_int), (void*) &hid);
  
	err = clEnqueueNDRangeKernel(cmd_queue, kernel1, 2, NULL, global_work, local_work, 0, 0, 0);
	if(err != CL_SUCCESS) { printf("ERROR: 1  clEnqueueNDRangeKernel()=>%d failed\n", err); return -1; }	
  /*
	err = clEnqueueReadBuffer(cmd_queue, hidden_partial_sum, 1, 0, num_blocks * WIDTH * sizeof(float), partial_sum, 0, 0, 0);
	if(err != CL_SUCCESS) { printf("ERROR: 1  clEnqueueReadBuffer: partial sum\n"); return -1; }	
  */
	for (int j = 1; j <= hid; j++) {
		sum = 0.0;
		for (int k = 0; k < num_blocks; k++) {	
		sum += hidden_partial_sum[k * hid + j-1] ;
    }
		sum += net->input_weights[0][j];
		net-> hidden_units[j] = float(1.0 / (1.0 + exp(-sum)));
	}

	bpnn_layerforward(net->hidden_units, net->output_units, net->hidden_weights, hid, out);
	bpnn_output_error(net->output_delta, net->target, net->output_units, out, &out_err);
	bpnn_hidden_error(net->hidden_delta, hid, net->output_delta, out, net->hidden_weights, net->hidden_units, &hid_err);  
	bpnn_adjust_weights(net->output_delta, out, net->hidden_units, hid, net->hidden_weights, net->hidden_prev_weights);
/*
	err = clEnqueueWriteBuffer(cmd_queue, hidden_delta_ocl,       1, 0, (hid + 1) * sizeof(float), net->hidden_delta, 0, 0, 0);
	if(err != CL_SUCCESS) { printf("ERROR: clEnqueueWriteBuffer hidden_delta_ocl\n"); return -1; }
	err = clEnqueueWriteBuffer(cmd_queue, input_prev_weights_ocl, 1, 0, (in + 1) * (hid + 1) * sizeof(float), input_weights_prev_one_dim, 0, 0, 0);
	if(err != CL_SUCCESS) { printf("ERROR: clEnqueueWriteBuffer input_prev_weights_ocl\n"); return -1; }
	err = clEnqueueWriteBuffer(cmd_queue, input_hidden_ocl,       1, 0, (in + 1) * (hid + 1) * sizeof(float), input_weights_one_dim, 0, 0, 0);
	if(err != CL_SUCCESS) { printf("ERROR: clEnqueueWriteBuffer input_hidden_ocl\n"); return -1; }
 */
 	memcpy(hidden_delta_ocl, net->hidden_delta, (hid + 1) * sizeof(float));

	clSetKernelArgSVMPointer(kernel2, 0, (void*) hidden_delta_ocl);
	clSetKernelArg(kernel2, 1, sizeof(cl_int), (void*) &hid);
	clSetKernelArgSVMPointer(kernel2, 2, (void*) input_ocl);
	clSetKernelArg(kernel2, 3, sizeof(cl_int), (void*) &in);
	clSetKernelArgSVMPointer(kernel2, 4, (void*) input_hidden_ocl);
	clSetKernelArgSVMPointer(kernel2, 5, (void*) input_prev_weights_ocl );
  
	err = clEnqueueNDRangeKernel(cmd_queue, kernel2, 2, NULL, global_work, local_work, 0, 0, 0);
	if(err != CL_SUCCESS) { printf("ERROR: 1  clEnqueueNDRangeKernel()=>%d failed\n", err); return -1; }	
 /* 
	err = clEnqueueReadBuffer(cmd_queue, input_ocl, 1, 0, (in + 1) * sizeof(float), net->input_units, 0, 0, 0);
	if(err != CL_SUCCESS) { printf("ERROR: 1  clEnqueueReadBuffer: input_ocl\n"); return -1; }	
	err = clEnqueueReadBuffer(cmd_queue, input_hidden_ocl, 1, 0, (in + 1) * (hid + 1) * sizeof(float), input_weights_one_dim, 0, 0, 0);
	if(err != CL_SUCCESS) { printf("ERROR: 1  clEnqueueReadBuffer: input_hidden_ocl\n"); return -1; }	
  */
	memcpy(net->input_units, input_ocl,  (in + 1) * sizeof(float));


    for(int i = 0; i < in; i++){
    	printf("%d ",(net->input_units)[i]);
    }
	clSVMFree(context, input_hidden_ocl);
	clSVMFree(context, input_ocl);
	clSVMFree(context, output_hidden_ocl);
	clSVMFree(context, hidden_partial_sum);
	clSVMFree(context, hidden_delta_ocl);
	clSVMFree(context, input_prev_weights_ocl);
  
//	free(input_weights_prev_one_dim);
	//free(partial_sum);
//	free(input_weights_one_dim);

}
