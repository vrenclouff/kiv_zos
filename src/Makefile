
program=pseudo_fat
CC=gcc
OBJ=pseudo_fat.o fat_rw.c
OPT=-O2

${program}: ${OBJ}
	${CC} -pthread ${OBJ} -o ${program} ${OPT} -lm -g

fat_read.o: fat_read.c
	${CC} fat_rw.c -c ${OPT} -lm

main.o: pseudo_fat.c
	${CC}  pseudo_fat.c -c ${OPT} -lm
