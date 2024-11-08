#define S_FUNCTION_NAME timestwo 
#define S_FUNCTION_LEVEL 2
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>

#include "simstruc.h"
#include "ringbuffer.h"


#define MDL_START

static void
report_and_exit(const char* msg, SimStruct *S)
{
    ssSetErrorStatus(S, msg);
}

static void
mdlStart(SimStruct *S)
{
    int fd;
    size_t ring_bytes;

    /* Start the shared memory */
    ring_bytes = sizeof(RingBuffer);
    fd = shm_open("matlab-test", O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

    if (fd == -1) {
        report_and_exit("could not open shared memory", S);
    } else {
        /* Extend the 0 bytes file to number of bytes occupied by ring */
        if (ftruncate(fd, ring_bytes) == -1){
            report_and_exit("could not truncate the shared memory", S);
        }

        /* Map it to external memory; */
        RingBuffer* ring_buffer = (RingBuffer*)mmap(NULL, ring_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        /* Initialize */
        ring_buffer->write_idx = 0;
        ring_buffer->oldest_idx = 0;
        ring_buffer->filled = false;
        ring_buffer->buf_size = BUFFER_SIZE;


        if (ring_buffer == MAP_FAILED) {
            report_and_exit("could not mmap the ring buffer", S);
        }

        /* Its safe to close after */
        close(fd);

        /* Pass the pointer to the rest of the callbacks. */
        ssSetPWorkValue(S, 0, ring_buffer);
    }
}

static void
mdlInitializeSizes(SimStruct *S)
{
    ssSetNumSFcnParams(S, 0);
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        return; /* Parameter mismatch reported by the Simulink engine*/
    }

    if (!ssSetNumInputPorts(S, 1)) return;
    ssSetInputPortWidth(S, 0, DYNAMICALLY_SIZED);
    ssSetInputPortDirectFeedThrough(S, 0, 1);

    if (!ssSetNumOutputPorts(S,1)) return;
    ssSetOutputPortWidth(S, 0, DYNAMICALLY_SIZED);

    ssSetNumSampleTimes(S, 1);

    /* Take care when specifying exception free code - see sfuntmpl.doc */
    ssSetOptions(S, SS_OPTION_EXCEPTION_FREE_CODE);

    /* Allocate space for the pointer to shared memory. */
    ssSetNumPWork(S, 1);
}

static void
mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, INHERITED_SAMPLE_TIME);
    ssSetOffsetTime(S, 0, 0.0);
}


static void
mdlOutputs(SimStruct *S, int_T tid)
{
    int_T i;
    InputRealPtrsType uPtrs = ssGetInputPortRealSignalPtrs(S,0);
    real_T *y = ssGetOutputPortRealSignal(S,0);
    int_T width = ssGetOutputPortWidth(S,0);

    for (i=0; i<width; i++) {
        *y++ = 2.0 *(*uPtrs[i]);
    }

    RingBuffer* ring_buffer = (RingBuffer*)ssGetPWorkValue(S, 0);
    publish(ring_buffer, *uPtrs[0]);
}

static void
mdlTerminate(SimStruct *S)
{
    shm_unlink("matlab-test");
}

#ifdef MATLAB_MEX_FILE /* Is this file being compiled as a MEX-file? */
#include "simulink.c" /* MEX-file interface mechanism */
#else
#include "cg_sfun.h" /* Code generation registration function */
#endif
