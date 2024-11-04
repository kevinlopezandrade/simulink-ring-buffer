#define S_FUNCTION_NAME mmapreader
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
#include "cbor.h"
#include "checksum.h"

#define NPRMS 1
#define sharedFileIdIdx 0

#define MDL_START

float
extract_float(const uint8_t *buffer, size_t len)
{
    CborParser parser;
    CborValue value;
    float result;

    cbor_parser_init(buffer, len, 0, &parser, &value);
    cbor_value_get_float(&value, &result);
    return result;
}

static void
report_and_exit(const char* msg, SimStruct *S)
{
    ssSetErrorStatus(S, msg);
}

static void
mdlStart(SimStruct *S)
{
    int shm_fd;
    void *shm_ptr;
    struct stat shm_stat;
    char* shm_file_id;

    /* Open the shared memory object for reading */
    shm_file_id = mxArrayToString(ssGetSFcnParam(S, sharedFileIdIdx));
    shm_fd = shm_open(shm_file_id, O_RDONLY, S_IWUSR);

    if (shm_fd == -1) {
        report_and_exit("could not open shared memory", S);
    }

    /* Memory map the shared memory object for reading only */
    shm_ptr = mmap(NULL, sizeof(RingBuffer), PROT_READ, MAP_SHARED, shm_fd, 0);

    RingBuffer* ring_buffer = (RingBuffer*)shm_ptr;
    ssSetPWorkValue(S, 0, ring_buffer);

    ReadToken read_token = {.idx = 0, .initialized = false};
    ssSetPWorkValue(S, 1, &read_token);
}

static void
mdlInitializeSizes(SimStruct *S)
{
    ssSetNumSFcnParams(S, NPRMS);
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        return; /* Parameter mismatch reported by the Simulink engine*/
    }

    /* Specify that none of the parameters are tunable */
    ssSetSFcnParamTunable(S, sharedFileIdIdx, false);

    if (!ssSetNumInputPorts(S, 0)) return;

    if (!ssSetNumOutputPorts(S,1)) return;
    ssSetOutputPortWidth(S, 0, DYNAMICALLY_SIZED);

    /* Only one sample time. */
    ssSetNumSampleTimes(S, 1);

    /* Take care when specifying exception free code - see sfuntmpl.doc */
    ssSetOptions(S, SS_OPTION_EXCEPTION_FREE_CODE);

    /* Allocate space for the pointer to shared memory. */
    ssSetNumPWork(S, 3);
}

static void
mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, 0.01);
    ssSetOffsetTime(S, 0, 0.0);
}


static void
mdlOutputs(SimStruct *S, int_T tid)
{
    RingBuffer* ring_buffer;
    ReadToken* read_token;
    Message msg;
    real_T *y;
    int_T width;
    int_T i;
    real_T result;
    CborValue it;
    CborParser parser;
    CborValue array_it;

    y = ssGetOutputPortRealSignal(S,0);
    width = ssGetOutputPortWidth(S,0);

    ring_buffer = (RingBuffer*)ssGetPWorkValue(S, 0);
    read_token = (ReadToken*)ssGetPWorkValue(S, 1);

    msg = read_next(read_token, ring_buffer);

    if (!msg.wait) {
        if ( crc((unsigned char *)&msg.data, CBOR_BUFFER_SIZE) != msg.checksum ) {
            return;
        }

        cbor_parser_init(msg.data, 60, 0, &parser, &it);
        cbor_value_enter_container(&it, &array_it);

        for (i=0; i<width; i++) {
            cbor_value_get_double(&array_it, &result);

            *y++ = result;
            cbor_value_advance_fixed(&array_it);
        }
    }

}

static void
mdlTerminate(SimStruct *S)
{

}

#ifdef MATLAB_MEX_FILE /* Is this file being compiled as a MEX-file? */
#include "simulink.c" /* MEX-file interface mechanism */
#else
#include "cg_sfun.h" /* Code generation registration function */
#endif
