# Monitor de Silêncio com Alertas Visuais e Sonoros.

Este projeto implementa a conversão ADC de entradas analógicas de um microfone MAX4466EXK para, a partir disso, realizar processamentos desses dados visando a emissão de alertas quando o barulho ambiente ultrapassar limites determinados. Além disso, são feitos envios de relatórios via comunicação serial UART com informações detalhadas sobre o nível de ruído presente nas proximidades da placa.  O objetivo é fornecer um equipamento dinâmico que colabore no monitoramento de ambientes que exigem um determinado nível de silêncio, como bibliotecas, salas de estudo ou alas hospitalares. Esse projeto foi realizado em um **Raspberry Pi Pico W** e o código é desenvolvido em **C** para sistemas embarcados e foi implementado na placa voltada a aprendizagem BitDogLab.

## Estrutura do Projeto

- **monitorador_de_sons**: Pasta que contém todos os arquivos necessários para a compilação do projeto.
- **monitorador_de_sons/inc**: Pasta contendo arquivos auxiliares usados no arquivo principal.
- **monitorador_de_sons/monitorador_de_sons.c**: Arquivo principal do projeto.
- **monitorador_de_sons/CMakeLists.txt**: Arquivo contendo todas as instruções necessárias para realização da compilação.


## Especificações do projeto

- **Periféricos da BitDogLab utilizados:**:
  - 1 Matriz de LED-RGB 5x5 WS2812.
  - 1 Microfone com amplificador de áudio MAX4466EXK.
  - 1 Push-Button presente no Joystick Analógico Plugin 13x13mm Multi-Dir ROHS.
  - 2 Push-Buttons (A e B) do tipo Chave Táctil 12x12x7.5 mm.
  - 2 Buzzers (Esquerda e Direita) do tipo 80dB Externally Driven Magnetic 2.7kHz SMD, 8.5x8.5mm Buzzers ROHS.
  - 1 Display Oled 0.96 polegadas I2C 128x64.

- **Periféricos do MCU RP2040 utilizados:**:
  - DMA (Direct Memory Access) - Para captação eficiente dos dados quantizados do microfone.
  - PIO (Programmable I/O) - Usado na comunicação com a Matriz 5x5 de LED-RGB endereçáveis.
  - PWM (Pulse Width Modulation) - Usado para modular o som produzido pelos 2 Buzzers passivos. 
  - I2C (Inter-Integrated Circuit) - Para comunicação com o Display Oled 128x64.
  - UART (Universal Asynchronous Receiver-Transmitter) - Para envio de relatórios ao computador com dados das amplitudes de áudio captadas. 
  - GPIO (General Purpose Input/Output) - Usado para se comunicar com os periféricos presentes na BitDogLab.

- **Outros componentes físicos necessários:**:
  - Cabo micro-USB para USB-A.
  - Um computador de uso geral. 

- **Softwares utilizados**:
  - Visual Studio Code
  - Pico SDK
  - Compilador ARM GCC

## Como utilizar:

- **Como Ativar ou Desativar a captação de áudio:**:
  - Pressionar o Botão A (Botão da Esquerda) muda o estado da captação de dados por meio do DMA, ativando ou desativando o processamento de áudio.

- **Como Ativar ou Desativar o uso dos Buzzers para emissão de som:**:
  - Pressionar o Botão B (Botão da Direita) muda o estado do slice de PWM utilizado, ativando ou desativando os buzzers passivos;

- **Como Ativar ou Desativar a comunicação serial:**:
  - Pressionar o Joystick muda a permissão de envio de dados via comunicação serial UART.

- **Como Entender as animações na Matriz 5x5 de LED-RGB:**:
  - Quando o buffer do DMA é preenchido por completo é feito um processamento que fornecerá o peso da amplitude de som captada naquele instante, assim, preenchendo as colunas da matriz com base nesses picos de áudio. Quanto mais LEDs acesos em uma coluna, maior foi a amplitude do som naquele instante. 


## Vídeo Demonstrativo
Assista aqui: <https://drive.google.com/file/d/1ntCjtM3N2V1FHxkgRuypB-mz5FX91FBW/view?usp=sharing>


## Autor
Desenvolvido por <https://github.com/Elmer-Carvalho>

## Licença
Este projeto está sob a licença MIT.



