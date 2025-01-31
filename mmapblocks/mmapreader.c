#define S_FUNCTION_NAME mmapreader
#define S_FUNCTION_LEVEL 2
#include <fcntl.h>
#include <ncctools/checksum.h>
#include <ncctools/ringbuffer.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <tinycbor/cbor.h>

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "simstruc.h"

#define NPRMS 2           /* Numbers of parasm taken by the s-function block */
#define sharedFileIdIdx 0 /* Index of where to look for the sared file name */

#define MDL_START

static void indent(int nestingLevel) {
  while (nestingLevel--)
    printf("  ");
}

static void dumpbytes(const uint8_t *buf, size_t len) {
  printf("\"");
  while (len--)
    printf("\\x%02X", *buf++);
  printf("\"");
}

/* This function is taken from the tinycbor examples repo,
use it for debugging purposes, since it parses recursively
the cbor data structure and prints it */
static CborError dumprecursive(CborValue *it, int nestingLevel) {
  while (!cbor_value_at_end(it)) {
    CborError err;
    CborType type = cbor_value_get_type(it);

    indent(nestingLevel);
    switch (type) {
    case CborArrayType:
    case CborMapType: {
      // recursive type
      CborValue recursed;
      assert(cbor_value_is_container(it));
      printf(type == CborArrayType ? "Array[" : "Map[");
      err = cbor_value_enter_container(it, &recursed);
      if (err)
        return err; // parse error
      err = dumprecursive(&recursed, nestingLevel + 1);
      if (err)
        return err; // parse error
      err = cbor_value_leave_container(it, &recursed);
      if (err)
        return err; // parse error
      indent(nestingLevel);
      printf("]");
      continue;
    }

    case CborIntegerType: {
      int64_t val;
      cbor_value_get_int64(it, &val); // can't fail
      printf("%lld\n", (long long)val);
      break;
    }

    case CborByteStringType: {
      uint8_t *buf;
      size_t n;
      err = cbor_value_dup_byte_string(it, &buf, &n, it);
      if (err)
        return err; // parse error
      dumpbytes(buf, n);
      free(buf);
      continue;
    }

    case CborTextStringType: {
      char *buf;
      size_t n;
      err = cbor_value_dup_text_string(it, &buf, &n, it);
      if (err)
        return err; // parse error
      printf("\"%s\"\n", buf);
      free(buf);
      continue;
    }

    case CborTagType: {
      CborTag tag;
      cbor_value_get_tag(it, &tag); // can't fail
      printf("Tag(%lld)\n", (long long)tag);
      break;
    }

    case CborSimpleType: {
      uint8_t type;
      cbor_value_get_simple_type(it, &type); // can't fail
      printf("simple(%u)\n", type);
      break;
    }

    case CborNullType:
      printf("null");
      break;

    case CborUndefinedType:
      printf("undefined");
      break;

    case CborBooleanType: {
      bool val;
      cbor_value_get_boolean(it, &val); // can't fail
      printf(val ? "true" : "false");
      break;
    }

    case CborDoubleType: {
      double val;
      if (false) {
        float f;
      case CborFloatType:
        cbor_value_get_float(it, &f);
        val = f;
      } else {
        cbor_value_get_double(it, &val);
      }
      printf("%g\n", val);
      break;
    }
    case CborHalfFloatType: {
      uint16_t val;
      cbor_value_get_half_float(it, &val);
      printf("__f16(%04x)\n", val);
      break;
    }

    case CborInvalidType:
      assert(false); // can't happen
      break;
    }

    err = cbor_value_advance_fixed(it);
    if (err)
      return err;
  }
  return CborNoError;
}

static void decode_cbor(SimStruct *S, const void *output_port, DTypeId dtype,
                        int num_elems, CborValue *it) {
  /* Entering this subroutine implies you are a bus */
  assert(cbor_value_is_container(it));

  int i;
  for (i = 0; i < num_elems; i++) {
    DTypeId elem_type = ssGetBusElementDataType(S, dtype, i);
    const char *elem_name = ssGetBusElementName(S, dtype, i);
    int_T offset = ssGetBusElementOffset(S, dtype, i);

    CborValue value;
    cbor_value_map_find_value(it, elem_name, &value);

    if (!cbor_value_is_valid(&value)) {
      printf("Could not find the bus element");
      continue;
    }

    if (ssIsDataTypeABus(S, elem_type) == 1) {
      decode_cbor(S, output_port + offset, elem_type,
                  ssGetNumBusElements(S, elem_type), &value);
    } else {
      switch (elem_type) {
      case SS_DOUBLE:
        if (!cbor_value_is_container(&value)) {
          printf("Decoded value is not an 2-D Array.\n");
          continue;
        }

        size_t num_rows;
        size_t num_cols;
        CborValue row_it;

        cbor_value_get_array_length(&value, &num_rows);

        cbor_value_enter_container(&value, &row_it);
        assert(cbor_value_is_container(&row_it));
        cbor_value_get_array_length(&row_it, &num_cols);

        double *arr = (double *)((const char *)output_port + offset);

        int r;
        for (r = 0; r < num_rows; r++) {
          int c;
          CborValue col_it;
          for (c = 0; c < num_cols; c++) {
            cbor_value_enter_container(&row_it, &col_it);
            assert(cbor_value_is_container(&col_it));

            /* Matlab requires column major format */
            double val;
            cbor_value_get_double(&col_it, &val);
            arr[(c * num_cols) + r] = val;

            cbor_value_advance_fixed(&col_it);
          }

          cbor_value_advance(&row_it);
        }
        cbor_value_leave_container(&value, &row_it);
      }
    }
  }
}

static void report_and_exit(const char *msg, SimStruct *S) {
  ssSetErrorStatus(S, msg);
}

static void mdlStart(SimStruct *S) {
  int shm_fd;
  void *shm_ptr;
  struct stat shm_stat;
  char *shm_file_id;

  /* Open the shared memory object for reading */
  shm_file_id = mxArrayToString(ssGetSFcnParam(S, sharedFileIdIdx));
  shm_fd = shm_open(shm_file_id, O_RDONLY, S_IWUSR);

  if (shm_fd == -1) {
    report_and_exit("could not open shared memory", S);
  }

  /* Memory map the shared memory object for reading only */
  shm_ptr =
      mmap(NULL, sizeof(NCCToolsRingBuffer), PROT_READ, MAP_SHARED, shm_fd, 0);

  NCCToolsRingBuffer *ring_buffer = (NCCToolsRingBuffer *)shm_ptr;
  ssSetPWorkValue(S, 0, ring_buffer);

  NCCToolsReadToken read_token = {.idx = 0, .initialized = false};
  ssSetPWorkValue(S, 1, &read_token);
}

static void mdlInitializeSizes(SimStruct *S) {
  ssSetNumSFcnParams(S, NPRMS);
  if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
    return; /* Parameter mismatch reported by the Simulink engine*/
  }

  /* Specify that none of the parameters are tunable */
  ssSetSFcnParamTunable(S, sharedFileIdIdx, false);

  if (!ssSetNumInputPorts(S, 0))
    return;

  if (!ssSetNumOutputPorts(S, 1))
    return;
  ssSetOutputPortWidth(S, 0, DYNAMICALLY_SIZED);

  /* Only one sample time. */
  ssSetNumSampleTimes(S, 1);

  /* Take care when specifying exception free code - see sfuntmpl.doc */
  ssSetOptions(S, SS_OPTION_EXCEPTION_FREE_CODE);

  /* Allocate memory for the pointers to shared memory and the ReadToken. */
  ssSetNumPWork(S, 2);

  DTypeId id;
  ssRegisterTypeFromParameter(S, 1, &id);

  if (id == INVALID_DTYPE_ID) {
    report_and_exit("Registration Did not Work", S);
  } else {
    const char *dtypeName = ssGetDataTypeName(S, id);
    if (dtypeName == NULL)
      return;

    ssSetOutputPortDataType(S, 0, id);

    /* Avoid compiler warnings from using the const qualifier */
    ssSetBusOutputObjectName(S, 0, strdup(dtypeName));

    ssSetOutputPortWidth(S, 0, DYNAMICALLY_SIZED);
    ssSetOutputPortComplexSignal(S, 0, COMPLEX_NO);
    ssSetBusOutputAsStruct(S, 0, true);
    ssSetOutputPortBusMode(S, 0, SL_BUS_MODE);
  }
}

static void mdlInitializeSampleTimes(SimStruct *S) {
  ssSetSampleTime(S, 0, INHERITED_SAMPLE_TIME);
  ssSetOffsetTime(S, 0, 0.0);
}

static void mdlOutputs(SimStruct *S, int_T tid) {
  NCCToolsRingBuffer *ring_buffer;
  NCCToolsReadToken *read_token;
  NCCToolsMessage msg;
  CborValue it;
  CborParser parser;

  ring_buffer = (NCCToolsRingBuffer *)ssGetPWorkValue(S, 0);
  read_token = (NCCToolsReadToken *)ssGetPWorkValue(S, 1);

  void *output_port = ssGetOutputPortSignal(S, 0);
  DTypeId dtype = ssGetOutputPortDataType(S, 0);
  int num_elems = ssGetNumBusElements(S, dtype);

  msg = ncctools_read_next(read_token, ring_buffer);

  if (!msg.wait) {
    if (ncctools_crc((unsigned char *)&msg.data, CBOR_BUFFER_SIZE) !=
        msg.checksum) {
      printf("Checksum doest not match");
      return;
    }
    cbor_parser_init(msg.data, CBOR_BUFFER_SIZE, 0, &parser, &it);
    decode_cbor(S, output_port, dtype, num_elems, &it);
  } else {
    printf("Waiting\n");
  }
}

static void mdlTerminate(SimStruct *S) {}

#ifdef MATLAB_MEX_FILE /* Is this file being compiled as a MEX-file? */
#include "simulink.c"  /* MEX-file interface mechanism */
#else
#include "cg_sfun.h" /* Code generation registration function */
#endif
