#include "status/led.h"

#include "pico/cyw43_arch.h"

bool led_init(void) { return cyw43_arch_init() == 0; }

void led_set(bool on) { cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on); }
