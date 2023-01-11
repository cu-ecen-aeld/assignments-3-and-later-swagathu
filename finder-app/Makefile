ifneq ($(CROSS_COMPILE),)
    CC_TEMP=${CROSS_COMPILE}gcc
else
    CC_TEMP=${CC}
endif

all:
	${CC_TEMP} -g -Wall -c -o writer.o writer.c
	${CC_TEMP} -g -Wall -I/ writer.o  -o writer

clean:
	rm -f *.o
	rm -f writer
