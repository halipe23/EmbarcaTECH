#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// Configurações de hardware
#define I2C_PORT i2c1
#define I2C_SDA 15
#define I2C_SCL 14
#define LED_B 12
#define LED_R 13
#define LED_G 11
#define SW 22
#define VRY 26
#define VRX 27
#define BUZZER_PIN 21

ssd1306_t disp;

// Estados do sistema
typedef enum {
    MENU_PRINCIPAL,
    EXECUTANDO_JOYSTICK,
    EXECUTANDO_BUZZER,
    EXECUTANDO_LED_RGB
} SistemaEstado;

volatile SistemaEstado estado = MENU_PRINCIPAL;
volatile bool botao_pressionado = false;

// Protótipos das funções
void setup();
void print_menu(int pos);
void irq_handler(uint gpio, uint32_t events);
void executar_joystick();
void executar_buzzer();
void executar_led_rgb();

// Interrupção para o botão
void irq_handler(uint gpio, uint32_t events) {
    if (gpio == SW) {
        botao_pressionado = true;
    }
}

void setup() {
    stdio_init_all();
    
    // Configuração do OLED
    i2c_init(I2C_PORT, 400*1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, I2C_PORT);
    ssd1306_clear(&disp);

    // Configuração dos LEDs
    gpio_init(LED_B);
    gpio_init(LED_R);
    gpio_init(LED_G);
    gpio_set_dir(LED_B, GPIO_OUT);
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_set_dir(LED_G, GPIO_OUT);

    // Configuração do joystick
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
    gpio_set_irq_enabled_with_callback(SW, GPIO_IRQ_EDGE_FALL, true, &irq_handler);

    // Configuração ADC
    adc_init();
    adc_gpio_init(VRY);
    adc_gpio_init(VRX);
}

void print_menu(int pos) {
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 52, 2, 1, "Menu");
    ssd1306_draw_empty_square(&disp, 2, pos + 2, 120, 12);
    ssd1306_draw_string(&disp, 6, 18, 1, "Joystick LED");
    ssd1306_draw_string(&disp, 6, 30, 1, "Tocar Buzzer");
    ssd1306_draw_string(&disp, 6, 42, 1, "LED RGB");
    ssd1306_show(&disp);
}

void executar_joystick() {
    // Configura PWM para LED_R e LED_B
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.0f);
    pwm_config_set_wrap(&config, 65535);

    gpio_set_function(LED_R, GPIO_FUNC_PWM);
    gpio_set_function(LED_B, GPIO_FUNC_PWM);
    
    uint slice_r = pwm_gpio_to_slice_num(LED_R);
    uint slice_b = pwm_gpio_to_slice_num(LED_B);
    
    pwm_init(slice_r, &config, true);
    pwm_init(slice_b, &config, true);

    while(!botao_pressionado) {
        adc_select_input(1);
        uint x_raw = adc_read();
        adc_select_input(0);
        uint y_raw = adc_read();

        // Atualiza brilho com multiplicador para maior amplitude
        pwm_set_gpio_level(LED_R, (4095 - y_raw) * 16); // Multiplicador 16x
        pwm_set_gpio_level(LED_B, x_raw * 16);          // Multiplicador 16x
        
        sleep_ms(50);
    }
    
    pwm_set_gpio_level(LED_R, 0);
    pwm_set_gpio_level(LED_B, 0);
    botao_pressionado = false;
}

void executar_buzzer() {
    // Configura PWM para o buzzer
    pwm_config config = pwm_get_default_config();
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_init(slice, &config, true);

    // Notas da abertura de Pokémon (frequências em Hz)
    const uint16_t notas[] = {
        659, 659, 659, 523, 659, 784, 392, 523, 
        392, 330, 440, 494, 466, 440, 392, 659
    };

    // Durações (em milissegundos)
    const uint16_t duracoes[] = {
        150, 150, 150, 150, 150, 300, 300, 150,
        300, 300, 150, 150, 150, 150, 300, 300
    };

    while(!botao_pressionado) {
        for(int i = 0; i < 16; i++) {
            if(notas[i] == 0) { // Pausa
                pwm_set_gpio_level(BUZZER_PIN, 0);
                sleep_ms(duracoes[i]);
                continue;
            }
            
            uint32_t top = clock_get_hz(clk_sys) / notas[i] - 1;
            pwm_set_wrap(slice, top);
            pwm_set_gpio_level(BUZZER_PIN, top / 2);
            sleep_ms(duracoes[i]);
            pwm_set_gpio_level(BUZZER_PIN, 0); // pausa entre notas
            sleep_ms(20);
        }
        sleep_ms(500); // Repetir após meio segundo
    }
    
    pwm_set_gpio_level(BUZZER_PIN, 0);
    botao_pressionado = false;
}

void executar_led_rgb() {
    // Configura PWM para LED_R e LED_G
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.0f);
    pwm_config_set_wrap(&config, 65535);

    gpio_set_function(LED_R, GPIO_FUNC_PWM);
    gpio_set_function(LED_G, GPIO_FUNC_PWM);
    
    uint slice_r = pwm_gpio_to_slice_num(LED_R);
    uint slice_g = pwm_gpio_to_slice_num(LED_G);
    
    pwm_init(slice_r, &config, true);
    pwm_init(slice_g, &config, true);

    uint16_t nivel = 0;
    bool direcao = true;

    while(!botao_pressionado) {
        if(direcao) {
            nivel += 3255;
            if(nivel >= 65535) direcao = false;
        } else {
            nivel -= 3255;
            if(nivel <= 3255) direcao = true;
        }
        
        pwm_set_gpio_level(LED_R, nivel);
        pwm_set_gpio_level(LED_G, 65535 - nivel);
        sleep_ms(10);
    }
    
    pwm_set_gpio_level(LED_R, 0);
    pwm_set_gpio_level(LED_G, 0);
    botao_pressionado = false;
}

int main() {
    setup();
    uint pos_y = 12;
    uint menu_selecao = 1;

    while(1) {
        if(estado == MENU_PRINCIPAL) {
            print_menu(pos_y);
            
            // Leitura do ADC para navegação do menu
            adc_select_input(0);
            uint bar_y_pos = adc_read() * 40 / 4095;
            
            if(bar_y_pos < 15 && menu_selecao < 3) {
                pos_y += 12;
                menu_selecao++;
                sleep_ms(100);
            } else if(bar_y_pos > 25 && menu_selecao > 1) {
                pos_y -= 12;
                menu_selecao--;
                sleep_ms(100);
            }

            // Seleciona opção do menu
            if(botao_pressionado) {
                switch(menu_selecao) {
                    case 1: estado = EXECUTANDO_JOYSTICK; break;
                    case 2: estado = EXECUTANDO_BUZZER; break;
                    case 3: estado = EXECUTANDO_LED_RGB; break;
                }
                botao_pressionado = false;
                ssd1306_clear(&disp);
            }
        }
        else {
            // Executa a função selecionada
            switch(estado) {
                case EXECUTANDO_JOYSTICK: executar_joystick(); break;
                case EXECUTANDO_BUZZER: executar_buzzer(); break;
                case EXECUTANDO_LED_RGB: executar_led_rgb(); break;
            }
            estado = MENU_PRINCIPAL;
            pos_y = 12;
            menu_selecao = 1;
        }
        sleep_ms(50);
    }
}
