#ifndef REPORTS_H
#define REPORTS_H

#include "physics.h"
#include "ga_engine.h"

// Assinatura corrigida com os parâmetros extras
void print_final_summary(const CarDesignOutrigger* car, const Individual* strategy,
                         const double M_total, const double Cd, const double CdA_total, const double A_frontal_total);

void calculate_extra_metrics(const CarDesignOutrigger* car, const double M_total, const double CdA_total);

#endif