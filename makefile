     CC := gcc
CCFLAGS := -O3 -Wall #-fopenmp
DBFLAGS := -g
#DBFLAGS := 
LDFLAGS := -lm

TARGETS := gridCollisions
  MAINS := $(addsuffix .o, $(TARGETS))
    OBJ := AABB.o HashTable.o gridCollisions.o main.o
   DEPS := AABB.h Cells.h Grid.h HashTable.h
   LIBS := -lSDL2

all: $(TARGETS)

clean:
	rm -f $(TARGETS) $(OBJ)

# If any of the source files or headers are newer than the output files
$(OBJ): %.o : %.c $(DEPS)
	$(CC) -c -o $@ $< $(CCFLAGS) $(DBFLAGS)


$(TARGETS): % : $(filter-out $(MAINS), $(OBJ)) %.o
	$(CC) -o $@ $^ $(LIBS) $(CCFLAGS) $(DBFLAGS) $(LDFLAGS) 



