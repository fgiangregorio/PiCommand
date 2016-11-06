
// Device: PIC16F1718
// Using xc8 v1.30
//
// Processor clock set to 32MHz
//
//***********************************************************************************
#include <pic.h>
#include <string.h>


#pragma config FOSC=INTOSC
#pragma config WDTE=OFF
#pragma config BOREN=0x00
#pragma config CLKOUTEN=1

#define EYETX                  LATBbits.LATB6  // RB6 is tx to eyes
#define LEDB                   LATBbits.LATB2  // RB2 is blue led
#define LEDG                   LATBbits.LATB3  // RB3 is green led
#define LEDR                   LATBbits.LATB4  // RB4 is red led

#define M1                      LATAbits.LATA6  // for 56kHz
#define debugpin                LATAbits.LATA4  // debug output pin

#define I2C_SLAVE_ADDRESS 0x4F 
#define I2C_SLAVE_MASK    0x7F

enum
{
   IDLE
   , START
   , ALLON
   , ALLOFF
   , Rled
   , Gled
   , Bled
   , WAIT
   , EYESON
   , EYESOFF
   , STOP
};

unsigned long  fiftymillisecs = 0;
unsigned int   timercnt = 0;
unsigned char  tick     = 0;
unsigned char  txSlot   = 0;
unsigned char  txEvent  = 0;
unsigned char  hipower  = 0;    // if set, two IOs are used to drive the IR leds
unsigned char  state    = IDLE;

/*
 0xAA, 0xCC, 0xE1, 0x25, 0x00   robot 1, crc = add 3 bytes, then >> 4
 0xAA, 0xCC, 0xD2, 0x24, 0x00
 0xAA, 0xCC, 0xB4, 0x22, 0x00
 0xAA, 0xCC, 0x96, 0x20, 0x00
 0xAA, 0xCC, 0x87, 0x1F, 0x00
 0xAA, 0xCC, 0x78, 0x1E, 0x00
 */
unsigned char  TxBuffer[5]  = {0xAA, 0xCC, 0xE1, 0x25, 0x00};
unsigned char  EyeBuffer[7] = {0x00, 0xff, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f};

volatile unsigned char  i2c_state = IDLE;
volatile unsigned char  i2c_start = 0;
volatile unsigned char  i2c_data = 0;   // data prepared for transmission to master
volatile unsigned char  i2c_ptr  = 0;
volatile unsigned char  i2c_buffer[30]; // data received from master for processing by controller

volatile unsigned char  database[30];   // data structure that keeps status and message from master

unsigned char  RxBuffer[5];

typedef struct
   {
   int wridx;
   int rdidx;
   int cmd[16];
   int on_ttl[16];
   int off_ttl[16];
   } led_msg_q_type;

led_msg_q_type ledRq;   // command structure for red led
led_msg_q_type ledGq;   // command structure for green led
led_msg_q_type ledBq;   // command structure for blue led



void interrupt OnlyOne_ISR(void)    // There is only one interrupt on this CPU
{
   /*============================================================================*/
   // Timer IRQ
   if (TMR2IF)
      {      
      TMR2  = 0x00 ;          // Create 56KHz
      TMR2IF   = 0;           // Clear interrupt flag
      M1 = ~M1;
      }
   
   /*============================================================================*/
   if (TMR0IF)          // check if Timer0 interrupt has occurred
      {
      TMR0   = 160;     //~~50us (measured)
      TMR0IF = 0;
      timercnt++;
      tick   = 1;
      }

   /*============================================================================*/
   if (SSP1IF)
    {
      SSP1IF   = 0;
      i2c_data = SSP1BUF;

      if (SSP1STATbits.P == 1)  // detect stop bit
      {
        i2c_ptr = 0;
        return;
      }
      
      // Master Write Operation.
         //this is write operation from master
         i2c_buffer[i2c_ptr] = i2c_data;
         CKP  = 1;
         if (i2c_ptr == 9)
         {
           // see where the received data needs to be placed in database memory, offset (location) given by i2c_buffer[2] and length by i2c_buffer[3]
           for (int i = 0; i < i2c_buffer[3]; i++)
           {
             database[i + i2c_buffer[2]] = i2c_buffer[i + 4];
           }
           
           if (i2c_buffer[2] == 6)        // this is RGB LED data
           {
             
            if ( (database[6]) == 0x04) // this is red led
            {
              ledRq.cmd[0] = database[7];
              ledRq.on_ttl [0] = (int) ((database[8] << 8) | database[9]);
              ledRq.off_ttl[0] = (int) ((database[10] << 8) | database[11]);
            }
            if ( (database[6]) == 0x05) // this is green led
            {
              ledGq.cmd[0] = database[7];
              ledGq.on_ttl [0] = (int) ((database[8] << 8) | database[9]);
              ledGq.off_ttl[0] = (int) ((database[10] << 8) | database[11]);
            }            
            if ( (database[6]) == 0x06) // this is blue led
            {
              ledBq.cmd[0] = database[7];
              ledBq.on_ttl [0] = (int) ((database[8] << 8) | database[9]);
              ledBq.off_ttl[0] = (int) ((database[10] << 8) | database[11]);
            }
           debugpin = 1;
           debugpin = 0;
           }
           
           if (i2c_buffer[2] == 12)       // this is the eye data
           {
            EyeBuffer[0] = 0x00;          // this is the sync word that must be added to protocol
            EyeBuffer[1] = database[12];  // first line data word
            EyeBuffer[2] = database[13];
            EyeBuffer[3] = database[14];
            EyeBuffer[4] = database[15];
            EyeBuffer[5] = database[16];
            EyeBuffer[6] = database[17];  // last line data word
           }
           
           

         }
         i2c_ptr++;

      }   // end I2C interrupt
  
   
   /*============================================================================*/
   if (RCIF)          // check if UART receive interrupt has occurred
      {
        RxBuffer[0] = RC1REG;
        RCIF = 0;
      // do something, read rx buffer etc
      }


}
/*============================================================================*/


/*============================================================================*/
void delayus(unsigned int us)
{
   //unsigned int start = TMR1 ;
   unsigned int delta = 0 ;
   TMR1 = 0;
   T1CONbits.TMR1ON = 1;
   while ( delta <= us )
      {
      delta = (unsigned int)(TMR1);
      }
   T1CONbits.TMR1ON = 0;
}

/*============================================================================*/
unsigned long get_milliseconds( )
{
   return( fiftymillisecs ) ;
}

/*============================================================================*/
unsigned long delta_milliseconds ( unsigned long old_value )
{
   return get_milliseconds ( ) - old_value ;
}

void EUSART_Write(char txData)
{
    while(0 == PIR1bits.TXIF);
    TX1REG = txData;                // Write the data byte to the USART.
    while (TX1STAbits.TRMT == 0);   // wait for buffer to empty

}

/*============================================================================*/
void sio_putstring(char *str)
{
   unsigned char i, len;

   len =  strlen ( (char *) str ) ;
   for ( i=0; i < len; i++ )
      {
      //sio_putch( str[i] );
      EUSART_Write (str[i]);
      //delayus(100);
      }
}

/*============================================================================*/
//Converts an unsigned 16 bit number into 5 (or less) digits and prints to UART
//void sio_puti(unsigned int number)
//{
//   char tmp[8];
//   sprintf( tmp, "%d", number );
//   sio_putstring( tmp );
//}
void EUSART_Baudrate(int baud)
{
  if (baud == 9600)
  {
    SP1BRGL = 0x41;
    SP1BRGH = 0x03;
    return;
  }
  else if (baud == 2400)
  {
    SP1BRGL = 0x05;
    SP1BRGH = 0x0D;
    return;
  }
}
/*============================================================================*/

void EUSART_Initialize(void)
{
    // Set the EUSART module to the options selected in the user interface.

    // ABDOVF no_overflow; SCKP Non-Inverted; BRG16 16bit_generator; WUE disabled; ABDEN disabled; 
    BAUD1CON = 0x08;

    // SPEN enabled; CREN enabled; ADDEN disabled; SREN disabled; 
    RC1STA = 0x90;

    // TX9 8-bit; TX9D 0; SENDB sync_break_complete; TXEN enabled; SYNC asynchronous; BRGH hi_speed; CSRC slave; 
    TX1STA = 0x24;
    EUSART_Baudrate (2400);
} 

void setup_cpu ( void )
{
   // set up oscillator control register
   OSCCONbits.SPLLEN =1;        // PLL is enabled
   OSCCONbits.IRCF   =0xE;      // select 32MHz
   OSCCONbits.SCS    =0x0;      // with SCS set to zero, clock source is set by FOSC

   PORTA = 0xFF;
   PORTB = 0xFF;
   PORTC = 0xFF;
   
   LEDR    = 0;
   LEDG    = 0;
   LEDB    = 0;

   // PORT assignments
   TRISA = 0xAB;    
   TRISB = 0x00;    
   TRISC = 0x9E;    // RC3 and RC4 to be used for I2C, must be set as inputs
   ANSELA = 0x3B;   // only RA0-RA4 are analog input; ra2 = dig output
   ANSELB = 0x00;   // all digital signals
   ANSELC = 0x00;   // all digital signals
   
   RXPPS  = 0x11;   // assign UART RX input to RC1

   RB0PPS = 0x06;   // assign CLC3OUT to RB0
   RB1PPS = 0x06;   // assign CLC3OUT to RB1
   RB5PPS = 0x06;   // assign CLC3OUT to RB5
   RC0PPS = 0x06;   // assign CLC3OUT to RC0
   RC5PPS = 0x06;   // assign CLC3OUT to RC5
   RC6PPS = 0x06;   // assign CLC3OUT to RC6


   RB7PPS = 0x14;   // this will be transmission to RIGHT EYE

   RB6PPS = 0x00;   // I/O for transmission to the eyes
   
	CLCIN0PPS = 0x06;
	CLCIN1PPS = 0x06;
	CLCIN2PPS = 0x0F;
	CLCIN3PPS = 0x0F;

        CLC3CON   = 0x80;
        CLC3POL   = 0x00;
	CLC3SEL0  = 0x00; // CLCIN0PPS
	CLC3SEL1  = 0x01; // CLCIN1PPS
	CLC3SEL2  = 0x02; // CLCIN2PPS
	CLC3SEL3  = 0x03; // CLCIN3PPS
	CLC3GLS0  = 0x0A;
	CLC3GLS1  = 0x0A;
	CLC3GLS2  = 0xA0;
	CLC3GLS3  = 0xA0;

        RC3PPS = 0x10;  // select SCL for this pin
        RC4PPS = 0x11;  // select SDA for this pin
    
   // Initials Timer 0 settings
   OPTION_REGbits.PS     = 1 ;   // pre-scaler not used for Timer0
   OPTION_REGbits.PSA    = 0 ;   // pre-scaler is assigned to Timer0
   OPTION_REGbits.TMR0CS = 0 ;
   INTCONbits.TMR0IE     = 1 ;     // Enable Timer0 interrupt

   // Initialize Timer 1
   T1CONbits.TMR1CS = 0x01;
   T1CONbits.T1CKPS = 0;
   TMR1 = 0x1000;
   
   // Initialize Timer 2 ... General timing timer. @8MHz
   PR2      = 59;   // 59=56.30kHz, 60=55.55, 50=64.43kHz, 70=48.73kHz, 93=38kHz
   TMR2     = 0x0;
   TMR2IF   = 0;
   T2CON    = 0x04; // Timer2 on
   TMR2IE   = 1;

   // Initialize the I2C
   SSP1IF  = 0;
   SSP1STAT = 0x80;
   SSP1ADD = (I2C_SLAVE_ADDRESS << 1);
   SSP1MSK = 0xff;
   SSP1CON1 = 0x3E; // enable start and stop bit interrupts
   SSP1CON2 = 0x00;
   SSP1CON3 = 0x20;
   SSP1IE  = 1;  // enable I2C interrupts
    
   EUSART_Initialize();

   //RCIE     = 1;
   INTCONbits.PEIE   = 1;
   INTCONbits.GIE    = 1;  //Enable all configured interrupts

}


/*============================================================================*/

int main ( )
{
   unsigned long idle_timer      = 0 ;

   setup_cpu();
   
    ledRq.rdidx = 0;
    ledRq.wridx = 0;
    ledRq.cmd[0] = 0;
    ledRq.on_ttl[0] = 0;
    ledRq.off_ttl[0] = 0;
    
    ledGq.rdidx = 0;
    ledGq.wridx = 0;
    ledGq.cmd[0] = 0;
    ledGq.on_ttl[0] = 0;
    ledGq.off_ttl[0] = 0;
    
    ledBq.rdidx = 0;
    ledBq.wridx = 0;
    ledBq.cmd[0] = 0;
    ledBq.on_ttl[0] = 0;
    ledBq.off_ttl[0] = 0;

    LEDR    = 1;
    LEDG    = 1;
    LEDB    = 1;
   
   while (1)
      {
      CLRWDT();


      if (tick == 1)
      {
         tick = 0;


        if (ledRq.cmd[0] != 0)
        {
          if (ledRq.cmd[0] == 0x03) LEDR = !LEDR; // toggle red led
          else if (ledRq.cmd[0] == 0x01) LEDR = 1; // turn red led off
          else if (ledRq.cmd[0] == 0x02) LEDR = 0; // turn red led on
          ledRq.cmd[0] = 0;
        }
        if (ledGq.cmd[0] != 0)
        {
          if (ledGq.cmd[0] == 0x03) LEDG = !LEDG; // toggle green led
          else if (ledGq.cmd[0] == 0x01) LEDG = 1; // turn green led off
          else if (ledGq.cmd[0] == 0x02) LEDG = 0; // turn green led on
          ledGq.cmd[0] = 0;
        }
        if (ledBq.cmd[0] != 0)
        {
          if (ledBq.cmd[0] == 0x03) LEDB = !LEDB; // toggle blue led
          else if (ledBq.cmd[0] == 0x01) LEDB = 1; // turn blue led off
          else if (ledBq.cmd[0] == 0x02) LEDB = 0; // turn blue led on
          ledBq.cmd[0] = 0;
        }
         
         // update timing parameters
         if (timercnt > 1000)
         {
            timercnt = 0;
            fiftymillisecs++;
            txSlot++;
            txEvent = 1;
            
             if (txSlot == 6)
             {
               //debugpin = 1;
               txSlot = 0;
               }
             else
            {
               //debugpin = 0;

            }
         }

        // now do next task
         if (txEvent == 1)
         {
            txEvent = 0;
            if (txSlot == 0)    // slot 0 is for the master 
            {
              
                //delayus (50);  // 
                //LEDR = 0;
                sio_putstring (TxBuffer);
                //LEDR = 1;
                
                
                
                // now transmit data to eyes
                RB0PPS = 0x00;   // make IR LED pins normal IOs
                RB1PPS = 0x00;
                RB5PPS = 0x00;
                RC0PPS = 0x00;
                RC5PPS = 0x00;
                RC6PPS = 0x00;
                RB6PPS = 0x14;   // switch IO to UART TX line

                EUSART_Baudrate (9600);

                for (int i = 0; i < 7; i++)
                {
                  EUSART_Write (EyeBuffer[i]);
                }
                RB6PPS = 0x00;   // switch IO back to plain digital output
                RB0PPS = 0x06;   // assign CLC3OUT to RB0
                RB1PPS = 0x06;   // assign CLC3OUT to RB1
                RB5PPS = 0x06;   // assign CLC3OUT to RB5
                RC0PPS = 0x06;   // assign CLC3OUT to RC0
                RC5PPS = 0x06;   // assign CLC3OUT to RC5
                RC6PPS = 0x06;   // assign CLC3OUT to RC6
                EUSART_Baudrate (2400); // restore baud rate
              
            }
            else
            {
            //sio_putstring ("6789");
            }
         }



      }  // if tick


      }
   return 0;
}
