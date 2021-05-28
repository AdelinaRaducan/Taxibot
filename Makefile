# -*- MAKEFILE -*-

#OBJS specifies which files to compile as part of the project
OBJS = main.c 

#CC specifies which compiler we're using
CC = gcc

#COMPILER_FLAGS specifies the additional compilation options we're using
COMPILER_FLAGS = -w -Wall -I [licenta]

#LINKER_FLAGS specifies the libraries we're linking against
CFLAGS = libSDL2.a libSDL2main.a -lSDL2_ttf -lSDL2_image -lSDL2_gfx -lm -lpthread -Wl,--no-as-needed -ldl 

#OBJ_NAME specifies the name of our exectuable
EXEC = main

run: build
	./$(EXEC)

build: 
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(CFLAGS) -o $(EXEC)

build_gdb:
	$(CC) -g $(OBJS) $(COMPILER_FLAGS) $(CFLAGS) -o $(EXEC)

clean:
	rm -f $(EXEC)



