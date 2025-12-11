#ifndef GA_ENGINE_H
#define GA_ENGINE_H
#include <stdio.h> // Necessário para FILE*

// Estrutura de um indivíduo (genérica)
typedef struct {
    double* genes;
} Individual;

// Variáveis globais de controle do GA
extern int POPULATION_SIZE;
extern int MAX_GENERATIONS;
extern int NUM_DIMENSIONS;
extern double* GENE_MIN_VALUE;
extern double* GENE_MAX_VALUE;

// NOVO: Ponteiro de arquivo para exportação dos dados CSV
// Se for NULL, não imprime nada. Se definido, escreve o CSV nele.
extern FILE* ga_csv_file;

// Protótipos das funções do GA
void initialize_population();
void free_population();
Individual run_ga_cycle(double (*fitness_func)(Individual, const void*), const void* extra_param, int is_shape_opt);

#endif