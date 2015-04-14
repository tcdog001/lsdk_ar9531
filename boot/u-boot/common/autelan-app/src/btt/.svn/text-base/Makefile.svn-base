
OBJS=btt.o
__TARGET=btt
TARGET=$(__TARGET)
LIBS_DEPEND=-lubacktrace -lautelan-appkey

.PHONY:all
all:$(TARGET)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)

$(TARGET):$(OBJS)
	${CC} ${CFLAGS} ${LDFLAGS} $(LIBS_DEPEND) -o $(TARGET) $(OBJS)
	echo $(OBJS) > $(FILENO_PATH)/$(__TARGET).fileno
%.o:%.c
	${CC} -c ${CFLAGS} -D__THIS_FILE=$(shell $(FILENO_BIN) $@ $(OBJS)) $< -o $@