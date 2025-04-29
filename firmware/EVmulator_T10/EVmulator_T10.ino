#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stdbool.h>

// Pin-Definitionen
#define LED_PIN PB0
#define CHARGE_PIN PB1

// Schwellenfrequenz in Hz
#define FREQ_THRESHOLD_HZ 500

// Watchdog-Prescaler-Makros
#define WDT_32MS (1 << WDP0)
#define WDT_125MS ((1 << WDP1) | (1 << WDP0))
#define WDT_1S ((1 << WDP2) | (1 << WDP1))

// Systemzustände
typedef enum {
  IDLE,
  MEASURE,
  CHARGE
} SystemState;

volatile SystemState state = IDLE;

// Zähler für Übergänge
volatile uint8_t nonzero_count = 0;
volatile uint8_t highfreq_count = 0;
volatile uint8_t lowfreq_count = 0;

// Aktuelle WDT-Zeit in Millisekunden
volatile uint16_t wdt_period_ms = 32;

void init_io(void) {
  DDRB |= (1 << LED_PIN) | (1 << CHARGE_PIN);      // LED und CHARGE als Ausgang
  PORTB &= ~((1 << LED_PIN) | (1 << CHARGE_PIN));  // Initial aus
}

void init_clock(void) {
  CCP = 0xD8;
  CLKPSR = 0x02;  // 128kHz / 4 → 32kHz
}

void disable_unused_modules(void) {
  //DIDR0 = 0xFF;
  ACSR |= (1 << ACD);      // Komparator aus
  ADCSRA &= ~(1 << ADEN);  // ADC aus
}

void init_timer(void) {
  TCCR0A = 0x00;
  //TCCR0B = (1 << CS00);  // Externer Clock an PB2, keine Teilung
  TCCR0B = (0 << ICNC0) | (0 << ICES0) | (0 << WGM03) | (1 << WGM02) | (1 << CS02) | (1 << CS01) | (1 << CS00);
  TCNT0 = 0;
}

// Umrechnung: 500 Hz * Intervall(ms) / 1000 = Schwelle
uint16_t get_frequency_threshold(void) {
  return (FREQ_THRESHOLD_HZ * wdt_period_ms) / 1000;
}

void set_watchdog(uint8_t wdt_prescaler) {
  cli();
  wdt_reset();

  // Intervall aktualisieren
  if (wdt_prescaler == WDT_32MS) {
    wdt_period_ms = 32;
  } else if (wdt_prescaler == WDT_125MS) {
    wdt_period_ms = 125;
  } else if (wdt_prescaler == WDT_1S) {
    wdt_period_ms = 1000;
  }

  CCP = 0xD8;
  WDTCSR = (1 << WDIE) | wdt_prescaler;
  sei();
}

void sleep_idle(void) {
  set_sleep_mode(SLEEP_MODE_IDLE);
  //set_sleep_mode(SLEEP_MODE_STANDBY);
  sleep_enable();
  sleep_cpu();
  sleep_disable();
}

// LED kurz blitzen (~1ms)
void pulse_led(void) {
  PORTB |= (1 << LED_PIN);
  for (volatile uint8_t i = 0; i < 40; i++) {
    __asm__ __volatile__("nop");
  }
  PORTB &= ~(1 << LED_PIN);
}

// Watchdog Interrupt-Service-Routine
ISR(WDT_vect) {
  uint8_t tcnt = TCNT0;
  TCNT0 = 0;
  uint16_t freq_thresh = get_frequency_threshold();

  pulse_led();

  switch (state) {
    case IDLE:
      if (tcnt == 0) {
        nonzero_count = 0;
      } else {
        nonzero_count++;
        if (nonzero_count >= 10) {
          nonzero_count = 0;
          set_watchdog(WDT_125MS);
          state = MEASURE;
        }
      }
      break;

    case MEASURE:
      
      if (tcnt == 0) {
        nonzero_count = 0;
        set_watchdog(WDT_32MS);
        state = IDLE;
      } else if (tcnt > freq_thresh) {
        highfreq_count++;
        if (highfreq_count >= 10) {
          highfreq_count = 0;
          set_watchdog(WDT_1S);
          PORTB |= (1 << CHARGE_PIN);
          state = CHARGE;
        }
      } else {
        highfreq_count = 0;
      }
      break;

    case CHARGE:
      if (tcnt < freq_thresh) {
        lowfreq_count++;
        if (lowfreq_count > 5) {
          lowfreq_count = 0;
          PORTB &= ~(1 << CHARGE_PIN);
          set_watchdog(WDT_125MS);
          state = MEASURE;
        }
      } else {
        lowfreq_count = 0;
      }
      break;
  }
}

void setup() {
  cli();
  init_clock();
  disable_unused_modules();
  init_io();
  init_timer();
  set_watchdog(WDT_32MS);  // Start im IDLE
  sei();
}

void loop() {
  sleep_idle();
}
