/*

Fuses:
L 0xC4
H 0xD7
E 0xFF
WD. Osc. 128 kHz
*/

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/io.h>
#include <util/delay.h>

#define LED_GN 0  // PB0 - Green indicator LED
#define LED_RD 1  // PB1 - Red indicator LED
#define CHARGE 4  // PB4 - MOSFET to activate second resistor

int pwm_good_cntr = 0;
int pwm_bad_cntr = 0;
uint8_t pwm_status = 0;

// Blank WDT interrupt
ISR(WDT_vect) {
}

// Enter Sleep Mode
void enterSleep(void) {
  //  Unfortunately, power down stops the timers, so we can't use it
  //set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_mode();  // Start sleep mode

  // Nothing happens until WDT timeout

  sleep_disable();
  power_all_enable();
}

// Setup the Watch Dog Timer (WDT)
// http://modelleisenbahn-steuern.de/controller/atmega8/6-5-wdtcr-register-atmega8.htm
// https://www.best-microcontroller-projects.com/attiny-ultra-low-power.html
void setupWDT() {

  MCUSR &= ~(1 << WDRF);  // Clear the WDRF (Reset Flag).

  // Setting WDCE allows updates for 4 clock cycles end is needed to
  // change WDE or the watchdog pre-scalers.
  WDTCR |= (1 << WDCE) | (1 << WDE);

  // WD triggers every ~74ms
  WDTCR = (0 << WDP3) | (0 << WDP2) | (1 << WDP1) | (0 << WDP0);

  WDTCR |= _BV(WDIE);  // Enable the WDT interrupt.
}

//  Timer 0 is used as a counter to check the presence of the pilot PWM signal
void setupT0() {

  // Normal port operation, OC0A/OC0B disconnected, Normal Timer operation
  TCCR0A = (0 << COM0A1) | (0 << COM0A0) | (0 << COM0B1) | (0 << COM0B0) | (0 << WGM01) | (0 << WGM02);

  // Ext. T0 as input. Rising edge
  TCCR0B = 0;
  TCCR0B = (0 << FOC0A) | (0 << FOC0B) | (0 << WGM02) | (1 << CS02) | (1 << CS01) | (1 << CS00);
}


void setup() {

  pinMode(LED_GN, OUTPUT);
  pinMode(LED_RD, OUTPUT);
  pinMode(CHARGE, OUTPUT);

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);

  digitalWrite(CHARGE, LOW);
  digitalWrite(LED_GN, LOW);
  digitalWrite(LED_RD, LOW);

  ADCSRA &= ~(1 << ADEN);  // Disable ADC

  setupT0();
  setupWDT();
}

void loop() {
  /*if ((TCNT0 >= 60) && (TCNT0 <= 90)) {
    digitalWrite(LED_RD, 1);
  } else {
    digitalWrite(LED_RD, 0);
  }
  TCNT0 = 0;*/
  
  // Each 74ms -> 1kHz means 74 counts
  // 30 = 900Hz, 40 =1.2kHz
  if ((TCNT0 >= 60) && (TCNT0 <= 90)) {
    pwm_bad_cntr = 0;
    pwm_good_cntr++;
  } else {
    pwm_good_cntr = 0;
    pwm_bad_cntr++;
  }
  TCNT0 = 0;

  //  approx 5 seconds of good pwm and low flag
  //  74ms period time means ~15Hz
  if (pwm_good_cntr > 5 * 15 && pwm_status == 0) {
    pwm_status = 1;
    pwm_good_cntr = 0;
  } else if (pwm_bad_cntr > 5 * 15 && pwm_status == 1) {
    pwm_status = 0;
    pwm_bad_cntr = 0;
  }

  //  Pilot PWM signal is good, switch on charging indication resistor
  if (pwm_status == 1) {
    digitalWrite(LED_GN, HIGH);
    _delay_ms(2);
    digitalWrite(LED_GN, LOW);

    digitalWrite(CHARGE, HIGH);

    // Pilot PWM signal lost, switch off charging resistor
  } else {
    digitalWrite(LED_RD, HIGH);
    _delay_ms(2);
    digitalWrite(LED_RD, LOW);

    digitalWrite(CHARGE, LOW);
  }

  //  Everything done, go to sleep
  enterSleep();
}