#ifndef GA_ENGINE_H
#define GA_ENGINE_H

/**
 * @file ga_engine.h
 * @brief Interface do Motor de Algoritmo Genético (AG).
 * * Este arquivo define as estruturas de dados e funções públicas do otimizador.
 * O motor é "agnóstico": ele não sabe o que está otimizando. Ele apenas recebe
 * um vetor de números (genes) e uma função de avaliação (fitness), e tenta
 * encontrar a melhor combinação numérica.
 */

#include <stdio.h> // Necessário para manipular o tipo FILE*

// ============================================================================
// ESTRUTURAS DE DADOS
// ============================================================================

/**
 * @brief Representa um "Indivíduo" ou "Cromossomo" na população.
 * * No contexto do AG, um indivíduo é apenas uma solução candidata.
 * - Na Fase 1: Os genes representam L_casco, W_casco, Area_solar, etc.
 * - Na Fase 2: Os genes representam a Velocidade na hora 1, hora 2, etc.
 */
typedef struct {
    double* genes; // Vetor dinâmico contendo as variáveis de decisão
} Individual;

// ============================================================================
// VARIÁVEIS DE CONFIGURAÇÃO (GLOBAIS)
// ============================================================================
// O uso de 'extern' indica que estas variáveis são definidas no .c, mas podem
// ser modificadas pelo main.c para ajustar o comportamento entre as fases.

/** @brief Tamanho da População (ex: 1000 indivíduos). */
extern int POPULATION_SIZE;

/** @brief Critério de Parada: Número máximo de ciclos evolutivos. */
extern int MAX_GENERATIONS;

/** @brief Quantidade de genes por indivíduo (Dimensão do problema). */
extern int NUM_DIMENSIONS;

/** @brief Vetores que definem os limites mínimos e máximos para cada gene. */
extern double* GENE_MIN_VALUE;
extern double* GENE_MAX_VALUE;

// ============================================================================
// INTEGRAÇÃO COM LOGS E DASHBOARD
// ============================================================================

/**
 * @brief Ponteiro para o arquivo de log CSV.
 * * Se este ponteiro for diferente de NULL, o motor escreverá as métricas
 * (Melhor Fitness, Média, Diversidade) a cada geração.
 * Isso permite que o script Python gere os gráficos em tempo real.
 */
extern FILE* ga_csv_file;

// ============================================================================
// API PÚBLICA (FUNÇÕES)
// ============================================================================

/**
 * @brief Aloca memória para a população inicial.
 * Deve ser chamada após definir POPULATION_SIZE e NUM_DIMENSIONS.
 */
void initialize_population();

/**
 * @brief Libera toda a memória alocada pelo AG.
 * Fundamental para evitar vazamento de memória (memory leaks) entre as fases.
 */
void free_population();

/**
 * @brief O Ciclo Principal da Evolução (The Main Loop).
 * Executa a sequência: Seleção -> Cruzamento -> Mutação -> Avaliação.
 * * 
 * * @param fitness_func Ponteiro para a função que avalia quão bom é um indivíduo.
 * Isso permite plugar lógicas diferentes (Design ou Estratégia)
 * sem reescrever o motor.
 * @param extra_param  Ponteiro genérico (void*) para passar dados auxiliares.
 * (Ex: Na fase de estratégia, passamos a struct do Carro físico aqui).
 * @param is_shape_opt Flag booleana (0 ou 1) para indicar se estamos otimizando
 * forma (para logs específicos) ou estratégia.
 * * @return Individual O melhor indivíduo encontrado após todas as gerações.
 */
Individual run_ga_cycle(double (*fitness_func)(Individual, const void*), 
                        const void* extra_param, 
                        int is_shape_opt);

#endif // GA_ENGINE_H