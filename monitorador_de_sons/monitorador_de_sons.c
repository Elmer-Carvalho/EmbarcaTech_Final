/*
Autor: Elmer Carvalho 
-- Projeto Final do Embarca Tech
Data de Finalização: 26/02/2025
*/

#include <stdio.h>
#include <stdlib.h>
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "pico/time.h"
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

#define DELAY_UART_MS 3000

// Definições do Display 
#define SSD_ADDR 0x3C
#define SSD_WIDTH 128
#define SSD_HEIGHT 64
#define BORDER_DUR_MS 50

// Definições do PWM para LEDs (10 kHz)
#define WRAP 2048
#define CLK_DIV 6.1

// Definições do PWM para Buzzers (2 kHz)
#define BUZZER_WRAP 62500  // 125 MHz / 2000 Hz = 62500
#define BUZZER_CLK_DIV 1.0
#define BUZZER_DURATION_MS 500

// Buzzers
#define BUZZER_A_PIN 21  // PWM 5A
#define BUZZER_B_PIN 10  // PWM 5B

// Matriz de Leds 
#define MATRIZ_LEDS_PIN 7
#define NUM_LEDS 25

// Microfone e DMA
#define MICROPHONE_PIN 28
#define MIC_CHANNEL 2
#define ADC_CLK_DIV 21.f
#define SILENCE_LEVEL 2048
#define DMA_BUFFER_SIZE 79
#define AMPL_LEVEL_1 100
#define AMPL_LEVEL_2 250
#define AMPL_LEVEL_3 350
#define AMPL_LEVEL_4 450
#define AMPL_LEVEL_5 750
#define UPDATE_INTERVAL_MS 75

// Definições de Botões
#define BUTTON_A_PIN 5 
#define BUTTON_B_PIN 6
#define BUTTON_JOY_PIN 22 

// Variáveis globais
ssd1306_t ssd;
uint dma_channel;
dma_channel_config dma_cfg;
PIO pio = pio0;
uint sm;
const uint coluns_index[5][5] = {
    {4, 5, 14, 15, 24},
    {3, 6, 13, 16, 23},
    {2, 7, 12, 17, 22},
    {1, 8, 11, 18, 21},
    {0, 9, 10, 19, 20}
};
volatile uint8_t heights[5] = {0};
volatile uint16_t mic_buffer[DMA_BUFFER_SIZE];
volatile uint event_time = 0;
volatile bool pwm_enable = true, dma_enabled = true, buzzers_enable = true, border_on = true, serial_on = true;
volatile uint32_t last_update_time = 0, last_buzzer_on = 0, last_border_blink = 0;
volatile uint8_t peak_height = 0;
volatile uint amplitude_peak_count = 0, max_peak = 0, sum_peaks = 0, count_peaks = 0, sum_sounds = 0, count_sounds = 0;

// Protótipos
void setup();
void setup_adc_dma();
void init_buttons();
void init_buzzers();
void init_i2c_display(ssd1306_t *ssd);
void init_matrix_leds();
void button_irq_handler(uint gpio, uint32_t events);
void update_leds();
void noise_alert();
uint32_t matrix_led_color(float red, float green, float blue);

void main() {
    stdio_init_all();
    setup();

    ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0);
    ssd1306_draw_string(&ssd, "MONITORANDO", (SSD_WIDTH/2) - ((sizeof("MONITORANDO") * 8) / 2), 20);
    ssd1306_draw_string(&ssd, "SONS", (SSD_WIDTH/2) - ((sizeof("SONS") * 8) / 2), 35);
    ssd1306_send_data(&ssd);

    while (true) {
        if (amplitude_peak_count > 0 && amplitude_peak_count % 20 == 0) {
            noise_alert();
            amplitude_peak_count = 0;  // Resetar após o alerta
            ssd1306_fill(&ssd, false);
            ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0);
            ssd1306_draw_string(&ssd, "MONITORANDO", (SSD_WIDTH/2) - ((sizeof("MONITORANDO") * 8) / 2), 20);
            ssd1306_draw_string(&ssd, "SONS", (SSD_WIDTH/2) - ((sizeof("SONS") * 8) / 2), 35);
            ssd1306_send_data(&ssd);
        }

        if (to_ms_since_boot(get_absolute_time()) % DELAY_UART_MS == 0) {
          if (serial_on) {
            printf("\n");
            printf("RELATÓRIO GERADO A CADA -- %ims --:\n", DELAY_UART_MS);
            printf("Nível da Amplitude Máxima -- %i\n", max_peak);
            printf("Nível Médio de Amplitudes maiores que %i -- %i\n", AMPL_LEVEL_5, sum_peaks / count_peaks);
            printf("Quantidade de amostras maiores que %i -- %i\n", AMPL_LEVEL_5, count_peaks);
            printf("Nível Médio de Amplitude -- %i\n", sum_sounds / count_sounds);
            printf("Quantidade de amostras: -- %i\n", count_sounds); 
          }
          sum_peaks = 0; count_peaks = 0; sum_sounds = 0; count_sounds = 0; max_peak = 0;
        }
    }
}

void setup() {
    init_buttons();
    void init_buzzers();
    init_matrix_leds();
    setup_adc_dma();
    init_i2c_display(&ssd);
}

void init_buttons() {
    uint8_t buttons[3] = {BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_JOY_PIN};
    for (uint8_t i = 0; i < 3; i++) {
        gpio_init(buttons[i]);
        gpio_set_dir(buttons[i], GPIO_IN);
        gpio_pull_up(buttons[i]);
        gpio_set_irq_enabled_with_callback(buttons[i], GPIO_IRQ_EDGE_FALL, true, button_irq_handler);
    }
}

void init_buzzers() {
    // Buzzers
    gpio_set_function(BUZZER_A_PIN, GPIO_FUNC_PWM);  // PWM 5A
    gpio_set_function(BUZZER_B_PIN, GPIO_FUNC_PWM);  // PWM 5B
    uint buzzer_slice = pwm_gpio_to_slice_num(BUZZER_A_PIN);
    pwm_set_clkdiv(buzzer_slice, BUZZER_CLK_DIV);
    pwm_set_wrap(buzzer_slice, BUZZER_WRAP);  // 2 kHz
    pwm_set_chan_level(buzzer_slice, PWM_CHAN_A, 0);
    pwm_set_chan_level(buzzer_slice, PWM_CHAN_B, 0);
    pwm_set_enabled(buzzer_slice, false);
}

void init_matrix_leds() {
    sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, sm, offset, MATRIZ_LEDS_PIN);
    pio_sm_set_enabled(pio, sm, true);
}

void init_i2c_display(ssd1306_t *ssd) {
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
    if (dma_channel_get_irq0_status(dma_channel) && dma_enabled) {
        dma_channel_acknowledge_irq0(dma_channel);
        uint16_t peak_amplitude = 0;
        for (uint i = 0; i < DMA_BUFFER_SIZE; i++) {
            int16_t adjusted = (int16_t)mic_buffer[i] - SILENCE_LEVEL;
            uint16_t amplitude = (adjusted >= 0) ? adjusted : -adjusted;

            sum_sounds += amplitude;
            count_sounds++;

            if (amplitude > peak_amplitude) peak_amplitude = amplitude;
        }

        max_peak = peak_amplitude > max_peak ? peak_amplitude : max_peak;

        uint8_t height = 0;
        if (peak_amplitude >= AMPL_LEVEL_1) {
            if (peak_amplitude < AMPL_LEVEL_2) height = 1;
            else if (peak_amplitude < AMPL_LEVEL_3) height = 2;
            else if (peak_amplitude < AMPL_LEVEL_4) height = 3;
            else if (peak_amplitude < AMPL_LEVEL_5) height = 4;
            else {height = 5; sum_peaks += peak_amplitude; count_peaks++;}
        }

        if (height == 5) amplitude_peak_count++;

        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        if (current_time - last_update_time >= UPDATE_INTERVAL_MS) {
            for (uint8_t i = 0; i < 4; i++) {
                heights[i] = heights[i + 1];
            }
            heights[4] = height;
            update_leds();
            last_update_time = current_time;
        }

        dma_channel_set_write_addr(dma_channel, mic_buffer, true);
    }
}

void setup_adc_dma() {
    adc_init();
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

void button_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - event_time > 200) {
        event_time = current_time;

        if (gpio == BUTTON_A_PIN) {
            dma_enabled = !dma_enabled;
            if (dma_enabled) {
                adc_run(true);
                dma_channel_set_irq0_enabled(dma_channel, true);
                dma_channel_configure(dma_channel, &dma_cfg, mic_buffer, &adc_hw->fifo, DMA_BUFFER_SIZE, true);
            } else {
                adc_run(false);
                dma_channel_set_irq0_enabled(dma_channel, false);
                for (uint8_t i = 0; i < NUM_LEDS; i++) {
                    pio_sm_put_blocking(pio, sm, matrix_led_color(0.0, 0.0, 0.0));
                }
            }
            if (serial_on) printf("\n--> Captação de Som foi: %s\n", dma_enabled ? "Ativada" : "Desativada");
        }

        if (gpio == BUTTON_B_PIN) {
            buzzers_enable = !buzzers_enable;
            if (serial_on) printf("\n--> Buzzers Passivos foram: %s\n", buzzers_enable ? "Ativados" : "Desativados");
        }

        if (gpio == BUTTON_JOY_PIN) {
            serial_on = !serial_on;
            if (serial_on) printf("\n-- COMUNICAÇÃO ATIVADA --\n");
        }
    }
}

void update_leds() {
    uint32_t colors[NUM_LEDS] = {0};
    for (uint index_1 = 0; index_1 < 5; index_1++) {
        if (heights[index_1] > 0) {
            float red = 0.0, green = 0.0, blue = 0.0;
            switch (heights[index_1]) {
                case 1: red = 0.0; green = 1.0; blue = 0.0; break;
                case 2: red = 0.5; green = 0.75; blue = 0.0; break;
                case 3: red = 1.0; green = 1.0; blue = 0.0; break;
                case 4: red = 1.0; green = 0.5; blue = 0.0; break;
                case 5: red = 1.0; green = 0.0; blue = 0.0; break;
                default: red = 0.0; green = 0.0; blue = 0.0; break;
            }
            uint32_t color = matrix_led_color(red, green, blue);
            for (uint index_2 = 0; index_2 < heights[index_1]; index_2++) {
                colors[coluns_index[index_1][index_2]] = color;
            }
        }
    }

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        pio_sm_put_blocking(pio, sm, colors[i]);
    }
}

void noise_alert() {
    uint buzzer_slice = pwm_gpio_to_slice_num(BUZZER_A_PIN);

    if (buzzers_enable) {
        pwm_set_chan_level(buzzer_slice, PWM_CHAN_A, BUZZER_WRAP / 2);
        pwm_set_chan_level(buzzer_slice, PWM_CHAN_B, BUZZER_WRAP / 2);
        pwm_set_enabled(buzzer_slice, true);
    }

    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "SILENCIO", (SSD_WIDTH/2) - ((sizeof("SILENCIO") * 8) / 2), 31);

    last_buzzer_on = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - last_buzzer_on < BUZZER_DURATION_MS) {
        if ((to_ms_since_boot(get_absolute_time()) - last_border_blink) >= BORDER_DUR_MS) {
          border_on = !border_on;
          ssd1306_rect(&ssd, 3, 3, 122, 60, border_on, 0);
          ssd1306_send_data(&ssd);
          last_border_blink = to_ms_since_boot(get_absolute_time());
        }
    }

    if (buzzers_enable) {
        pwm_set_enabled(buzzer_slice, false);
        pwm_set_chan_level(buzzer_slice, PWM_CHAN_A, 0);
        pwm_set_chan_level(buzzer_slice, PWM_CHAN_B, 0);
    }
}


uint32_t matrix_led_color(float red, float green, float blue) {
    unsigned char G = green * 255;
    unsigned char R = red * 255;
    unsigned char B = blue * 255;
    return (G << 24) | (R << 16) | (B << 8);  // Ordem GRB
}

