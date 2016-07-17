
// Device: PIC16LF1829
// Using xc8 v1.30
//
//  Timer 0  -> 
//  Timer 1  -> Use this for Main IDLE timing ( get_us ).
//  Timer 2  -> 
//
//  1.00aA  FG, 19 Aug 15, created
//  1.00aB  FG, 11 Jul 16, started adding support for UART RX
//
//***********************************************************************************
#include <pic.h>
#include <string.h>


#pragma config FOSC=INTOSC
#pragma config WDTE=OFF
#pragma config PLLEN=OFF
#pragma config BOREN=0x00
#pragma config CLKOUTEN=1

#define COL1                   LATCbits.LATC4  // led column1, active high
#define COL2                   LATCbits.LATC7  // led column2
#define COL3                   LATBbits.LATB7  // led column3
#define COL4                   LATBbits.LATB6  // led column4
#define COL5                   LATBbits.LATB5  // led column5
#define COL6                   LATBbits.LATB4  // led column6

#define ROW1                   LATCbits.LATC6  // led row1, active low
#define ROW2                   LATCbits.LATC3  // led row2
#define ROW3                   LATCbits.LATC2  // led row3
#define ROW4                   LATCbits.LATC1  // led row4
#define ROW5                   LATCbits.LATC0  // led row5
#define ROW6                   LATAbits.LATA2  // led row6

#define EYE_ID                 PORTAbits.RA4    // input, eye identifier, left or right
#define EYE_RX                 PORTCbits.RC5    // input, eye rx


#define TEST                   LATAbits.LATA5   // output, debug

static char symbol[6] = { 0x1e, 0x21, 0x27, 0x27, 0x21, 0x1e };

const char image1[6] = { 0x1e, 0x21, 0x2d, 0x2d, 0x21, 0x1e };
const char image2[6] = { 0x1e, 0x21, 0x39, 0x39, 0x21, 0x1e };
const char image3[6] = { 0x1e, 0x21, 0x27, 0x27, 0x21, 0x1e };
const char image4[6] = { 0x1e, 0x21, 0x00, 0x00, 0x21, 0x1e };

unsigned long  fiftymillisecs = 0 ; // IRQ Internal 50 millisecond counter.
unsigned long  swtime         = 0 ;
unsigned char  font_selection = 0;
unsigned int   timercnt = 0;
unsigned char  txSlot   = 0;
unsigned char  txEvent  = 0;
unsigned char  tick     = 0;
unsigned char  RxCmd    = 0;    // UART received character/command


void interrupt OnlyOne_ISR(void)    // There is only one interrupt on this CPU
{
   /*============================================================================*/
   // Timer IRQ
   if (TMR1IE && TMR1IF)
      {
      TMR1   = 0xF858 ; //FG 0xF858
      TMR1IF = 0;
		tick   = 1;
      timercnt++;
      }

   /*============================================================================*/
   if (TMR0IE && TMR0IF)          // check if Timer0 interrupt has occurred
      {
		TMR0IF = 0;
		TMR0   = 65;
      }

   /*============================================================================*/
   if (RCIE && RCIF)          // UART RX interrupt has occurred
      {
		RCIF  = 0;
		RxCmd = RCREG;
        TEST   = ~TEST;
      }
   /*============================================================================*/

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

/*============================================================================*/
void setup_cpu ( void )
{
   // set up oscillator control register
   OSCCONbits.SPLLEN =1;      // PLL is enabled
   OSCCONbits.IRCF   =0xf ;   //set OSCCON IRCF bits to select OSC frequency=4Mhz
   OSCCONbits.SCS    =0x03;    //set the SCS bits to select .......

   ANSELA = 0x00;          // disable analog functions

   // port output direction assignments
   TRISAbits.TRISA2 = 0;   // 
   // for testing purposes only TRISAbits.TRISA4 = 0;   // CLKOUT
   TRISAbits.TRISA5 = 0;   // TEST
                           //
   TRISBbits.TRISB4 = 0;   // 
   TRISBbits.TRISB5 = 0;   // 
   TRISBbits.TRISB6 = 0;   // 
   TRISBbits.TRISB7 = 0;   // 
                           //
   TRISCbits.TRISC0 = 0;   // 
   TRISCbits.TRISC1 = 0;   // 
   TRISCbits.TRISC2 = 0;   // 
   TRISCbits.TRISC3 = 0;   // 
   TRISCbits.TRISC4 = 0;   // 
   TRISCbits.TRISC6 = 0;   // 
   TRISCbits.TRISC7 = 0;   // 

   // Initials Timer 0 settings
   OPTION_REGbits.PS     = 0 ;	// pre-scaler is assigned to Timer0
   OPTION_REGbits.PSA    = 0 ;	// pre-scaler is set to 2
   OPTION_REGbits.TMR0CS = 0 ;
   INTCONbits.TMR0IE     = 0 ;   // Disable Timer0 interrupt

   // Initalize Timer 1 ... General timing timer. @8MHz / 8 = 1Mhz
   TMR1     = 0x1000;
   TMR1IF   = 0;
   T1CONbits.T1CKPS = 0;   //FG###
   T1CONbits.TMR1CS = 0x00;
   T1CONbits.TMR1ON = 1;
   TMR1IE   = 1;

    // Setup UART for receiving command inputs
   SPEN  = 1;       // enable the serial port
   SYNC  = 0;       // enable asynchronous mode
   CREN  = 1;       // enable receiver
   BRG16 = 1;
   BRGH  = 1;
   SPBRGH = 0x06;
   SPBRGL = 0x82;   // 1666 = 2400 baud
   APFCON0 = 0x84;  // Alternate Pin Configuration: set TX on pin RC4, RX on pin RC5
   TXEN    = 0;     // Set to 1 if need the TX for debug purposes
   RCIF    = 0;     // clear RX interrupt flag
   RCIE    = 1;     // enable RX interrupt
   
   INTCONbits.PEIE   = 1;   // enable peripheral interrupts
   INTCONbits.GIE    = 1;   // enable all configured interrupts

   ROW1 = 1; COL1 = 0;
   ROW2 = 1; COL2 = 0;
   ROW3 = 1; COL3 = 0;
   ROW4 = 1; COL4 = 0;
   ROW5 = 1; COL5 = 0;
   ROW6 = 1; COL6 = 0;

}



/*============================================================================*/
int main ( )
{
   unsigned long idle_timer      = 0 ;
   unsigned char ridx, cidx, i = 0;

   setup_cpu();
   ridx = 0;
   cidx = 0;

   while (1)
      {
      CLRWDT();

		if (tick == 1)
		{
			tick = 0;
         if (cidx == 5)       // column index
         {

            //TEST   = ~TEST;

            cidx = 0;
            if (ridx == 5)    // row index
            {
               ridx = 0;
            }
            else
            {
               ridx++;
            }
         }
         else
         {
            cidx++;
         }

         if (timercnt == 200)
         {
            timercnt = 0;
            font_selection++;
            if (font_selection == 4)
            {
               font_selection = 0;
            }

            switch (font_selection)
            {
            case 0: 
               for (i = 0; i < 6; i++)
               {
                  symbol[i] = image2[i];
               }
               break;
            case 1: 
               for (i = 0; i < 6; i++)
               {
                  symbol[i] = image1[i];
               }
               break;
            case 2: 
               for (i = 0; i < 6; i++)
               {
                  symbol[i] = image3[i];
               }
               break;
            case 3: 
               for (i = 0; i < 6; i++)
               {
                  symbol[i] = image1[i];
               }
               break;
            }
         }



         switch (ridx)
         {
         default : 
            ROW1 = 1; COL1 = 0;
            ROW2 = 1; COL2 = 0;
            ROW3 = 1; COL3 = 0;
            ROW4 = 1; COL4 = 0;
            ROW5 = 1; COL5 = 0;
            ROW6 = 1; COL6 = 0;
            break;

         case 0 :
            ROW1 = 0; COL1 = 0; 
            ROW2 = 1; COL2 = 0; 
            ROW3 = 1; COL3 = 0; 
            ROW4 = 1; COL4 = 0; 
            ROW5 = 1; COL5 = 0; 
            ROW6 = 1; COL6 = 0; 
            
            switch (cidx)
            {
            case 0: if ( (symbol[0] & 0x20) > 0) COL1 = 1; else COL1 = 0;
               break;
            case 1: if ( (symbol[0] & 0x10) > 0) COL2 = 1; else COL2 = 0;
               break;
            case 2: if ( (symbol[0] & 0x08) > 0) COL3 = 1; else COL3 = 0;
               break;
            case 3: if ( (symbol[0] & 0x04) > 0) COL4 = 1; else COL4 = 0;
               break;
            case 4: if ( (symbol[0] & 0x02) > 0) COL5 = 1; else COL5 = 0;
               break;
            case 5: if ( (symbol[0] & 0x01) > 0) COL6 = 1; else COL6 = 0;
               break;
            }
				break;

		   case 1 :
            ROW1 = 1; COL1 = 0;
            ROW2 = 0; COL2 = 0;
            ROW3 = 1; COL3 = 0;
            ROW4 = 1; COL4 = 0;
            ROW5 = 1; COL5 = 0;
            ROW6 = 1; COL6 = 0;
            switch (cidx)
            {
            case 0: if ( (symbol[1] & 0x20) > 0) COL1 = 1; else COL1 = 0;
               break;
            case 1: if ( (symbol[1] & 0x10) > 0) COL2 = 1; else COL2 = 0;
               break;
            case 2: if ( (symbol[1] & 0x08) > 0) COL3 = 1; else COL3 = 0;
               break;
            case 3: if ( (symbol[1] & 0x04) > 0) COL4 = 1; else COL4 = 0;
               break;
            case 4: if ( (symbol[1] & 0x02) > 0) COL5 = 1; else COL5 = 0;
               break;
            case 5: if ( (symbol[1] & 0x01) > 0) COL6 = 1; else COL6 = 0;
               break;
            }

				break;

         case 2 :
            ROW1 = 1; COL1 = 0;
            ROW2 = 1; COL2 = 0;
            ROW3 = 0; COL3 = 0;
            ROW4 = 1; COL4 = 0;
            ROW5 = 1; COL5 = 0;
            ROW6 = 1; COL6 = 0;

            switch (cidx)
            {
            case 0: if ( (symbol[2] & 0x20) > 0) COL1 = 1; else COL1 = 0;
               break;
            case 1: if ( (symbol[2] & 0x10) > 0) COL2 = 1; else COL2 = 0;
               break;
            case 2: if ( (symbol[2] & 0x08) > 0) COL3 = 1; else COL3 = 0;
               break;
            case 3: if ( (symbol[2] & 0x04) > 0) COL4 = 1; else COL4 = 0;
               break;
            case 4: if ( (symbol[2] & 0x02) > 0) COL5 = 1; else COL5 = 0;
               break;
            case 5: if ( (symbol[2] & 0x01) > 0) COL6 = 1; else COL6 = 0;
               break;
            }

				break;

         case 3 :
            ROW1 = 1; COL1 = 0;
            ROW2 = 1; COL2 = 0;
            ROW3 = 1; COL3 = 0;
            ROW4 = 0; COL4 = 0;
            ROW5 = 1; COL5 = 0;
            ROW6 = 1; COL6 = 0;

            switch (cidx)
            {
            case 0: if ( (symbol[3] & 0x20) > 0) COL1 = 1; else COL1 = 0;
               break;
            case 1: if ( (symbol[3] & 0x10) > 0) COL2 = 1; else COL2 = 0;
               break;
            case 2: if ( (symbol[3] & 0x08) > 0) COL3 = 1; else COL3 = 0;
               break;
            case 3: if ( (symbol[3] & 0x04) > 0) COL4 = 1; else COL4 = 0;
               break;
            case 4: if ( (symbol[3] & 0x02) > 0) COL5 = 1; else COL5 = 0;
               break;
            case 5: if ( (symbol[3] & 0x01) > 0) COL6 = 1; else COL6 = 0;
               break;
            }

            break;

         case 4 :
            ROW1 = 1; COL1 = 0;
            ROW2 = 1; COL2 = 0;
            ROW3 = 1; COL3 = 0;
            ROW4 = 1; COL4 = 0;
            ROW5 = 0; COL5 = 0;
            ROW6 = 1; COL6 = 0;

            switch (cidx)
            {
            case 0: if ( (symbol[4] & 0x20) > 0) COL1 = 1; else COL1 = 0;
               break;
            case 1: if ( (symbol[4] & 0x10) > 0) COL2 = 1; else COL2 = 0;
               break;
            case 2: if ( (symbol[4] & 0x08) > 0) COL3 = 1; else COL3 = 0;
               break;
            case 3: if ( (symbol[4] & 0x04) > 0) COL4 = 1; else COL4 = 0;
               break;
            case 4: if ( (symbol[4] & 0x02) > 0) COL5 = 1; else COL5 = 0;
               break;
            case 5: if ( (symbol[4] & 0x01) > 0) COL6 = 1; else COL6 = 0;
               break;
            }

            break;

         case 5 :
            ROW1 = 1; COL1 = 0;
            ROW2 = 1; COL2 = 0;
            ROW3 = 1; COL3 = 0;
            ROW4 = 1; COL4 = 0;
            ROW5 = 1; COL5 = 0;
            ROW6 = 0; COL6 = 0;

            switch (cidx)
            {
            case 0: if ( (symbol[5] & 0x20) > 0) COL1 = 1; else COL1 = 0;
               break;
            case 1: if ( (symbol[5] & 0x10) > 0) COL2 = 1; else COL2 = 0;
               break;
            case 2: if ( (symbol[5] & 0x08) > 0) COL3 = 1; else COL3 = 0;
               break;
            case 3: if ( (symbol[5] & 0x04) > 0) COL4 = 1; else COL4 = 0;
               break;
            case 4: if ( (symbol[5] & 0x02) > 0) COL5 = 1; else COL5 = 0;
               break;
            case 5: if ( (symbol[5] & 0x01) > 0) COL6 = 1; else COL6 = 0;
               break;
            }

            break;

         }  // switch


		}	   // if tick


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

