# Agri_LoRa

Este projeto simula uma arquitetura de IoT ciberfísica para agricultura de precisão. A comunicação entre os dispositivos é feita através de um link serial isolado (UART2), emulando a camada física de módulos de rádio frequência (como o LoRa SX1276). O sistema utiliza um protocolo binário customizado, garantindo integridade dos dados via Sync Word e validação CRC-8.

📡 Node 1: Sensor e Atuador de Borda (Edge Node) - aquisição de telemetria e execução de ações locais (irrigação).

- Aquisição de Dados: Lê periodicamente a temperatura e a umidade do ar (DHT22) e a umidade do solo (simulada via potenciômetro).
  
- Empacotamento e Transmissão: Converte os dados para uma estrutura binária enxuta (struct) e transmite o pacote envelopado com um cabeçalho de sincronização (0x55 0xAA) e um byte de verificação de erro (CRC-8).
  
- Recepção de Comandos (Downlink): Após transmitir, o nó aguarda instruções do Gateway para ligar ou desligar a bomba de água (led azul), atuando como um dispositivo 'slave' na orquestração da rede.

- Fail-Safe (Computação de Borda): Possui um mecanismo de sobrevivência local. Se a comunicação com o Gateway for perdida por mais de 30 segundos, o nó assume a tomada de decisão. Caso a umidade do solo atinja níveis críticos (abaixo de 15%), ele aciona a bomba de forma autônoma para garantir a sobrevivência da planta até que a rede seja restabelecida.

🧠 Gateway: Orquestrador Central - processa os dados recebidos, aplicar regras de negócio e comandar os nós da rede.

- Recepção Robusta: Utiliza uma máquina de estados finitos (FSM) para ler o barramento de dados continuamente. Ele ignora ruídos e apenas inicia a leitura quando detecta o padrão exato de sincronização (0x55 0xAA).

- Validação de Integridade: Antes de processar qualquer dado, recalcula o CRC-8 do pacote recebido e o compara com o CRC enviado pelo Node 1. Pacotes corrompidos são descartados silenciosamente.

- Motor de Regras (Cultivo Inteligente): Analisa a telemetria limpa. A regra principal aciona a bomba de irrigação apenas se o solo estiver seco (abaixo de 30%) e a temperatura ambiente for segura (abaixo de 35°C),evitando o cozimento das raízes.

- Comando e Interface: Envia o comando de atuação de volta ao Node 1 e atualiza em tempo real um display OLED local com o status completo da operação, servindo como painel de monitoramento. Em uma arquitetura de produção, este Gateway estaria conectado à internet, repassando essas métricas para bancos de dados de séries temporais e contêineres de processamento em nuvem.
<br><br>

## Link - [Video](Agri_LoRa_Simulation.mp4) da Simulação 

## Caso queira abrir a simulação, ela é feita no site WOKWi
  OBS: Para que a simulação funcione corretamente, é necessário usar um programa para virtualizar portas SERIAIS as quais os ESPs irão conectar. Para este projeto foi utilizado o Free Virtual Serial Ports, contudo fique a vontade para usar o que preferir.

  - 1 - Instale o virtualizador de portas seriais na sua máquina
  - 2 - Cria uma conexão SERIAL virtual entre duas portas (EX: COM1 e COM2)
  - 3 - Entre no link do [Node 1](https://wokwi.com/projects/470072923601500161)
  - 4 - Entre no link do [Gateway](https://wokwi.com/projects/470055661978825729)
  - 5 - Posicione as duas janelas (node e gatway) lado a lado
  - 6 - Inicie a simulação na janela do node 1 e ao abrir o pop up -> selecione a porta SERIAL gerada pelo virtualizador (Ex: selecione a COM1)
  - 7 - Faça o passo 6 para o gateway e selecione a porta virtual restante (Ex: COM2)


  
