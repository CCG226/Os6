CC	= gcc -g3
CFLAGS	= -g3 -Wall
TARGET1 = oss
TARGET2 = worker

OBJS1	= oss.o
OBJS2	= worker.o

all: $(TARGET1) $(TARGET2)

$(TARGET1):	$(OBJS1)
	$(CC) -o $(TARGET1) $(OBJS1)

$(TARGET2):	$(OBJS2)
	$(CC) -o $(TARGET2) $(OBJS2)

oss.o: oss.c oss.h
	$(CC) $(CFLAGS) -c oss.c oss.h

worker.o: worker.c worker.h
	$(CC) $(CFLAGS) -c worker.c worker.h

clean:
	/bin/rm -f *.o $(TARGET1) $(TARGET2)
