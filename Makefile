CC = gcc
CFLAGS = -I. -lpthread 
DEPS = llini.h llog.h 
OBJ = llog.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

llog_test: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	
.PHONY: clean

clean:
	rm -f *.o *~ llog_test