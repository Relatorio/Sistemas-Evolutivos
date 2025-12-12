#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ga_engine.h"

/**
 * @file ga_engine.c
 * @brief Implementação do Motor Evolutivo (O Cérebro do Projeto).
 * * Este arquivo contém toda a lógica do Algoritmo Genético Adaptativo.
 * Diferente de um AG simples, este motor possui "estados de humor":
 * 1. Atração: Converge para a melhor solução.
 * 2. Repulsão: Se afasta da melhor solução se ficar preso (estagnação).
 * 3. Reset (Apocalipse): Se nada funcionar, mata 50% da população e recria
 * usando estatística (EDA) e pedaços dos sobreviventes (Frankenstein).
 */

// ============================================================================
// VARIÁVEIS GLOBAIS (Alocadas dinamicamente)
// ============================================================================
Individual* population = NULL;
double* fitness = NULL;

// Parâmetros configurados externamente (pelo main.c)
int POPULATION_SIZE;
int MAX_GENERATIONS;
int NUM_DIMENSIONS;
double* GENE_MIN_VALUE = NULL;
double* GENE_MAX_VALUE = NULL;

// Inicializa o ponteiro de arquivo como NULL (Segurança)
FILE* ga_csv_file = NULL;

// ============================================================================
// PARÂMETROS INTERNOS (CONSTANTES DE AJUSTE FINO)
// ============================================================================
#define MUTATION_INITIAL 5.0     // Taxa de mutação base (%)
#define MUTATION_MAX 25.0        // Teto da mutação em crise
#define MUTATION_MIN 0.1         // Mínimo de mutação na convergência fina
#define STAGNATION_LIMIT 50      // Gerações sem melhora antes de entrar em pânico
#define GENETIC_DIVERSITY_THRESHOLD 1.5 // Distância mínima para considerar "diverso"

// Controle de Modos de Cruzamento
#define REPULSION_BASE_FACTOR 0.5
#define RESET_AFTER_REPULSION_GENS 20
#define RESET_PERCENTAGE 0.50     // Mata 50% da população no Reset
#define MODE_ATTRACTION 0
#define MODE_REPULSION 1

#define PI 3.1415926535

// ============================================================================
// FUNÇÕES AUXILIARES
// ============================================================================

/** Verifica se dois indivíduos são idênticos (clones) para evitar duplicação */
int are_individuals_equal(Individual a, Individual b) {
    if (a.genes == NULL || b.genes == NULL) return 0;
    for (int i = 0; i < NUM_DIMENSIONS; i++) {
        if (fabs(a.genes[i] - b.genes[i]) > 1e-9) return 0;
    }
    return 1;
}

/** Calcula a dispersão da população (Desvio Padrão Espacial) */
double calculate_genetic_diversity() {
    if (population == NULL || POPULATION_SIZE == 0) return 0.0;
    
    // 1. Calcula o Centroide (Média de todos os genes)
    Individual centroid;
    centroid.genes = (double*)malloc(sizeof(double) * NUM_DIMENSIONS);
    for (int j = 0; j < NUM_DIMENSIONS; j++) {
        double sum = 0.0;
        for (int i = 0; i < POPULATION_SIZE; i++) sum += population[i].genes[j];
        centroid.genes[j] = sum / POPULATION_SIZE;
    }
    
    // 2. Calcula distância média de cada indivíduo até o centroide
    double total_distance = 0.0;
    for (int i = 0; i < POPULATION_SIZE; i++) {
        double sq_dist = 0.0;
        for (int j = 0; j < NUM_DIMENSIONS; j++) sq_dist += pow(population[i].genes[j] - centroid.genes[j], 2.0);
        total_distance += sqrt(sq_dist);
    }
    free(centroid.genes);
    return total_distance / POPULATION_SIZE;
}

/** Cria uma cópia profunda de um indivíduo */
Individual clone_individual(const Individual* src) {
    Individual dest;
    dest.genes = (double*)malloc(sizeof(double) * NUM_DIMENSIONS);
    memcpy(dest.genes, src->genes, sizeof(double) * NUM_DIMENSIONS);
    return dest;
}

// ============================================================================
// GERENCIAMENTO DE MEMÓRIA
// ============================================================================

void initialize_population() {
    if (population) free_population();
    population = (Individual*)malloc(sizeof(Individual) * POPULATION_SIZE);
    fitness = (double*)malloc(sizeof(double) * POPULATION_SIZE);
    
    for (int i = 0; i < POPULATION_SIZE; i++) {
        population[i].genes = (double*)malloc(sizeof(double) * NUM_DIMENSIONS);
        fitness[i] = -1e300; // Começa com fitness muito baixo
        
        // Inicialização Aleatória Uniforme dentro dos limites
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

// ============================================================================
// MOTOR PRINCIPAL (MAIN LOOP)
// ============================================================================
// 

Individual run_ga_cycle(double (*fitness_func)(Individual, const void*), const void* extra_param, int is_shape_opt) {
    
    initialize_population();
    
    // Variáveis de Estado do AG Adaptativo
    double mutation_rate = MUTATION_INITIAL;
    double baseline_mutation = MUTATION_INITIAL;
    int stagnation_counter = 0;
    int repulsion_mode_counter = 0;
    int crossover_mode = MODE_ATTRACTION;
    int post_reset_cnt = 0; // Contador de proteção pós-reset
    Individual prev_best = {NULL};

    // --- CSV HEADER ---
    // Escreve o cabeçalho se o arquivo estiver aberto (apenas na geração 0 implícita)
    if (ga_csv_file != NULL) {
        fprintf(ga_csv_file, "Geracao,MelhorFitness,FitnessMedio,DesvioPadraoFit,DiversidadeGenetica,TaxaMutacao,FatorRepulsao,Evento\n");
    }

    // --- LOOP DE GERAÇÕES ---
    for (int gen = 0; gen < MAX_GENERATIONS; gen++) {
        double total_fitness = 0.0;
        double max_fit = -1e300;
        int best_idx = 0;
        int valid = 0;
        char event_str[20] = "-"; // String para logar eventos (ex: "RESET")

        // 1. AVALIAÇÃO (Fitness)
        for (int i = 0; i < POPULATION_SIZE; i++) {
            double f = fitness_func(population[i], extra_param);
            
            // Tratamento de erros numéricos (-inf)
            if (f > -1e200) {
                fitness[i] = f;
                total_fitness += f;
                if (f > max_fit) { max_fit = f; best_idx = i; }
                valid++;
            } else {
                fitness[i] = -1e300; // Penalidade máxima
            }
        }
        
        // Estatísticas básicas
        double avg_fit = (valid > 0) ? total_fitness/valid : 0.0;
        
        // Cálculo do Desvio Padrão do Fitness (para o gráfico de sombra)
        double variance_fit = 0.0;
        if (valid > 0) {
            for (int i = 0; i < POPULATION_SIZE; i++) {
                if (fitness[i] > -1e200)
                    variance_fit += pow(fitness[i] - avg_fit, 2.0);
            }
            variance_fit /= valid;
        }
        double std_dev_fit = sqrt(variance_fit);

        // 2. LÓGICA ADAPTATIVA (O Cérebro)
        Individual curr_best = clone_individual(&population[best_idx]);
        int improved = 0;
        
        // Verifica se houve melhoria em relação à geração anterior
        if (prev_best.genes && max_fit > -1e200) {
             double prev_f = fitness_func(prev_best, extra_param);
             if (max_fit > prev_f + 1e-9 && !are_individuals_equal(curr_best, prev_best)) improved = 1;
        } else if (max_fit > -1e200) improved = 1;

        if (post_reset_cnt > 0) {
            // Período de graça pós-reset: Alta mutação para explorar novos caminhos
            post_reset_cnt--; mutation_rate = baseline_mutation * 3.0; crossover_mode = MODE_ATTRACTION;
        } else {
            if (improved) {
                // SUCESSO: Reseta contadores de pânico e refina a busca
                stagnation_counter = 0; repulsion_mode_counter = 0; crossover_mode = MODE_ATTRACTION;
                
                // Se a diversidade está baixa, força mutação leve para evitar colapso
                if (calculate_genetic_diversity() < GENETIC_DIVERSITY_THRESHOLD) mutation_rate /= 1.1;
                else mutation_rate = baseline_mutation;
            } else {
                // FALHA: Incrementa contador de estagnação
                stagnation_counter++;
                if (stagnation_counter >= STAGNATION_LIMIT) {
                    // Estágio 1: Aumenta Mutação
                    if (mutation_rate < MUTATION_MAX) { mutation_rate *= 1.2; }
                    else {
                        // Estágio 2: Ativa Modo Repulsão (foge do melhor indivíduo)
                        crossover_mode = MODE_REPULSION; repulsion_mode_counter++;
                        
                        // Estágio 3: RESET (APOCALIPSE)
                        if (repulsion_mode_counter >= RESET_AFTER_REPULSION_GENS) {
                            strcpy(event_str, "RESET");
                            
                            // Mantém 50% dos melhores, apaga o resto
                            int reset_cnt = (int)(POPULATION_SIZE * RESET_PERCENTAGE);
                            int survivor_count = POPULATION_SIZE - reset_cnt;
                            int current_fill_idx = survivor_count; 

                            // TÁTICA A: FRANKENSTEIN (Mistura aleatória dos sobreviventes)
                            if (current_fill_idx < POPULATION_SIZE) {
                                for (int d = 0; d < NUM_DIMENSIONS; d++) {
                                    int random_parent = rand() % survivor_count; 
                                    population[current_fill_idx].genes[d] = population[random_parent].genes[d];
                                }
                                fitness[current_fill_idx] = -1e300; current_fill_idx++; 
                            }
                            
                            // TÁTICA B: EDA (Estimation of Distribution Algorithm)
                            // Cria indivíduos baseados na média e desvio padrão dos sobreviventes
                            // Isso "chuta" onde novas soluções boas podem estar.
                            if (current_fill_idx < POPULATION_SIZE) {
                                for (int d = 0; d < NUM_DIMENSIONS; d++) {
                                    double sum = 0.0, sum_sq = 0.0;
                                    for(int k = 0; k < survivor_count; k++) {
                                        double val = population[k].genes[d];
                                        sum += val; sum_sq += val * val;
                                    }
                                    double mean = sum / survivor_count;
                                    double variance = (sum_sq / survivor_count) - (mean * mean);
                                    double std_dev = (variance > 0) ? sqrt(variance) : 0.0;
                                    
                                    // Geração Gaussiana (Box-Muller)
                                    double u1 = ((double)rand() / RAND_MAX);
                                    double u2 = ((double)rand() / RAND_MAX);
                                    if(u1 < 1e-9) u1 = 1e-9;
                                    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
                                    double new_gene = mean + z0 * std_dev;
                                    
                                    if(new_gene > GENE_MAX_VALUE[d]) new_gene = GENE_MAX_VALUE[d];
                                    if(new_gene < GENE_MIN_VALUE[d]) new_gene = GENE_MIN_VALUE[d];
                                    population[current_fill_idx].genes[d] = new_gene;
                                }
                                fitness[current_fill_idx] = -1e300; current_fill_idx++;
                            }
                            
                            // TÁTICA C: ALEATÓRIO PURO (Sangue novo)
                            for(int k = current_fill_idx; k < POPULATION_SIZE; k++) {
                                for(int d = 0; d < NUM_DIMENSIONS; d++) 
                                    population[k].genes[d] = GENE_MIN_VALUE[d] + ((double)rand()/RAND_MAX)*(GENE_MAX_VALUE[d]-GENE_MIN_VALUE[d]);
                                fitness[k] = -1e300;
                            }
                            
                            post_reset_cnt = 30; // Proteção
                            repulsion_mode_counter = 0; 
                            stagnation_counter = 0;
                        }
                    }
                }
            }
        }
        // Clamps de Mutação
        if(mutation_rate < MUTATION_MIN) mutation_rate = MUTATION_MIN;
        if(mutation_rate > MUTATION_MAX) mutation_rate = MUTATION_MAX;

        // Atualiza Melhor Histórico
        if (max_fit > -1e200) {
            if (prev_best.genes) free(prev_best.genes);
            prev_best = clone_individual(&curr_best);
        }
        free(curr_best.genes);

        // --- 3. EXPORTAÇÃO DE DADOS (CSV) ---
        if (ga_csv_file != NULL) {
            double rep_fact = (crossover_mode == MODE_REPULSION) ? REPULSION_BASE_FACTOR * (1 + repulsion_mode_counter/(double)STAGNATION_LIMIT) : 0;
            double current_div = calculate_genetic_diversity();
            
            fprintf(ga_csv_file, "%d,%.5f,%.5f,%.5f,%.5f,%.2f,%.2f,%s\n", 
                gen+1, 
                max_fit > -1e200 ? max_fit : 0, 
                avg_fit, 
                std_dev_fit,
                current_div,
                mutation_rate, 
                rep_fact,
                event_str);
        }

        // Barra de progresso visual no terminal
        if (gen % (MAX_GENERATIONS/10) == 0) {
            printf(" [GA] Progresso: %d%% (Fit: %.2f)\r", (gen*100)/MAX_GENERATIONS, max_fit);
            fflush(stdout);
        }

        // --- 4. REPRODUÇÃO (Crossover & Mutação) ---
        Individual* new_pop = (Individual*)malloc(sizeof(Individual)*POPULATION_SIZE);
        
        // Elitismo: O melhor sempre passa intacto
        Individual elite = clone_individual(&population[best_idx]);
        // Proteção contra primeira geração inválida
        if(max_fit < -1e200) { for(int d=0; d<NUM_DIMENSIONS; d++) elite.genes[d] = GENE_MIN_VALUE[d]; }
        new_pop[0] = elite;
        
        // Fator de Repulsão dinâmico
        double rep_fact = (crossover_mode == MODE_REPULSION) ? REPULSION_BASE_FACTOR * (1 + repulsion_mode_counter/(double)STAGNATION_LIMIT) : 0;

        for(int i=1; i<POPULATION_SIZE; i++) {
            new_pop[i].genes = (double*)malloc(sizeof(double)*NUM_DIMENSIONS);
            for(int j=0; j<NUM_DIMENSIONS; j++) {
                // Crossover:
                // Se Atração: Puxa o gene em direção ao elite (Média)
                // Se Repulsão: Empurra o gene para longe do elite
                if(crossover_mode == MODE_ATTRACTION)
                    new_pop[i].genes[j] = (elite.genes[j] + population[i].genes[j]) / 2.0;
                else
                    new_pop[i].genes[j] = population[i].genes[j] + rep_fact * (population[i].genes[j] - elite.genes[j]);
                
                // Mutação: Adiciona ruído aleatório
                double range = GENE_MAX_VALUE[j] - GENE_MIN_VALUE[j];
                new_pop[i].genes[j] += ((double)rand()/RAND_MAX - 0.5) * (range * mutation_rate/100.0);
                
                // Respeito aos Limites (Clamping)
                if(new_pop[i].genes[j] > GENE_MAX_VALUE[j]) new_pop[i].genes[j] = GENE_MAX_VALUE[j];
                if(new_pop[i].genes[j] < GENE_MIN_VALUE[j]) new_pop[i].genes[j] = GENE_MIN_VALUE[j];
            }
        }
        
        // Troca de População (Swap Pointers)
        free_population();
        population = new_pop;
        fitness = (double*)malloc(sizeof(double)*POPULATION_SIZE);
    }
    printf("\n"); // Nova linha após barra de progresso

    // --- FINALIZAÇÃO ---
    int best = 0;
    for(int i=1; i<POPULATION_SIZE; i++) 
        if(fitness[i] > fitness[best]) best = i;
    
    Individual final_res = clone_individual(&population[best]);
    if(prev_best.genes) free(prev_best.genes);
    free_population();
    
    return final_res; // Retorna o campeão
}