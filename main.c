/* ============================================================================
 * Smart Weather Station ? Final Arduino Serial Plotter Edition
 * PIC16F877A @ 20 MHz, XC8
 * ============================================================================
 */

#pragma config FOSC  = HS
#pragma config WDTE  = OFF
#pragma config PWRTE = ON
#pragma config BOREN = ON
#pragma config LVP   = OFF
#pragma config CPD   = OFF
#pragma config WRT   = OFF
#pragma config CP    = OFF

#include <xc.h>

#define _XTAL_FREQ      20000000UL
#define LCD_I2C_ADDR    0x4E

#define LCD_RS          0x01
#define LCD_RW          0x02
#define LCD_E           0x04
#define LCD_BL          0x08

/* --- Starting Time Configuration (For LCD only now) --- */
#define START_HOUR      14    
#define START_MINUTE    30    
#define START_SECOND    0     

#define START_DAY       17    
#define START_MONTH     6     
#define START_YEAR      26    

/* --- Hardware Pin Configuration --- */
#define DHT22_DATA      PORTDbits.RD0
#define DHT22_TRIS      TRISDbits.TRISD0

#define ALERT_LED       PORTBbits.RB0
#define ALERT_TRIS      TRISBbits.TRISB0

#define TEMP_ALERT_X10  230         // LED glows exactly at 23.0°C

// Software clock tracking variables
static unsigned char rtc_sec  = START_SECOND;
static unsigned char rtc_min  = START_MINUTE;
static unsigned char rtc_hr   = START_HOUR;
static unsigned char rtc_date = START_DAY;
static unsigned char rtc_mon  = START_MONTH;
static unsigned char rtc_yr   = START_YEAR;

volatile unsigned char timer1_ticks = 0;
volatile unsigned char take_sample_flag = 0;

void __interrupt() ISR(void) {
    if (PIR1bits.TMR1IF) {
        TMR1H = 0x0B; TMR1L = 0xDC; 
        PIR1bits.TMR1IF = 0;
        timer1_ticks++;
        if (timer1_ticks >= 10) {   // 1.0 Second Interval
            timer1_ticks = 0;
            take_sample_flag = 1;
        }
    }
}

static void I2C_Init(void) {
    TRISC3 = 1; TRISC4 = 1;
    SSPSTAT = 0x80;
    SSPCON  = 0x28;
    SSPCON2 = 0x00;
    SSPADD  = 49;
}

static void I2C_Wait(void) { 
    unsigned int t = 1000;
    while ((SSPCON2 & 0x1F) || (SSPSTAT & 0x04)) {
        if (--t == 0) break;
    }
}

static void I2C_Start(void) { I2C_Wait(); SEN = 1; while (SEN) { } }
static void I2C_Stop(void) { I2C_Wait(); PEN = 1; while (PEN) { } }

static void I2C_Write(unsigned char b) {
    unsigned int timeout = 0;
    I2C_Wait(); SSPIF = 0; SSPBUF = b;
    while (!SSPIF && ++timeout < 10000) { }
}

static void LCD_I2C_SendNibble(unsigned char nibble, unsigned char rs) {
    unsigned char val = (nibble & 0xF0) | LCD_BL | (rs ? LCD_RS : 0);
    I2C_Start();
    I2C_Write(LCD_I2C_ADDR);
    I2C_Write(val | LCD_E);  __delay_us(50);
    I2C_Write(val & ~LCD_E); __delay_us(50);
    I2C_Stop();
    __delay_us(100);
}

static void LCD_I2C_SendByte(unsigned char b, unsigned char rs) {
    unsigned char high = b & 0xF0;
    unsigned char low  = (b << 4) & 0xF0;
    unsigned char base = LCD_BL | (rs ? LCD_RS : 0);
    I2C_Start();
    I2C_Write(LCD_I2C_ADDR);
    I2C_Write(high | base | LCD_E);  __delay_us(50);
    I2C_Write(high | base);            __delay_us(50);
    I2C_Write(low  | base | LCD_E);  __delay_us(50);
    I2C_Write(low  | base);            __delay_us(50);
    I2C_Stop();
    __delay_us(200);
}

static void LCD_Cmd(unsigned char c)  { LCD_I2C_SendByte(c, 0); __delay_ms(5); }
static void LCD_Data(unsigned char d) { LCD_I2C_SendByte(d, 1); }

static void LCD_Init(void) {
    __delay_ms(100);
    LCD_I2C_SendNibble(0x30, 0); __delay_ms(10);
    LCD_I2C_SendNibble(0x30, 0); __delay_ms(10);
    LCD_I2C_SendNibble(0x30, 0); __delay_ms(10);
    LCD_I2C_SendNibble(0x20, 0); __delay_ms(10);
    LCD_Cmd(0x28);
    LCD_Cmd(0x08);
    LCD_Cmd(0x01);
    __delay_ms(5);
    LCD_Cmd(0x06);
    LCD_Cmd(0x0C);
    __delay_ms(5);
}

static void LCD_Clear(void) { LCD_Cmd(0x01); __delay_ms(5); }
static void LCD_Goto(unsigned char row, unsigned char col) {
    LCD_Cmd((unsigned char)(((row == 0) ? 0x80 : 0xC0) + col));
}

static void LCD_Print(const char *s) { while (*s) LCD_Data((unsigned char)*s++); }

static void LCD_Num2(unsigned char n) {
    LCD_Data((unsigned char)((n / 10) + '0'));
    LCD_Data((unsigned char)((n % 10) + '0'));
}

static void LCD_Temp(signed int temp_x10) {
    if (temp_x10 < 0) {
        LCD_Data('-');
        temp_x10 = -temp_x10;
    }
    LCD_Num2((unsigned char)(temp_x10 / 10));
    LCD_Data('.');
    LCD_Data((unsigned char)((temp_x10 % 10) + '0'));
}

static unsigned char DHT22_Read(signed int *temp_x10, unsigned int *hum_x10) {
    unsigned char data[5] = {0};
    unsigned char i, j;
    unsigned int timeout;

    DHT22_TRIS = 0; DHT22_DATA = 0; __delay_ms(18);
    DHT22_DATA = 1; __delay_us(30); DHT22_TRIS = 1; __delay_us(10);

    timeout = 1000; while (DHT22_DATA && --timeout); if (!timeout) return 1;
    timeout = 1000; while (!DHT22_DATA && --timeout); if (!timeout) return 1;
    timeout = 1000; while (DHT22_DATA && --timeout); if (!timeout) return 1;

    for (i = 0; i < 5; i++) {
        data[i] = 0;
        for (j = 0; j < 8; j++) {
            timeout = 1000; while (!DHT22_DATA && --timeout); if (!timeout) return 1;
            __delay_us(35);
            if (DHT22_DATA) {
                data[i] |= (1 << (7 - j));
                timeout = 1000; while (DHT22_DATA && --timeout); if (!timeout) return 1;
            }
        }
    }
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) return 2;
    *hum_x10 = ((unsigned int)data[0] << 8) | data[1];
    signed int t = ((unsigned int)(data[2] & 0x7F) << 8) | data[3];
    if (data[2] & 0x80) t = -t; *temp_x10 = t;
    return 0;
}

static void ADC_Init(void) {
    TRISAbits.TRISA1 = 1; TRISAbits.TRISA2 = 1;
    ADCON1 = 0b10000000; ADCON0 = 0b01000001; __delay_us(50);
}

static unsigned int ADC_Read(unsigned char ch) {
    ADCON0 = (unsigned char)((ADCON0 & 0b11000111) | ((ch & 0x07) << 3));
    __delay_us(50); ADCON0bits.GO_DONE = 1; while (ADCON0bits.GO_DONE) { }
    return (unsigned int)(((unsigned int)ADRESH << 8) | ADRESL);
}

static unsigned int ADC_Read_Averaged(unsigned char ch) {
    unsigned long sum = 0;
    for (unsigned char i = 0; i < 8; i++) {
        sum += ADC_Read(ch); __delay_us(100);
    }
    return (unsigned int)(sum / 8);
}

static void UART_Init(void) {
    TRISCbits.TRISC6 = 0; // TX output
    TRISCbits.TRISC7 = 1; // RX input
    SPBRG = 129; 
    TXSTA = 0b00100100; 
    RCSTA = 0b10010000; 
}

static void UART_Write(char c) { 
    while (!TXIF) { } 
    TXREG = (unsigned char)c; 
}

static void UART_Print(const char *s) { 
    while (*s) UART_Write(*s++); 
}

static void UART_Num2(unsigned char n) { 
    UART_Write((char)((n / 10) + '0')); 
    UART_Write((char)((n % 10) + '0')); 
}

/* ============================================================================
 * MAIN FIRMWARE PROGRAM LOOP                 
 * ============================================================================
 */
void main(void) {
    unsigned int lm35_raw, ldr_raw, lm35_x10, ldr_pct, dht_hum_x10;
    unsigned char hum_pct, alert;
    signed int dht_temp_x10;

    OPTION_REG = 0xFF; INTCON = 0x00; PIE1 = 0x00; PIR1 = 0x00;
    TRISE = 0x00; TRISD = 0x00; PORTD = 0x00;
    TRISBbits.TRISB0 = 0; ALERT_LED = 0;

    I2C_Init();
    LCD_Init();
    ADC_Init();
    UART_Init(); 

    T1CON = 0x31; TMR1H = 0x0B; TMR1L = 0xDC;
    PIE1bits.TMR1IE = 1; INTCONbits.PEIE = 1; INTCONbits.GIE  = 1;

    LCD_Goto(0,0); LCD_Print("Smart Weather   ");
    LCD_Goto(1,0); LCD_Print("System Online   ");
    __delay_ms(1000); 
    LCD_Clear();

    // ?? ????? ???? ???????? ???? ???? ??? ???? ??????? ???? ?????? ?? ??? ???

    while (1) {
        if (take_sample_flag) {
            take_sample_flag = 0;

            // 1. Clock Internal Operations
            rtc_sec += 1; 
            if (rtc_sec >= 60) { rtc_sec = 0; rtc_min++; }
            if (rtc_min >= 60) { rtc_min = 0; rtc_hr++; }
            if (rtc_hr  >= 24) { rtc_hr = 0; rtc_date++; } 
            if (rtc_date > 30) { rtc_date = 1; rtc_mon++; }

            // 2. Fetch Sensor States
            lm35_raw = ADC_Read_Averaged(1);
            lm35_x10 = (unsigned int)(((unsigned long)lm35_raw * 5000UL) / 1024UL);

            ldr_raw  = ADC_Read(2);
            ldr_pct  = (unsigned int)(((unsigned long)ldr_raw  * 100UL) / 1023UL);
            if (ldr_pct > 100) ldr_pct = 100;

            GIE = 0;
            unsigned char dht_err = DHT22_Read(&dht_temp_x10, &dht_hum_x10);
            GIE = 1;

            if (dht_err) hum_pct = 0;
            else {
                hum_pct = (unsigned char)(dht_hum_x10 / 10);
                if (hum_pct > 100) hum_pct = 100;
            }

            // Temperature conditional switch evaluated strictly against 23.0C
            if (lm35_x10 > TEMP_ALERT_X10) { alert = 1; ALERT_LED = 1; } 
            else { alert = 0; ALERT_LED = 0; }

            // 3. Render Output on local LCD (????? ???????? ?????? ??? ???? ????)
            LCD_Goto(0, 0);
            LCD_Num2(rtc_hr);  LCD_Data(':'); 
            LCD_Num2(rtc_min); LCD_Data(':'); 
            LCD_Num2(rtc_sec); LCD_Data(' ');
            LCD_Data('T'); LCD_Data(':'); 
            LCD_Temp((signed int)lm35_x10); 
            LCD_Data(0xDF); LCD_Data('C');

            LCD_Goto(1, 0);
            LCD_Data('H'); LCD_Data(':'); 
            LCD_Num2(hum_pct); LCD_Data('%'); LCD_Data(' ');
            LCD_Data('L'); LCD_Data(':'); 
            LCD_Num2((unsigned char)ldr_pct); LCD_Data('%'); LCD_Data(' ');
            LCD_Data('A'); LCD_Data(':'); 
            LCD_Data((unsigned char)(alert + '0'));

            // 4. Stream output formatted strictly for Arduino Serial Plotter
            
            // Temperature
            UART_Print("Temp:");
            UART_Num2((unsigned char)(lm35_x10 / 10));
            UART_Write('.');
            UART_Write((char)((lm35_x10 % 10) + '0'));
            UART_Write(',');
            
            // Humidity
            UART_Print("Humidity:");
            UART_Num2(hum_pct); 
            UART_Write(',');
            
            // Light
            UART_Print("Light:");
            UART_Num2((unsigned char)ldr_pct); 
            UART_Print("\r\n"); // End of Plotter frame
        }
    }
}
