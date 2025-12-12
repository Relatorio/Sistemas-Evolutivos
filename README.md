# Carro elétrico movido somente à energia solar, competição BWSC(Bridgestone World Solar Challenge) adaptada

  O objetivo do projeto é definir e otimizar um carro competidor do desafio BWSC(Bridgestone World Solar Challenge) na categoria Challenger com todas as regras da categoria porém adaptada para a cidade de São Carlos-SP, considerando sua altitude e latitude, temperatura ambiente de 25ºC, umidade relativa de 50%, velocidade do vento zero e incidência solar na cidade no dia 21 de junho.

# Algoritmo Evolutivo

  Ele se baseia na modelagem de um carro elétrico e movido exclusivamente por energia solar a partir de equações gerais de mecânica dos fluidos e transferência de calor que vão influenciar nos 7 genes geométricos do modelo(com ranges delimitados de acordo com a física/espaço disponível e especificações da competição), 7 genes definem um indivíduo e  eles são avaliados a partir do resultado que produzem definindo o carro que percorrerá a maior distância com velocidade de cruzeiro de 60 km/h maximizando a sobra de potência. O indivíduo com o melhor resultado terá seus genes(parâmetros do carro) mesclados com de todos os indivíduos daquela geração por meio de média aritmética simples, como se fosse um cruzamento genético onde você herda metade dos genes do pai e metade da mãe) para que eles deem origem a nova geração, com isso o melhor de todos “puxa” a variabilidade genética da população para mais perto dele fazendo com que após múltiplas gerações a população escale aquele pico da função. Após níveis de estagnação serem detectados, mecanismos de mutação, já existentes, começam a aumentar seus níveis até um limite de 25% e forçar as novas gerações a explorar o ambiente, se mesmo assim o melhor indivíduo não for trocado, o algoritmo ativa um modo de repulsão do melhor de todos e passa a explorar mais ainda, se isso ainda não gerar um indivíduo melhor que o melhor de todos a última arma de exploração é a eliminação de 50% dos piores indivíduos daquela geração e reposição na próxima geração por novos indivíduos aleatórios e dentro dessa reposição são feitos outros dois indivíduos com estratégias levemente diferentes, um deles chamado de fransktein é obtido pelo sorteio de gene por gene dos 50% melhores indivíduos para compor o novo e na segunda estratégia, o EDA, é composto pela média e desvio padrão dos 50% melhores de acordo com uma curva Gaussiana.
  Após 100.000 gerações dessa primeira otimização, o melhor indivíduo é levado a outra otimização para descobrir a melhor estratégia de gerenciamento do pacote de energia ao longo das nove horas(os nove genes) de duração da competição por mais 100.000 gerações a fim de completar os 3000 km no menor tempo possível.


# Dependências # 

### Simulação (C)
- Compilador GCC
- Biblioteca matemática padrão (`math.h`)

### Dashboard (Python)
- Python 3.8+
- Bibliotecas:
  ```bash
  pip install pandas matplotlib

# Vídeo de explicação do projeto:
https://drive.google.com/file/d/1H7Q8o_XzQrUjTMzWIORcDIM8DmMTZg3v/view?usp=drive_link
