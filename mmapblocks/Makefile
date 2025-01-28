LINK_OPTS = -lncctools-checksum -lncctools-ringbuffer -ltinycbor

all: mmapreader.mexa64 mmapwriter.mexa64

mmapreader.mexa64: mmapreader.c
	LANG=C mex mmapreader.c $(LINK_OPTS)

mmapwriter.mexa64: mmapwriter.c
	LANG=C mex mmapwriter.c $(LINK_OPTS)
