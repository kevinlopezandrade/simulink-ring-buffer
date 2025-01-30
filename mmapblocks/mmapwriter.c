#define S_FUNCTION_NAME mmapwriter
#define S_FUNCTION_LEVEL 2

#include <fcntl.h> /* For O_* constants */
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tinycbor/cbor.h>
#include <unistd.h>

#include "matrix.h"
#include "simstruc.h"
#include <ncctools/ringbuffer.h>

#define NPRMS 2
#define sharedFileIdIdx 1

#define MDL_START
#define MDL_CHECK_PARAMETERS

static void encode_cbor(SimStruct *S, CborEncoder *encoder,
                        const void *input_port, DTypeId dtype, int num_elems,
                        int level, int *counter, int32_t *input_sizes,
                        int_T nrows) {
  CborEncoder map_encoder;
  cbor_encoder_create_map(encoder, &map_encoder, num_elems);

  int i;
  for (i = 0; i < num_elems; i++) {
    DTypeId elem_type = ssGetBusElementDataType(S, dtype, i);
    int_T offset = ssGetBusElementOffset(S, dtype, i);

    if (ssIsDataTypeABus(S, elem_type) == 1) {

      cbor_encode_text_stringz(&map_encoder, ssGetBusElementName(S, dtype, i));

      encode_cbor(S, &map_encoder, input_port + offset, elem_type,
                  ssGetNumBusElements(S, elem_type), (level + 1), counter,
                  input_sizes, nrows);

    } else {

      // Array is 2D Dimensional Array with column major order.
      int element_dims[2];
      element_dims[0] = input_sizes[*counter];
      element_dims[1] = input_sizes[(1 * nrows) + (*counter)];

      *counter = *counter + 1;

      switch (elem_type) {
      case SS_DOUBLE:
        /* Assume that the bus element is of type double */
        const double *double_in =
            (const double *)((const char *)input_port + offset);

        cbor_encode_text_stringz(&map_encoder,
                                 ssGetBusElementName(S, dtype, i));

        CborEncoder array_encoder;
        cbor_encoder_create_array(&map_encoder, &array_encoder,
                                  element_dims[0]);

        int r, c;
        for (r = 0; r < element_dims[0]; r++) {
          CborEncoder row_encoder;
          cbor_encoder_create_array(&array_encoder, &row_encoder,
                                    element_dims[1]);
          for (c = 0; c < element_dims[1]; c++) {
            cbor_encode_double(&row_encoder,
                               double_in[(element_dims[0] * c) + r]);
          }
          cbor_encoder_close_container(&array_encoder, &row_encoder);
        }

        cbor_encoder_close_container(&map_encoder, &array_encoder);
        break;

      case SS_SINGLE:
        const float *float_in =
            (const float *)((const char *)input_port + offset);

        cbor_encode_text_stringz(&map_encoder,
                                 ssGetBusElementName(S, dtype, i));
        cbor_encode_float(&map_encoder, *float_in);
        break;
      case SS_INT8:
      case SS_INT16:
      case SS_INT32:
        /* CBOR Doest not provide a function to encode int8 or int16, so we cast
        it as a normal integer */
        const long int *int_in =
            (const long int *)((const char *)input_port + offset);

        cbor_encode_text_stringz(&map_encoder,
                                 ssGetBusElementName(S, dtype, i));
        cbor_encode_int(&map_encoder, *int_in);
        break;
      case SS_UINT8:
      case SS_UINT16:
      case SS_UINT32:
        /* CBOR Doest not provide a function to encode uint8 or uint16, so we
        cast it as a normal unsigned integer */
        const unsigned long int *uint_in =
            (const unsigned long int *)((const char *)input_port + offset);

        cbor_encode_text_stringz(&map_encoder,
                                 ssGetBusElementName(S, dtype, i));
        cbor_encode_uint(&map_encoder, *uint_in);
        break;
      default:
        const int *enum_value_in =
            (const int *)((const char *)input_port + offset);
        if (dtaGetDataTypeIsEnumType(ssGetDataTypeAccess(S), ssGetPath(S),
                                     elem_type) == 1) {

          /* printf("%s\n",
             dtaGetEnumTypeStringFromValue(ssGetDataTypeAccess(S), ssGetPath(S),
             elem_type, *enum_value_in)); */
          cbor_encode_text_stringz(&map_encoder,
                                   ssGetBusElementName(S, dtype, i));
          cbor_encode_int(&map_encoder, *enum_value_in);
        }
        break;
      }
    }
  }

  cbor_encoder_close_container(encoder, &map_encoder);
}

static void report_and_exit(const char *msg, SimStruct *S) {
  ssSetErrorStatus(S, msg);
}

#if defined(MDL_CHECK_PARAMETERS) && defined(MATLAB_MEX_FILE)
static void mdlCheckParameters(SimStruct *S) {
  const char *shm_file_id;
  shm_file_id = mxArrayToString(ssGetSFcnParam(S, sharedFileIdIdx));
  shm_unlink(shm_file_id);
}
#else
#define mdlCheckParameters(S)
#endif

static void mdlStart(SimStruct *S) {
  int fd;
  size_t ring_bytes;
  char *shm_file_id;

  /* Start the shared memory */
  shm_file_id = mxArrayToString(ssGetSFcnParam(S, sharedFileIdIdx));
  ring_bytes = sizeof(NCCToolsRingBuffer);
  fd = shm_open(shm_file_id, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd == -1) {
    report_and_exit("could not open shared memory", S);
  } else {
    /* Extend the 0 bytes file to number of bytes occupied by ringbuffer */
    if (ftruncate(fd, ring_bytes) == -1) {
      report_and_exit("could not truncate the shared memory", S);
    }

    /* Map it to external memory; */
    NCCToolsRingBuffer *ring_buffer = (NCCToolsRingBuffer *)mmap(
        NULL, ring_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    /* Initialize */
    ring_buffer->write_idx = 0;
    ring_buffer->oldest_idx = 0;
    ring_buffer->filled = false;
    ring_buffer->buf_size = BUFFER_SIZE;
    ring_buffer->cbor_buffer_size = CBOR_BUFFER_SIZE;

    if (ring_buffer == MAP_FAILED) {
      report_and_exit("could not mmap the ring buffer", S);
    }

    /* Its safe to close after */
    close(fd);

    /* Pass the pointer to the rest of the callbacks. */
    ssSetPWorkValue(S, 0, ring_buffer);

    /* Allocate memory for the cbor buffer. */
    uint8_t *cbor_buffer = (uint8_t *)malloc(ring_buffer->cbor_buffer_size);
    ssSetPWorkValue(S, 1, cbor_buffer);
  }
}

static void mdlInitializeSizes(SimStruct *S) {
  ssSetNumSFcnParams(S, NPRMS);
  if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
    return; /* Parameter mismatch reported by the Simulink engine*/
  }

  if (!ssSetNumInputPorts(S, 2))
    return;

  DTypeId id;
  ssRegisterTypeFromParameter(S, 0, &id);

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

  ssSetInputPortDirectFeedThrough(S, 1, 1);
  /* Input is a real signal */
  ssSetInputPortComplexSignal(S, 1, COMPLEX_NO);
  /* Input is a dynamically sized 2-D matrix */
  ssSetInputPortMatrixDimensions(S, 1, DYNAMICALLY_SIZED, DYNAMICALLY_SIZED);
  /* Input inherits its sample time */
  ssSetInputPortSampleTime(S, 1, INHERITED_SAMPLE_TIME);
  /* Input signal must be contiguous */
  ssSetInputPortRequiredContiguous(S, 1, 1);
  /* Dimensions are specified as int32 */
  ssSetInputPortDataType(S, 1, SS_INT32);

  mdlCheckParameters(S);
  if (ssGetErrorStatus(S) != NULL)
    return;

  /* Specify that none of the parameters are tunable */
  ssSetSFcnParamTunable(S, sharedFileIdIdx, false);

  if (!ssSetNumOutputPorts(S, 0))
    return;

  ssSetNumSampleTimes(S, 1);

  /* Take care when specifying exception free code - see sfuntmpl.doc */
  ssSetOptions(S, SS_OPTION_EXCEPTION_FREE_CODE);

  /* Allocate space for the pointer to shared memory and the cbor buffer */
  ssSetNumPWork(S, 2);
}

static void mdlInitializeSampleTimes(SimStruct *S) {
  ssSetSampleTime(S, 0, INHERITED_SAMPLE_TIME);
  ssSetOffsetTime(S, 0, 0.0);
}

static void mdlOutputs(SimStruct *S, int_T tid) {
  NCCToolsRingBuffer *ring_buffer = (NCCToolsRingBuffer *)ssGetPWorkValue(S, 0);
  uint8_t *cbor_buffer = (uint8_t *)ssGetPWorkValue(S, 1);

  const void *input_port = ssGetInputPortSignal(S, 0);
  DTypeId dtype = ssGetInputPortDataType(S, 0);
  int num_elems = ssGetNumBusElements(S, dtype);

  CborEncoder encoder;
  unsigned int cbor_buffer_size = ring_buffer->cbor_buffer_size;

  const void *input_sizes = ssGetInputPortSignal(S, 1);
  int_T *dims_flat_bus = ssGetInputPortDimensions(S, 1);
  int counter = 0;

  cbor_encoder_init(&encoder, cbor_buffer, cbor_buffer_size, 0);

  encode_cbor(S, &encoder, input_port, dtype, num_elems, 0, &counter,
              (int32_t *)input_sizes, dims_flat_bus[0]);

  ncctools_publish(ring_buffer, cbor_buffer, cbor_buffer_size);
}

static void mdlTerminate(SimStruct *S) {
  const char *shm_file_id;

  shm_file_id = mxArrayToString(ssGetSFcnParam(S, sharedFileIdIdx));
  shm_unlink(shm_file_id);
}

#ifdef MATLAB_MEX_FILE /* Is this file being compiled as a MEX-file? */
#include "simulink.c"  /* MEX-file interface mechanism */
#else
#include "cg_sfun.h" /* Code generation registration function */
#endif
