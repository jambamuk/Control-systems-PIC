/* This program configures the PIC32 for communication with a computer at 57600 baud.
 * When the communication link is set up, the program will simply take input text
 * from the user and mock him/her. It demonstrates the basics of UART data Rx/Tx.
 */
 
// Include Header Files
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <p32xxxx.h>                // include chip specific header file
#include <sys/attribs.h>
//#include <plib.h>                   // include peripheral library functions
 
// Configuration Bits
#pragma config FNOSC = FRCPLL       // Internal Fast RC oscillator (8 MHz) w/ PLL
#pragma config FPLLIDIV = DIV_2     // Divide FRC before PLL (now 4 MHz)
#pragma config FPLLMUL = MUL_20     // PLL Multiply (now 80 MHz)
#pragma config FPLLODIV = DIV_2     // Divide After PLL (now 40 MHz)
                                    // see figure 8.1 in datasheet for more info
#pragma config FWDTEN = OFF         // Watchdog Timer Disabled
#pragma config ICESEL = ICS_PGx1    // ICE/ICD Comm Channel Select
#pragma config JTAGEN = OFF         // Disable JTAG
#pragma config FSOSCEN = OFF        // Disable Secondary Oscillator
#pragma config FPBDIV = DIV_1       // PBCLK = SYCLK

 
// Defines
#define SYSCLK 40000000L
#define PWMFREQ     1000
#define DUTYCYCLE   10

#define Baud2BRG(desired_baud)      ( (SYSCLK / (16*desired_baud))-1)
#define TIMERSAMPLESIZE 1000

//Gloabl variables
int    TimerCount;
int    TimerBuffer[TIMERSAMPLESIZE];
int    FirstPress;

// Function Prototypes
void configurePWM();
void changeDutyCycle(int NEW_DUTYCYCLE);
int SerialTransmit(const char *buffer);
unsigned int SerialReceive(char *buffer, unsigned int max_size);
int UART2Configure( int baud);
void configureT1();
void startRPMTimer();
void ConfigureEncoder();

int main(void)
{
 
    char   buf[1024];       // declare receive buffer with max size 1024
     //keeping track of timer value
    TimerCount=0;
    //Start System
    FirstPress=0;
    // Peripheral Pin Select
    U2RXRbits.U2RXR = 4;    //SET RX to RB8
    RPB9Rbits.RPB9R = 2;    //SET RB9 to Tx
    
    
    UART2Configure(57600);  // Configure UART2 for a baud rate of 57600
    U2MODESET = 0x8000;     // enable UART2
    
    configurePWM();
    configureT1();
    ConfigureEncoder();
    int i;
    for(i=0;i<TIMERSAMPLESIZE;i++){
        TimerBuffer[i]=0;
    }
    
    for( i=0 ; i < 100000 ; i++){
        if(i<50){
            SerialTransmit("\r\n");
        }
    }
    
    SerialTransmit("###########EBB 320 Prac 1##############\r\n");
    SerialTransmit("Press Any Key To start PWM\r\n\r\n");
 
    unsigned int rx_size;
 
    while( 1){
        rx_size = SerialReceive(buf, 1024);     
        //SerialTransmit(buf);                    
 
        // if anything was entered by user, be obnoxious and add a '?'
        //if( rx_size > 0){ 
           // SerialTransmit("?\r\n");
        //}
        SerialTransmit("\r\n");
 
    }//END while( 1)
 
    return 1;
} // END main()
 
/* UART2Configure() sets up the UART2 for the most standard and minimal operation
 *  Enable TX and RX lines, 8 data bits, no parity, 1 stop bit, idle when HIGH
 *
 * Input: Desired Baud Rate
 * Output: Actual Baud Rate from baud control register U2BRG after assignment*/
int UART2Configure( int desired_baud){
 
    U2MODE = 0;         // disable autobaud, TX and RX enabled only, 8N1, idle=HIGH
    U2STA = 0x1400;     // enable TX and RX
    U2BRG = Baud2BRG(desired_baud); // U2BRG = (FPb / (16*baud)) - 1
 
    // Calculate actual assigned baud rate
    int actual_baud = SYSCLK / (16 * (U2BRG+1));
 
    return actual_baud;
} // END UART2Configure()
 
/* SerialTransmit() transmits a string to the UART2 TX pin MSB first
 *
 * Inputs: *buffer = string to transmit */
int SerialTransmit(const char *buffer)
{
    unsigned int size = strlen(buffer);
    while( size)
    {
        while(U2STAbits.UTXBF);    // wait while TX buffer full
        U2TXREG = *buffer;          // send single character to transmit buffer
        //IFS1bits.U2RXIF=0;
        buffer++;                   // transmit next character on following loop
        size--;                     // loop until all characters sent (when size = 0)
    }
 
    while( !U2STAbits.TRMT);        // wait for last transmission to finish
 
    return 0;
}
 
/* SerialReceive() is a blocking function that waits for data on
//// *  the UART2 RX buffer and then stores all incoming data into *buffer
//// *
 * Note that when a carriage return '\r' is received, a nul character
 *  is appended signifying the strings end
 *
 * Inputs:  *buffer = Character array/pointer to store received data into
 *          max_size = number of bytes allocated to this pointer
 * Outputs: Number of characters received */
unsigned int SerialReceive(char *buffer, unsigned int max_size)
{
    unsigned int num_char = 0;
    char temp[100];
    int RPM=0;
    
    while(num_char < max_size)
    {
        if(!FirstPress){
            while(!U2STAbits.URXDA){
            }
            
            //Turn PWM On
            OC1CONbits.ON=1;
            T2CONbits.ON=1;
            T4CONSET = 0x8000; //Start 32-bit timer
            
            
            //Start collection RPM Data
            T2CONbits.ON;
            T2CONSET = 0x8000;
            
            
            //System has started
            FirstPress=1;
        }
        
        int EdgeCounter=0;
        int state=0;
        /*We wont be pressing anything until we done sampling*/
        while(!U2STAbits.URXDA){
            //Sample values & Print
            // wait until data available in RX buffer        
                ConfigureEncoder();
                startRPMTimer();  
                while(1){
            //        PORTBbits.RB15=1;

                    
                    if(PORTBbits.RB15==1 && state==0){
                        EdgeCounter++;              
                        state=1;
                    }else{
                        state=0;
                    }
                    
                    if(EdgeCounter >= 8){
                        EdgeCounter=0;
                        break;
                    }        
                }
            RPM=TMR2*(pow(2.5,-8));
            
            RPM=(RPM)/60;
           // RPM=(1/RPM)*60;

            if(TimerCount==0){
                TimerBuffer[TimerCount]=TMR4;
                if(RPM!=0){
                sprintf(temp,"%.4f;\t%d\r\n",TimerBuffer[TimerCount],RPM);
                }
                
            }else{
                if(TMR4<0xFFFFFFFF){
                    TimerBuffer[TimerCount]=TMR4;
                }else{
                    TimerBuffer[TimerCount]=TMR5;
                }
                if(RPM!=0){
                sprintf(temp,"%.4f;\t%d\r\n",abs(TimerBuffer[TimerCount]-TimerBuffer[TimerCount-1])*pow(6.4,-6),RPM);
                }
            }
            TimerCount++;
           
            if(TimerCount==500){
                changeDutyCycle(70);
            }
            SerialTransmit(temp);
                            
        }        
        
        *buffer = U2RXREG;          // empty contents of RX buffer into *buffer pointer        
        // insert nul character to indicate end of string
        if(*buffer == 'q'){
            
              //sprintf(temp,"%f.4\t%d\r\n",TimerBuffer[TimerCount],);
            //changeDutyCycle(0.1*3.25+0.2*(5-0.08));
        }else if(*buffer == 'w'){
            //Turn on pwm @ 20%
            
            changeDutyCycle(10);
        }else if(*buffer == 'e'){
            //Turn pwm of
            changeDutyCycle(30);
        }
        else if (*buffer == 'r'){
            changeDutyCycle(50);
        }
        else if(*buffer == 't'){
              changeDutyCycle(75);
        }
        else if(*buffer == 'y'){
            changeDutyCycle(100);
        }
        
        if( *buffer == '\r'){
            *buffer = '\0';     
            break;
        }
 
        buffer++;
        num_char++;
        
    }
 
    return num_char;
} // END SerialReceive()


void configurePWM(){
   //Set RB7 as PWM output
   RPB7Rbits.RPB7R = 0x0005;
    /*
    1. Calculate the PWM period.
 */
   
    int PWMPERIOD = (1/PWMFREQ);
    
 /*
    2. Calculate the PWM duty cycle.
  */
   
    /*
    3. Determine if the Output Compare module will be
    used in 16 or 32-bit mode based on the previous
    calculations.
  * */
    
    /*
    4. Configure the timer to be used as the time base
    for 16 or 32-bit mode by writing to the T32 bit
    (TxCON<T32>).
         * */
    //T2CONbits.T32 = 0;
    
            /*
    5. Configure the output compare channel for 16 or
    32-bit operation by writing to the OC32 bit
    (OCxCON<5>).*/
    OC1CONbits.OC32=0;
    /*
    6. Set the PWM period by writing to the selected
    Timer Period register (PR).*/
    PR1 =  (SYSCLK/(PWMFREQ))-1;
    
    /*
    7. Set the PWM duty cycle by writing to the OCxRS
    register.
     */
    OC1RS=(PR1 + 1) * ((float)(DUTYCYCLE)/100);
    
    /*
    8. Write the OCxR register with the initial duty
    cycle.
    9. Enable interrupts, if required, for the timer and
    output compare modules. The output compare
    interrupt is required for PWM Fault pin
    utilization.
    10. Configure the output compare module for one of
    two PWM operation modes by writing to the Out-
    put Compare mode bits OCM<2:0>
    (OCxCON<2:0>).*/
    OC1CONbits.OCM=0b110;
    /*11. Set the TMRx prescale value and enable the
    time base by setting ON (TxCON<15>) = 1.
*/
    T1CONbits.TCKPS=0b000;   
}

void changeDutyCycle(int NEW_DUTYCYCLE){
    OC1RS=(PR1 + 1) * ((float)NEW_DUTYCYCLE/100);
}

void configureT1(){
T4CON = 0x0; //Stop Timer4 and clear
T5CON = 0x0; //Stop Timer5 and clear
T4CONSET = 0x00C8; //32-bit mode,
//gate enable,
//internal clock,
//1:16 prescale
T4CONbits.TCKPS=0b111;
T4CONbits.TCS=0;
T4CONbits.TGATE=0;
TMR4 = 0x0; //Clear TMR4 and TMR5
//Same as TMR4 = 0x0
PR4 = 0xFFFFFFFF; //Load PR4 and PR5 regs
//Same as PR4 =0xFFFFFFFF

   
}

void ConfigureEncoder(){
       //setup timer 2 & 3
    T2CON = 0x0; //Stop Timer4 and clear
    T2CON = 0x0; //Stop Timer5 and clear
    T2CONSET = 0x00C8; //32-bit mode,
    //gate enable,
    //internal clock,
    //1:16 prescale
    T2CONbits.TCKPS=0b000;
    T2CONbits.TCS=0;
    T2CONbits.TGATE=0;
    TMR2 = 0x0;
    TMR3= 0x0;//Clear TMR4 and TMR5
    //Same as TMR4 = 0x0
    PR2 = 0xFFFFFFFF; 
    PR3 = 0xFFFFFFFF; //Load PR4 and PR5 regs
    //Same as PR4 =0xFFFFFFFF
    
    TRISBbits.TRISB15=1;
    ANSELBbits.ANSB15=0;        
}

void startRPMTimer(){
    T2CONbits.ON;
    T2CONSET = 0x8000;
}