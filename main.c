#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "ga_engine.h"
#include "physics.h"
#include "reports.h"

int main() {
    // Declaração antecipada conforme original
    Individual final_strategy_3000km; 
    final_strategy_3000km.genes = (double*)malloc(sizeof(double) * 9);

    // 1. Inicialização
    srand(time(NULL));
    printf("====================================================\n");
    printf(" PROJETO SOLAR - SUPER OTIMIZADOR MODULAR (v7.3 Dashboard)\n");
    printf(" Integração: GA Engine + Physics + Reports + CSV Logs\n");
    printf("====================================================\n\n");

    // ==================================================================
    // === ESTÁGIO 1: OTIMIZAR DESIGN DO CARRO (Forma) ===
    // ==================================================================
    printf("### ESTAGIO 1: Otimizando Geometria do Carro (Item 19, 21) ###\n");

    // --- LOG CSV FASE 1 ---
    ga_csv_file = fopen("fase1.csv", "w");
    if (ga_csv_file == NULL) printf("AVISO: Nao foi possivel criar fase1.csv\n");
    // ----------------------

    // --- CONFIGURAÇÃO DO GA PARA ESTÁGIO 1 ---
    POPULATION_SIZE = 1000;   
    MAX_GENERATIONS = 100000;   
    
    NUM_DIMENSIONS = 7;
    GENE_MIN_VALUE = (double*)malloc(NUM_DIMENSIONS * sizeof(double));
    GENE_MAX_VALUE = (double*)malloc(NUM_DIMENSIONS * sizeof(double));
    
    // Limites físicos "Outrigger v6.0"
    double min_shape[] = {3.0, MIN_CASCO_WIDTH, MIN_CASCO_HEIGHT, 1.5, MIN_POD_DIAMETER, 4.0, (MIN_CASCO_WIDTH + MIN_POD_DIAMETER + MIN_COMPONENT_SEP)};
    double max_shape[] = {5.8, 0.9,             1.2,              3.0, 0.7,              6.0, MAX_VEHICLE_WIDTH};
    
    memcpy(GENE_MIN_VALUE, min_shape, NUM_DIMENSIONS * sizeof(double));
    memcpy(GENE_MAX_VALUE, max_shape, NUM_DIMENSIONS * sizeof(double));

    // Roda o GA
    double ref_speed_ms = 22.0;
    Individual best_shape_ind = run_ga_cycle(fitness_shape_wrapper, &ref_speed_ms, 1);

    // Fecha CSV da Fase 1
    if (ga_csv_file) { fclose(ga_csv_file); ga_csv_file = NULL; }

    // Converte resultado
    CarDesignOutrigger car;
    car.L_casco = best_shape_ind.genes[0];
    car.W_casco = best_shape_ind.genes[1];
    car.H_casco = best_shape_ind.genes[2];
    car.L_pod   = best_shape_ind.genes[3];
    car.D_pod   = best_shape_ind.genes[4];
    car.A_solar = best_shape_ind.genes[5];
    car.W_sep   = best_shape_ind.genes[6];
    
    free(best_shape_ind.genes);
    printf(">>> Design Otimizado: Casco=%.2fm, Pod=%.2fm, Solar=%.2fm2\n\n", car.L_casco, car.D_pod, car.A_solar);


    // ==================================================================
    // === ESTÁGIO 2: OTIMIZAR ESTRATÉGIA (3000 km - Item 31) ===
    // ==================================================================
    printf("### ESTAGIO 2: Otimizando Estrategia para 3000km (Item 31) ###\n");

    // --- LOG CSV FASE 2 ---
    ga_csv_file = fopen("fase2.csv", "w");
    if (ga_csv_file == NULL) printf("AVISO: Nao foi possivel criar fase2.csv\n");
    // ----------------------

    // --- CONFIGURAÇÃO DO GA PARA ESTÁGIO 2 ---
    POPULATION_SIZE = 1000;  
    MAX_GENERATIONS = 100000;   
    
    // Reconfigura os limites para ESTRATÉGIA (9 genes)
    free(GENE_MIN_VALUE);
    free(GENE_MAX_VALUE);
    NUM_DIMENSIONS = 9;
    GENE_MIN_VALUE = (double*)malloc(NUM_DIMENSIONS * sizeof(double));
    GENE_MAX_VALUE = (double*)malloc(NUM_DIMENSIONS * sizeof(double));
    
    for(int i=0; i<NUM_DIMENSIONS; i++) {
        GENE_MIN_VALUE[i] = 15.0; 
        GENE_MAX_VALUE[i] = 25.0; 
    }

    // Roda o GA
    Individual best_strat_3000 = run_ga_cycle(fitness_strategy_wrapper, &car, 0);

    // Fecha CSV da Fase 2
    if (ga_csv_file) { fclose(ga_csv_file); ga_csv_file = NULL; }

    // Cálculos Finais para Relatório
    double M_total_final, Cd_final, CdA_total_final, A_frontal_final;
    {
        double avg_speed_ms = 0;
        for(int i=0; i<9; i++) avg_speed_ms += best_strat_3000.genes[i];
        avg_speed_ms = fmax(avg_speed_ms / 9.0, 1.0);
        
        double Am_c, Am_p;
        double CdA_c = calcular_drag_body(car.L_casco, car.W_casco, car.H_casco, avg_speed_ms, &Am_c);
        double CdA_p = calcular_drag_body(car.L_pod, car.D_pod, car.D_pod, avg_speed_ms, &Am_p);
        
        double L_chord = car.A_solar / car.W_sep;
        double Re_w = fmax(1.0, (RHO_AIR * avg_speed_ms * L_chord) / MU_AIR);
        double frac = fmin(0.3, RE_CRIT / Re_w);
        double Cf_w = frac * (1.328/sqrt(Re_w)) + (1-frac)*(0.074/pow(Re_w, 0.2));
        double CdA_w = Cf_w * (2.0 * car.A_solar) * 0.5;
        
        CdA_total_final = (CdA_c + CdA_p + CdA_w) * 1.10;
        A_frontal_final = (PI/4 * car.W_casco * car.H_casco) + (PI/4 * car.D_pod * car.D_pod);
        Cd_final = (A_frontal_final > 1e-6) ? (CdA_total_final / A_frontal_final) : 0.0;
        
        double M_est = (RHO_CARENAGEM * (Am_c + Am_p)) + ((RHO_CHASSI + RHO_PAINEL) * car.A_solar);
        M_total_final = M_est + FIXED_MASS + 80.0;
    }

    print_final_summary(&car, &best_strat_3000, M_total_final, Cd_final, CdA_total_final, A_frontal_final);


    // ==================================================================
    // === ESTÁGIO 3: OTIMIZAR ALCANCE DIÁRIO (Item 28) ===
    // ==================================================================
    printf("\n\n====================================================\n");
    printf("### ESTAGIO 3: Otimizando Estrategia para Alcance Diario (Item 28) ###\n");
    printf("====================================================\n\n");

    // --- LOG CSV FASE 3 ---
    ga_csv_file = fopen("fase3.csv", "w");
    if (ga_csv_file == NULL) printf("AVISO: Nao foi possivel criar fase3.csv\n");
    // ----------------------

    // --- CONFIGURAÇÃO DO GA PARA ESTÁGIO 3 ---
    MAX_GENERATIONS = 100000; 
    
    Individual best_strat_daily = run_ga_cycle(fitness_strategy_daily_wrapper, &car, 0);

    // Fecha CSV da Fase 3
    if (ga_csv_file) { fclose(ga_csv_file); ga_csv_file = NULL; }

    // Simulação Final do Dia (RECUPERADA COMPLETA)
    double P_dreno_horario[9] = {0};
    double distancia_final_alcance = 0;
    double bateria_final_alcance = 0;
    {
        double* perfil_v = best_strat_daily.genes;
        double cap_bat_wh = CAPACIDADE_BATERIA_KWH * 1000.0;
        double bat_atual = cap_bat_wh;
        
        for (int hora = 0; hora < 9; hora++) {
            double v_ms = perfil_v[hora];
            double v_kmh = v_ms * 3.6;
            SolarData sol = get_solar_data(hora);
            double P_sol = calcular_potencia_solar(sol.irradiance, car.A_solar, sol.T_amb);
            double P_liq = P_sol * EFF_MPPT;

            if (bat_atual <= 0.01 * cap_bat_wh) {
                 bat_atual += P_liq;
                 if (bat_atual > cap_bat_wh) bat_atual = cap_bat_wh;
                 P_dreno_horario[hora] = 0;
                 continue;
            }
            
            double T_asf = temperatura_asfalto(hora, sol.T_amb);
            double P_res = calcular_potencia_resistiva(v_ms, M_total_final, CdA_total_final, T_asf);
            double eta = EFF_MPPT * EFF_DRIVER * eficiencia_motor(P_res) * EFF_TRANS;
            double P_bat = (eta > 1e-6) ? (P_res / eta) : 1e6;
            P_dreno_horario[hora] = P_bat;

            double balanco = P_liq - P_bat;
            
            if (balanco < 0 && fabs(balanco) > bat_atual) {
                double frac = bat_atual / fabs(balanco);
                distancia_final_alcance += v_kmh * frac;
                bat_atual = 0;
            } else {
                bat_atual += balanco;
                if (bat_atual > cap_bat_wh) bat_atual = cap_bat_wh;
                distancia_final_alcance += v_kmh;
            }
        }
        bateria_final_alcance = bat_atual;
    }

    printf("\n--- (Itens 28, 32, 35) PERFORMANCE E ESTRATEGIA (Alcance Diario) ---\n");
    printf(" 28) Alcance Maximo Diario (c/ 30%% bat.): %.2f km\n", distancia_final_alcance);
    printf("     (Bateria final: %.1f%%)\n", (bateria_final_alcance / (CAPACIDADE_BATERIA_KWH * 1000.0)) * 100.0);
    
    printf(" 32) Perfil de Velocidade Otimizado (km/h) - (p/ Alcance Max):\n");
    for (int i = 0; i < 9; i++) {
        SolarData sol = get_solar_data(i);
        printf("     %02d-%02d h: %5.1f km/h (GHI: %6.1f W/m2)\n", 
               i + 8, i + 9, best_strat_daily.genes[i] * 3.6, sol.irradiance);
    }
    
    printf(" 35) Consumo energetico do motor (W) - (p/ Alcance Max):\n");
    for (int i = 0; i < 9; i++) {
        printf("     %02d-%02d h: %.1f W\n", i + 8, i + 9, P_dreno_horario[i]);
    }

    // Libera memória
    free(final_strategy_3000km.genes);
    free(best_strat_daily.genes);
    free(GENE_MIN_VALUE);
    free(GENE_MAX_VALUE);

    return 0;
}