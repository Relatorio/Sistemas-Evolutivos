#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ga_engine.h"

// =============================================================================
// VARIÁVEIS GLOBAIS E CONFIGURAÇÕES
// =============================================================================
Individual* population = NULL;
double* fitness = NULL;
int POPULATION_SIZE;
int MAX_GENERATIONS;
int NUM_DIMENSIONS;
double* GENE_MIN_VALUE = NULL;
double* GENE_MAX_VALUE = NULL;

FILE* ga_csv_file = NULL;

// --- PARÂMETROS BIOLÓGICOS E ADAPTATIVOS ---
#define MUTATION_PROB_INITIAL 5.0   // Começa com 5% de chance de mutar
#define MUTATION_PROB_MAX 25.0      // Se chegar a 25%, ativa REPULSÃO
#define MUTATION_PROB_MIN 0.1       // Mínimo para refinamento fino

// Severidade: Quando a mutação ocorre, o gene muda em até 15% do seu range total
#define MUTATION_SEVERITY 15.0      

// Controle de Estagnação
#define STAGNATION_LIMIT 20           // Gerações sem melhora para aumentar a mutação
#define CONVERGENCE_BUFFER 10          // Gerações sustentadas de melhora para reduzir a mutação
#define GENETIC_DIVERSITY_THRESHOLD 1.5 // Se diversidade < isso, estamos convergindo
#define REPULSION_BASE_FACTOR 0.5     // Força da repulsão
#define RESET_AFTER_REPULSION_GENS 20 // Tempo tolerado em modo repulsão antes do Reset
#define RESET_PERCENTAGE 0.50         // Mata 50% da população no Reset
#define MODE_ATTRACTION 0
#define MODE_REPULSION 1
#define PI 3.1415926535

// =============================================================================
// FUNÇÕES AUXILIARES (Matemática e Memória)
// =============================================================================

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
    
    // 1. Acha o centro da população
    for (int j = 0; j < NUM_DIMENSIONS; j++) {
        double sum = 0.0;
        for (int i = 0; i < POPULATION_SIZE; i++) sum += population[i].genes[j];
        centroid.genes[j] = sum / POPULATION_SIZE;
    }
    
    // 2. Calcula média das distâncias até o centro
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
        fitness[i] = -1e300; 
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

// =============================================================================
// MOTOR PRINCIPAL (GA CYCLE)
// =============================================================================
Individual run_ga_cycle(double (*fitness_func)(Individual, const void*), const void* extra_param, int is_shape_opt) {
    
    initialize_population();
    
    // Variáveis de Estado
    double mutation_prob = MUTATION_PROB_INITIAL; // Probabilidade (%)
    double baseline_mutation = MUTATION_PROB_INITIAL;
    
    int stagnation_counter = 0;
    int convergence_counter = 0; // Buffer para redução suave
    int repulsion_mode_counter = 0;
    int crossover_mode = MODE_ATTRACTION;
    int post_reset_cnt = 0; // Contador de proteção pós-reset
    
    Individual prev_best = {NULL};

    // Cabeçalho CSV
    if (ga_csv_file != NULL) {
        fprintf(ga_csv_file, "Geracao,MelhorFitness,FitnessMedio,DesvioPadraoFit,DiversidadeGenetica,TaxaMutacao,FatorRepulsao,Evento\n");
    }

    for (int gen = 0; gen < MAX_GENERATIONS; gen++) {
        double total_fitness = 0.0;
        double max_fit = -1e300;
        int best_idx = 0;
        int valid = 0;
        char event_str[30] = "-"; // String para logar eventos

        // ---------------------------------------------------------
        // 1. AVALIAÇÃO DA POPULAÇÃO
        // ---------------------------------------------------------
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
        
        // Estatísticas Básicas
        double avg_fit = (valid > 0) ? total_fitness/valid : 0.0;
        double variance_fit = 0.0;
        if (valid > 0) {
            for (int i = 0; i < POPULATION_SIZE; i++) {
                if (fitness[i] > -1e200) variance_fit += pow(fitness[i] - avg_fit, 2.0);
            }
            variance_fit /= valid;
        }
        double std_dev_fit = sqrt(variance_fit);

        // Verifica Melhora (Elitismo Global)
        Individual curr_best = clone_individual(&population[best_idx]);
        int improved = 0;
        if (prev_best.genes && max_fit > -1e200) {
             double prev_f = fitness_func(prev_best, extra_param);
             // Considera melhora se fitness aumentou E genes mudaram significativamente
             if (max_fit > prev_f + 1e-9 && !are_individuals_equal(curr_best, prev_best)) improved = 1;
        } else if (max_fit > -1e200) improved = 1;

        // ---------------------------------------------------------
        // 2. LÓGICA ADAPTATIVA (O Cérebro do Algoritmo)
        // ---------------------------------------------------------
        
        if (post_reset_cnt > 0) {
            // Proteção Pós-Reset: Alta mutação para misturar genes
            post_reset_cnt--; 
            mutation_prob = baseline_mutation * 2.0; 
            crossover_mode = MODE_ATTRACTION;
            strcpy(event_str, "POS-RESET");
        } else {
            if (improved) {
                // SUCESSO: Algoritmo está avançando
                stagnation_counter = 0; 
                repulsion_mode_counter = 0; 
                crossover_mode = MODE_ATTRACTION;
                
                // Controle pela Dispersão com Buffer
                if (calculate_genetic_diversity() < GENETIC_DIVERSITY_THRESHOLD) {
                    convergence_counter++; // Acumula consistência
                    
                    if (convergence_counter >= CONVERGENCE_BUFFER) {
                        mutation_prob /= 1.5; // Redução suave 
                        convergence_counter = 0; // Reseta buffer
                    }
                } else {
                    mutation_prob = baseline_mutation; // Mantém ritmo normal
                    convergence_counter = 0;
                }

            } else {
                // FRACASSO: Algoritmo travou
                convergence_counter = 0; // Perdeu a consistência da convergência
                stagnation_counter++;
                
                if (stagnation_counter >= STAGNATION_LIMIT) {
                    // Começa a aumentar a chance de mutação
                    mutation_prob *= 1.5; 
                    
                    // Se a chance de mutação bateu no teto (25%) -> Ativa REPULSÃO
                    if (mutation_prob >= MUTATION_PROB_MAX) {
                        mutation_prob = MUTATION_PROB_MAX; // Trava em 25%
                        crossover_mode = MODE_REPULSION;
                        repulsion_mode_counter++;
                        strcpy(event_str, "REPULSAO");

                        // Se Repulsão falhou por muito tempo -> RESET (PREDAÇÃO)
                        if (repulsion_mode_counter >= RESET_AFTER_REPULSION_GENS) {
                            strcpy(event_str, "RESET-HIBRIDO");
                            
                            int reset_cnt = (int)(POPULATION_SIZE * RESET_PERCENTAGE); // 50%
                            int survivor_count = POPULATION_SIZE - reset_cnt; 
                            int current_fill_idx = survivor_count; 

                            // --- 1. O FRANKENSTEIN ---
                            if (current_fill_idx < POPULATION_SIZE) {
                                for (int d = 0; d < NUM_DIMENSIONS; d++) {
                                    int random_parent = rand() % survivor_count; 
                                    population[current_fill_idx].genes[d] = population[random_parent].genes[d];
                                }
                                fitness[current_fill_idx] = -1e300; 
                                current_fill_idx++; 
                            }
                            
                            // --- 2. O EDA (Estimation of Distribution) ---
                            if (current_fill_idx < POPULATION_SIZE) {
                                for (int d = 0; d < NUM_DIMENSIONS; d++) {
                                    // Estatísticas da Elite
                                    double sum = 0.0, sum_sq = 0.0;
                                    for(int k = 0; k < survivor_count; k++) {
                                        double val = population[k].genes[d];
                                        sum += val; sum_sq += val * val;
                                    }
                                    double mean = sum / survivor_count;
                                    double variance = (sum_sq / survivor_count) - (mean * mean);
                                    double std_dev = (variance > 0) ? sqrt(variance) : 0.0;
                                    
                                    // Box-Muller (Gaussiana)
                                    double u1 = ((double)rand() / RAND_MAX);
                                    double u2 = ((double)rand() / RAND_MAX);
                                    if(u1 < 1e-9) u1 = 1e-9;
                                    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
                                    double new_gene = mean + z0 * std_dev;
                                    
                                    // Clamps
                                    if(new_gene > GENE_MAX_VALUE[d]) new_gene = GENE_MAX_VALUE[d];
                                    if(new_gene < GENE_MIN_VALUE[d]) new_gene = GENE_MIN_VALUE[d];
                                    
                                    population[current_fill_idx].genes[d] = new_gene;
                                }
                                fitness[current_fill_idx] = -1e300; 
                                current_fill_idx++;
                            }

                            // --- 3. ALEATÓRIOS PUROS ---
                            for(int k = current_fill_idx; k < POPULATION_SIZE; k++) {
                                for(int d = 0; d < NUM_DIMENSIONS; d++) 
                                    population[k].genes[d] = GENE_MIN_VALUE[d] + ((double)rand()/RAND_MAX)*(GENE_MAX_VALUE[d]-GENE_MIN_VALUE[d]);
                                fitness[k] = -1e300;
                            }
                            
                            // Reseta contadores
                            post_reset_cnt = 30; 
                            repulsion_mode_counter = 0; 
                            stagnation_counter = 0;
                            mutation_prob = baseline_mutation; // Volta a chance normal
                        }
                    }
                }
            }
        }
        
        // Travas de segurança da probabilidade
        if(mutation_prob < MUTATION_PROB_MIN) mutation_prob = MUTATION_PROB_MIN;
        if(mutation_prob > MUTATION_PROB_MAX) mutation_prob = MUTATION_PROB_MAX;

        // Atualiza o melhor global
        if (max_fit > -1e200) {
            if (prev_best.genes) free(prev_best.genes);
            prev_best = clone_individual(&curr_best);
        }
        free(curr_best.genes);

        // CSV Log
        double rep_fact = (crossover_mode == MODE_REPULSION) ? REPULSION_BASE_FACTOR * (1 + repulsion_mode_counter/(double)STAGNATION_LIMIT) : 0;
        double current_div = calculate_genetic_diversity();
        if (ga_csv_file != NULL) {
            fprintf(ga_csv_file, "%d,%.5f,%.5f,%.5f,%.5f,%.2f,%.2f,%s\n", 
                gen+1, max_fit > -1e200 ? max_fit : 0, avg_fit, std_dev_fit, current_div, mutation_prob, rep_fact, event_str);
        }

        // Progresso no Terminal
        if (gen % (MAX_GENERATIONS/20) == 0) {
            printf(" [GA] Progresso: %3d%% (Melhor Fit: %.2f) | Mut(Chance): %.1f%%\r", (gen*100)/MAX_GENERATIONS, max_fit, mutation_prob);
            fflush(stdout);
        }

        // ---------------------------------------------------------
        // 3. EVOLUÇÃO (CROSSOVER + MUTAÇÃO BIOLÓGICA)
        // ---------------------------------------------------------
        Individual* new_pop = (Individual*)malloc(sizeof(Individual)*POPULATION_SIZE);
        Individual elite = clone_individual(&population[best_idx]);
        if(max_fit < -1e200) { for(int d=0; d<NUM_DIMENSIONS; d++) elite.genes[d] = GENE_MIN_VALUE[d]; }
        new_pop[0] = elite; // Elitismo
        
        for(int i=1; i<POPULATION_SIZE; i++) {
            new_pop[i].genes = (double*)malloc(sizeof(double)*NUM_DIMENSIONS);
            for(int j=0; j<NUM_DIMENSIONS; j++) {
                
                // A. CROSSOVER (Atração ou Repulsão)
                double base_gene;
                if(crossover_mode == MODE_ATTRACTION)
                    base_gene = (elite.genes[j] + population[i].genes[j]) / 2.0;
                else
                    base_gene = population[i].genes[j] + rep_fact * (population[i].genes[j] - elite.genes[j]);
                
                new_pop[i].genes[j] = base_gene;

                // B. MUTAÇÃO BIOLÓGICA (Probabilística)
                double chance_roll = (double)rand() / RAND_MAX * 100.0;
                
                if (chance_roll < mutation_prob) {
                    // A MUTAÇÃO OCORRE
                    double range = GENE_MAX_VALUE[j] - GENE_MIN_VALUE[j];
                    double change = ((double)rand()/RAND_MAX - 0.5) * (range * MUTATION_SEVERITY / 100.0);
                    new_pop[i].genes[j] += change;
                }
                
                // C. CLAMPS (Travas de Segurança Físicas)
                if(new_pop[i].genes[j] > GENE_MAX_VALUE[j]) new_pop[i].genes[j] = GENE_MAX_VALUE[j];
                if(new_pop[i].genes[j] < GENE_MIN_VALUE[j]) new_pop[i].genes[j] = GENE_MIN_VALUE[j];
            }
        }
        free_population();
        population = new_pop;
        fitness = (double*)malloc(sizeof(double)*POPULATION_SIZE);
    }
    printf("\n"); 

    int best = 0;
    for(int i=1; i<POPULATION_SIZE; i++) 
        if(fitness[i] > fitness[best]) best = i;
    
    Individual final_res = clone_individual(&population[best]);
    if(prev_best.genes) free(prev_best.genes);
    free_population();
    return final_res;
}