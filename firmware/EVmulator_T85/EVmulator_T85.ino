#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/io.h>

#define LED_GN 0
#define LED_RD 1
#define CHARGE 4

// WDT Interrupt Need some code for coming out of sleep.
// but is does not need to do anything! (just exist).
ISR(WDT_vect) {
}

// Enters the arduino into sleep mode.
void enterSleep(void) {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();  // Start sleep mode

  // Hang here until WDT timeout

  sleep_disable();
  power_all_enable();
}

// Setup the Watch Dog Timer (WDT)
void setupWDT() {

  MCUSR &= ~(1 << WDRF);  // Clear the WDRF (Reset Flag).

  // Setting WDCE allows updates for 4 clock cycles end is needed to
  // change WDE or the watchdog pre-scalers.
  WDTCR |= (1 << WDCE) | (1 << WDE);

  // WD each 500ms
  // WDTCR  = (0<<WDP3) | (1<<WDP2) | (0<<WDP0) | (1<<WDP0);

  // WD each ~34ms
  WDTCR = (0 << WDP3) | (0 << WDP2) | (0 << WDP1) | (0 << WDP1);

  WDTCR |= _BV(WDIE);  // Enable the WDT interrupt.
}

void setupT0() {

TCCR0A=0x00;             //Normal mode
  // Ext. T0 as input. Rising edge
  TCCR0B = (1 << CS02) | (1 << CS01) | (1 << CS00);
}

void setup() {

  pinMode(LED_GN, OUTPUT);
  pinMode(LED_RD, OUTPUT);
  pinMode(CHARGE, OUTPUT);

  ADCSRA &= ~(1 << ADEN);  // Disable ADC


  setupT0();
  setupWDT();

}

void _loop() {
  // Re-enter sleep mode.
  enterSleep();
}

void loop() {

  // Each 34ms -> 1kHz means 34 counts
  if ((TCNT0 >= 10) && (TCNT0 <= 100)) {

    digitalWrite(LED_GN, 1);
    delay(2);
    digitalWrite(LED_GN, 0);
  } else {
    digitalWrite(LED_RD, 1);
    delay(2);
    digitalWrite(LED_RD, 0);
  }

  TCNT0 = 0;

  enterSleep();
}