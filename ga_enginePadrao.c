#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ga_engine.h"

// Definição das variáveis globais
Individual* population = NULL;
double* fitness = NULL;
int POPULATION_SIZE;
int MAX_GENERATIONS;
int NUM_DIMENSIONS;
double* GENE_MIN_VALUE = NULL;
double* GENE_MAX_VALUE = NULL;

// Parâmetros internos do AG
#define MUTATION_INITIAL 5.0
#define MUTATION_MAX 25.0
#define MUTATION_MIN 0.1
#define STAGNATION_LIMIT 50
#define GENETIC_DIVERSITY_THRESHOLD 1.5
#define REPULSION_BASE_FACTOR 0.5
#define RESET_AFTER_REPULSION_GENS 20
#define RESET_PERCENTAGE 0.50
#define MODE_ATTRACTION 0
#define MODE_REPULSION 1

// Funções auxiliares internas
int are_individuals_equal(Individual a, Individual b) {
    if (a.genes == NULL || b.genes == NULL) return 0;
    for (int i = 0; i < NUM_DIMENSIONS; i++) {
        if (fabs(a.genes[i] - b.genes[i]) > 1e-9) return 0;
    }
    return 1;
}

double calculate_genetic_diversity() {
    if (population == NULL || POPULATION_SIZE == 0) return 0.0;
    Individual centroid;
    centroid.genes = (double*)malloc(sizeof(double) * NUM_DIMENSIONS);
    for (int j = 0; j < NUM_DIMENSIONS; j++) {
        double sum = 0.0;
        for (int i = 0; i < POPULATION_SIZE; i++) sum += population[i].genes[j];
        centroid.genes[j] = sum / POPULATION_SIZE;
    }
    double total_distance = 0.0;
    for (int i = 0; i < POPULATION_SIZE; i++) {
        double sq_dist = 0.0;
        for (int j = 0; j < NUM_DIMENSIONS; j++) sq_dist += pow(population[i].genes[j] - centroid.genes[j], 2.0);
        total_distance += sqrt(sq_dist);
    }
    free(centroid.genes);
    return total_distance / POPULATION_SIZE;
}

Individual clone_individual(const Individual* src) {
    Individual dest;
    dest.genes = (double*)malloc(sizeof(double) * NUM_DIMENSIONS);
    memcpy(dest.genes, src->genes, sizeof(double) * NUM_DIMENSIONS);
    return dest;
}

void initialize_population() {
    if (population) free_population();
    population = (Individual*)malloc(sizeof(Individual) * POPULATION_SIZE);
    fitness = (double*)malloc(sizeof(double) * POPULATION_SIZE);
    for (int i = 0; i < POPULATION_SIZE; i++) {
        population[i].genes = (double*)malloc(sizeof(double) * NUM_DIMENSIONS);
        fitness[i] = -1e300; // DBL_MAX substitute
        for (int j = 0; j < NUM_DIMENSIONS; j++) {
            double range = (GENE_MAX_VALUE[j] - GENE_MIN_VALUE[j]);
            if (range < 1e-9) range = 1e-9;
            population[i].genes[j] = GENE_MIN_VALUE[j] + ((double)rand()/RAND_MAX) * range;
        }
    }
}

void free_population() {
    if (population) {
        for (int i = 0; i < POPULATION_SIZE; i++) free(population[i].genes);
        free(population); population = NULL;
    }
    if (fitness) { free(fitness); fitness = NULL; }
}

// Motor Principal
Individual run_ga_cycle(double (*fitness_func)(Individual, const void*), const void* extra_param, int is_shape_opt) {
    // Alocação de limites deve ser feita antes de chamar esta função no main,
    // ou configurada aqui se for padronizada.
    // Assumindo que GENE_MIN/MAX já foram setados no main antes do call.
    
    initialize_population();
    
    double mutation_rate = MUTATION_INITIAL;
    double baseline_mutation = MUTATION_INITIAL;
    int stagnation_counter = 0;
    int repulsion_mode_counter = 0;
    int crossover_mode = MODE_ATTRACTION;
    int post_reset_cnt = 0;
    Individual prev_best = {NULL};

    printf("Geracao,MelhorFitness,FitnessMedio,TaxaMutacao\n");

    for (int gen = 0; gen < MAX_GENERATIONS; gen++) {
        double total_fitness = 0.0;
        double max_fit = -1e300;
        int best_idx = 0;
        int valid = 0;

        // Avaliação
        for (int i = 0; i < POPULATION_SIZE; i++) {
            double f = fitness_func(population[i], extra_param);
            if (f > -1e200) {
                fitness[i] = f;
                total_fitness += f;
                if (f > max_fit) { max_fit = f; best_idx = i; }
                valid++;
            } else {
                fitness[i] = -1e300;
            }
        }
        double avg_fit = (valid > 0) ? total_fitness/valid : 0.0;

        Individual curr_best = clone_individual(&population[best_idx]);
        int improved = 0;
        if (prev_best.genes && max_fit > -1e200) {
             double prev_f = fitness_func(prev_best, extra_param);
             if (max_fit > prev_f + 1e-9 && !are_individuals_equal(curr_best, prev_best)) improved = 1;
        } else if (max_fit > -1e200) improved = 1;

        if (post_reset_cnt > 0) {
            post_reset_cnt--; mutation_rate = baseline_mutation * 3.0; crossover_mode = MODE_ATTRACTION;
        } else {
            if (improved) {
                stagnation_counter = 0; repulsion_mode_counter = 0; crossover_mode = MODE_ATTRACTION;
                if (calculate_genetic_diversity() < GENETIC_DIVERSITY_THRESHOLD) mutation_rate /= 1.1;
                else mutation_rate = baseline_mutation;
            } else {
                stagnation_counter++;
                if (stagnation_counter >= STAGNATION_LIMIT) {
                    if (mutation_rate < MUTATION_MAX) { mutation_rate *= 1.2; }
                    else {
                        crossover_mode = MODE_REPULSION; repulsion_mode_counter++;
                        if (repulsion_mode_counter >= RESET_AFTER_REPULSION_GENS) {
                            printf("# RESET (Gen %d)\n", gen);
                            // Lógica de Reset Parcial
                            int reset_cnt = (int)(POPULATION_SIZE * RESET_PERCENTAGE);
                            for(int k=POPULATION_SIZE-reset_cnt; k<POPULATION_SIZE; k++) {
                                for(int d=0; d<NUM_DIMENSIONS; d++) 
                                    population[k].genes[d] = GENE_MIN_VALUE[d] + ((double)rand()/RAND_MAX)*(GENE_MAX_VALUE[d]-GENE_MIN_VALUE[d]);
                                fitness[k] = -1e300;
                            }
                            post_reset_cnt = 30; repulsion_mode_counter=0; stagnation_counter=0;
                        }
                    }
                }
            }
        }
        if(mutation_rate < MUTATION_MIN) mutation_rate = MUTATION_MIN;
        if(mutation_rate > MUTATION_MAX) mutation_rate = MUTATION_MAX;

        if (max_fit > -1e200) {
            if (prev_best.genes) free(prev_best.genes);
            prev_best = clone_individual(&curr_best);
        }
        free(curr_best.genes);

        if (gen % (MAX_GENERATIONS/10) == 0 || gen == MAX_GENERATIONS-1) {
            printf("%d,%.5f,%.5f,%.2f\n", gen+1, max_fit > -1e200 ? max_fit : 0, avg_fit, mutation_rate);
        }

        // Evolução
        Individual* new_pop = (Individual*)malloc(sizeof(Individual)*POPULATION_SIZE);
        Individual elite = clone_individual(&population[best_idx]);
        if(max_fit < -1e200) { // Fallback se tudo falhar
             for(int d=0; d<NUM_DIMENSIONS; d++) elite.genes[d] = GENE_MIN_VALUE[d];
        }
        new_pop[0] = elite;

        double rep_fact = (crossover_mode == MODE_REPULSION) ? REPULSION_BASE_FACTOR * (1 + repulsion_mode_counter/(double)STAGNATION_LIMIT) : 0;

        for(int i=1; i<POPULATION_SIZE; i++) {
            new_pop[i].genes = (double*)malloc(sizeof(double)*NUM_DIMENSIONS);
            for(int j=0; j<NUM_DIMENSIONS; j++) {
                if(crossover_mode == MODE_ATTRACTION)
                    new_pop[i].genes[j] = (elite.genes[j] + population[i].genes[j]) / 2.0;
                else
                    new_pop[i].genes[j] = population[i].genes[j] + rep_fact * (population[i].genes[j] - elite.genes[j]);
                
                double range = GENE_MAX_VALUE[j] - GENE_MIN_VALUE[j];
                new_pop[i].genes[j] += ((double)rand()/RAND_MAX - 0.5) * (range * mutation_rate/100.0);
                
                if(new_pop[i].genes[j] > GENE_MAX_VALUE[j]) new_pop[i].genes[j] = GENE_MAX_VALUE[j];
                if(new_pop[i].genes[j] < GENE_MIN_VALUE[j]) new_pop[i].genes[j] = GENE_MIN_VALUE[j];
            }
        }
        free_population();
        population = new_pop;
        fitness = (double*)malloc(sizeof(double)*POPULATION_SIZE);
    }

    int best = 0;
    for(int i=1; i<POPULATION_SIZE; i++) 
        if(fitness[i] > fitness[best]) best = i;
    
    Individual final_res = clone_individual(&population[best]);
    if(prev_best.genes) free(prev_best.genes);
    free_population();
    return final_res;
}