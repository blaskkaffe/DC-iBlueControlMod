# KallistiOS makefile for the brctl test program
TARGET = brctl_test.elf
OBJS = brctl_test.o ../brctl.o
KOS_CFLAGS += -I..

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)
