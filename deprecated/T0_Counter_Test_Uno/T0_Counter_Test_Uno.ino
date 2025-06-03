void setupT1() {

  TCCR1A = 0x00;  //Normal mode
  // Ext. T0 as input. Rising edge
  TCCR1B = 0;
  TCCR1B |= (1 << CS12) | (1 << CS11) | (1 << CS10);
}

void setup() {

  ADCSRA &= ~(1 << ADEN);  // Disable ADC

  Serial.begin(115200);


  setupT1();
  //setupWDT();
}

void loop() {

  Serial.println("Counter T1:" + String(TCNT1));
  TCNT1 = 0;

  delay(100);
}