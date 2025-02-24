#include <stdio.h>
#include <stdlib.h>
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "ssd1306.h"
#include "font.h"

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


// Definições de Botões e LEDS
#define BUTTON_A_PIN 5 
#define LED_G_PIN 11
#define LED_B_PIN 12
#define LED_R_PIN 13


uint16_t joy_x, joy_y;
uint slice_1, slice_2;
ssd1306_t ssd;
volatile uint event_time = 0; 
volatile bool pwm_enable = true;


// Protótipos
void setup_buttons();
void setup_leds();
void setup_i2c_display(ssd1306_t *ssd);
void setup_joystick();
void set_led_brightness(uint pos_x, uint pos_y);
void button_irq_handler(uint gpio, uint32_t events);
void draw_square(uint pos_x, uint pos_y);



void main() {

  // Configurações básicas
  setup_buttons();
  setup_leds();
  setup_joystick();
  setup_i2c_display(&ssd);

  ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0); // Desenha ou um retângulo

  while (true) {
    // Capta as posições analogicas dos Joysticks
    adc_select_input(0);
    joy_x = adc_read();
    adc_select_input(1);
    joy_y = adc_read();

    // Altera o brilho dos LEDS via PWM
    set_led_brightness(joy_x, joy_y);
    // Faz a animação de movimento do quadrado
    draw_square(joy_x, joy_y);
  }
}


void setup_buttons() {
  // Configuro meus botões e suas interrupções com uma função de callback
  gpio_init(BUTTON_A_PIN);
  gpio_set_dir(BUTTON_A_PIN, GPIO_OUT);
  gpio_pull_up(BUTTON_A_PIN);
  gpio_put(BUTTON_A_PIN, true);
  gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, button_irq_handler);

  gpio_init(BUTTON_JOY_PIN);
  gpio_set_dir(BUTTON_JOY_PIN, GPIO_OUT);
  gpio_pull_up(BUTTON_JOY_PIN);
  gpio_put(BUTTON_JOY_PIN, true);
  gpio_set_irq_enabled_with_callback(BUTTON_JOY_PIN, GPIO_IRQ_EDGE_FALL, true, button_irq_handler);
}

void setup_leds() {
  gpio_init(LED_G_PIN);
  gpio_set_dir(LED_G_PIN, GPIO_OUT);
  gpio_put(LED_G_PIN, false);

  // Configuro os LEDS Azul e Vermelho para funcionarem por meio de PWM
  gpio_set_function(LED_B_PIN, GPIO_FUNC_PWM);
  slice_1 = pwm_gpio_to_slice_num(LED_B_PIN);
  pwm_set_clkdiv(slice_1, CLK_DIV);
  pwm_set_wrap(slice_1, WRAP);
  pwm_set_chan_level(slice_1, PWM_CHAN_A, 0);
  pwm_set_enabled(slice_1, true);

  gpio_set_function(LED_R_PIN, GPIO_FUNC_PWM);
  slice_2 = pwm_gpio_to_slice_num(LED_R_PIN);
  pwm_set_clkdiv(slice_2, CLK_DIV);
  pwm_set_wrap(slice_2, WRAP);
  pwm_set_chan_level(slice_2, PWM_CHAN_B, 0);
  pwm_set_enabled(slice_2, true);
}

void setup_i2c_display(ssd1306_t *ssd) {
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

void setup_joystick() {
  adc_init();
  adc_gpio_init(X_JOY_PIN);
  adc_gpio_init(Y_JOY_PIN);
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
      pwm_enable = !pwm_enable;
      pwm_set_enabled(slice_1, pwm_enable);
      pwm_set_enabled(slice_2, pwm_enable);
    }
    
    // Ativa ou Desativa o LED VERDE e as bordas do Display
    if (gpio == BUTTON_JOY_PIN) {
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
