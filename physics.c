#include <math.h>
#include <float.h>
#include "physics.h"

// --- DADOS SOLARES (Irradiância W/m2 e Temp Ambiente C) ---
SolarData get_solar_data(int hora_do_dia) {
    // Dados aproximados de um dia de verão (8h as 17h)
    static const SolarData data[9] = {
        {188.2, 20.0}, // 08:00
        {353.8, 21.5}, // 09:00
        {486.1, 23.0}, // 10:00
        {566.6, 24.0}, // 11:00
        {586.2, 25.0}, // 12:00 (Pico)
        {542.6, 25.5}, // 13:00
        {440.7, 25.0}, // 14:00
        {292.7, 23.5}, // 15:00
        {122.7, 21.0}  // 16:00
    };
    if (hora_do_dia < 0 || hora_do_dia > 8) return (SolarData){0.0, 25.0};
    return data[hora_do_dia];
}

// --- CÁLCULO DE POTÊNCIA SOLAR ---
double calcular_potencia_solar(double irradiance, double A_solar, double T_amb) {
    if (irradiance < 1e-3) return 0.0;
    // Modelo simplificado com coeficiente de temperatura
    double T_celula = T_amb + (NOCT - 20) * (irradiance / 800.0);
    double eta_painel = EFF_PANEL_REF * (1 + PANEL_TEMP_COEFF * (T_celula - 25));
    return irradiance * A_solar * eta_painel;
}

// --- CÁLCULO DE CRR (Resistência ao Rolamento Dinâmica) ---
double calcular_crr_dinamico(double v_kmh, double T_asfalto_C) {
    double Crr = CR_ROLLING_BASE;
    // Fator de velocidade
    Crr *= (1 + CR_SPEED_COEFF * v_kmh);
    // Fator de temperatura (Pneu mais quente = menor resistência)
    Crr *= (1 + CR_TEMP_COEFF * (T_asfalto_C - 25));
    return Crr;
}

// --- ESTIMATIVA DE TEMP DO ASFALTO ---
double temperatura_asfalto(int hora_do_dia, double T_amb) {
    // Modelo senoidal simples: asfalto esquenta mais que o ar
    double hora_float = hora_do_dia + 8.0;
    double delta_T = 20.0 * sin(PI * (hora_float - 6) / 12.0);
    return T_amb + fmax(0, delta_T);
}

// --- MAPA DE EFICIÊNCIA DO MOTOR ---
double eficiencia_motor(double P_resist) {
    // Curva de eficiência baseada na carga (Load)
    double carga = P_resist / P_MOTOR_NOMINAL;
    double eta_m;
    
    if (carga < 0.2) eta_m = 0.80 + 0.10 * (carga / 0.2);       // Carga muito baixa (ruim)
    else if (carga < 0.8) eta_m = 0.90 + 0.05 * ((carga - 0.2) / 0.6); // Zona ótima
    else if (carga <= 2.5) eta_m = 0.95 - 0.05 * ((carga - 0.8) / 1.7); // Sobrecarga
    else eta_m = 0.70; // Saturação
    
    return fmin(0.95, fmax(0.70, eta_m));
}

// --- AERODINÂMICA (ARRASTO) ---
double calcular_drag_body(double L, double W, double H, double v_ms, double* A_molhada_out) {
    // 1. Área Frontal (Elipse aproximada)
    double A_frontal = PI/4 * W * H;
    if (A_frontal < 1e-6) { *A_molhada_out = 0; return 0; }

    // 2. Área Molhada (Aproximação de Knud Thomsen para elipsoides)
    double p = 1.6075;
    double a = L/2, b = W/2, c = H/2;
    *A_molhada_out = 4 * PI * pow((pow(a*b,p) + pow(a*c,p) + pow(b*c,p))/3.0, 1.0/p);
    
    // 3. Número de Reynolds
    double Re = fmax(1.0, (RHO_AIR * v_ms * L) / MU_AIR);

    // 4. Coeficiente de Forma (Cd_pressure) baseado na Finura (L/D_equiv)
    double finura = L / sqrt(W * H);
    double Cd_forma;
    
    // Corpos mais esbeltos (finura alta) têm menor arrasto de pressão
    if (finura > 8.0) Cd_forma = 0.04;
    else if (finura > 4.0) Cd_forma = 0.04 + 0.02 * (8.0 - finura) / 4.0;
    else Cd_forma = 0.06 + 0.04 * (4.0 - finura) / 2.0;
    
    // 5. Coeficiente de Atrito (Skin Friction)
    // Transição laminar-turbulenta
    double frac_laminar = fmin(0.3, RE_CRIT / Re); // Quanto do corpo tem fluxo laminar?
    
    double Cf_lam = 1.328 / sqrt(Re);
    double Cf_turb = 0.074 / pow(Re, 0.2);
    
    double Cf = frac_laminar * Cf_lam + (1 - frac_laminar) * Cf_turb;
    
    // 6. Cd Total referenciado à área frontal
    double Cd_atrito = Cf * (*A_molhada_out / A_frontal);
    
    return (Cd_forma + Cd_atrito) * A_frontal; // Retorna CdA (m2)
}

// --- POTÊNCIA RESISTIVA TOTAL ---
double calcular_potencia_resistiva(double v_ms, double M_total, double CdA_total, double T_amb_pneu) {
    double v_kmh = v_ms * 3.6;
    
    // Força Aerodinâmica
    double F_arrasto = 0.5 * RHO_AIR * CdA_total * pow(v_ms, 2);
    
    // Força de Rolamento
    double Crr = calcular_crr_dinamico(v_kmh, T_amb_pneu);
    double F_rolamento = Crr * M_total * GRAVITY;
    
    // Potência = Força Total * Velocidade
    return (F_arrasto + F_rolamento) * v_ms;
}

// =============================================================================
// WRAPPERS DE FITNESS (A "Cola" entre GA e Física)
// =============================================================================

// FASE 1: OTIMIZAÇÃO DA GEOMETRIA (SHAPE)
double fitness_shape_wrapper(Individual ind, const void* param) {
    double simulated_velocity_ms = *(double*)param; // Velocidade de referência (ex: 22 m/s)
    
    // Decodifica genes
    double L_casco = ind.genes[0];
    double W_casco = ind.genes[1];
    double H_casco = ind.genes[2];
    double L_pod = ind.genes[3];
    double D_pod = ind.genes[4];
    double A_solar = ind.genes[5];
    double W_sep = ind.genes[6];

    // Restrições Geométricas Rígidas (Penalidade Morte Súbita)
    if (A_solar > MAX_SOLAR_AREA) return -DBL_MAX;
    if (fmax(L_casco, L_pod) > MAX_VEHICLE_LENGTH) return -DBL_MAX;
    if (H_casco > MAX_VEHICLE_HEIGHT) return -DBL_MAX;
    if (W_sep > MAX_VEHICLE_WIDTH) return -DBL_MAX;
    if (W_casco + D_pod + MIN_COMPONENT_SEP > W_sep) return -DBL_MAX;

    // 1. Calcula Aerodinâmica e Massa dos Componentes
    double Am_c, Am_p;
    double CdA_c = calcular_drag_body(L_casco, W_casco, H_casco, simulated_velocity_ms, &Am_c);
    double CdA_p = calcular_drag_body(L_pod, D_pod, D_pod, simulated_velocity_ms, &Am_p);
    
    // Asa de conexão (Painel Solar)
    double L_chord = A_solar / W_sep; // Corda média
    double Re_w = fmax(1.0, (RHO_AIR * simulated_velocity_ms * L_chord) / MU_AIR);
    double frac = fmin(0.3, RE_CRIT / Re_w);
    double Cf_w = frac * (1.328/sqrt(Re_w)) + (1-frac)*(0.074/pow(Re_w, 0.2));
    double CdA_w = Cf_w * (2.0 * A_solar) * 0.5; // Fator 0.5 pois é placa plana fina

    // Soma tudo (com fator de interferência 10%)
    double CdA_tot = (CdA_c + CdA_p + CdA_w) * 1.10;
    
    // Massa Estimada
    double M_est = (RHO_CARENAGEM*(Am_c+Am_p)) + ((RHO_CHASSI+RHO_PAINEL)*A_solar);
    double M_tot = M_est + FIXED_MASS + 80.0; // +80kg Piloto

    // 2. Calcula Potências
    double Crr = calcular_crr_dinamico(simulated_velocity_ms*3.6, 25.0);
    double F_res = (0.5*RHO_AIR*CdA_tot*pow(simulated_velocity_ms, 2)) + (Crr*M_tot*GRAVITY);
    double P_res = F_res * simulated_velocity_ms;
    
    // Solar (Considera Sol de meio-dia para o design nominal)
    SolarData sol = get_solar_data(4); // 12h
    double P_sol = calcular_potencia_solar(sol.irradiance, A_solar, sol.T_amb);
    
    // Eficiência da cadeia
    double eta = EFF_MPPT * EFF_DRIVER * eficiencia_motor(P_res) * EFF_TRANS;
    double P_bat = (eta > 1e-6) ? (P_res / eta) : 1e6;
    
    // 3. Resultado: Saldo Energético (Net Power)
    double net = (P_sol * EFF_MPPT) - P_bat;
    
    // CORREÇÃO CRÍTICA: Retorna valor real, permitindo negativos para criar gradiente
    return net; 
}

// FASE 2: OTIMIZAÇÃO DE ESTRATÉGIA (3000km)
double fitness_strategy_wrapper(Individual ind, const void* param) {
    const CarDesignOutrigger* car = (CarDesignOutrigger*)param;
    double* perfil_v = ind.genes; // 9 velocidades (uma por hora)
    
    double dist = 0, tempo = 0;
    double cap_bat = CAPACIDADE_BATERIA_KWH * 1000.0; // Wh
    double bat_atual = cap_bat;
    int dias = 0;

    // Recalcula física do carro fixo (uma única vez para economizar CPU)
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

    // Simulação dia após dia até completar 3000km
    while (dist < 3000.0 && dias < 10) { // Limite de 10 dias para não loopar infinito
        dias++;
        for (int hora = 0; hora < 9; hora++) {
            double v_ms = perfil_v[hora];
            double v_kmh = v_ms * 3.6;
            
            // Dados ambientais da hora
            SolarData sol = get_solar_data(hora);
            double P_sol_liq = calcular_potencia_solar(sol.irradiance, car->A_solar, sol.T_amb) * EFF_MPPT;

            // Se bateria vazia (<1%), fica parado carregando
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
            
            double balanco = P_sol_liq - P_bat;
            
            // Verifica se a bateria aguenta a hora inteira
            if (balanco < 0 && fabs(balanco) > bat_atual) {
                // Morreu no meio da hora
                double f_h = bat_atual / fabs(balanco); // Fração da hora que andou
                dist += v_kmh * f_h;
                tempo += 1.0; // O tempo passa igual (ficou parado o resto da hora)
                bat_atual = 0;
            } else {
                // Hora completa
                bat_atual += balanco;
                if (bat_atual > cap_bat) bat_atual = cap_bat;
                dist += v_kmh;
                tempo += 1.0;
            }
            if (dist >= 3000.0) break;
        }
        // Penalidade noturna (15h de noite)
        if (dist < 3000.0) tempo += 15.0;
    }

    // Retorna pontuação baseada no tempo (Menor tempo = Maior Fitness)
    // Usamos inversão: Fitness = Constante - Tempo
    // Ou melhor: Fitness = Velocidade Média Global
    if (dist >= 3000.0) return 3000.0 + (1000.0 / tempo); // Bônus por terminar rápido
    return dist; // Se não terminou, o fitness é a distância (incentiva ir mais longe)
}

// FASE 3: ALCANCE DIÁRIO (Item 28)
double fitness_strategy_daily_wrapper(Individual ind, const void* param) {
    const CarDesignOutrigger* car = (CarDesignOutrigger*)param;
    double* perfil_v = ind.genes;
    double dist = 0;
    double cap_bat = CAPACIDADE_BATERIA_KWH * 1000.0;
    double bat_atual = cap_bat;
    
    // Recálculo físico simplificado (copiado da lógica acima)
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

    // Simula apenas 1 dia (9h)
    for (int hora = 0; hora < 9; hora++) {
        double v_ms = perfil_v[hora];
        double v_kmh = v_ms * 3.6;
        SolarData sol = get_solar_data(hora);
        double P_sol_liq = calcular_potencia_solar(sol.irradiance, car->A_solar, sol.T_amb) * EFF_MPPT;

        if (bat_atual <= 0.01 * cap_bat) {
             bat_atual += P_sol_liq;
             if (bat_atual > cap_bat) bat_atual = cap_bat;
             continue;
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
    
    // Penalidade se terminar com bateria < 30%
    double limite_minimo_wh = cap_bat * 0.30;
    
    if (bat_atual >= limite_minimo_wh) {
        // Cumpriu a meta: Fitness = Distância
        double erro_positivo_wh = bat_atual - limite_minimo_wh;
        // Pequena penalidade por sobrar demais (queremos usar tudo até 30%)
        return dist - (erro_positivo_wh * 0.1);
    } else {
        // Falhou na meta: Penalidade negativa baseada na falta de energia
        return (bat_atual - limite_minimo_wh); 
    }
}