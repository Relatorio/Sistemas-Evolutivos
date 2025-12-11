#ifndef PHYSICS_H
#define PHYSICS_H

#include "ga_engine.h"

// --- CONSTANTES FÍSICAS GLOBAIS ---
#define PI 3.1415926535
#define CAPACIDADE_BATERIA_KWH 3.05     // [cite: 1566]
#define RHO_AIR 1.184
#define MU_AIR 1.849e-5
#define GRAVITY 9.81
#define RE_CRIT 5e5

// Constantes do Carro
#define CR_ROLLING_BASE 0.0045
#define FIXED_MASS 92.0
#define P_MOTOR_NOMINAL 900.0
#define MAX_SOLAR_AREA 6.0
#define MAX_VEHICLE_WIDTH 2.3
#define MAX_VEHICLE_LENGTH 5.8
#define MAX_VEHICLE_HEIGHT 1.65

// Constantes de Eficiência e Material
#define EFF_PANEL_REF 0.245
#define PANEL_TEMP_COEFF -0.0037
#define NOCT 47.0
#define EFF_MPPT 0.985
#define EFF_DRIVER 0.975
#define EFF_TRANS 0.98
#define RHO_CHASSI 4.5
#define RHO_CARENAGEM 0.8
#define RHO_PAINEL 6.5
#define CR_TEMP_COEFF 0.0015
#define CR_SPEED_COEFF 0.0001

// Restrições Geométricas Internas
#define MIN_POD_DIAMETER 0.55
#define MIN_CASCO_HEIGHT 0.85
#define MIN_CASCO_WIDTH 0.6
#define MIN_COMPONENT_SEP 0.1

// --- ESTRUTURAS ---
typedef struct {
    double L_casco, W_casco, H_casco, L_pod, D_pod, A_solar, W_sep;
} CarDesignOutrigger;

typedef struct { double irradiance; double T_amb; } SolarData;

// --- PROTÓTIPOS DE FUNÇÕES ---
SolarData get_solar_data(int hora_do_dia);
double calcular_potencia_solar(double irradiance, double A_solar, double T_amb);
double calcular_crr_dinamico(double v_kmh, double T_asfalto_C);
double temperatura_asfalto(int hora_do_dia, double T_amb);
double eficiencia_motor(double P_resist);
double calcular_drag_body(double L, double W, double H, double v_ms, double* A_molhada_out);
double calcular_potencia_resistiva(double v_ms, double M_total, double CdA_total, double T_amb_pneu);

// Wrappers de Fitness (compatíveis com void*)
double fitness_shape_wrapper(Individual ind, const void* param);
double fitness_strategy_wrapper(Individual ind, const void* param);
double fitness_strategy_daily_wrapper(Individual ind, const void* param);

#endif