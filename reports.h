#ifndef REPORTS_H
#define REPORTS_H

/**
 * @file reports.h
 * @brief Cabeçalho para funções de relatório e métricas do projeto.
 * * Este arquivo define as interfaces para exibir os resultados finais da 
 * simulação e calcular métricas analíticas do veículo evoluído.
 */

// Dependências necessárias para entender os tipos de dados (structs) usados abaixo
#include "physics.h"   // Para acessar a struct CarDesignOutrigger
#include "ga_engine.h" // Para acessar a struct Individual

/**
 * @brief Exibe o relatório final do melhor veículo encontrado.
 * * Esta função formata e imprime no console os dados cruciais do design,
 * incluindo aerodinâmica, massa e parâmetros genéticos.
 * * @param car Ponteiro para a estrutura física do carro (dimensões, rodas, etc).
 * @param strategy Ponteiro para o indivíduo do AG (contém o genótipo/cromossomo).
 * @param M_total Massa total calculada do veículo (em kg).
 * @param Cd Coeficiente de arrasto (adimensional).
 * @param CdA_total Área de arrasto efetiva (Cd * Área Frontal).
 * @param A_frontal_total Área frontal projetada do veículo (em m²).
 */
void print_final_summary(const CarDesignOutrigger* car, 
                         const Individual* strategy,
                         const double M_total, 
                         const double Cd, 
                         const double CdA_total, 
                         const double A_frontal_total);

/**
 * @brief Calcula e exibe métricas secundárias de engenharia.
 * * Gera dados que podem não ser usados diretamente na função de fitness,
 * mas são úteis para análise técnica (ex: relação peso/potência, eficiência, etc).
 * * @param car Ponteiro para a estrutura do carro.
 * @param M_total Massa total do veículo.
 * @param CdA_total Área de arrasto efetiva.
 */
void calculate_extra_metrics(const CarDesignOutrigger* car, 
                             const double M_total, 
                             const double CdA_total);

#endif // REPORTS_H