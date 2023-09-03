#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/io.h>

#define LED_GN 0
#define LED_RD 1
#define CHARGE 4

int pwm_good_cntr = 0;
int pwm_bad_cntr = 0;
uint8_t pwm_status = 0;

// Blank WDT interrupt
ISR(WDT_vect) {
}

// Enter Sleep Mode
void enterSleep(void) {
  //set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_mode();  // Start sleep mode

  // Nothing happens until WDT timeout

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
  WDTCR = (0 << WDP2) | (0 << WDP1) | (1 << WDP0);

  WDTCR |= _BV(WDIE);  // Enable the WDT interrupt.
}

void setupT0() {

  TCCR0A = 0x00;  //Normal mode

  // Ext. T0 as input. Rising edge
  TCCR0B = 0;
  TCCR0B = (1 << CS02) | (1 << CS01) | (1 << CS00);
}



void setup() {

  pinMode(LED_GN, OUTPUT);
  pinMode(LED_RD, OUTPUT);
  pinMode(CHARGE, OUTPUT);

  digitalWrite(CHARGE, LOW);

  ADCSRA &= ~(1 << ADEN);  // Disable ADC

  setupT0();
  setupWDT();
}

void loop() {

  // Each 36ms -> 1kHz means 36 counts
  // 30 = 900Hz, 40 =1.2kHz
  if ((TCNT0 >= 30) && (TCNT0 <= 40)) {
    pwm_bad_cntr = 0;
    pwm_good_cntr++;
  } else {
    pwm_good_cntr = 0;
    pwm_bad_cntr++;
  }
  TCNT0 = 0;

  //approx 5 seconds of good pwm and low flag
  if (pwm_good_cntr > 5*30 && pwm_status == 0) {
    pwm_status = 1;
    pwm_good_cntr = 0;
  } else if (pwm_bad_cntr > 5*30 && pwm_status == 1) {
    pwm_status = 0;
    pwm_bad_cntr = 0;
  }

  if (pwm_status == 1) {
    digitalWrite(LED_GN, 1);
    delay(1);
    digitalWrite(LED_GN, 0);

    digitalWrite(CHARGE, HIGH);

  } else {
    digitalWrite(LED_RD, 1);
    delay(1);
    digitalWrite(LED_RD, 0);

    digitalWrite(CHARGE, LOW);
  }

  enterSleep();
}