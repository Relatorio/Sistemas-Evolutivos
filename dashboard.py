"""
dashboard_evolution.py

Script de visualização de dados para o Algoritmo Genético do Carro Solar.
Este script lê arquivos CSV gerados pela simulação em C e gera um painel 
comparativo (Dashboard) com métricas de Fitness, Diversidade e Controle.

Dependências:
    - pandas: Manipulação de dados (CSV)
    - matplotlib: Criação dos gráficos
"""

import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

# ==============================================================================
# --- CONFIGURAÇÃO DO USUÁRIO ---
# ==============================================================================
# Controle mestre da escala do eixo X (Gerações).
# True  = Escala Logarítmica: Ideal para ver detalhes do início da evolução (convergência rápida).
# False = Escala Linear: Ideal para ter noção da duração real e estabilidade a longo prazo.
USE_LOG_SCALE = True 
# ==============================================================================

# Configuração global de estilo do Matplotlib para gráficos mais bonitos e legíveis
plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams.update({'font.size': 10, 'font.family': 'sans-serif'})

def plot_phase_data(ax_col, filename, phase_title):
    """
    Processa um arquivo CSV e plota os 3 gráficos de uma fase específica.

    Args:
        ax_col (list): Lista com os 3 objetos Axes (subplots) onde os dados serão desenhados.
        filename (str): Caminho do arquivo CSV (ex: 'fase1.csv').
        phase_title (str): Título principal desta coluna de gráficos (ex: 'Fase 1').
    """
    
    # 1. Validação de Arquivo
    # Verifica se o arquivo existe antes de tentar ler para evitar crash feio.
    if not os.path.exists(filename):
        for ax in ax_col:
            # Escreve uma mensagem de erro visual dentro do gráfico vazio
            ax.text(0.5, 0.5, f"Arquivo '{filename}' não encontrado", 
                    ha='center', va='center', transform=ax.transAxes, color='red')
        return

    try:
        df = pd.read_csv(filename)
    except Exception as e:
        print(f"Erro crítico ao ler {filename}: {e}")
        return

    # Extração de dados comuns para facilitar a leitura
    geracao = df['Geracao']
    
    # Filtra onde ocorreram 'RESETS' (reinícios de população) para marcar no gráfico
    resets = df[df['Evento'].str.contains('RESET', na=False)]['Geracao']

    # --------------------------------------------------------------------------
    # GRÁFICO 1: FITNESS (Desempenho)
    # Mostra a evolução da qualidade da solução.
    # --------------------------------------------------------------------------
    ax1 = ax_col[0]
    # Plota o "campeão" da geração
    ax1.plot(geracao, df['MelhorFitness'], label='Melhor Indivíduo', color='#1f77b4', linewidth=1.5)
    # Plota a média da população
    ax1.plot(geracao, df['FitnessMedio'], label='Média', color='#ff7f0e', linestyle='--', linewidth=1, alpha=0.9)
    
    # Área sombreada: Representa o Desvio Padrão.
    # Útil para ver se a população está muito homogênea (faixa estreita) ou variada (faixa larga).
    ax1.fill_between(geracao, df['FitnessMedio'] - df['DesvioPadraoFit'], 
                     df['FitnessMedio'] + df['DesvioPadraoFit'], color='#ff7f0e', alpha=0.2)
    
    ax1.set_title(f"{phase_title}\nEvolução do Fitness", fontsize=11, fontweight='bold')
    ax1.set_ylabel("Fitness")
    ax1.legend(loc='lower right', fontsize=8, frameon=True, framealpha=0.9)
    ax1.grid(True, which="both", linestyle=':', alpha=0.6)

    # --------------------------------------------------------------------------
    # GRÁFICO 2: DIVERSIDADE (Convergência)
    # Mostra se a população ainda está explorando ou se já convergiu.
    # --------------------------------------------------------------------------
    ax2 = ax_col[1]
    ax2.plot(geracao, df['DiversidadeGenetica'], color='#9467bd', linewidth=1.2)
    ax2.set_title("Convergência (Diversidade Genética)", fontsize=10)
    ax2.set_ylabel("Distância Euclidiana")
    
    # Marcadores visuais de RESET
    # Se houve muitos resets, afinamos a linha para o gráfico não virar um borrão vermelho.
    if len(resets) > 0:
        if USE_LOG_SCALE or len(resets) > 50:
            lw, alp = 0.5, 0.3 # Linhas sutis
        else:
            lw, alp = 1.0, 0.6 # Linhas fortes
            
        for r in resets:
            ax2.axvline(x=r, color='red', linestyle='-', alpha=alp, linewidth=lw)
            
    ax2.grid(True, which="both", linestyle=':', alpha=0.6)

    # --------------------------------------------------------------------------
    # GRÁFICO 3: CONTROLE (Mutação e Repulsão)
    # Usa Eixo Duplo (Twin Axis) para mostrar duas grandezas diferentes no mesmo gráfico.
    # --------------------------------------------------------------------------
    ax3 = ax_col[2]
    
    # Eixo Y da Esquerda (Mutação)
    ln1 = ax3.plot(geracao, df['TaxaMutacao'], color='#2ca02c', label='Mutação %', linewidth=1.2)
    ax3.set_ylabel("Mutação (%)", color='#2ca02c')
    ax3.tick_params(axis='y', labelcolor='#2ca02c') # Pinta os números do eixo da mesma cor da linha
    
    # Define label X dinamicamente
    xlabel_text = "Gerações (Escala Log)" if USE_LOG_SCALE else "Gerações (Escala Linear)"
    ax3.set_xlabel(xlabel_text)
    
    # Eixo Y da Direita (Repulsão) - Cria um eixo gêmeo que compartilha o mesmo X
    ax3_r = ax3.twinx()
    ln2 = ax3_r.plot(geracao, df['FatorRepulsao'], color='#d62728', linestyle='-.', alpha=0.5, linewidth=1.2, label='Repulsão')
    ax3_r.set_ylabel("Fator Repulsão", color='#d62728')
    ax3_r.tick_params(axis='y', labelcolor='#d62728')
    ax3_r.set_ylim(0, 1.1) # Fixa escala de 0 a 1.1 para facilitar leitura

    # Truque para juntar legendas de dois eixos diferentes numa caixa só
    lns = ln1 + ln2
    labs = [l.get_label() for l in lns]
    ax3.legend(lns, labs, loc='upper left', fontsize=8, frameon=True, framealpha=0.9)
    ax3.grid(True, which="both", linestyle=':', alpha=0.6)

    # --------------------------------------------------------------------------
    # LÓGICA DE ESCALA
    # --------------------------------------------------------------------------
    if USE_LOG_SCALE:
        # Escala Log exige cuidado: não existe log(0). Começamos de 1.
        ax1.set_xscale('log')
        ax2.set_xscale('log')
        ax3.set_xscale('log')
        ax3.set_xlim(left=1, right=max(geracao))
    else:
        ax1.set_xscale('linear')
        ax2.set_xscale('linear')
        ax3.set_xscale('linear')
        ax3.set_xlim(left=0, right=max(geracao))

# ==============================================================================
# --- EXECUÇÃO PRINCIPAL ---
# ==============================================================================

# Cria a figura e uma grade de subplots (3 linhas, 2 colunas)
# sharex='col' faz com que dar zoom em um gráfico aplique o zoom nos outros da mesma coluna
fig, axes = plt.subplots(nrows=3, ncols=2, figsize=(16, 12), sharex='col')

# Ajusta espaço vertical entre os gráficos
plt.subplots_adjust(hspace=0.3)

# Processa a Coluna da Esquerda (Fase 1 - Design)
# Passamos axes[:, 0] que pega todas as linhas da coluna 0
plot_phase_data(axes[:, 0], 'fase1.csv', 'FASE 1: Design (Geometria)')

# Processa a Coluna da Direita (Fase 2 - Estratégia)
# Passamos axes[:, 1] que pega todas as linhas da coluna 1
plot_phase_data(axes[:, 1], 'fase2.csv', 'FASE 2: Estratégia (3000km)')

# Título Geral do Painel
mode_str = "Logarítmica" if USE_LOG_SCALE else "Linear"
fig.suptitle(f'Painel de Otimização Evolutiva - Carro Solar (Escala {mode_str})', fontsize=16, y=0.98)

# Tight Layout ajusta as margens para nada ficar cortado
plt.tight_layout(rect=[0, 0.03, 1, 0.97])

# Salva o arquivo final
filename_out = f'dashboard_carro_solar_{"log" if USE_LOG_SCALE else "linear"}.png'
plt.savefig(filename_out, dpi=150)
print(f"Gráfico salvo com sucesso: {filename_out}")