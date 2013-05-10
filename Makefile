all: hfsj

clean:
	rm -f hfsj *.o

.PHONY: all clean

OPT = -g -O0
CFLAGS = -Wall
LIBS = -lguestfs

hfsj: hfsj.o
	$(CC) $(OPT) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(OPT) $(CFLAGS) -o $@ -c $<

