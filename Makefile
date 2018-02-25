all: start clean
start: main.o
	gcc main.o fake_mem.o -o defrag_tool
comp: main.c
	gcc main.c -c
clean: main.o
	rm main.o
