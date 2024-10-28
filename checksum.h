#ifndef CHECKSUM_H
#define CHECKSUM_H

unsigned int
crc32(unsigned char *message);

unsigned long
crc(unsigned char *buf, int len);

#endif
