// Device: PIC16LF1829
// Using xc8 v1.30
//
// The cpu runs at 16MHz
//
//  Timer 0  -> 
//  Timer 1  -> Use this for Main IDLE timing ( get_us ).
//  Timer 2  -> 
//
//  1.00aA  FG, 19 Aug 15, created
//  1.00aB  FG, 11 Jul 16, started adding support for UART RX
//  1.00aC  DC, 15 Jul 16, added several expressions and mirroring
//  1.00aD  DC, 22 Jul 16, added additional expressions, serial commands implemented but untested
//  1.00aE  FG, 02 Oct 16, modified to accept 6 bytes + sync byte to define the eye expression / LED output
//                         baud rate is changed to 9600
//                         need to consider separate buffer for left and right eyes so that display can be different on each eye
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

#define TEST                   LATAbits.LATA0   // output, debug

static char visibleSymbol[6] = { 0x1e, 0x21, 0x27, 0x27, 0x21, 0x1e };
static char tempRev[6];

const char lookCentre[6] = { 0x1e, 0x21, 0x2d, 0x2d, 0x21, 0x1e };
const char lookLeft[6] = { 0x1e, 0x21, 0x27, 0x27, 0x21, 0x1e };
const char lookRight[6] = { 0x1e, 0x21, 0x39, 0x39, 0x21, 0x1e };
const char lookNowhere[6] = { 0x1e, 0x21, 0x21, 0x21, 0x21, 0x1e };
const char lookUp[6] = { 0x1e, 0x2d, 0x2d, 0x21, 0x21, 0x1e };
const char lookDown[6] = { 0x1e, 0x21, 0x21, 0x2d, 0x2d, 0x1e };
const char lookAngryUP[6] = { 0x00, 0x3c, 0x26, 0x27, 0x21, 0x1e };
const char lookAngryDOWN[6] = { 0x00, 0x3c, 0x22, 0x27, 0x27, 0x1e };
const char lookSad[6] = { 0x00, 0xF, 0x17, 0x27, 0x21, 0x1e };
const char lookBored[6] = { 0x0, 0x1e, 0x3f, 0x2d, 0x21, 0x1e };
const char lookSquint[6] = { 0x0, 0x1e, 0xd2, 0x2d, 0x1e, 0x0 };
const char lookThoughtful[6] = { 0x0, 0x1e, 0x21, 0x2d, 0x2d, 0x1e };

unsigned long  fiftymillisecs = 0 ; // IRQ Internal 50 millisecond counter.
unsigned long  swtime         = 0 ;
unsigned char  font_selection = 0;
unsigned int   timercnt = 0;
unsigned char  txSlot   = 0;
unsigned char  txEvent  = 0;
unsigned char  tick     = 0;
unsigned char  RxCmd    = 0;    // UART received character/command
unsigned char  RxByte   = 0;    // received character
unsigned char  RxCount  = 0;    // counts number of bytes received
unsigned char  RxStart  = 0;    // flag to indicate we are receiving data bytes
unsigned char  RxReady  = 0;    // flag to indicate that a complete message received
unsigned char  RxBuffer[7];

int orientation = 1;

/*============================================================================*/
unsigned char Reverse_bits(unsigned char num) 
{
    int i=5; //size of unsigned char -1, on most machine is 8bits
    unsigned char j=0;
    unsigned char temp=0;

    while(i>=0){
    temp |= ((num>>j)&1)<< i;
    i--;
    j++;
    }
    return(temp); 

}

/*============================================================================*/
void interrupt OnlyOne_ISR(void)    // There is only one interrupt on this CPU
{
   // Timer IRQ
   if (TMR1IE && TMR1IF)
      {
      TMR1   = 0xF858 ; //FG 0xF858
      TMR1IF = 0;
      tick   = 1;
      timercnt++;
      }

   if (TMR0IE && TMR0IF)          // check if Timer0 interrupt has occured
      {
      TMR0IF = 0;
      TMR0   = 65;
      }

   if (RCIE && RCIF)          // UART RX interrupt has occurred
      {
      TEST = 1;
      RCIF  = 0;
      RxByte = RCREG;

      TEST = 0;
        if (RxByte == 0 && RxCount == 0)    // we have our sync word
        {
            RxStart = 1;
            RxCount++;
        }
        else if (RxStart == 1)
        {

            RxBuffer[RxCount-1] = RxByte;
            RxCount++;
            if (RxCount == 7)
            {
                RxCount = 0;
                RxStart = 0;
                RxReady = 1;
            }
        }
        
/*
      switch (RxCmd) {
            default :   {for (int i = 0; i < 6; i++)
                        {
                           visibleSymbol[i] = lookCentre[i];
                        }
                        break; }
             case 1 :   {for (int i = 0; i < 6; i++)
                        {
                           visibleSymbol[i] = lookCentre[i];
                        }
                        break; }
             case 2 :   {for (int i = 0; i < 6; i++)
                        {
                           visibleSymbol[i] = lookUp[i];
                        }
                        break; }
             case 3 :   {for (int i = 0; i < 6; i++)
                        {
                           visibleSymbol[i] = lookDown[i];
                        }
                        break; }
             case 4 :   {for (int i = 0; i < 6; i++)
                        {
                           visibleSymbol[i] = lookLeft[i];
                        }
                        break; }
             case 5 :   {for (int i = 0; i < 6; i++)
                        {
                           visibleSymbol[i] = lookRight[i];
                        }
                        break; }
         }
          

      */
      

      }
}

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

//UART transmit function, parameter Ch is the character to send
void sio_putch(char ch)
{
   TXREG = ch;
   while (TXSTAbits.TRMT == 0);   // wait for buffer to empty
}

/*============================================================================*/
void sio_putstring(char *str)
{
   int i, len;

   len =  strlen ( (char *) str ) ;
   for ( i=0; i<len; i++ )
      {
      sio_putch( str[i] );
      //delayus(100);
      }
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
   TRISAbits.TRISA0 = 0;   // make PGD an output for testing
   TRISAbits.TRISA2 = 0;   // 
   TRISAbits.TRISA4 = 1;   // This is EYE_ID or orientation input
   TRISAbits.TRISA5 = 0;   //
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

   if (EYE_ID == 1)         // read external pin assignment
   {
       orientation = 1;     // right eye
   }
   else
   {
       orientation = 0;     // left eye
   }


   
   // Initials Timer 0 settings
   OPTION_REGbits.PS     = 0 ;   // pre-scaler is assigned to Timer0
   OPTION_REGbits.PSA    = 0 ;   // pre-scaler is set to 2
   OPTION_REGbits.TMR0CS = 0 ;
   INTCONbits.TMR0IE     = 0 ;   // Disable Timer0 interrupt

   // Initialize Timer 1 ... General timing timer. @8MHz / 8 = 1Mhz
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
   SPBRGH = 0x01;
   SPBRGL = 0xA0;   // 416 = 9600, 1666 = 2400 baud
   APFCON0 = 0x84;  // Alternate Pin Configuration: set TX on pin RC4, RX on pin RC5
   TXEN    = 0;     // Set to 1 if need the TX for debug purposes
   RCIF    = 0;     // clear RX interrupt flag
   RCIE    = 1;     // enable RX interrupt
   TEST    = 0;
   
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
void columnSwitch(int colInd, int rowInd){
    switch (colInd)
            {
            case 0: if ( (visibleSymbol[rowInd] & 0x20) > 0) COL1 = 1; else COL1 = 0;
               break;
            case 1: if ( (visibleSymbol[rowInd] & 0x10) > 0) COL2 = 1; else COL2 = 0;
               break;
            case 2: if ( (visibleSymbol[rowInd] & 0x08) > 0) COL3 = 1; else COL3 = 0;
               break;
            case 3: if ( (visibleSymbol[rowInd] & 0x04) > 0) COL4 = 1; else COL4 = 0;
               break;
            case 4: if ( (visibleSymbol[rowInd] & 0x02) > 0) COL5 = 1; else COL5 = 0;
               break;
            case 5: if ( (visibleSymbol[rowInd] & 0x01) > 0) COL6 = 1; else COL6 = 0;
               break;
            }
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
              
                // update visible buffer with latest received data
                if (RxReady == 1)
                {
                   RxReady = 0;
                   for (int i = 0; i < 6; i++)
                        {
                           visibleSymbol[i] = RxBuffer[i];
                        }
                   
                    // flush buffer
                    for (int i = 0; i < 6; i++)
                        {
                           RxBuffer[i] = 0;
                        }
                   
                   if(orientation == 1){
                      for (int i = 0; i < 6; i++){
                          tempRev[i] = visibleSymbol[5-i];
                      }
                      for (int i = 0; i < 6; i++)
                      {
                         visibleSymbol[i] = Reverse_bits(tempRev[i]);
                      }
                   }             
                }              
              
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
           
            columnSwitch(cidx, ridx);
            break;

         case 1 :
            ROW1 = 1; COL1 = 0;
            ROW2 = 0; COL2 = 0;
            ROW3 = 1; COL3 = 0;
            ROW4 = 1; COL4 = 0;
            ROW5 = 1; COL5 = 0;
            ROW6 = 1; COL6 = 0;
            
            columnSwitch(cidx, ridx);
            break;

         case 2 :
            ROW1 = 1; COL1 = 0;
            ROW2 = 1; COL2 = 0;
            ROW3 = 0; COL3 = 0;
            ROW4 = 1; COL4 = 0;
            ROW5 = 1; COL5 = 0;
            ROW6 = 1; COL6 = 0;

            columnSwitch(cidx, ridx);
            break;

         case 3 :
            ROW1 = 1; COL1 = 0;
            ROW2 = 1; COL2 = 0;
            ROW3 = 1; COL3 = 0;
            ROW4 = 0; COL4 = 0;
            ROW5 = 1; COL5 = 0;
            ROW6 = 1; COL6 = 0;

            columnSwitch(cidx, ridx);
            break;

         case 4 :
            ROW1 = 1; COL1 = 0;
            ROW2 = 1; COL2 = 0;
            ROW3 = 1; COL3 = 0;
            ROW4 = 1; COL4 = 0;
            ROW5 = 0; COL5 = 0;
            ROW6 = 1; COL6 = 0;

            columnSwitch(cidx, ridx);
            break;

         case 5 :
            ROW1 = 1; COL1 = 0;
            ROW2 = 1; COL2 = 0;
            ROW3 = 1; COL3 = 0;
            ROW4 = 1; COL4 = 0;
            ROW5 = 1; COL5 = 0;
            ROW6 = 0; COL6 = 0;

            columnSwitch(cidx, ridx);
            break;

         }  // switch

      }     // if tick

      }
   return 0;
}