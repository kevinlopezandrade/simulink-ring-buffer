#include "checksum.h"

/* crc32 Implementation taken from excellent lecture notes from
KU Leuven University:
https://perswww.kuleuven.be/~u0068190/Onderwijs/Extra_info/crc.pdf

A more detailed explanation of the look-up table algorithm
can be found in these excellent lecture notes from
Ross N. Williams:
http://www.ross.net/crc/download/crc_v3.txt

crc Implementation take from the rfc:
https://www.rfc-editor.org/rfc/rfc1952#section-8 */

unsigned int
crc32(unsigned char *message)
{
    int i, j;
    unsigned int byte, crc, mask;
    static unsigned int table[256];

    /* Set up the table, if necessary. */
    if (table[1] == 0) {
        for (byte = 0; byte <= 255; byte++) {
            crc = byte;
            for (j = 7; j >= 0; j--) { // Do eight times.
                mask = -(crc & 1);
                crc = (crc >> 1) ^ (0xEDB88320 & mask);
            }
            table[byte] = crc;
        }
    }

    /* Through with table setup, now calculate the CRC. */
    i = 0;
    crc = 0xFFFFFFFF;
    while ((byte = message[i]) != 0) {
        crc = (crc >> 8) ^ table[(crc ^ byte) & 0xFF];
        i = i + 1;
    }
    return ~crc;
}



/* Table of CRCs of all 8-bit messages. */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* Make the table for a fast CRC. */
void make_crc_table(void)
{
    unsigned long c;
    int n, k;
    for (n = 0; n < 256; n++) {
      c = (unsigned long) n;
      for (k = 0; k < 8; k++) {
        if (c & 1) {
          c = 0xedb88320L ^ (c >> 1);
        } else {
          c = c >> 1;
        }
      }
      crc_table[n] = c;
    }
    crc_table_computed = 1;
}

/*
 Update a running crc with the bytes buf[0..len-1] and return
the updated crc. The crc should be initialized to zero. Pre- and
post-conditioning (one's complement) is performed within this
function so it shouldn't be done by the caller. Usage example:

 unsigned long crc = 0L;

 while (read_buffer(buffer, length) != EOF) {
   crc = update_crc(crc, buffer, length);
 }
 if (crc != original_crc) error();
*/
unsigned long update_crc(unsigned long crc,
              unsigned char *buf, int len)
{
    unsigned long c = crc ^ 0xffffffffL;
    int n;

    if (!crc_table_computed)
      make_crc_table();
    for (n = 0; n < len; n++) {
      c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c ^ 0xffffffffL;
}

/* Return the CRC of the bytes buf[0..len-1]. */
unsigned long crc(unsigned char *buf, int len)
{
    return update_crc(0L, buf, len);
}
