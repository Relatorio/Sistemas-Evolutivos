CC = gcc
CFLAGS = -Wall -O2
LIBS = -lm

# Lista de objetos
OBJS = main.o ga_engine.o physics.o reports.o

# Regra principal
ProjetoSolar: $(OBJS)
	$(CC) -o ProjetoSolar $(OBJS) $(LIBS)

# Regras de compilação individuais
main.o: main.c ga_engine.h physics.h reports.h
	$(CC) $(CFLAGS) -c main.c

ga_engine.o: ga_engine.c ga_engine.h
	$(CC) $(CFLAGS) -c ga_engine.c

physics.o: physics.c physics.h ga_engine.h
	$(CC) $(CFLAGS) -c physics.c

reports.o: reports.c reports.h physics.h ga_engine.h
	$(CC) $(CFLAGS) -c reports.c

# Limpeza
clean:
	rm -f *.o ProjetoSolar