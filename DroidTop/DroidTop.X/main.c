
// Device: PIC16F1718
// Using xc8 v1.30
//
// now working onI2C
//
//***********************************************************************************
#include <pic.h>
#include <string.h>


#pragma config FOSC=INTOSC
#pragma config WDTE=OFF
#pragma config BOREN=0x00
#pragma config CLKOUTEN=1

#define LEDB                   LATBbits.LATB2  // RB2 is blue led
#define LEDG                   LATBbits.LATB3  // RB3 is green led
#define LEDR                   LATBbits.LATB4  // RB4 is red led

#define M1                      LATAbits.LATA6  // for 56kHz
#define debugpin                LATBbits.LATB6  // debug output pin

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
unsigned char  MsgBuffer[10] = {0x55,   // status - lsb
                                0xAA,   // status - msb (spare for now)
                                0x00,   // MyID - lsb
                                0x34,   // MyID - msb
                                0x00,   // robot 0 ID - lsb
                                0x00,   // robot 0 ID - msb
                                0x00,   // robot 1 ID - lsb
                                0x00,   // robot 1 ID - msb
                                0x07,   // LED setting R=bit2, G=bit1, B=bit0
                                0x02};  // spare

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




}
/*============================================================================*/


/*============================================================================*/
void delayus(unsigned int us)
{
   unsigned int start = TMR1 ;
   unsigned int delta = 0 ;
   while ( delta <= us )
      {
      delta = (unsigned int)(TMR1 - start );
      }
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
   int i, len;

   len =  strlen ( (char *) str ) ;
   for ( i=0; i<len; i++ )
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

/*============================================================================*/

void EUSART_Initialize(void)
{
    // Set the EUSART module to the options selected in the user interface.

    // ABDOVF no_overflow; SCKP Non-Inverted; BRG16 16bit_generator; WUE disabled; ABDEN disabled; 
    BAUD1CON = 0x08;

    // SPEN enabled; RX9 8-bit; CREN disabled; ADDEN disabled; SREN disabled; 
    RC1STA = 0x80;

    // TX9 8-bit; TX9D 0; SENDB sync_break_complete; TXEN enabled; SYNC asynchronous; BRGH hi_speed; CSRC slave; 
    TX1STA = 0x24;
    SP1BRGL = 0x05; // Baud Rate = 2400; SP1BRGL 130;
    SP1BRGH = 0x0D; // Baud Rate = 2400; SP1BRGH 6; 
} 

void setup_cpu ( void )
{
   // set up oscillator control register
   OSCCONbits.SPLLEN =1;        // PLL is enabled
   OSCCONbits.IRCF   =0xE;      // select 32MHz
   OSCCONbits.SCS    =0x0;      // with SCS set to zero, clock source is set by FOSC

   LEDR    = 1;
   LEDG    = 1;
   LEDB    = 1;

   // PORT assignments
   TRISA = 0xAB;    
   TRISB = 0x00;    
   TRISC = 0x90;    // make lower 4 bits outputs   
   ANSELA = 0x3B;   // only RA0-RA4 are analog input; ra2 = dig output
   ANSELB = 0x00;   // all digital signals
   ANSELC = 0x00;   // all digital signals
   
   RC0PPS = 0x04;   // assign CLC1OUT to RC0
   RC1PPS = 0x04;   // assign CLC1OUT to RC0
   
   RC5PPS = 0x04;   // assign CLC1OUT to RC5
   //RC6PPS = 0x04;   // assign CLC1OUT to RC6
   
   RC6PPS = 0x14;   // assign TX      to RC6
   
   //RB6PPS = 0x14;   // this will be transmission to LEFT EYE
   RB6PPS = 0x00;
   RB7PPS = 0x14;   // this will be transmission to RIGHT EYE
   

   
	CLC1GLS0  = 0x0A;
	CLC1GLS1  = 0x0A;
	CLC1GLS2  = 0xA0;
	CLC1GLS3  = 0xA0;
	CLC1SEL0  = 0x00;
	CLC1SEL1  = 0x01;
	CLC1SEL2  = 0x02;
	CLC1SEL3  = 0x03;
	CLC1POL   = 0x00;
	CLC1CON   = 0x80;
	CLCIN0PPS = 0x06;
	CLCIN1PPS = 0x06;
	CLCIN2PPS = 0x16;
	CLCIN3PPS = 0x16;

    
   // Initials Timer 0 settings
   OPTION_REGbits.PS     = 1 ;   // pre-scaler not used for Timer0
   OPTION_REGbits.PSA    = 0 ;   // pre-scaler is assigned to Timer0
   OPTION_REGbits.TMR0CS = 0 ;
   INTCONbits.TMR0IE     = 1 ;     // Enable Timer0 interrupt

   // Initialize Timer 2 ... General timing timer. @8MHz
   PR2      = 59;   // 59=56.30kHz, 60=55.55, 50=64.43kHz, 70=48.73kHz, 93=38kHz
   TMR2     = 0x0;
   TMR2IF   = 0;
   T2CON    = 0x04; // Timer2 on
   TMR2IE   = 1;


   EUSART_Initialize();

   INTCONbits.PEIE   = 1;
   INTCONbits.GIE    = 1;  //Enable all configured interrupts

}

void UpdateRGBled ()
{
    if ((MsgBuffer[8] & 0x04) == 0x04)
    {
       LEDR = 0;
    }
    else
    {
       LEDR = 1;
    }
    
    if ((MsgBuffer[8] & 0x02) == 0x02)
    {
       LEDG = 0;
    }
    else
    {
       LEDG = 1;
    }

    if ((MsgBuffer[8] & 0x01) == 0x01)
    {
       LEDB = 0;
    }
    else
    {
       LEDB = 1;
    }    
}

/*============================================================================*/

int main ( )
{
   unsigned long idle_timer      = 0 ;

   setup_cpu();

   while (1)
      {
      CLRWDT();

      if (tick == 1)
      {
         tick = 0;

         // update timing parameters
         if (timercnt >= 1000)
         {
            timercnt = 0;
            fiftymillisecs++;
            txSlot++;
            txEvent = 1;
            
             if (txSlot == 5)
             {
               debugpin = 1;
               txSlot = 0;
               }
             else
            {
                 debugpin = 0;

            }
         }

         // now do next task
         if (txEvent == 1)
         {
            txEvent = 0;
            if (txSlot == 0)
            {
            sio_putstring ("1234");
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


/*











         fiftymillisecs++;
          txSlot++;
         tick = 0;
EYE1 = ~EYE1;




         if (txEvent == 1)
         {
            txEvent = 0;
            if (txSlot == 0)
            {
            sio_putstring (messageSync);
            }
            else
            {
            sio_putstring (messageID);
            }
         }

      }




      CLRWDT();

      }
   return 0;
}




*/

