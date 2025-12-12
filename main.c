#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "ga_engine.h"
#include "physics.h"
#include "reports.h"

/**
 * @file main.c
 * @brief Ponto de Entrada da Simulação (Pipeline de Otimização).
 * * Este arquivo orquestra os três estágios da evolução:
 * 1. Design: Encontrar a melhor geometria física.
 * 2. Estratégia Longa: Encontrar o melhor perfil de velocidade para 3000km.
 * 3. Estratégia Curta: Otimizar o alcance diário com restrição de bateria.
 * * Também é responsável por gerar os logs CSV que alimentam o Dashboard em Python.
 */

int main() {
    // Alocação de estrutura auxiliar para uso posterior
    Individual final_strategy_3000km; 
    final_strategy_3000km.genes = (double*)malloc(sizeof(double) * 9);

    // 1. Inicialização do Gerador de Números Aleatórios (Semente Temporal)
    srand(time(NULL));

    printf("====================================================\n");
    printf(" PROJETO SOLAR - SUPER OTIMIZADOR MODULAR (v7.3 Dashboard)\n");
    printf(" Integração: GA Engine + Physics + Reports + CSV Logs\n");
    printf("====================================================\n\n");

    // ==================================================================
    // === ESTÁGIO 1: OTIMIZAR DESIGN DO CARRO (Forma Física) ===
    // ==================================================================
    // Objetivo: Achar L, W, H, Pods e Área Solar que maximizem a sobra de energia.
    printf("### ESTAGIO 1: Otimizando Geometria do Carro (Item 19, 21) ###\n");

    // --- SETUP DE LOG (Dashboard) ---
    // Abre o arquivo para gravar a evolução frame a frame
    ga_csv_file = fopen("fase1.csv", "w");
    if (ga_csv_file == NULL) printf("AVISO: Nao foi possivel criar fase1.csv\n");
    
    // --- CONFIGURAÇÃO DO AG (Geometria) ---
    POPULATION_SIZE = 1000;      // População grande para explorar bem o espaço 3D
    MAX_GENERATIONS = 100000;    // Gerações altas (critério de parada por estagnação ativa no engine)
    
    NUM_DIMENSIONS = 7; // 7 Genes: [L_casco, W_casco, H_casco, L_pod, D_pod, A_solar, W_sep]
    GENE_MIN_VALUE = (double*)malloc(NUM_DIMENSIONS * sizeof(double));
    GENE_MAX_VALUE = (double*)malloc(NUM_DIMENSIONS * sizeof(double));
    
    // Definição do "Espaço de Busca" (Limites Físicos permitidos pelo regulamento)
    double min_shape[] = {3.0, MIN_CASCO_WIDTH, MIN_CASCO_HEIGHT, 1.5, MIN_POD_DIAMETER, 4.0, (MIN_CASCO_WIDTH + MIN_POD_DIAMETER + MIN_COMPONENT_SEP)};
    double max_shape[] = {5.8, 0.9,             1.2,              3.0, 0.7,              6.0, MAX_VEHICLE_WIDTH};
    
    memcpy(GENE_MIN_VALUE, min_shape, NUM_DIMENSIONS * sizeof(double));
    memcpy(GENE_MAX_VALUE, max_shape, NUM_DIMENSIONS * sizeof(double));

    // EXECUÇÃO DO AG (Fase 1)
    // Passamos uma velocidade de referência fixa (22 m/s) para otimizar a forma
    double ref_speed_ms = 22.0; 
    Individual best_shape_ind = run_ga_cycle(fitness_shape_wrapper, &ref_speed_ms, 1);

    // Finalização do Log Fase 1
    if (ga_csv_file) { fclose(ga_csv_file); ga_csv_file = NULL; }

    // --- CONSOLIDAÇÃO DO DESIGN ---
    // Transformamos os genes abstratos (array) em uma struct física utilizável
    CarDesignOutrigger car;
    car.L_casco = best_shape_ind.genes[0];
    car.W_casco = best_shape_ind.genes[1];
    car.H_casco = best_shape_ind.genes[2];
    car.L_pod   = best_shape_ind.genes[3];
    car.D_pod   = best_shape_ind.genes[4];
    car.A_solar = best_shape_ind.genes[5];
    car.W_sep   = best_shape_ind.genes[6];
    
    free(best_shape_ind.genes); // Limpa memória do indivíduo temporário
    printf(">>> Design Otimizado: Casco=%.2fm, Pod=%.2fm, Solar=%.2fm2\n\n", car.L_casco, car.D_pod, car.A_solar);


    // ==================================================================
    // === ESTÁGIO 2: OTIMIZAR ESTRATÉGIA (Corrida Completa 3000 km) ===
    // ==================================================================
    // Objetivo: Usando o CARRO FIXO do Estágio 1, achar as melhores velocidades horárias.
    printf("### ESTAGIO 2: Otimizando Estrategia para 3000km (Item 31) ###\n");

    // --- SETUP DE LOG (Dashboard) ---
    ga_csv_file = fopen("fase2.csv", "w");
    if (ga_csv_file == NULL) printf("AVISO: Nao foi possivel criar fase2.csv\n");

    // --- RECONFIGURAÇÃO DO AG (Estratégia) ---
    // Agora o problema mudou: genes não são mais metros, são m/s (velocidade)
    POPULATION_SIZE = 1000;  
    MAX_GENERATIONS = 100000;   
    
    free(GENE_MIN_VALUE);
    free(GENE_MAX_VALUE);
    NUM_DIMENSIONS = 9; // 9 Genes: Velocidade média para cada hora do dia (08h às 16h)
    GENE_MIN_VALUE = (double*)malloc(NUM_DIMENSIONS * sizeof(double));
    GENE_MAX_VALUE = (double*)malloc(NUM_DIMENSIONS * sizeof(double));
    
    // Define limites de velocidade permitidos (entre 15 m/s e 25 m/s)
    for(int i=0; i<NUM_DIMENSIONS; i++) {
        GENE_MIN_VALUE[i] = 15.0; // ~54 km/h (Mínimo tático)
        GENE_MAX_VALUE[i] = 25.0; // ~90 km/h (Máximo seguro)
    }

    // EXECUÇÃO DO AG (Fase 2)
    // Passamos a struct 'car' como parâmetro, para a física calcular o arrasto correto
    Individual best_strat_3000 = run_ga_cycle(fitness_strategy_wrapper, &car, 0);

    // Finalização do Log Fase 2
    if (ga_csv_file) { fclose(ga_csv_file); ga_csv_file = NULL; }

    // --- CÁLCULOS FINAIS PARA RELATÓRIO ---
    // O AG nos dá os genes, mas precisamos recalcular a física detalhada (Massa, Cd, CdA)
    // para imprimir no relatório final.
    double M_total_final, Cd_final, CdA_total_final, A_frontal_final;
    {
        // 1. Calcula velocidade média da estratégia vencedora
        double avg_speed_ms = 0;
        for(int i=0; i<9; i++) avg_speed_ms += best_strat_3000.genes[i];
        avg_speed_ms = fmax(avg_speed_ms / 9.0, 1.0);
        
        // 2. Recalcula Aerodinâmica com base nessa velocidade (Reynolds muda!)
        double Am_c, Am_p;
        double CdA_c = calcular_drag_body(car.L_casco, car.W_casco, car.H_casco, avg_speed_ms, &Am_c);
        double CdA_p = calcular_drag_body(car.L_pod, car.D_pod, car.D_pod, avg_speed_ms, &Am_p);
        
        // Aerodinâmica da Asa
        double L_chord = car.A_solar / car.W_sep;
        double Re_w = fmax(1.0, (RHO_AIR * avg_speed_ms * L_chord) / MU_AIR);
        double frac = fmin(0.3, RE_CRIT / Re_w);
        double Cf_w = frac * (1.328/sqrt(Re_w)) + (1-frac)*(0.074/pow(Re_w, 0.2));
        double CdA_w = Cf_w * (2.0 * car.A_solar) * 0.5;
        
        // Totais Físicos
        CdA_total_final = (CdA_c + CdA_p + CdA_w) * 1.10;
        A_frontal_final = (PI/4 * car.W_casco * car.H_casco) + (PI/4 * car.D_pod * car.D_pod);
        Cd_final = (A_frontal_final > 1e-6) ? (CdA_total_final / A_frontal_final) : 0.0;
        
        // Massa estimada final
        double M_est = (RHO_CARENAGEM * (Am_c + Am_p)) + ((RHO_CHASSI + RHO_PAINEL) * car.A_solar);
        M_total_final = M_est + FIXED_MASS + 80.0;
    }

    // Imprime o Relatório "Master" consolidando Carro + Estratégia Longa
    print_final_summary(&car, &best_strat_3000, M_total_final, Cd_final, CdA_total_final, A_frontal_final);


    // ==================================================================
    // === ESTÁGIO 3: OTIMIZAR ALCANCE DIÁRIO (Constraint Battery > 30%) ===
    // ==================================================================
    // Objetivo: Rodar o máximo possível num dia só, mas terminando com "tanque" sobrando.
    printf("\n\n====================================================\n");
    printf("### ESTAGIO 3: Otimizando Estrategia para Alcance Diario (Item 28) ###\n");
    printf("====================================================\n\n");

    // --- SETUP DE LOG (Dashboard) ---
    ga_csv_file = fopen("fase3.csv", "w");
    if (ga_csv_file == NULL) printf("AVISO: Nao foi possivel criar fase3.csv\n");

    // --- CONFIGURAÇÃO DO AG ---
    // Mantém os mesmos limites de velocidade, mas muda a função de fitness
    MAX_GENERATIONS = 100000; 
    
    Individual best_strat_daily = run_ga_cycle(fitness_strategy_daily_wrapper, &car, 0);

    // Finalização do Log Fase 3
    if (ga_csv_file) { fclose(ga_csv_file); ga_csv_file = NULL; }

    // --- RE-SIMULAÇÃO DETALHADA ---
    // Como a função de fitness só retorna um número (score), precisamos re-rodar
    // a física passo-a-passo usando os genes vencedores para extrair dados 
    // específicos para os Itens 28 e 35 (Consumo em Watts e Bateria Final).
    double P_dreno_horario[9] = {0};
    double distancia_final_alcance = 0;
    double bateria_final_alcance = 0;
    {
        double* perfil_v = best_strat_daily.genes;
        double cap_bat_wh = CAPACIDADE_BATERIA_KWH * 1000.0;
        double bat_atual = cap_bat_wh;
        
        for (int hora = 0; hora < 9; hora++) {
            // ... [Lógica de simulação idêntica à fitness_strategy_daily_wrapper] ...
            // Recriamos o loop aqui para capturar 'P_bat' e 'bat_atual' a cada hora.
            
            double v_ms = perfil_v[hora];
            double v_kmh = v_ms * 3.6;
            SolarData sol = get_solar_data(hora);
            double P_sol = calcular_potencia_solar(sol.irradiance, car.A_solar, sol.T_amb);
            double P_liq = P_sol * EFF_MPPT;

            if (bat_atual <= 0.01 * cap_bat_wh) {
                 bat_atual += P_liq;
                 if (bat_atual > cap_bat_wh) bat_atual = cap_bat_wh;
                 P_dreno_horario[hora] = 0; // Carro parado não gasta motor
                 continue;
            }
            
            double T_asf = temperatura_asfalto(hora, sol.T_amb);
            double P_res = calcular_potencia_resistiva(v_ms, M_total_final, CdA_total_final, T_asf);
            double eta = EFF_MPPT * EFF_DRIVER * eficiencia_motor(P_res) * EFF_TRANS;
            double P_bat = (eta > 1e-6) ? (P_res / eta) : 1e6;
            
            // Guarda dado para o Relatório (Item 35)
            P_dreno_horario[hora] = P_bat;

            double balanco = P_liq - P_bat;
            
            // Integração de distância
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

    // --- IMPRESSÃO DOS RESULTADOS DO ESTÁGIO 3 ---
    printf("\n--- (Itens 28, 32, 35) PERFORMANCE E ESTRATEGIA (Alcance Diario) ---\n");
    printf(" 28) Alcance Maximo Diario (c/ 30%% bat.): %.2f km\n", distancia_final_alcance);
    // Mostra a bateria em porcentagem
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

    // Liberação de Memória (Boas práticas)
    free(final_strategy_3000km.genes);
    free(best_strat_daily.genes);
    free(GENE_MIN_VALUE);
    free(GENE_MAX_VALUE);

    return 0; // Fim do programa com sucesso
}