all: aesdsocket

% : %.c
	${CC} -o $@  $<
.PHONY = default
default: aesdsocket.c
	${CC} -o aesdsocket $<
clean:
	$(eval RET=$(shell find . -mindepth 1 -maxdepth 1 -executable -name "*"))
	rm -f ${RET}
	$(eval RET=$(shell find . -mindepth 1 -maxdepth 1 -name "*.o"))
	rm -f ${RET}
