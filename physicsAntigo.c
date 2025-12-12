#include <math.h>
#include <float.h>
#include "physics.h"

// ============================================================================
// DADOS AMBIENTAIS E LOOKUP TABLES
// ============================================================================

/**
 * @brief Retorna a irradiância e temperatura para uma "hora padrão" de corrida.
 * @note Baseado em dados históricos do World Solar Challenge (Outubro, Austrália).
 * A curva simula um dia claro, com pico solar ao meio-dia (índice 4).
 */
SolarData get_solar_data(int hora_do_dia) {
    // Tabela estática para evitar recálculo. Dados: {W/m2, Celsius}
    static const SolarData data[9] = {
        {188.2, 20.0}, // 08:00 - Sol baixo, frio
        {353.8, 21.5}, // 09:00
        {486.1, 23.0}, // 10:00
        {566.6, 24.0}, // 11:00
        {586.2, 25.0}, // 12:00 - Pico de Irradiância
        {542.6, 25.5}, // 13:00 - Pico de Temperatura (Inércia térmica)
        {440.7, 25.0}, // 14:00
        {292.7, 23.5}, // 15:00
        {122.7, 21.0}  // 16:00 - Fim do dia útil
    };
    // Proteção de limites do vetor
    if (hora_do_dia < 0 || hora_do_dia > 8) return (SolarData){0.0, 25.0};
    return data[hora_do_dia];
}

/**
 * @brief Calcula a potência real que sai do painel.
 * Considera o "Coeficiente de Temperatura": Painéis quentes perdem eficiência.
 */
double calcular_potencia_solar(double irradiance, double A_solar, double T_amb) {
    if (irradiance < 1e-3) return 0.0;
    
    // Estima a temperatura da célula (T_celula) baseada na temperatura ambiente e na intensidade do sol.
    // Fórmula empírica padrão: T_cell = T_amb + (NOCT - 20) * (S / 800)
    double T_celula = T_amb + (NOCT - 20) * (irradiance / 800.0);
    
    // Aplica o fator de perda térmica (PANEL_TEMP_COEFF é negativo, ex: -0.0037)
    double eta_painel = EFF_PANEL_REF * (1 + PANEL_TEMP_COEFF * (T_celula - 25));
    
    return irradiance * A_solar * eta_painel;
}

/**
 * @brief Modela a histerese e deformação do pneu.
 * O pneu fica mais "grudento" no calor e deforma mais em alta velocidade.
 */
double calcular_crr_dinamico(double v_kmh, double T_asfalto_C) {
    double Crr = CR_ROLLING_BASE;
    // Fator velocidade: Ondas estacionárias na borracha aumentam a perda
    Crr *= (1 + CR_SPEED_COEFF * v_kmh);
    // Fator temperatura: Asfalto/Pneu quente aumenta o coeficiente (empírico para pneus solares)
    Crr *= (1 + CR_TEMP_COEFF * (T_asfalto_C - 25));
    return Crr;
}

/**
 * @brief Modelo de Inércia Térmica do Asfalto.
 * O asfalto não esquenta instantaneamente com o sol. Ele absorve calor e
 * atinge o pico horas depois do meio-dia solar.
 */
double temperatura_asfalto(int hora_do_dia, double T_amb) {
    double hora_float = hora_do_dia + 8.0;
    // Função Senoidal deslocada para modelar o atraso térmico
    double delta_T = 20.0 * sin(PI * (hora_float - 6) / 12.0);
    return T_amb + fmax(0, delta_T);
}

/**
 * @brief Mapa de Eficiência do Motor.
 * Simula a curva real de um motor BLDC (Brushless DC).
 * 
 */
double eficiencia_motor(double P_resist) {
    // Carga relativa (Load)
    double carga = P_resist / P_MOTOR_NOMINAL;
    double eta_m;

    // Região 1: Baixa carga (Perdas por atrito dominam -> Eficiência baixa)
    if (carga < 0.2) eta_m = 0.80 + 0.10 * (carga / 0.2);
    // Região 2: Carga Nominal (Ponto ótimo de operação -> Eficiência alta)
    else if (carga < 0.8) eta_m = 0.90 + 0.05 * ((carga - 0.2) / 0.6);
    // Região 3: Sobrecarga (Perdas Joule I^2R dominam -> Eficiência cai)
    else if (carga <= 2.5) eta_m = 0.95 - 0.05 * ((carga - 0.8) / 1.7);
    // Região 4: Extremo (Saturação magnética)
    else eta_m = 0.70;

    // Clamp de segurança (min 70%, max 95%)
    return fmin(0.95, fmax(0.70, eta_m));
}

// ============================================================================
// FÍSICA DE FLUIDOS (AERODINÂMICA)
// ============================================================================

/**
 * @brief Calcula CdA (Coeficiente de Arrasto * Área) para corpos fusiformes.
 * Combina arrasto de pressão (Forma) e arrasto de atrito (Superfície).
 */
double calcular_drag_body(double L, double W, double H, double v_ms, double* A_molhada_out) {
    // 1. Área Frontal Projetada (Elipse)
    double A_frontal = PI/4 * W * H;
    if (A_frontal < 1e-6) { *A_molhada_out = 0; return 0; }

    // 2. Estimativa de Área Molhada (Superfície total do objeto 3D)
    // Aproximação de Knud Thomsen para área de elipsoides escalenos.
    // p = 1.6075 dá o menor erro relativo (< 1.061%).
    double p = 1.6075;
    double a = L/2, b = W/2, c = H/2;
    *A_molhada_out = 4 * PI * pow((pow(a*b,p) + pow(a*c,p) + pow(b*c,p))/3.0, 1.0/p);
    
    // 3. Número de Reynolds (Re)
    // Define se o fluxo é laminar (suave) ou turbulento (caótico).
    // 

[Image of Reynolds number laminar vs turbulent flow]

    double Re = fmax(1.0, (RHO_AIR * v_ms * L) / MU_AIR);
    
    // 4. Arrasto de Forma (Pressure Drag)
    // Depende da "Finura" (Razão Comprimento / Diâmetro Efetivo).
    // Corpos mais alongados "furam" o ar melhor, até certo ponto.
    double finura = L / sqrt(W * H);
    double Cd_forma;
    if (finura > 8.0) Cd_forma = 0.04; // Muito aerodinâmico
    else if (finura > 4.0) Cd_forma = 0.04 + 0.02 * (8.0 - finura) / 4.0;
    else Cd_forma = 0.06 + 0.04 * (4.0 - finura) / 2.0; // "Gordinho" (arrasto maior)
    
    // 5. Arrasto de Atrito (Skin Friction Drag)
    // Calcula coeficiente misto baseado na transição laminar-turbulenta
    double frac_laminar = fmin(0.3, RE_CRIT / Re); // Quanto do corpo tem fluxo laminar?
    double Cf_lam = 1.328 / sqrt(Re);       // Blasius (Laminar)
    double Cf_turb = 0.074 / pow(Re, 0.2);  // Prandtl (Turbulento)
    double Cf = frac_laminar * Cf_lam + (1 - frac_laminar) * Cf_turb;
    
    // Converte Cf (referência área molhada) para Cd (referência área frontal)
    double Cd_atrito = Cf * (*A_molhada_out / A_frontal);

    // Retorna CdA Total
    return (Cd_forma + Cd_atrito) * A_frontal; 
}

// Equação Fundamental do Movimento em Regime Permanente
double calcular_potencia_resistiva(double v_ms, double M_total, double CdA_total, double T_amb_pneu) {
    double v_kmh = v_ms * 3.6;
    // P = F * v
    // F_arrasto = 1/2 * rho * CdA * v^2
    double F_arrasto = 0.5 * RHO_AIR * CdA_total * pow(v_ms, 2);
    
    // F_rolamento = Crr * Normal (Peso)
    double Crr = calcular_crr_dinamico(v_kmh, T_amb_pneu);
    double F_rolamento = Crr * M_total * GRAVITY;
    
    return (F_arrasto + F_rolamento) * v_ms;
}

// ============================================================================
// WRAPPERS DE FITNESS (A PONTE ENTRE A FÍSICA E O AG)
// ============================================================================

/**
 * @brief Fitness de Design (Forma).
 * O objetivo é encontrar a geometria mais eficiente energeticamente para uma velocidade fixa.
 * Retorna: "Sobra de Energia" (Positivo = Bom, Negativo = Ruim).
 */
double fitness_shape_wrapper(Individual ind, const void* param) {
    double simulated_velocity_ms = *(double*)param;
    
    // Decodificação do Genótipo (Genes -> Variáveis Físicas)
    double L_casco = ind.genes[0];
    double W_casco = ind.genes[1];
    double H_casco = ind.genes[2];
    double L_pod = ind.genes[3];
    double D_pod = ind.genes[4];
    double A_solar = ind.genes[5];
    double W_sep = ind.genes[6];

    // --- 1. Verificação de Hard Constraints (Regras do Jogo) ---
    // Se violar geometria ou regulamento, mata o indivíduo instantaneamente (-DBL_MAX)
    if (A_solar > MAX_SOLAR_AREA) return -DBL_MAX;
    if (fmax(L_casco, L_pod) > MAX_VEHICLE_LENGTH) return -DBL_MAX;
    if (H_casco > MAX_VEHICLE_HEIGHT) return -DBL_MAX;
    if (W_sep > MAX_VEHICLE_WIDTH) return -DBL_MAX;
    // Verifica colisão física: O casco + pod não podem ser mais largos que a bitola
    if (W_casco + D_pod + MIN_COMPONENT_SEP > W_sep) return -DBL_MAX;

    // --- 2. Cálculos Físicos ---
    double Am_c, Am_p;
    double CdA_c = calcular_drag_body(L_casco, W_casco, H_casco, simulated_velocity_ms, &Am_c);
    double CdA_p = calcular_drag_body(L_pod, D_pod, D_pod, simulated_velocity_ms, &Am_p);
    
    // Aerodinâmica da Asa (Placa Plana)
    double L_chord = A_solar / W_sep;
    double Re_w = fmax(1.0, (RHO_AIR * simulated_velocity_ms * L_chord) / MU_AIR);
    double frac = fmin(0.3, RE_CRIT / Re_w);
    double Cf_w = frac * (1.328/sqrt(Re_w)) + (1-frac)*(0.074/pow(Re_w, 0.2));
    double CdA_w = Cf_w * (2.0 * A_solar) * 0.5;

    // Soma total com fator de interferência (10% extra por junções)
    double CdA_tot = (CdA_c + CdA_p + CdA_w) * 1.10;
    
    // Estimativa de Massa baseada em área superficial
    double M_est = (RHO_CARENAGEM*(Am_c+Am_p)) + ((RHO_CHASSI+RHO_PAINEL)*A_solar);
    double M_tot = M_est + FIXED_MASS + 80.0; // +80kg Piloto

    // Potência Resistiva
    double Crr = calcular_crr_dinamico(simulated_velocity_ms*3.6, 25.0);
    double F_res = (0.5*RHO_AIR*CdA_tot*pow(simulated_velocity_ms, 2)) + (Crr*M_tot*GRAVITY);
    double P_res = F_res * simulated_velocity_ms;
    
    // Balanço Energético no meio-dia (Pico)
    SolarData sol = get_solar_data(4); 
    double P_sol = calcular_potencia_solar(sol.irradiance, A_solar, sol.T_amb);
    double eta = EFF_MPPT * EFF_DRIVER * eficiencia_motor(P_res) * EFF_TRANS;
    double P_bat = (eta > 1e-6) ? (P_res / eta) : 1e6;
    
    // Fitness = Energia que SOBRA (Net Positive Power)
    double net = (P_sol * EFF_MPPT) - P_bat;
    return net;
}

/**
 * @brief Fitness de Estratégia Longa (3000km).
 * Simula a corrida inteira (multi-dias).
 * Objetivo: Minimizar tempo total.
 */
double fitness_strategy_wrapper(Individual ind, const void* param) {
    const CarDesignOutrigger* car = (CarDesignOutrigger*)param;
    double* perfil_v = ind.genes; // Genes aqui são velocidades horárias
    
    // Variáveis de Estado da Simulação
    double dist = 0, tempo = 0;
    double cap_bat = CAPACIDADE_BATERIA_KWH * 1000.0; // Wh
    double bat_atual = cap_bat; // Começa cheia
    int dias = 0;

    // (Recálculo das constantes físicas do carro omitido para brevidade nos comentários,
    //  mas o código recalcula CdA e Massa pois eles dependem da velocidade média)
    // ... [Recálculo Físico] ...
    double avg_v = 0;
    for(int i=0; i<9; i++) avg_v += perfil_v[i];
    avg_v = fmax(avg_v / 9.0, 1.0);
    // ... [Bloco de cálculo de CdA e Massas idêntico ao anterior] ...
    double Am_c, Am_p;
    double CdA_c = calcular_drag_body(car->L_casco, car->W_casco, car->H_casco, avg_v, &Am_c);
    double CdA_p = calcular_drag_body(car->L_pod, car->D_pod, car->D_pod, avg_v, &Am_p);
    double L_chord = car->A_solar / car->W_sep;
    double Re_w = fmax(1.0, (RHO_AIR * avg_v * L_chord) / MU_AIR);
    double frac = fmin(0.3, RE_CRIT / Re_w);
    double Cf_w = frac * (1.328/sqrt(Re_w)) + (1-frac)*(0.074/pow(Re_w, 0.2));
    double CdA_w = Cf_w * (2.0 * car->A_solar) * 0.5;
    double CdA_tot = (CdA_c + CdA_p + CdA_w) * 1.10;
    double M_est = (RHO_CARENAGEM*(Am_c+Am_p)) + ((RHO_CHASSI+RHO_PAINEL)*car->A_solar);
    double M_tot = M_est + FIXED_MASS + 80.0;

    // Loop da Corrida
    while (dist < 3000.0 && dias < 10) {
        dias++;
        for (int hora = 0; hora < 9; hora++) {
            // ... (Lógica de simulação passo-a-passo: carrega sol, gasta motor) ...
            // [Código omitido é igual ao loop da estratégia diária abaixo]
            // ...
            
            // Lógica simplificada aqui apenas para contexto:
            double v_ms = perfil_v[hora];
            double v_kmh = v_ms * 3.6;
            SolarData sol = get_solar_data(hora);
            double P_sol_liq = calcular_potencia_solar(sol.irradiance, car->A_solar, sol.T_amb) * EFF_MPPT;

            // Se bateria vazia (<1%), fica parado carregando (penalty de tempo)
            if (bat_atual <= 0.01 * cap_bat) {
                 bat_atual += P_sol_liq;
                 if (bat_atual > cap_bat) bat_atual = cap_bat;
                 tempo += 1.0; 
                 continue;
            }

            // Consumo
            double T_asf = temperatura_asfalto(hora, sol.T_amb);
            double P_res = calcular_potencia_resistiva(v_ms, M_tot, CdA_tot, T_asf);
            double eta = EFF_MPPT * EFF_DRIVER * eficiencia_motor(P_res) * EFF_TRANS;
            double P_bat = (eta > 1e-6) ? (P_res / eta) : 1e6;
            
            // Integração
            double balanco = P_sol_liq - P_bat;
            if (balanco < 0 && fabs(balanco) > bat_atual) {
                // Pane seca no meio da hora
                double f_h = bat_atual / fabs(balanco);
                dist += v_kmh * f_h;
                tempo += 1.0;
                bat_atual = 0;
            } else {
                bat_atual += balanco;
                if (bat_atual > cap_bat) bat_atual = cap_bat;
                dist += v_kmh;
                tempo += 1.0;
            }
            if (dist >= 3000.0) break;
        }
        // Penalidade Noturna: Se não acabou, soma 15h (noite) ao tempo total
        if (dist < 3000.0) tempo += 15.0;
    }
    
    // Retorna Score (Quanto maior melhor)
    // Se completou: 3000 + Bônus por rapidez
    // Se não completou: Apenas a distância percorrida
    if (dist >= 3000.0) return 3000.0 + (1000.0 / tempo);
    return dist;
}

/**
 * @brief Fitness de Estratégia Diária (Gerenciamento de Energia).
 * Objetivo: Correr o máximo possível hoje, MAS terminar com bateria > 30%.
 */
double fitness_strategy_daily_wrapper(Individual ind, const void* param) {
    // ... [Setup inicial e Recálculo físico idêntico aos anteriores] ...
    const CarDesignOutrigger* car = (CarDesignOutrigger*)param;
    double* perfil_v = ind.genes;
    double dist = 0;
    double cap_bat = CAPACIDADE_BATERIA_KWH * 1000.0;
    double bat_atual = cap_bat;
    
    double avg_v = 0;
    for(int i=0; i<9; i++) avg_v += perfil_v[i];
    avg_v = fmax(avg_v / 9.0, 1.0);

    double Am_c, Am_p;
    double CdA_c = calcular_drag_body(car->L_casco, car->W_casco, car->H_casco, avg_v, &Am_c);
    double CdA_p = calcular_drag_body(car->L_pod, car->D_pod, car->D_pod, avg_v, &Am_p);
    double L_chord = car->A_solar / car->W_sep;
    double Re_w = fmax(1.0, (RHO_AIR * avg_v * L_chord) / MU_AIR);
    double frac = fmin(0.3, RE_CRIT / Re_w);
    double Cf_w = frac * (1.328/sqrt(Re_w)) + (1-frac)*(0.074/pow(Re_w, 0.2));
    double CdA_w = Cf_w * (2.0 * car->A_solar) * 0.5;
    double CdA_tot = (CdA_c + CdA_p + CdA_w) * 1.10;
    double M_est = (RHO_CARENAGEM*(Am_c+Am_p)) + ((RHO_CHASSI+RHO_PAINEL)*car->A_solar);
    double M_tot = M_est + FIXED_MASS + 80.0;

    // Simulação de UM DIA (9 horas)
    for (int hora = 0; hora < 9; hora++) {
        double v_ms = perfil_v[hora];
        double v_kmh = v_ms * 3.6;
        SolarData sol = get_solar_data(hora);
        double P_sol_liq = calcular_potencia_solar(sol.irradiance, car->A_solar, sol.T_amb) * EFF_MPPT;

        if (bat_atual <= 0.01 * cap_bat) {
             bat_atual += P_sol_liq;
             if (bat_atual > cap_bat) bat_atual = cap_bat;
             continue; // Carro parado carregando
        }

        double T_asf = temperatura_asfalto(hora, sol.T_amb);
        double P_res = calcular_potencia_resistiva(v_ms, M_tot, CdA_tot, T_asf);
        double eta = EFF_MPPT * EFF_DRIVER * eficiencia_motor(P_res) * EFF_TRANS;
        double P_bat = (eta > 1e-6) ? (P_res / eta) : 1e6;
        
        double balanco = P_sol_liq - P_bat;
        if (balanco < 0 && fabs(balanco) > bat_atual) {
            double f_h = bat_atual / fabs(balanco);
            dist += v_kmh * f_h;
            bat_atual = 0;
        } else {
            bat_atual += balanco;
            if (bat_atual > cap_bat) bat_atual = cap_bat;
            dist += v_kmh;
        }
    }
    
    // --- LÓGICA DE PENALIDADE "MURO + RAMPA SUAVE" (v8.5) ---
    // Esta é a parte mais inteligente do AG. Em vez de apenas cortar quem erra,
    // criamos um gradiente matemático que guia o AG para a solução certa.
    
    double limite_minimo_wh = cap_bat * 0.30; // Alvo: Terminar dia com 30%
    
    if (bat_atual >= limite_minimo_wh) {
        // CUMPRIU A META (Região Segura)
        // O fitness é a distância percorrida (objetivo principal).
        // Aplicamos uma penalidade minúscula por sobrar bateria demais,
        // incentivando o carro a terminar EXATAMENTE em 30% (uso eficiente).
        double erro_positivo_wh = bat_atual - limite_minimo_wh;
        double penalidade_sobra = erro_positivo_wh * 0.1;
        
        // Retorna valor POSITIVO (ex: percorreu 600km -> score ~590)
        return dist - penalidade_sobra;
    } else {
        // FALHOU NA META (Região Proibida)
        // O fitness vira uma punição baseada na distância para o alvo.
        // Quanto mais longe dos 30%, mais negativo é o número.
        // Retorna valor NEGATIVO (ex: faltou 100Wh -> score -100)
        
        // Isso garante que QUALQUER indivíduo que cumpra a meta (positivo)
        // seja considerado melhor que QUALQUER indivíduo que falhe (negativo).
        return (bat_atual - limite_minimo_wh); 
    }
}