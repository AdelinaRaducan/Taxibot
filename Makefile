
#OBJS specifies which files to compile as part of the project
OBJS = main.c 

#CC specifies which compiler we're using
CC = gcc

#COMPILER_FLAGS specifies the additional compilation options we're using
# -w suppresses all warnings
COMPILER_FLAGS = -w -Wall -I [licenta]

#LINKER_FLAGS specifies the libraries we're linking against
CFLAGS = libSDL2.a libSDL2main.a -lSDL2_ttf -lSDL2_image -lSDL2_gfx -lm -lpthread -Wl,--no-as-needed -ldl 

#OBJ_NAME specifies the name of our exectuable
EXEC = main

#This is the target that compiles our executable
build: $(OBJS)
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(CFLAGS) -o $(EXEC)

run:
	./main

clean:
	rm -f $(EXEC)