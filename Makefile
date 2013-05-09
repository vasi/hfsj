all: hfsj-write

clean:
	rm -f hfsj-write *.o

.PHONY: all clean

OPT = -g -O0
CFLAGS = -Wall
LIBS = -lguestfs

hfsj-write: hfsj-write.o
	$(CC) $(OPT) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(OPT) $(CFLAGS) -o $@ -c $<

