#define S_FUNCTION_NAME mmapwriter
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
#include <errno.h>

#include "matrix.h"
#include "simstruc.h"
#include "ringbuffer.h"
#include "cbor.h"

#define NPRMS 2
#define sharedFileIdIdx 1

#define MDL_START
#define MDL_CHECK_PARAMETERS


static
void encode_cbor(
    SimStruct* S,
    CborEncoder* encoder,
    const void* inputPort,
    DTypeId elemType,
    int numInnerElems,
    int level
)
{
    CborEncoder arrayEncoder;
    cbor_encoder_create_array(encoder, &arrayEncoder, numInnerElems);

    int i;
    for (i = 0; i < numInnerElems; i++) {
        DTypeId subDtype = ssGetBusElementDataType(S, elemType, i);

        if (ssIsDataTypeABus(S, subDtype) == 1) {
            encode_cbor(S, &arrayEncoder, inputPort, subDtype, ssGetNumBusElements(S, subDtype), (level + 1));
        }
        else {
            switch (subDtype) {
                case SS_DOUBLE:
                    int_T offset = ssGetBusElementOffset(S, elemType, i);
                    /* Assume that the bus element is of type double */
                    const double *in = (const double*) ((const char*)inputPort + offset);
                    cbor_encode_double(&arrayEncoder, *in);
                    // printf("Level: %d | Elements %d | SS_DOUBLE %f\n", level,  numInnerElems, *in);
                    break;
            }
        }
    }

    cbor_encoder_close_container(encoder, &arrayEncoder);
}


static void
report_and_exit(const char* msg, SimStruct *S)
{
    ssSetErrorStatus(S, msg);
}


#if defined(MDL_CHECK_PARAMETERS) && defined(MATLAB_MEX_FILE)
static void
mdlCheckParameters(SimStruct *S)
{
    const char* shm_file_id;
    shm_file_id = mxArrayToString(ssGetSFcnParam(S, sharedFileIdIdx));
    shm_unlink(shm_file_id);

}
#else
#define mdlCheckParameters(S)
#endif


static void
mdlStart(SimStruct *S)
{
    int fd;
    size_t ring_bytes;
    char* shm_file_id;

    /* Start the shared memory */
    shm_file_id = mxArrayToString(ssGetSFcnParam(S, sharedFileIdIdx));
    ring_bytes = sizeof(RingBuffer);
    fd = shm_open(shm_file_id, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

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
    ssSetNumSFcnParams(S, NPRMS);
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        return; /* Parameter mismatch reported by the Simulink engine*/
    }

    if (!ssSetNumInputPorts(S, 1)) return;

    DTypeId id;
    ssRegisterTypeFromNamedObject(S, "TopBus", &id);

    if (id == INVALID_DTYPE_ID) {
        report_and_exit("Registration Did not Work", S);
    } else {
        ssSetInputPortDataType(S, 0, id);
        ssSetInputPortWidth(S, 0, 1);
        ssSetInputPortComplexSignal(S, 0, COMPLEX_NO);
        ssSetInputPortDirectFeedThrough(S, 0, 1);
        ssSetInputPortRequiredContiguous(S, 0, 1);
        ssSetBusInputAsStruct(S, 0, 1);
        ssSetInputPortBusMode(S, 0, SL_BUS_MODE);
    }


    mdlCheckParameters(S);
    if (ssGetErrorStatus(S) != NULL) return;

    /* Specify that none of the parameters are tunable */
    ssSetSFcnParamTunable(S, sharedFileIdIdx, false);

    if (!ssSetNumOutputPorts(S,0)) return;

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
    RingBuffer* ring_buffer = (RingBuffer*)ssGetPWorkValue(S, 0);

    const void* inputPort = ssGetInputPortSignal(S, 0);
    DTypeId dType = ssGetInputPortDataType(S, 0);
    int numElems = ssGetNumBusElements(S, dType);
    // printf("Bus with n: %d\n", numElems);
    //
    // int i;
    // for (i = 0; i < numElems; i++) {
    //     int_T elemType = ssGetBusElementDataType(S, dType, i);
    //
    //     if (ssIsDataTypeABus(S, elemType) == 1) {
    //         /* Sub-bus element */
    //         printf("Element %d is a sub-bus\n", i);
    //         printf("Sub bus with n: %d\n", ssGetNumBusElements(S, elemType));
    //     } else if (elemType == SS_DOUBLE) {
    //         printf("Element %d is a double\n", i);
    //     }
    // }

    // publish(ring_buffer, uPtrs, inp_width);
    CborEncoder encoder;
    uint8_t cborBuffer[100];

    cbor_encoder_init(&encoder, cborBuffer, sizeof(cborBuffer), 0);
    encode_cbor(S, &encoder, inputPort, dType, numElems, 0);
    publish(ring_buffer, cborBuffer, sizeof(cborBuffer));
}

static void
mdlTerminate(SimStruct *S)
{
    const char* shm_file_id;

    shm_file_id = mxArrayToString(ssGetSFcnParam(S, sharedFileIdIdx));
    shm_unlink(shm_file_id);
}

#ifdef MATLAB_MEX_FILE /* Is this file being compiled as a MEX-file? */
#include "simulink.c" /* MEX-file interface mechanism */
#else
#include "cg_sfun.h" /* Code generation registration function */
#endif
