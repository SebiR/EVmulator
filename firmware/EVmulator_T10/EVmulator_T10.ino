#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stdbool.h>

// Pin Definitions
#define LED_PIN PB0
#define CHARGE_PIN PB1

// Threshold frequency to detect good PWM
#define FREQ_THRESHOLD_HZ 500

// Watchdog Prescaler
#define WDT_32MS (1 << WDP0)
#define WDT_125MS ((1 << WDP1) | (1 << WDP0))
#define WDT_1S ((1 << WDP2) | (1 << WDP1))

// System states
typedef enum {
  IDLE,
  MEASURE,
  CHARGE
} SystemState;

volatile SystemState state = IDLE;

// Counters to switch between states
volatile uint8_t nonzero_count = 0;
volatile uint8_t highfreq_count = 0;
volatile uint8_t lowfreq_count = 0;

// Current WDT interval in ms
volatile uint16_t wdt_period_ms = 32;

void init_io(void) {
  DDRB |= (1 << LED_PIN) | (1 << CHARGE_PIN);      // LED and CHARGE as output
  PORTB &= ~((1 << LED_PIN) | (1 << CHARGE_PIN));  // Initially off
}

void init_clock(void) {
  //CCP = 0xD8;
  //CLKPSR = 0x02;  // 128kHz / 4 → 32kHz system clock
  //CCP = 0xD8;                              // Unprotect CLKMSR reg
  //CLKMSR = (0 << CLKMS1) | (1 << CLKMS0);  // Set Clock source to 128kHz WDT oscillator

  CCP = 0xD8;                                                              // Unprotect CLKPSR reg
  CLKPSR = (0 << CLKPS3) | (0 << CLKPS2) | (1 << CLKPS1) | (0 << CLKPS0);  // Divide Clock by 4 -> 32kHz
}

void disable_unused_modules(void) {
  //DIDR0 = 0xFF;           // DO NOT USE
  ACSR |= (1 << ACD);      // analog comparator off
  ADCSRA &= ~(1 << ADEN);  // ADC off
  PRR = (1 << PRADC);      // Power down ADC
}

void init_timer(void) {
  TCCR0A = 0x00;
  TCCR0B = (0 << ICNC0) | (0 << ICES0) | (0 << WGM03) | (1 << WGM02) | (1 << CS02) | (1 << CS01) | (1 << CS00);
  TCNT0 = 0;
}

// Calculation: 500 Hz * Interval(ms) / 1000 = threshold
uint16_t get_frequency_threshold(void) {
  return (FREQ_THRESHOLD_HZ * wdt_period_ms) / 1000;
}

void set_watchdog(uint8_t wdt_prescaler) {
  cli();
  wdt_reset();

  // Update WDT interval
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

// Flash LED (~1ms)
void pulse_led(void) {
  PORTB |= (1 << LED_PIN);
  for (volatile uint8_t i = 0; i < 80; i++) {
    __asm__ __volatile__("nop");
  }
  PORTB &= ~(1 << LED_PIN);
}

// Watchdog ISR
ISR(WDT_vect) {
  uint16_t tcnt = TCNT0;
  TCNT0 = 0;
  uint16_t freq_thresh = get_frequency_threshold();

  pulse_led();

  // State FSM
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
  set_watchdog(WDT_32MS);  // Start in IDLE state
  sei();
}

void loop() {
  sleep_idle();
}
