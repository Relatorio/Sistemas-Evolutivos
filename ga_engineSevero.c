#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ga_engine.h"

// =============================================================================
// 1. VARIÁVEIS GLOBAIS E CONFIGURAÇÕES
// =============================================================================
Individual* population = NULL;
double* fitness = NULL;
int POPULATION_SIZE;
int MAX_GENERATIONS;
int NUM_DIMENSIONS;
double* GENE_MIN_VALUE = NULL;
double* GENE_MAX_VALUE = NULL;

FILE* ga_csv_file = NULL;

// --- Parâmetros do Modelo de Fisher (Severidade Adaptativa) ---
#define MUTATION_RATE_FIXED 20.0     // 20% dos genes sofrem mutação (Frequência Fixa)
#define SEVERITY_INITIAL 0.50        // Começa cobrindo 50% do range (Busca Ampla)
#define SEVERITY_MIN 0.0001          // Refinamento de precisão (Microscópio)
#define SEVERITY_MAX 1.0             // Expansão máxima (Salto total)

// Fatores de Adaptação
#define SEVERITY_DECAY 0.85          // Se melhorou: reduz severidade (Refinamento)
#define SEVERITY_EXPAND 2.5          // Se estagnou: aumenta severidade (Expansão)

// Controles de Estagnação
#define STAGNATION_LIMIT 15          // Gerações sem melhora antes de expandir a severidade
#define CATASTROPHE_LIMIT 5          // Quantas vezes expandimos sem sucesso antes do Reset total

#ifndef PI
#define PI 3.14159265358979323846
#endif

// =============================================================================
// 2. FUNÇÕES AUXILIARES MATEMÁTICAS
// =============================================================================

// Gera ruído Gaussiano (Normal Distribution) usando Transformada de Box-Muller
// Média 0, Desvio Padrão 1.
double generate_gaussian_noise() {
    static int have_spare = 0;
    static double rand1, rand2;
    
    if (have_spare) {
        have_spare = 0;
        return sqrt(rand1) * sin(rand2);
    }
    
    have_spare = 1;
    rand1 = rand() / ((double) RAND_MAX);
    if (rand1 < 1e-100) rand1 = 1e-100;
    rand1 = -2 * log(rand1);
    rand2 = (rand() / ((double) RAND_MAX)) * 2 * PI;
    
    return sqrt(rand1) * cos(rand2);
}

// Verifica se dois indivíduos são idênticos (para evitar clones na detecção de melhoria)
int are_individuals_equal(Individual a, Individual b) {
    if (a.genes == NULL || b.genes == NULL) return 0;
    for (int i = 0; i < NUM_DIMENSIONS; i++) {
        if (fabs(a.genes[i] - b.genes[i]) > 1e-9) return 0;
    }
    return 1;
}

// Clona um indivíduo (Deep Copy)
Individual clone_individual(const Individual* src) {
    Individual dest;
    dest.genes = (double*)malloc(sizeof(double) * NUM_DIMENSIONS);
    memcpy(dest.genes, src->genes, sizeof(double) * NUM_DIMENSIONS);
    return dest;
}

// =============================================================================
// 3. GERENCIAMENTO DE MEMÓRIA E INICIALIZAÇÃO
// =============================================================================

void free_population() {
    if (population) {
        for (int i = 0; i < POPULATION_SIZE; i++) {
            if (population[i].genes) free(population[i].genes);
        }
        free(population); population = NULL;
    }
    if (fitness) { free(fitness); fitness = NULL; }
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
            population[i].genes[j] = GENE_MIN_VALUE[j] + ((double)rand()/RAND_MAX) * range;
        }
    }
}

// =============================================================================
// 4. MOTOR PRINCIPAL (GA ENGINE)
// =============================================================================

Individual run_ga_cycle(double (*fitness_func)(Individual, const void*), const void* extra_param, int is_shape_opt) {
    
    initialize_population();
    
    // --- ESTADO DO ALGORITMO ---
    double current_severity = SEVERITY_INITIAL; // Começa explorando bem
    int stagnation_counter = 0;     // Contagem para aumentar severidade
    int expansion_events = 0;       // Contagem para acionar catástrofe (Reset)
    
    Individual prev_best_ind = {NULL};
    double prev_best_fit = -1e300;

    // Prepara CSV
    if (ga_csv_file != NULL) {
        fprintf(ga_csv_file, "Geracao,MelhorFitness,FitnessMedio,Severidade,Estagnacao,Evento\n");
    }

    // --- LOOP GERACIONAL ---
    for (int gen = 0; gen < MAX_GENERATIONS; gen++) {
        
        double total_fitness = 0.0;
        double max_fit = -1e300;
        int best_idx = 0;
        int valid_count = 0;
        char event_log[50] = "-";

        // 1. AVALIAÇÃO
        for (int i = 0; i < POPULATION_SIZE; i++) {
            double f = fitness_func(population[i], extra_param);
            
            // Tratamento de erros de simulação
            if (f <= -1e200 || isnan(f)) {
                fitness[i] = -1e300;
            } else {
                fitness[i] = f;
                total_fitness += f;
                if (f > max_fit) { max_fit = f; best_idx = i; }
                valid_count++;
            }
        }
        
        double avg_fit = (valid_count > 0) ? total_fitness / valid_count : 0.0;

        // 2. DETECÇÃO DE MELHORIA (Lógica Fisheriana)
        int improved = 0;
        Individual current_best_ind = population[best_idx]; // Apenas referência, cuidado ao usar pós-free

        if (max_fit > prev_best_fit + 1e-6) {
            // Verifica se não é apenas ruído numérico, mas mudança real?
            // (Opcional: checar se genes mudaram. Aqui assumimos confiança no fitness)
            improved = 1;
        }

        // 3. ADAPTAÇÃO DA SEVERIDADE (O Cérebro)
        if (improved) {
            // === SUCESSO: REFINAMENTO ===
            // Estamos subindo o pico. Reduzimos o passo para focar no cume.
            prev_best_fit = max_fit;
            
            // Guarda cópia segura do novo melhor
            if (prev_best_ind.genes) free(prev_best_ind.genes);
            prev_best_ind = clone_individual(&current_best_ind);
            
            stagnation_counter = 0;
            expansion_events = 0;
            
            // Decaimento da Severidade
            double old_sev = current_severity;
            current_severity *= SEVERITY_DECAY; 
            if (current_severity < SEVERITY_MIN) current_severity = SEVERITY_MIN;
            
            if (old_sev != current_severity) strcpy(event_log, "REFINAMENTO");

        } else {
            // === FALHA: ESTAGNAÇÃO ===
            stagnation_counter++;
            
            if (stagnation_counter >= STAGNATION_LIMIT) {
                // Estagnamos. Hora de aumentar a severidade (Expansão / Levy Flight)
                current_severity *= SEVERITY_EXPAND;
                
                // Trava no máximo
                if (current_severity > SEVERITY_MAX) current_severity = SEVERITY_MAX;
                
                stagnation_counter = 0; // Reseta para dar tempo da expansão funcionar
                expansion_events++;
                strcpy(event_log, "EXPANSAO_SEVERIDADE");

                // === FAILSAFE: CATÁSTROFE / RESET ===
                // Se expandimos várias vezes e nada aconteceu, estamos num ótimo local profundo.
                // Resetamos a população (Mantendo o melhor).
                if (expansion_events >= CATASTROPHE_LIMIT) {
                    strcpy(event_log, "RESET_CATASTROFICO");
                    current_severity = SEVERITY_INITIAL; // Reseta severidade também
                    expansion_events = 0;
                    
                    // Nota: O reset real acontece na fase de reprodução abaixo
                }
            }
        }

        // Log e Print
        if (ga_csv_file != NULL) {
            fprintf(ga_csv_file, "%d,%.6f,%.6f,%.6f,%d,%s\n", 
                gen+1, max_fit, avg_fit, current_severity, stagnation_counter, event_log);
        }
        
        if (gen % (MAX_GENERATIONS/20) == 0 || gen == 0) {
            printf("Gen %d | Best: %.4f | Severidade: %.5f | Evento: %s\r", 
                gen, max_fit, current_severity, event_log);
            fflush(stdout);
        }

        // 4. REPRODUÇÃO E MUTAÇÃO ADAPTATIVA
        Individual* new_pop = (Individual*)malloc(sizeof(Individual) * POPULATION_SIZE);
        
        // A. ELITISMO (Sempre salvamos o melhor histórico)
        // Se houve reset, o elitismo é a única coisa que sobra.
        if (prev_best_ind.genes != NULL) {
            new_pop[0] = clone_individual(&prev_best_ind);
        } else {
            new_pop[0] = clone_individual(&population[best_idx]);
        }

        // Define se é um ciclo de Reset
        int is_reset_cycle = (strcmp(event_log, "RESET_CATASTROFICO") == 0);

        for (int i = 1; i < POPULATION_SIZE; i++) {
            new_pop[i].genes = (double*)malloc(sizeof(double) * NUM_DIMENSIONS);

            if (is_reset_cycle) {
                // --- MODO RESET ---
                // Gera aleatórios puros para re-explorar o espaço global
                for (int j = 0; j < NUM_DIMENSIONS; j++) {
                     double range = GENE_MAX_VALUE[j] - GENE_MIN_VALUE[j];
                     new_pop[i].genes[j] = GENE_MIN_VALUE[j] + ((double)rand()/RAND_MAX) * range;
                }
            } else {
                // --- MODO EVOLUTIVO PADRÃO ---
                
                // 1. Seleção (Torneio Simples)
                int r1 = rand() % POPULATION_SIZE;
                int r2 = rand() % POPULATION_SIZE;
                // Preferimos quem tem fitness maior
                int parent_idx = (fitness[r1] > fitness[r2]) ? r1 : r2;
                // Se ambos forem inválidos, pega qualquer um
                if (fitness[r1] < -1e200 && fitness[r2] < -1e200) parent_idx = r1; 
                
                // Cruzamento (Uniforme com o Elitista/Best atual para acelerar convergência)
                // Usamos o best_idx da geração atual como um dos pais (Estratégia agressiva)
                for (int j = 0; j < NUM_DIMENSIONS; j++) {
                    if (rand() % 2 == 0) {
                        new_pop[i].genes[j] = population[parent_idx].genes[j];
                    } else {
                        new_pop[i].genes[j] = population[best_idx].genes[j];
                    }
                }

                // 2. MUTAÇÃO BASEADA EM SEVERIDADE
                // A taxa (rate) é fixa. A severidade (magnitude) dita o tamanho da mudança.
                for (int j = 0; j < NUM_DIMENSIONS; j++) {
                    double chance = (double)rand() / RAND_MAX * 100.0;
                    
                    if (chance < MUTATION_RATE_FIXED) {
                        double range = GENE_MAX_VALUE[j] - GENE_MIN_VALUE[j];
                        
                        // --- A MÁGICA DE FISHER ---
                        // Usamos distribuição normal (Gaussiana).
                        // Se severidade for pequena (0.001), gera um desvio minúsculo perto da média.
                        // Se severidade for grande (0.5), o desvio padrão é enorme.
                        double noise = generate_gaussian_noise(); // Normal(0, 1)
                        double delta = noise * (range * current_severity);
                        
                        new_pop[i].genes[j] += delta;
                        
                        // Clamping (Manter no domínio físico)
                        if (new_pop[i].genes[j] > GENE_MAX_VALUE[j]) new_pop[i].genes[j] = GENE_MAX_VALUE[j];
                        if (new_pop[i].genes[j] < GENE_MIN_VALUE[j]) new_pop[i].genes[j] = GENE_MIN_VALUE[j];
                    }
                }
            }
        }

        // Troca populações
        free_population(); // Libera a antiga
        population = new_pop;
        fitness = (double*)malloc(sizeof(double) * POPULATION_SIZE);
        // Reseta fitness array para a próxima iteração
        for(int k=0; k<POPULATION_SIZE; k++) fitness[k] = -1e300;
    }

    printf("\n[GA] Concluido. Melhor Fitness Final: %.5f\n", prev_best_fit);

    // Retorna o melhor histórico encontrado
    Individual final_result = clone_individual(&prev_best_ind);
    
    // Limpeza Final
    if (prev_best_ind.genes) free(prev_best_ind.genes);
    free_population();

    return final_result;
}