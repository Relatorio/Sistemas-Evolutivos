import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

# ==============================================================================
# --- CONFIGURAÇÃO DO USUÁRIO ---
# ==============================================================================
# Mude para True para ver em escala Logarítmica (bom para ver o início detalhado)
# Mude para False para ver em escala Linear (bom para ver a duração total)
USE_LOG_SCALE = True 
# ==============================================================================

# Configuração visual
plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams.update({'font.size': 10, 'font.family': 'sans-serif'})

def plot_phase_data(ax_col, filename, phase_title):
    """Lê o CSV e plota na coluna de eixos fornecida."""
    if not os.path.exists(filename):
        for ax in ax_col:
            ax.text(0.5, 0.5, f"Arquivo '{filename}' não encontrado", 
                    ha='center', va='center', transform=ax.transAxes, color='red')
        return

    try:
        df = pd.read_csv(filename)
    except Exception as e:
        print(f"Erro ao ler {filename}: {e}")
        return

    geracao = df['Geracao']
    resets = df[df['Evento'].str.contains('RESET', na=False)]['Geracao']

    # --- GRÁFICO 1: FITNESS (Desempenho) ---
    ax1 = ax_col[0]
    ax1.plot(geracao, df['MelhorFitness'], label='Melhor Indivíduo', color='#1f77b4', linewidth=1.5)
    ax1.plot(geracao, df['FitnessMedio'], label='Média', color='#ff7f0e', linestyle='--', linewidth=1, alpha=0.9)
    
    # Sombra do desvio padrão
    ax1.fill_between(geracao, df['FitnessMedio'] - df['DesvioPadraoFit'], 
                     df['FitnessMedio'] + df['DesvioPadraoFit'], color='#ff7f0e', alpha=0.2)
    
    ax1.set_title(f"{phase_title}\nEvolução do Fitness", fontsize=11, fontweight='bold')
    ax1.set_ylabel("Fitness")
    ax1.legend(loc='lower right', fontsize=8, frameon=True, framealpha=0.9)
    ax1.grid(True, which="both", linestyle=':', alpha=0.6)

    # --- GRÁFICO 2: DIVERSIDADE (Convergência) ---
    ax2 = ax_col[1]
    ax2.plot(geracao, df['DiversidadeGenetica'], color='#9467bd', linewidth=1.2)
    ax2.set_title("Convergência (Diversidade Genética)", fontsize=10)
    ax2.set_ylabel("Distância Euclidiana")
    
    # Linhas de Reset (Adapta a espessura se houver muitos resets)
    if len(resets) > 0:
        if USE_LOG_SCALE or len(resets) > 50:
            # Em Log ou com muitos resets, linhas finas para não poluir
            lw, alp = 0.5, 0.3
        else:
            # Em Linear com poucos resets, linhas visíveis
            lw, alp = 1.0, 0.6
            
        for r in resets:
            ax2.axvline(x=r, color='red', linestyle='-', alpha=alp, linewidth=lw)
            
    ax2.grid(True, which="both", linestyle=':', alpha=0.6)

    # --- GRÁFICO 3: CONTROLE (Mutação e Repulsão) ---
    ax3 = ax_col[2]
    ln1 = ax3.plot(geracao, df['TaxaMutacao'], color='#2ca02c', label='Mutação %', linewidth=1.2)
    ax3.set_ylabel("Mutação (%)", color='#2ca02c')
    ax3.tick_params(axis='y', labelcolor='#2ca02c')
    
    # Label do Eixo X dinâmico
    xlabel_text = "Gerações (Escala Log)" if USE_LOG_SCALE else "Gerações (Escala Linear)"
    ax3.set_xlabel(xlabel_text)
    
    ax3_r = ax3.twinx()
    ln2 = ax3_r.plot(geracao, df['FatorRepulsao'], color='#d62728', linestyle='-.', alpha=0.5, linewidth=1.2, label='Repulsão')
    ax3_r.set_ylabel("Fator Repulsão", color='#d62728')
    ax3_r.tick_params(axis='y', labelcolor='#d62728')
    ax3_r.set_ylim(0, 1.1)

    # Legenda combinada
    lns = ln1 + ln2
    labs = [l.get_label() for l in lns]
    ax3.legend(lns, labs, loc='upper left', fontsize=8, frameon=True, framealpha=0.9)
    ax3.grid(True, which="both", linestyle=':', alpha=0.6)

    # --- LÓGICA DE ESCALA (A Mágica acontece aqui) ---
    if USE_LOG_SCALE:
        ax1.set_xscale('log')
        ax2.set_xscale('log')
        ax3.set_xscale('log')
        # Em log, começamos de 1 para não dar erro matemático
        ax3.set_xlim(left=1, right=max(geracao))
    else:
        ax1.set_xscale('linear')
        ax2.set_xscale('linear')
        ax3.set_xscale('linear')
        # Em linear, começamos do 0
        ax3.set_xlim(left=0, right=max(geracao))

# --- EXECUÇÃO ---
fig, axes = plt.subplots(nrows=3, ncols=2, figsize=(16, 12), sharex='col')

# Ajuste de espaçamento
plt.subplots_adjust(hspace=0.3)

plot_phase_data(axes[:, 0], 'fase1.csv', 'FASE 1: Design (Geometria)')
plot_phase_data(axes[:, 1], 'fase2.csv', 'FASE 2: Estratégia (3000km)')

# Título e Nome do Arquivo Dinâmicos
mode_str = "Logarítmica" if USE_LOG_SCALE else "Linear"
fig.suptitle(f'Painel de Otimização Evolutiva - Carro Solar (Escala {mode_str})', fontsize=16, y=0.98)

plt.tight_layout(rect=[0, 0.03, 1, 0.97])

filename_out = f'dashboard_carro_solar_{"log" if USE_LOG_SCALE else "linear"}.png'
plt.savefig(filename_out, dpi=150)
print(f"Gráfico salvo com sucesso: {filename_out}")