#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "reports.h"
#include "physics.h" 

// ============================================================================
// FUNÇÕES AUXILIARES (Static)
// ============================================================================
// Estas funções são 'static', o que significa que são privadas a este arquivo.
// Elas encapsulam lógicas específicas de teste (Item 29 e 30) para não poluir
// a função principal de relatório.
// ============================================================================

/**
 * @brief Simula a autonomia do veículo apenas com baterias (Item 29).
 * * Considera velocidade constante de 60km/h em plano, sem incidência solar.
 * Calcula o dreno de potência considerando toda a cadeia de eficiência.
 * 
 */
static void relatorio_autonomia_sem_sol(const double M_total, const double CdA_total) {
    printf("\n--- (Item 29) SIMULACAO DE AUTONOMIA (Bateria Cheia, Sem Sol) ---\n");
    
    // Parâmetros do teste
    double v_ms = 16.67; // 16.67 m/s = 60 km/h
    double v_kmh = v_ms * 3.6;
    
    // 1. Cálculo da Carga (Load)
    // Usa T_amb de 25°C padrão para padronizar o teste estático
    double P_resist_w = calcular_potencia_resistiva(v_ms, M_total, CdA_total, 25.0);
    
    // 2. Cálculo da Eficiência da Cadeia (Powertrain)
    // Bateria -> Controlador -> Motor -> Transmissão -> Roda
    double eta_motor = eficiencia_motor(P_resist_w);
    double eta_drivetrain = EFF_DRIVER * eta_motor * EFF_TRANS;
    
    // 3. Potência real drenada da bateria (P_eletrica = P_mecanica / eficiencia)
    double P_dreno_bateria_w = (eta_drivetrain > 1e-6) ? (P_resist_w / eta_drivetrain) : (P_resist_w * 1e6);
    
    // 4. Resultados
    double capacidade_bateria_wh = CAPACIDADE_BATERIA_KWH * 1000.0;
    double horas_autonomia = capacidade_bateria_wh / P_dreno_bateria_w;
    double dist_autonomia_km = v_kmh * horas_autonomia;
    
    printf(" 29) Autonomia a 60 km/h (sem sol): %.1f km (%.2f horas)\n", dist_autonomia_km, horas_autonomia);
    printf("     (Consumo @ 60km/h: %.1f W)\n", P_dreno_bateria_w);
}

/**
 * @brief Encontra a velocidade máxima teórica (Item 30).
 * * Realiza uma busca iterativa aumentando a velocidade até que a potência
 * resistiva (Arrasto + Atrito) supere a potência máxima do motor.
 */
static void relatorio_velocidade_maxima(const double M_total, const double CdA_total) {
    printf("\n--- (Item 30) SIMULACAO DE VELOCIDADE MAXIMA (Plano, Sem Sol) ---\n");
    
    // Define o teto de potência (Overclock de 150% do nominal permitido por curto período)
    double P_motor_max_w = (P_MOTOR_NOMINAL * 1.5); 
    // Aplica perdas de transmissão para saber quanto chega na roda
    double P_max_disponivel_w = P_motor_max_w * EFF_DRIVER * EFF_TRANS;
    
    double v_ms = 0.1;
    
    // Loop de Busca: Incrementa 0.1 m/s até o motor não aguentar mais
    for (v_ms = 0.1; v_ms < 60.0; v_ms += 0.1) {
        double P_resist_w = calcular_potencia_resistiva(v_ms, M_total, CdA_total, 25.0);
        if (P_resist_w > P_max_disponivel_w) break;
    }
    
    // Exibe o último passo válido
    printf(" 30) Velocidade Maxima (Plano): %.1f km/h (%.1f m/s)\n", (v_ms - 0.1) * 3.6, (v_ms - 0.1));
}

// ============================================================================
// FUNÇÃO PRINCIPAL DE RELATÓRIO
// ============================================================================

void print_final_summary(const CarDesignOutrigger* car, const Individual* strategy,
                         const double M_total, const double Cd, const double CdA_total, const double A_frontal_total) 
{
    // --- 1. Geração de Timestamp ---
    // Útil para diferenciar logs de execuções diferentes no terminal
    time_t raw_time;
    struct tm *time_info;
    char time_buffer[80];
    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(time_buffer, 80, "%d/%m - %H:%M", time_info);

    // --- 2. Detalhamento Aerodinâmico (Breakdown) ---
    // O AG trabalha com totais, mas o engenheiro precisa ver as partes.
    // Recalculamos os componentes individuais aqui para exibição.
    
    // Velocidade média estimada para cálculo do Reynolds
    double avg_speed_ms = 0;
    for(int i=0; i<9; i++) avg_speed_ms += strategy->genes[i];
    avg_speed_ms = fmax(avg_speed_ms / 9.0, 1.0);

    // Componentes de Arrasto (Casco e Pod)
    double Am_casco = 0, Am_pod = 0;
    double CdA_casco = calcular_drag_body(car->L_casco, car->W_casco, car->H_casco, avg_speed_ms, &Am_casco);
    double CdA_pod = calcular_drag_body(car->L_pod, car->D_pod, car->D_pod, avg_speed_ms, &Am_pod);
    
    // Componentes de Arrasto (Asa Solar - Placa Plana)
    // 
    double A_molhada_wing = 2.0 * car->A_solar; // Cima e baixo
    double L_wing_chord = car->A_solar / car->W_sep; // Corda média
    
    // Cálculo do Número de Reynolds (Re)
    double Re_wing = fmax(1.0, (RHO_AIR * avg_speed_ms * L_wing_chord) / MU_AIR);
    
    // Coeficiente de fricção (Cf) misto (Laminar + Turbulento)
    double frac = fmin(0.3, RE_CRIT / Re_wing); // Fração laminar estimada
    double Cf_w = frac * (1.328/sqrt(Re_wing)) + (1-frac)*(0.074/pow(Re_wing, 0.2));
    double CdA_wing = Cf_w * A_molhada_wing * 0.5; // Fator de forma simplificado

    // Normalização: Quanto cada parte contribui para o Cd total?
    // O fator 1.10 representa 10% de arrasto de interferência entre as peças
    double Cd_casco_norm = (CdA_casco * 1.10) / A_frontal_total;
    double Cd_pod_norm = (CdA_pod * 1.10) / A_frontal_total;
    double Cd_wing_norm = (CdA_wing * 1.10) / A_frontal_total;
    
    // Cálculo das áreas projetadas frontais (para referência visual)
    double A_frontal_casco = (PI/4 * car->W_casco * car->H_casco);
    double A_frontal_pod = (PI/4 * car->D_pod * car->D_pod);

    // --- 3. Re-Simulação da Corrida (Log Detalhado) ---
    // Reexecutamos a estratégia hora a hora para imprimir o status da bateria e velocidade.
    // Esta lógica deve ser IDÊNTICA à função de fitness para garantir consistência.
    
    double distancia_total_km = 0, tempo_total_horas = 0;
    double capacidade_bateria_wh = CAPACIDADE_BATERIA_KWH * 1000.0;
    double bateria_atual_wh = capacidade_bateria_wh;
    int dias = 0;

    // Loop principal: Corre até completar 3000km ou estourar 10 dias
    while (distancia_total_km < 3000.0 && dias < 10) {
        dias++;
        // Loop diário: 9 horas de sol (ex: 8h às 17h)
        for (int hora = 0; hora < 9; hora++) {
            double v_ms = strategy->genes[hora];
            double v_kmh = v_ms * 3.6;
            
            // Entradas Ambientais
            SolarData sol = get_solar_data(hora);
            double P_sol = calcular_potencia_solar(sol.irradiance, car->A_solar, sol.T_amb);
            double P_sol_liq = P_sol * EFF_MPPT; // O que realmente entra na bateria

            // Regra de Proteção: Se bateria < 1%, o carro para para carregar (velocidade 0)
            if (bateria_atual_wh <= 0.01 * capacidade_bateria_wh) {
                 bateria_atual_wh += P_sol_liq; 
                 if (bateria_atual_wh > capacidade_bateria_wh) bateria_atual_wh = capacidade_bateria_wh;
                 tempo_total_horas += 1.0;
                 continue; // Pula para a próxima hora
            }

            // Consumo de Energia
            double T_asf = temperatura_asfalto(hora, sol.T_amb);
            double P_res = calcular_potencia_resistiva(v_ms, M_total, CdA_total, T_asf);
            
            // Eficiência total do sistema
            double eta = EFF_MPPT * EFF_DRIVER * eficiencia_motor(P_res) * EFF_TRANS;
            double P_bat = (eta > 1e-6) ? (P_res / eta) : 1e6; // Evita divisão por zero
            
            // Balanço Energético (Entra Sol - Sai Motor)
            double balanco = P_sol_liq - P_bat;
            
            // Verificação de pane seca no meio da hora
            if (balanco < 0 && fabs(balanco) > bateria_atual_wh) {
                // Carro parou no meio da hora
                double frac = bateria_atual_wh / fabs(balanco);
                distancia_total_km += v_kmh * frac;
                tempo_total_horas += 1.0; // Conta a hora inteira (penalidade implicita)
                bateria_atual_wh = 0;
            } else {
                // Hora completada com sucesso
                bateria_atual_wh += balanco;
                // Clamp na capacidade máxima (não sobrecarrega)
                if (bateria_atual_wh > capacidade_bateria_wh) bateria_atual_wh = capacidade_bateria_wh;
                distancia_total_km += v_kmh;
                tempo_total_horas += 1.0;
            }
            
            // Chegada na meta
            if (distancia_total_km >= 3000.0) break;
        }
        // Penalidade noturna (15h paradas + pernoite) se não terminou a corrida
        if (distancia_total_km < 3000.0) tempo_total_horas += 15.0;
    }

    // --- 4. Impressão Formatada no Console ---
    printf("\n====================================================\n");
    printf(" RELATORIO FINAL - OTIMIZACAO (OUTRIGGER v7.2 - MODULAR)\n");
    printf(" (Fisica: Assimétrica Corrigida | Motor: Adaptativo)\n");
    printf("====================================================\n");
    
    // Bloco de Dados Físicos
    printf("\n--- (Itens 1-22) ESPECIFICACOES DO VEICULO OTIMO ---\n");
    printf(" 1) Nome do Carro:           Só Roda De Dia - OUTRIGGER %s\n", time_buffer);
    printf(" 18) Massa Total (kg):       %.2f\n", M_total);
    printf(" 19) Coef. de Arrasto (Cd):  %.4f\n", Cd);
    printf("     - CdA Casco (norm):     %.4f\n", Cd_casco_norm);
    printf("     - CdA Pod (norm):       %.4f\n", Cd_pod_norm);
    printf("     - CdA Asa (norm):       %.4f\n", Cd_wing_norm);
    printf(" 20) Crr base:               %.5f (Dinamico)\n", CR_ROLLING_BASE);
    printf(" 21) Area Frontal (m2):      %.3f\n", A_frontal_total);
    printf("     - A. Frontal Casco:     %.3f m2\n", A_frontal_casco);
    printf("     - A. Frontal Pod:       %.3f m2\n", A_frontal_pod);
    printf(" 22) Capacidade Bateria:     %.2f kWh (limite: 3.056 kWh)\n", CAPACIDADE_BATERIA_KWH);

    // Bloco de Geometria
    printf("\n--- GEOMETRIA OUTRIGGER OTIMIZADA ---\n");
    printf(" - Area Painel Solar:        %.2f m2 (max %.1f)\n", car->A_solar, MAX_SOLAR_AREA);
    printf(" - Envergadura Total (W_sep):%.2f m (max %.1f)\n", car->W_sep, MAX_VEHICLE_WIDTH);
    printf(" - Corda Media da Asa:       %.2f m\n", L_wing_chord);
    // Finura = Razão Comprimento / Largura (importante para aerodinâmica)
    printf(" - Comprimento Casco:        %.2f m (Finura: %.2f)\n", car->L_casco, car->L_casco / sqrt(car->W_casco * car->H_casco));
    printf(" - Largura Casco:            %.2f m\n", car->W_casco);
    printf(" - Altura Casco:             %.2f m\n", car->H_casco);
    printf(" - Comprimento Pod:          %.2f m (Finura: %.2f)\n", car->L_pod, car->L_pod / car->D_pod);
    printf(" - Diametro Pod:             %.2f m\n", car->D_pod);
    printf(" - Comprimento Total:        %.2f m (max %.1f)\n", fmax(car->L_casco, car->L_pod), MAX_VEHICLE_LENGTH);
    printf(" - Area Molhada Total:       %.2f m2\n", Am_casco + Am_pod + (2.0*car->A_solar));

    // Bloco de Estratégia
    printf("\n--- (Itens 31, 32) PERFORMANCE E ESTRATEGIA (3000km) ---\n");
    if (distancia_total_km >= 3000.0) {
        int dias_final = (int)(tempo_total_horas / 24);
        double horas_final = fmod(tempo_total_horas, 24);
        printf(" 31) Tempo p/ 3000km:        %d dias e %.1f horas\n", dias_final, horas_final);
        printf("     Velocidade media:       %.1f km/h\n", 3000.0 / tempo_total_horas);
    } else {
        printf(" 31) Corrida NAO completada. Distancia: %.2f km\n", distancia_total_km);
    }
    
    printf(" 32) Perfil de Velocidade Otimizado (km/h):\n");
    for (int i = 0; i < 9; i++) {
        SolarData sol = get_solar_data(i);
        // Exibe a estratégia horária junto com as condições ambientais daquele horário
        printf("     %02d-%02d h: %5.1f km/h (GHI: %6.1f W/m2, T: %.1fC)\n", 
               i + 8, i + 9, strategy->genes[i] * 3.6, sol.irradiance, sol.T_amb);
    }

    // Executa os testes extras (Item 29 e 30) definidos nas funções static acima
    relatorio_autonomia_sem_sol(M_total, CdA_total);
    relatorio_velocidade_maxima(M_total, CdA_total);
}

// Wrapper público para chamar apenas as métricas extras se necessário, sem imprimir o relatório todo
void calculate_extra_metrics(const CarDesignOutrigger* car, const double M_total, const double CdA_total) {
    relatorio_autonomia_sem_sol(M_total, CdA_total);
    relatorio_velocidade_maxima(M_total, CdA_total);
}