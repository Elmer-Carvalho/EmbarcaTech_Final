#  Conversão ADC de Joystick para controle de Luminosidade de LEDS e Animação em Display.

Este projeto implementa a conversão ADC das posições de um Joystick para realizar alterações dinâmicas na luminosidade de cores de um LED RGB via PWM e permitir a movimentação de um quadrado 8x8 no Display SSD1306. Além disso, são implementadas funcionalidades envolvendo outro botão e o botão do próprio Joystick através de interrupções para mudanças de estado na placa, ligando ou desligando LEDS e realizando outros comportamentos. Esse projeto foi realizado em um **Raspberry Pi Pico W**. O código é desenvolvido em **C** para sistemas embarcados e foi implementado na placa voltada a aprendizagem BitDogLab.

## Estrutura do Projeto

- **adc**: Pasta com o projeto de conversão ADC.
- **adc.c**: Arquivo principal que deve ser compilado e implementado na placa.
- **ssd1306.c**: Arquivo que contém as rotinas de manipulação do display.


## Requisitos

- **Hardware**:
  - Computador pessoal.
  - Raspberry Pi Pico W (RP2040).
  - 1 Botão.
  - 1 Joystick.
  - 1 Monitor SSD1306.
  - 1 LED RGB ou 3 LEDs de cores distintas.
  - Um conjunto de resistores.
  - Cabo micro-USB para USB-A.

- **Software**:
  - Visual Studio Code
  - Pico SDK
  - Compilador ARM GCC

## Vídeo Demonstrativo
Assista aqui: <https://drive.google.com/file/d/1YoxnSxm_CrV9srb5r7XMkOmUvMzc1QBQ/view?usp=sharing>


## Autor
Desenvolvido por <https://github.com/Elmer-Carvalho>

## Licença
Este projeto está sob a licença MIT.



