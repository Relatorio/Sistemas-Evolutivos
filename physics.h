#ifndef PHYSICS_H
#define PHYSICS_H

/**
 * @file physics.h
 * @brief Motor de Física e Constantes da Simulação.
 * * Este arquivo centraliza todas as "leis do universo" da simulação.
 * Contém constantes aerodinâmicas, elétricas, restrições de regulamento (BWSC/FSGP)
 * e definições de materiais.
 */

#include "ga_engine.h"

// ============================================================================
// --- CONSTANTES FÍSICAS E AMBIENTAIS ---
// ============================================================================
#define PI 3.1415926535

/** * @brief Capacidade Máxima Permitida (Regulamento).
 * @note Baseado em células Li-Ion típicas (ex: 20kg de bateria).
 * Unidade: Quilowatt-hora (kWh)
 */
#define CAPACIDADE_BATERIA_KWH 3.05     //

// Propriedades do Ar (nível do mar, ~25°C)
#define RHO_AIR 1.184      // Densidade do ar (kg/m^3)
#define MU_AIR 1.849e-5    // Viscosidade dinâmica (Pa.s) - Usado no cálculo de Reynolds
#define GRAVITY 9.81       // Aceleração da gravidade (m/s^2)

/**
 * @brief Número de Reynolds Crítico.
 * Ponto de transição onde o fluxo passa de Laminar (liso) para Turbulento (caótico).
 * Fundamental para o cálculo correto do arrasto da asa e carenagens.
 */
#define RE_CRIT 5e5

// ============================================================================
// --- CONSTANTES DO VEÍCULO (GENÉRICAS) ---
// ============================================================================

// Resistência ao Rolamento (Pneus Solares ex: Michelin/Bridgestone Ecopia)
#define CR_ROLLING_BASE 0.0045 // Coeficiente base (adimensional)

/**
 * @brief Massa Fixa (Não-otimizável).
 * Inclui: Piloto (80kg) + Lastro + Eletrônica básica + Rodas/Suspensão.
 * A massa do chassi e carenagem será somada a este valor.
 */
#define FIXED_MASS 92.0 // kg

#define P_MOTOR_NOMINAL 900.0 // Potência nominal do motor (Watts)

// Limites Dimensionais (Regras da Competição)
#define MAX_SOLAR_AREA 6.0       // Área máxima de células ativas (m^2)
#define MAX_VEHICLE_WIDTH 2.3    // Largura máxima total (m)
#define MAX_VEHICLE_LENGTH 5.8   // Comprimento máximo total (m)
#define MAX_VEHICLE_HEIGHT 1.65  // Altura máxima do topo do canopy (m)

// ============================================================================
// --- EFICIÊNCIA E MATERIAIS ---
// ============================================================================

// Painel Solar (Baseado em Silício Monocristalino Alta Eficiência)
#define EFF_PANEL_REF 0.245    // Eficiência de referência (24.5%)
#define PANEL_TEMP_COEFF -0.0037 // Perda de eficiência por °C acima do NOCT (-0.37%/°C)
#define NOCT 47.0              // Temperatura Nominal de Operação da Célula (°C)

// Cadeia de Potência (Powertrain)
#define EFF_MPPT 0.985   // Eficiência do rastreador de ponto de potência (98.5%)
#define EFF_DRIVER 0.975 // Eficiência do controlador do motor (97.5%)
#define EFF_TRANS 0.98   // Eficiência mecânica (rolamentos/corrente) (98%)

// Densidades Superficiais (para estimativa de massa estrutural)
#define RHO_CHASSI 4.5      // kg/m^2 (Fibra de Carbono + Honeycomb estrutural)
#define RHO_CARENAGEM 0.8   // kg/m^2 (Fibra leve/Kevlar apenas para aerodinâmica)
#define RHO_PAINEL 6.5      // kg/m^2 (Células + Encapsulamento + Estrutura de suporte)

// Comportamento Dinâmico dos Pneus
#define CR_TEMP_COEFF 0.0015  // Aumento do Crr com a temperatura (empírico)
#define CR_SPEED_COEFF 0.0001 // Aumento do Crr com a velocidade (histerese)

// ============================================================================
// --- RESTRIÇÕES GEOMÉTRICAS (HARD CONSTRAINTS) ---
// ============================================================================
// Usadas para penalizar indivíduos que criam carros impossíveis de construir
#define MIN_POD_DIAMETER 0.55   // Espaço mínimo para a roda girar (m)
#define MIN_CASCO_HEIGHT 0.85   // Altura mínima para o piloto sentado (m)
#define MIN_CASCO_WIDTH 0.6     // Largura mínima de ombros do piloto (m)
#define MIN_COMPONENT_SEP 0.1   // Separação mínima entre peças (evitar colisão física)

// ============================================================================
// --- ESTRUTURAS DE DADOS ---
// ============================================================================

/**
 * @brief Geometria do Veículo (Design Outrigger/Catamarã).
 * Define as dimensões físicas que afetam Massa, Aerodinâmica e Área Solar.
 */
typedef struct {
    double L_casco, W_casco, H_casco; // Fuselagem central (Piloto)
    double L_pod, D_pod;              // Carenagem das rodas laterais
    double A_solar;                   // Área do painel (Asa)
    double W_sep;                     // Distância lateral (bitola) entre casco e pod
} CarDesignOutrigger;

/**
 * @brief Dados Ambientais Horários.
 */
typedef struct { 
    double irradiance; // Irradiância Solar Global (W/m^2)
    double T_amb;      // Temperatura Ambiente (°C)
} SolarData;

// ============================================================================
// --- PROTÓTIPOS DE FUNÇÕES ---
// ============================================================================

/**
 * @brief Retorna os dados solares médios para uma hora específica do dia.
 * @param hora_do_dia Inteiro entre 8 e 17 (horário de corrida).
 */
SolarData get_solar_data(int hora_do_dia);

/**
 * @brief Calcula a potência elétrica gerada pelo painel.
 * Considera a correção térmica (painéis quentes geram menos).
 * 
 */
double calcular_potencia_solar(double irradiance, double A_solar, double T_amb);

/**
 * @brief Calcula o Coeficiente de Resistência ao Rolamento (Crr) ajustado.
 * O Crr não é constante: varia levemente com velocidade e temperatura do asfalto.
 */
double calcular_crr_dinamico(double v_kmh, double T_asfalto_C);

/**
 * @brief Estima a temperatura do asfalto com base na temperatura ambiente e hora.
 * Importante pois asfalto quente afeta a resistência do pneu.
 */
double temperatura_asfalto(int hora_do_dia, double T_amb);

/**
 * @brief Curva de eficiência do motor elétrico.
 * Motores têm eficiência baixa em cargas muito leves ou extremas.
 * @param P_resist Potência mecânica exigida na roda (Watts).
 * @return Eficiência (0.0 a 1.0).
 */
double eficiencia_motor(double P_resist);

/**
 * @brief Calcula o arrasto aerodinâmico (CdA) de um corpo fusiforme.
 * Usa teorias de placa plana para fricção e fatores de forma para pressão.
 * @param v_ms Velocidade do ar (m/s).
 * @param A_molhada_out [Saída] Preenche a área superficial calculada (m^2).
 * @return CdA (Área de arrasto efetiva em m^2).
 */
double calcular_drag_body(double L, double W, double H, double v_ms, double* A_molhada_out);

/**
 * @brief Calcula a potência total necessária para manter a velocidade constante.
 * Soma: P_arrasto_aerodinamico + P_atrito_rolamento.
 */
double calcular_potencia_resistiva(double v_ms, double M_total, double CdA_total, double T_amb_pneu);

// --- WRAPPERS PARA O ALGORITMO GENÉTICO ---
// Estas funções adaptam a interface física para o ponteiro genérico do AG.
// O parâmetro 'const void* param' permite passar estruturas extras sem mexer na assinatura do AG.

double fitness_shape_wrapper(Individual ind, const void* param);
double fitness_strategy_wrapper(Individual ind, const void* param);
double fitness_strategy_daily_wrapper(Individual ind, const void* param);

#endif // PHYSICS_H