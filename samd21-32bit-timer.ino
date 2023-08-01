// Parts of this taken from https://forum.arduino.cc/index.php?topic=658058.0
// Parts of this taken from https://forum.arduino.cc/index.php?topic=425385.0
// Setup TC4/TC5 to capture pulse-width and period
volatile boolean periodComplete, outlevel=0;
volatile uint32_t isrPeriod;
volatile uint32_t isrPulsewidth;
uint32_t period;
uint32_t pulsewidth;
boolean reSet=1;

#define SerialUSB Serial // this is needed for trinket m0
#define PIN 7            // 7 is PB9 on XIAO, 2 is PA9 on trinket m0, both have pin #9 which is odd 


void setup()
{
  //pinMode(1, OUTPUT);
  //digitalWrite(1, outlevel);
  //Serial1.begin(1200);
  SerialUSB.begin(115200);                       // Send data back on the native port
  while(!SerialUSB);                             // Wait for the SerialUSB port to be ready
 
  REG_PM_APBCMASK |= PM_APBCMASK_EVSYS |         // Switch on the event system peripheral
                     PM_APBCMASK_TC4   |         // Switch on the TC4 peripheral
                     PM_APBCMASK_TC5;            // Switch on the TC5 peripheral

  REG_GCLK_GENDIV = GCLK_GENDIV_DIV(0) |         // Divide the 48MHz system clock by 1 = 48MHz
                    GCLK_GENDIV_ID(1);           // Set division on Generic Clock Generator (GCLK) 1
  while (GCLK->STATUS.bit.SYNCBUSY);             // Wait for synchronization

  REG_GCLK_GENCTRL = GCLK_GENCTRL_IDC |          // Set the duty cycle to 50/50 HIGH/LOW
                     GCLK_GENCTRL_GENEN |        // Enable GCLK
                     GCLK_GENCTRL_SRC_DFLL48M |  // Set the clock source to 48MHz
                     GCLK_GENCTRL_ID(1);         // Set clock source on GCLK 1
  while (GCLK->STATUS.bit.SYNCBUSY);             // Wait for synchronization

  // second write to CLKCTRL, different ID
  REG_GCLK_CLKCTRL = GCLK_CLKCTRL_CLKEN      |   // Enable the generic clock
                     GCLK_CLKCTRL_GEN_GCLK1  |   // on GCLK1
                     GCLK_CLKCTRL_ID_TC4_TC5;    // Feed the GCLK1 also to TC4 and TC5 
  while (GCLK->STATUS.bit.SYNCBUSY);             // Wait for synchronization

  // Enable the port multiplexer on pin number "PIN"
  PORT->Group[g_APinDescription[PIN].ulPort].PINCFG[g_APinDescription[PIN].ulPin].bit.PULLEN = 1; // out is default low so pull-down
  PORT->Group[g_APinDescription[PIN].ulPort].PINCFG[g_APinDescription[PIN].ulPin].bit.INEN   = 1;
  PORT->Group[g_APinDescription[PIN].ulPort].PINCFG[g_APinDescription[PIN].ulPin].bit.PMUXEN = 1;
  // Set-up the pin as an EIC (interrupt) peripheral on an odd pin. "0" means odd, "A" function is EIC
  PORT->Group[g_APinDescription[PIN].ulPort].PMUX[g_APinDescription[PIN].ulPin >> 1].reg |= PORT_PMUX_PMUXO_A;

  EIC->EVCTRL.reg     = EIC_EVCTRL_EXTINTEO9;                           // Enable event output on external interr
  //EIC->CONFIG[1].reg  = EIC_CONFIG_SENSE1_HIGH;                         // Set event detecting a high (config 1, #1 is 9
  EIC->CONFIG[1].reg  = EIC_CONFIG_SENSE1_LOW;                         // Set event detecting a high (config 1, #1 is 9
  EIC->INTENCLR.reg   = EIC_INTENCLR_EXTINT9;                           // Clear the interrupt flag on channel 9
  EIC->CTRL.reg       = EIC_CTRL_ENABLE;                                // Enable EIC peripheral
  while (EIC->STATUS.bit.SYNCBUSY);                                     // Wait for synchronization

  REG_EVSYS_CHANNEL = EVSYS_CHANNEL_EDGSEL_NO_EVT_OUTPUT |              // No event edge detection, we already have it on the EIC
                      EVSYS_CHANNEL_PATH_ASYNCHRONOUS    |              // Set event path as asynchronous
                      EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_EIC_EXTINT_9) |  // Set event generator (sender) as external interrupt 9
                      EVSYS_CHANNEL_CHANNEL(0);                         // Attach the generator (sender) to channel 0
  
  REG_EVSYS_USER = EVSYS_USER_CHANNEL(1) |                              // Attach the event user (receiver) to channel 0 (n + 1)
                   EVSYS_USER_USER(EVSYS_ID_USER_TC4_EVU);              // Set the event user (receiver) as timer TC4

  REG_TC4_EVCTRL  |= TC_EVCTRL_TCEI |            // Enable the TC event input
                     TC_EVCTRL_EVACT_PPW;         // Set up the timer for capture: CC0 period, CC1 pulsewidth
                   
  REG_TC4_READREQ = TC_READREQ_RREQ |            // Enable a read request
                    TC_READREQ_ADDR(0x18);       // Offset of the CTRLC register, 32bit
  while (TC4->COUNT32.STATUS.bit.SYNCBUSY);      // Wait for (read) synchronization
  
  
  REG_TC4_CTRLC |= TC_CTRLC_CPTEN1 |             // Enable capture on CC1
                   TC_CTRLC_CPTEN0;              // Enable capture on CC0
  while (TC4->COUNT32.STATUS.bit.SYNCBUSY);      // Wait for (write) synchronization

  NVIC_SetPriority(TC4_IRQn, 0);                 // Set Nested Vector Interrupt Controller (NVIC) priority for TC4 to 0 (highest)
  NVIC_EnableIRQ(TC4_IRQn);                      // Connect TC4 timer to the Nested Vector Interrupt Controller (NVIC)
 
  REG_TC4_INTENSET = TC_INTENSET_MC1 |           // Enable compare channel 1 (CC1) interrupts
                     TC_INTENSET_MC0;            // Enable compare channel 0 (CC0) interrupts
 
  REG_TC4_CTRLA |= TC_CTRLA_PRESCALER_DIV64 |     // Set prescaler to 16, 48MHz/64 = 0.75MHz
                   TC_CTRLA_MODE_COUNT32   |     // Set the TC4 timer to 32-bit mode in conjuction with timer TC5
                   TC_CTRLA_ENABLE;              // Enable TC4
  while (TC4->COUNT32.STATUS.bit.SYNCBUSY);      // Wait for synchronization
}

void loop()
{
  if (periodComplete)                            // Check if the period is complete
  {
    noInterrupts();                              // Read the new period and pulse-width
    period     = isrPeriod;                   
    pulsewidth = isrPulsewidth;
    interrupts();
    if(!reSet)  {
      SerialUSB.print("per=");                  // Output the results in microseconds
      SerialUSB.println( (period)/3*4/1000.0 );                    
      //SerialUSB.print("wid=");
      //SerialUSB.println( (pulsewidth)/3*4/1000.0 );
      //SerialUSB.println( (period-pulsewidth)/3*4/1000.0 );
    }
    periodComplete = false;                      // Start a new period
    reSet = 0;
  }

}

void TC4_Handler()                               // Interrupt Service Routine (ISR) for timer TC4
{     
  // Check for match counter 0 (MC0) interrupt
  if (TC4->COUNT32.INTFLAG.bit.MC0)             
  {
    REG_TC4_READREQ = TC_READREQ_RREQ |                    // Enable a read request
                      TC_READREQ_ADDR(0x18);               // Offset address of the CC0 register
    while (TC4->COUNT32.STATUS.bit.SYNCBUSY);              // Wait for (read) synchronization
    isrPeriod = REG_TC4_COUNT32_CC0 ;               // Copy the period, adjusted to 3*microseconds
    periodComplete = true;                                 // Indicate that the period is complete
  }

  // Check for match counter 1 (MC1) interrupt
  if (TC4->COUNT32.INTFLAG.bit.MC1)           
  {
    REG_TC4_READREQ = TC_READREQ_RREQ |                    // Enable a read request
                      TC_READREQ_ADDR(0x1A);               // Offset address of the CC1 register
    while (TC4->COUNT32.STATUS.bit.SYNCBUSY);              // Wait for (read) synchronization
    isrPulsewidth = REG_TC4_COUNT32_CC1;               // Copy the pulse-width, adjusted to 3*microseconds
  }
}
