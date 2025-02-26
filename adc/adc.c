#include <stdio.h>
#include <stdlib.h>
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "pio_matrix.pio.h"
#include "./inc/ssd1306.h"
#include "./inc/font.h"

// Comunicação Serial I2C
#define I2C_PORT i2c1
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 15

// Definições do Display 
#define SSD_ADDR 0x3C
#define SSD_WIDTH 128
#define SSD_HEIGHT 64

// Definições do PWM para alcançar uma fPWM de 10kHz
#define WRAP 2048
#define CLK_DIV 6.1

// Definições do Joystick
#define X_JOY_PIN 26  
#define Y_JOY_PIN 27  
#define BUTTON_JOY_PIN 22 
#define MOV_TOLERANCE 200

#define LIM_MAX_X 52 
#define LIM_INIT_X 4

#define LIM_MAX_Y 110
#define LIM_INIT_Y 4

// Buzzers
#define BUZZER_A_PIN 21
#define BUZZER_B_PIN 10

// Matriz de Leds 
#define MATRIZ_LEDS_PIN 7
#define NUM_LEDS 25

// Microfone e DMA
#define MICROPHONE_PIN 28
#define MIC_CHANNEL 2
#define ADC_CLK_DIV 21.f // Para conseguir aproximadamente 8kHz de amostragem para cada canal de ADC
#define NUM_SAMPLES 250
#define SILENCE_LEVEL 2048
#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f)
#define DMA_BUFFER_SIZE 79

// Definições de Botões e LEDS
#define BUTTON_A_PIN 5 
#define BUTTON_B_PIN 6
#define LED_G_PIN 11
#define LED_B_PIN 12
#define LED_R_PIN 13


uint16_t joy_x, joy_y;
uint slice_1, slice_2;
ssd1306_t ssd;
uint dma_channel;
dma_channel_config dma_cfg;
PIO pio = pio0;
uint sm;
const uint coluns_index[5][5] = {
  {4, 5, 14, 15, 24}, // Primeira Coluna da Esquerda
  {3, 6, 13, 16, 23},
  {2, 7, 12, 17, 22},
  {1, 8, 11, 18, 21},
  {0, 9, 10, 19, 20}, // Ultima Coluna da Direita
};

volatile uint8_t heights[5] = {0};
volatile uint16_t mic_buffer[DMA_BUFFER_SIZE];
volatile uint event_time = 0; 
volatile bool pwm_enable = true;



// Protótipos
void setup();
void setup_adc_dma();
void init_buttons();
void init_leds();
void init_i2c_display(ssd1306_t *ssd);
void init_matrix_leds();
void set_led_brightness(uint pos_x, uint pos_y);
void button_irq_handler(uint gpio, uint32_t events);
void draw_square(uint pos_x, uint pos_y);
void update_leds();



void main() {
  stdio_init_all();

  // Configurações básicas
  setup();

  ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0); // Desenha ou um retângulo

  while (true) {
    /*
    // Capta as posições analogicas dos Joysticks
    adc_select_input(0);
    joy_x = adc_read();
    adc_select_input(1);
    joy_y = adc_read();

    // Altera o brilho dos LEDS via PWM
    set_led_brightness(joy_x, joy_y);
    // Faz a animação de movimento do quadrado
    draw_square(joy_x, joy_y);
    */
  }
}


void setup() {
  init_buttons();
  init_leds();
  init_matrix_leds();
  setup_adc_dma();
  init_i2c_display(&ssd);
}

void init_buttons() {
  uint8_t buttons[3] = {BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_JOY_PIN};
  
  // Inicializa os Botões e configura sua interrupção
  for (uint8_t i = 0; i < 3; i++) {
    gpio_init(buttons[i]);
    gpio_set_dir(buttons[i], GPIO_OUT);
    gpio_pull_up(buttons[i]);
    gpio_put(buttons[i], true);
    gpio_set_irq_enabled_with_callback(buttons[i], GPIO_IRQ_EDGE_FALL, true, button_irq_handler);
  }
}

void init_leds() {
  uint8_t leds[3] = {LED_G_PIN, LED_B_PIN, LED_R_PIN};

  // Inicializa os leds RGB e configura seu PWM
  for (uint8_t i = 0; i < 3; i++) {
    gpio_set_function(leds[i], GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(leds[i]);
    pwm_set_clkdiv(slice, CLK_DIV);
    pwm_set_wrap(slice, WRAP);
    pwm_set_chan_level(slice, PWM_CHAN_A, 0);
    pwm_set_enabled(slice, true);
  }
}

void init_matrix_leds() {
  sm = pio_claim_unused_sm(pio, true);
  uint offset = pio_add_program(pio, &pio_matrix_program);
  pio_matrix_program_init(pio, sm, offset, MATRIZ_LEDS_PIN);
  pio_sm_set_enabled(pio, sm, true);
}

void init_i2c_display(ssd1306_t *ssd) {
  // Configuração da comunição serial para utilizar o display SSD1306
  i2c_init(I2C_PORT, 400000);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);

  ssd1306_init(ssd, SSD_WIDTH, SSD_HEIGHT, false, SSD_ADDR, I2C_PORT);
  ssd1306_config(ssd);
  ssd1306_fill(ssd, false);
  ssd1306_send_data(ssd); 
}

void dma_irq_handler() {
  if (dma_channel_get_irq0_status(dma_channel)) {
      dma_channel_acknowledge_irq0(dma_channel);
      uint32_t sum_amplitude = 0;
      for (uint i = 0; i < DMA_BUFFER_SIZE; i++) {
          int16_t adjusted = (int16_t)mic_buffer[i] - SILENCE_LEVEL;  // Centraliza em 0
          sum_amplitude += (adjusted >= 0) ? adjusted : -adjusted;    // Amplitude absoluta
      }
      uint16_t avg_amplitude = sum_amplitude / DMA_BUFFER_SIZE;           // Média da amplitude
      uint8_t height;
      if (avg_amplitude < 50) {
          height = 0;  // Silêncio ou som muito baixo
      } else if (avg_amplitude < 100) {
          height = 1;
      } else if (avg_amplitude < 200) {
          height = 2;
      } else if (avg_amplitude < 1635) {
          height = 3;
      } else {
          height = 4;  // Máxima amplitude
      }
      for (uint8_t i = 0; i < 4; i++) {
          heights[i + 1] = heights[i];
      }
      heights[0] = height;
      update_leds();
      //buffer_ready = true;
      printf("NOVO PESO: %i\n", height);
      dma_channel_set_write_addr(dma_channel, mic_buffer, true);
  }
}

void setup_adc_dma() {
  adc_init();
  adc_gpio_init(X_JOY_PIN);
  adc_gpio_init(Y_JOY_PIN);
  adc_gpio_init(MICROPHONE_PIN);
  adc_select_input(MIC_CHANNEL);
  adc_fifo_setup(true, true, 1, false, false);
  adc_set_clkdiv(ADC_CLK_DIV);

  dma_channel = dma_claim_unused_channel(true);
  dma_cfg = dma_channel_get_default_config(dma_channel);
  channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
  channel_config_set_read_increment(&dma_cfg, false);
  channel_config_set_write_increment(&dma_cfg, true);
  channel_config_set_dreq(&dma_cfg, DREQ_ADC);
  dma_channel_set_irq0_enabled(dma_channel, true);

  irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
  irq_set_enabled(DMA_IRQ_0, true);

  dma_channel_configure(dma_channel, &dma_cfg, mic_buffer, &adc_hw->fifo, DMA_BUFFER_SIZE, true);
  adc_run(true);
}

void set_led_brightness(uint pos_x, uint pos_y) {
  int bright_A = abs(pos_x - WRAP) < MOV_TOLERANCE ? 0 : abs(pos_x - WRAP);
  int bright_B = abs(pos_y - WRAP) < MOV_TOLERANCE ? 0 : abs(pos_y - WRAP);

  pwm_set_chan_level(slice_1, PWM_CHAN_A, bright_A);
  pwm_set_chan_level(slice_2, PWM_CHAN_B, bright_B);
}

void button_irq_handler(uint gpio, uint32_t events) {
  // Uso de Deboucing para evitar Ruídos
  uint32_t current_time = to_ms_since_boot(get_absolute_time());
  if (current_time - event_time > 200) {  
    event_time = current_time;

    // Ativa ou desativa a mudança de brilho dos LEDS via PWM
    if (gpio == BUTTON_A_PIN) {
      printf("ESTOU NO BOTAO A (NOVO)\n");
      pwm_enable = !pwm_enable;
      pwm_set_enabled(slice_1, pwm_enable);
      pwm_set_enabled(slice_2, pwm_enable);
    }
    
    // Ativa ou Desativa o LED VERDE e as bordas do Display
    if (gpio == BUTTON_JOY_PIN) {
      printf("ESTOU NO BOTAO DO JOYSTICK\n");
      gpio_put(LED_G_PIN, !(gpio_get(LED_G_PIN)));
      ssd1306_rect(&ssd, 3, 3, 122, 60, !(gpio_get(LED_G_PIN)), 0); // Desenha ou Apaga um retângulo
    }
  }
}

void draw_square(uint pos_x, uint pos_y) {
  static uint8_t current_pos_x = (SSD_HEIGHT / 2) - 4;
  static uint8_t current_pos_y = (SSD_WIDTH / 2) - 4;

  uint mov_div_x = (WRAP * 2) / SSD_HEIGHT;
  uint mov_div_y = (WRAP * 2) / SSD_WIDTH;

  // Apaga o quadrado anterior
  ssd1306_rect(&ssd, current_pos_x, current_pos_y, 8, 8, 0, 1);

  // Verifica se o quadrado está dentro dos limites
  if ((uint8_t)(pos_x / mov_div_x) < LIM_MAX_X && (uint8_t)(pos_x / mov_div_x) > LIM_INIT_X)
    current_pos_x = (uint8_t)(pos_x / mov_div_x);

  if ((uint8_t)(pos_y / mov_div_y) < LIM_MAX_Y && (uint8_t)(pos_y / mov_div_y) > LIM_INIT_Y)    
    current_pos_y = (uint8_t)(pos_y / mov_div_y);

  // Desenha um novo quadrado
  ssd1306_rect(&ssd, current_pos_x, current_pos_y, 8, 8, 1, 1);
  ssd1306_send_data(&ssd);
}

void update_leds() {
  uint32_t colors[NUM_LEDS] = {0};
  const uint32_t GREEN = 0x00FF00;

  for (uint index_1 = 0; index_1 < 5; index_1++) {
    if (heights[index_1] > 0) {
      for (uint index_2 = 0; index_2 < heights[index_1]; index_2++) {
        colors[coluns_index[index_1][index_2]] = GREEN;
      }
    }
  }
  /*
  for (uint8_t col = 0; col < 5; col++) {
      for (uint8_t row = 0; row < 5; row++) {
          if (row < heights[col]) {
              colors[col * 5 + row] = GREEN;
          }
      }
  }*/
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
      pio_sm_put_blocking(pio, sm, colors[i]);
  }
}

// ------

/*
// Exibe um frame na matriz
void exibirFrame(const float frame[ALTU][LARG], PIO pio, uint sm) {
  uint32_t cor_led = matrix_rgb();

  for (uint linha = 0; linha < ALTU; linha++) {
      for (uint coluna = 0; coluna < LARG; coluna++) {
          uint32_t valor_cor = (linha > 0 && linha % 2 != 0) ? cor_led * frame[ALTU - linha - 1][coluna] : cor_led * frame[ALTU - linha - 1][LARG - coluna - 1];
          pio_sm_put_blocking(pio, sm, valor_cor);
      }
  }

  atualizou = false;
}

// Função para definir a cor do LED RGB
uint32_t matrix_rgb() {
  unsigned char R = 0.5 * 255;
  unsigned char G = 0.7 * 255;
  unsigned char B = 0.5 * 255;
  return (G << 24) | (R << 16) | (B << 8);
} */
