EXE	= test
CC	= g++
COPTS	= -fPIC -DLINUX -Wall
FLAGS	= -Wall
LIBS	= -l CAENVME -lc -lm
OBJS	= main.o vsdc4.o

#########################################################################

all: $(EXE)

clean:
	/bin/rm -f $(OBJS) $(EXE)

$(EXE):	$(OBJS)
	/bin/rm -f $(EXE)
	$(CC) $(FLAGS) -o $(EXE) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(COPTS) -c -o $@ $<

