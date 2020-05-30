
#include <htc.h>
#include <pic.h>

__CONFIG(FCMEN_OFF & IESO_OFF & CLKOUTEN_OFF & BOREN_OFF & CPD_OFF & CP_OFF & MCLRE_OFF & PWRTE_ON & 
			WDTE_OFF & FOSC_INTOSC);
__CONFIG(WRT_OFF & PLLEN_OFF & STVREN_OFF & BORV_19 & LVP_OFF);

//red (channel A)
#define ENC0CLK PORTAbits.RA4

//white (channel B)
#define ENC0DIR PORTAbits.RA3

#define ENC1CLK PORTCbits.RC3
#define ENC1DIR PORTCbits.RC6
#define ENC2CLK PORTCbits.RC7
#define ENC2DIR PORTBbits.RB7
#define ENC3CLK PORTCbits.RC0
#define ENC3DIR PORTCbits.RC1
#define ENC4CLK PORTCbits.RC2
#define ENC4DIR PORTBbits.RB4
#define ENC5CLK PORTBbits.RB5
#define ENC5DIR PORTBbits.RB6
#define ENC3FWD 'R'
#define ENC3REV 'E'
#define ENC4FWD 'F'
#define ENC4REV 'D'
#define ENC5FWD 'V'
#define ENC5REV 'C'
#define ENC0FWD 'W'
#define ENC0REV 'Q'
#define ENC1FWD 'A'
#define ENC1REV 'S'
#define ENC2FWD 'Z'
#define ENC2REV 'X'
#define SW1REL 'T'
#define SW2REL 'G'
//Bluetooth connection 1:connected 0:none
#define BLUETOOTH_CONNECTED PORTAbits.RA2
#define RS232C_CONNECTED PORTCbits.RC5
#define PUSH_SW2 PORTAbits.RA0
#define PUSH_SW1 PORTAbits.RA1
#define POWER_SW PUSH_SW2
#define MODE_BLUETOOTH 1
#define MODE_RS232C 2

unsigned int iTimer1;
unsigned int iPowerSW_Low;
unsigned char ucMode;

void interrupt isr(void){
	if(TMR1IF){
		iTimer1++;
		if (POWER_SW) iPowerSW_Low = 0; else iPowerSW_Low++;
		TMR1IF = 0;
	}
	return;
}

void wait(int icycle) {
	for (int i=0; i<icycle; i++) {}
}

#define NINPUT 8
unsigned char ucEnc[NINPUT][4];
unsigned int uiTick;

void initMCU(void){
	unsigned char uc0, uc1;

	OSCCONbits.SPLLEN = 0;//4xPLL disabled
//	OSCCONbits.SCS = 2;//internal osc
//	OSCCONbits.IRCF = 0xd;//4 MHz
	OSCCONbits.IRCF = 0xe;//8 MHz
	APFCON0bits.RXDTSEL = 1;//RX/DT is on RC5
	APFCON0bits.TXCKSEL = 1;//TX/CK is on RC4

	uiTick = 0;
	for (uc0=0; uc0<NINPUT; uc0++) {
		for (uc1=0; uc1<4; uc1++) {ucEnc[uc0][uc1] = 1;}
	}

//Timer1: 3.8 Hz interrupt when Fosc = 8 MHz
	iTimer1 = 0;
	iPowerSW_Low = 0;
	T1CONbits.TMR1CS = 0;//Fosc/4 clock
	T1CONbits.T1CKPS0 = 1;//prescale 1/8
	T1CONbits.T1CKPS1 = 1;
	T1CONbits.T1OSCEN = 0;//LP osc off
	T1GCONbits.TMR1GE = 0;//Timer1 gate off
	TMR1H = 0;
	TMR1L = 0;
	T1CONbits.TMR1ON = 1;
	TMR1IE=1;	// timer 1 interrupt enabled
	PEIE=1;		// enable peripheral interrupts
	GIE=1;		// turn on interrupts

//Ports
	ANSELA = 0;//port A to be digital
	ANSELB = 0;//port B to be digital
	ANSELC = 0;//port C to be digital
	INLVLC = 0b11011111;//RC5: TTL level input
	OPTION_REGbits.nWPUEN = 0;//enable indiv pull-ups
	WPUA = 0b00011011;//RA4,3,1,0: pull up; RA2 is hard-wired to pull down
	WPUB = 0b11110000;//RB7-4: pull up
	WPUC = 0b11011111;//RC7,6,4-0: pull up

	TRISA = 0b11011111;//RA5 is output
	TRISB = 0b11111111;//RB7-0:input;
	//190315 TRISC = 0b11011111;//RC5/RX for LED driving
	TRISC = 0b11111111;//RC5/RX for RS232C sense

	if (RS232C_CONNECTED) {//RS232C powered boot
		ucMode = MODE_RS232C;
		PORTAbits.RA5 = 1;//RN42 off
	} else {
		ucMode = MODE_BLUETOOTH;
		PORTAbits.RA5 = 0;//RN42 on
	}

//UART 9600 baud
	SPBRGH = 0;
//9600 baud at 8 MHz
	SPBRG = 12;
	TXSTAbits.BRGH = 0;
	BAUDCONbits.BRG16 = 0;
//9600 baud at 4 MHz
//	SPBRG = 103;
//	TXSTAbits.BRGH = 1;
//	BAUDCONbits.BRG16 = 1;
	//Asynchronous
	TXSTAbits.SYNC = 0;
	RCSTAbits.SPEN = 1;
	TXSTAbits.TX9 = 0;
	TXSTAbits.TXEN = 1;

//	TRISAbits.TRISA2 = 1;//Bluetooth connection on:1 off:0 (RN42 GPIO2)
	//RC4: UART TX
	//RC5: UART RX
}

void enterSleepMode() {
	OSCCONbits.IRCF = 0x0;//31 kHz
	T1CONbits.TMR1ON = 0;
	TMR1IE=0;	// timer 1 interrupt disabled
	PEIE=0;		// disable peripheral interrupts
	GIE=0;		// turn off all interrupts
	RCSTAbits.SPEN = 0;//turn off UART
	TXSTAbits.TXEN = 0;
//turn off pull ups to reduce sleep current (220 uA ==> 30-50 uA)
	WPUA = 0x01;//keep the POWER_SW pull up
	WPUB = 0x00;
	WPUC = 0b00;//RC7-0: pull up
	//190315 PORTCbits.RC5 = 0;//turn off LED
	TRISA = 0b11111111;
	ANSELA = 0b11111010;//turn off RA4,3,1 digital inputs
	ANSELB = 0xff;//port B
	ANSELC = 0b11011111;//port C
}

void initBluetooth() {
	//unsigned char ucLATA5 = LATAbits.LATA5;
	PORTAbits.RA5 = 0;//force RN42 on
	//SPP mode only
	int i;
	for (int i=0; i<5; i++) {wait(30000);}
	//$$$
	while(!PIR1bits.TXIF);  TXREG = '$';
	while(!PIR1bits.TXIF);  TXREG = '$';
	while(!PIR1bits.TXIF);  TXREG = '$';
	wait(10000);
	//S-,BthDials; device name
	while(!PIR1bits.TXIF);  TXREG = 'S';
	while(!PIR1bits.TXIF);  TXREG = '-';
	while(!PIR1bits.TXIF);  TXREG = ',';
	while(!PIR1bits.TXIF);  TXREG = 'B';
	while(!PIR1bits.TXIF);  TXREG = 'T';
	while(!PIR1bits.TXIF);  TXREG = 'd';
	while(!PIR1bits.TXIF);  TXREG = 'i';
	while(!PIR1bits.TXIF);  TXREG = 'a';
	while(!PIR1bits.TXIF);  TXREG = 'l';
	while(!PIR1bits.TXIF);  TXREG = 's';
	while(!PIR1bits.TXIF);  TXREG = 0x0d;
	wait(10000);
	//SM,0; slave mode
	while(!PIR1bits.TXIF);  TXREG = 'S';
	while(!PIR1bits.TXIF);  TXREG = 'M';
	while(!PIR1bits.TXIF);  TXREG = ',';
	while(!PIR1bits.TXIF);  TXREG = '0';
	while(!PIR1bits.TXIF);  TXREG = 0x0d;
	wait(10000);
	//S~,0; SPP
	while(!PIR1bits.TXIF);  TXREG = 'S';
	while(!PIR1bits.TXIF);  TXREG = '~';
	while(!PIR1bits.TXIF);  TXREG = ',';
	while(!PIR1bits.TXIF);  TXREG = '0';
	while(!PIR1bits.TXIF);  TXREG = 0x0d;
	wait(10000);
	if (!POWER_SW) {//sniffing off
		//SW,0000; no sniff
		while(!PIR1bits.TXIF);  TXREG = 'S';
		while(!PIR1bits.TXIF);  TXREG = 'W';
		while(!PIR1bits.TXIF);  TXREG = ',';
		while(!PIR1bits.TXIF);  TXREG = '0';
		while(!PIR1bits.TXIF);  TXREG = '0';
		while(!PIR1bits.TXIF);  TXREG = '0';
		while(!PIR1bits.TXIF);  TXREG = '0';
		while(!PIR1bits.TXIF);  TXREG = 0x0d;
		wait(10000);
	} else {//sniffing on
		//SW,0320; 500 ms sniff
		while(!PIR1bits.TXIF);  TXREG = 'S';
		while(!PIR1bits.TXIF);  TXREG = 'W';
		while(!PIR1bits.TXIF);  TXREG = ',';
		while(!PIR1bits.TXIF);  TXREG = '0';
		while(!PIR1bits.TXIF);  TXREG = '3';
		while(!PIR1bits.TXIF);  TXREG = '2';
		while(!PIR1bits.TXIF);  TXREG = '0';
		while(!PIR1bits.TXIF);  TXREG = 0x0d;
		wait(10000);
	}
	//R,1
	while(!PIR1bits.TXIF);  TXREG = 'R';
	while(!PIR1bits.TXIF);  TXREG = ',';
	while(!PIR1bits.TXIF);  TXREG = '1';
	while(!PIR1bits.TXIF);  TXREG = 0x0d;

	//for (int i=0; i<5; i++) {wait(30000);}
	//PORTAbits.RA5 = ucLATA5;//RN42 resume
}

void main(void){
	unsigned char uc0, uc1, uc2, uc3;

	initMCU();
	if (ucMode & MODE_BLUETOOTH) initBluetooth();
	iPowerSW_Low = 0;

	while (1) {
		uiTick++;

		uc0 = (uiTick & 0x03);
		ucEnc[0][uc0] = ENC0CLK;
		ucEnc[1][uc0] = ENC1CLK;
		ucEnc[2][uc0] = ENC2CLK;
		ucEnc[3][uc0] = ENC3CLK;
		ucEnc[4][uc0] = ENC4CLK;
		ucEnc[5][uc0] = ENC5CLK;
		ucEnc[6][uc0] = PUSH_SW1 ? 0 : 1;//detect switch release
		ucEnc[7][uc0] = PUSH_SW2 ? 0 : 1;
		uc1 = (uc0 + 4 - 1) & 0x03;
		uc2 = (uc0 + 4 - 2) & 0x03;
		for (uc3=0; uc3<NINPUT; uc3++) {
			if ((ucEnc[uc3][uc2]==1)&&(ucEnc[uc3][uc1]==0)&&(ucEnc[uc3][uc0]==0)) {//detect 'A terminal' = off-on-on
				switch (uc3) {
					case 0: {
						if (ENC0DIR) {while(!PIR1bits.TXIF);  TXREG = ENC0FWD;}
						else {while(!PIR1bits.TXIF);  TXREG = ENC0REV;}
						break;}
					case 1: {
						if (ENC1DIR) {while(!PIR1bits.TXIF);  TXREG = ENC1FWD;}
						else {while(!PIR1bits.TXIF);  TXREG = ENC1REV;}
						break;}
					case 2: {
						if (ENC2DIR) {while(!PIR1bits.TXIF);  TXREG = ENC2FWD;}
						else {while(!PIR1bits.TXIF);  TXREG = ENC2REV;}
						break;}
					case 3: {
						if (ENC3DIR) {while(!PIR1bits.TXIF);  TXREG = ENC3FWD;}
						else {while(!PIR1bits.TXIF);  TXREG = ENC3REV;}
						break;}
					case 4: {
						if (ENC4DIR) {while(!PIR1bits.TXIF);  TXREG = ENC4FWD;}
						else {while(!PIR1bits.TXIF);  TXREG = ENC4REV;}
						break;}
					case 5: {
						if (ENC5DIR) {while(!PIR1bits.TXIF);  TXREG = ENC5FWD;}
						else {while(!PIR1bits.TXIF);  TXREG = ENC5REV;}
						break;}
					case 6: {
						while(!PIR1bits.TXIF);  TXREG = SW1REL;
						break;}
					case 7: {
						while(!PIR1bits.TXIF);  TXREG = SW2REL;
						break;}
				}
			}
		}
	
		if (ucMode & MODE_BLUETOOTH) {
			if (BLUETOOTH_CONNECTED) iTimer1 = 0;
			else if (RS232C_CONNECTED) {
				iTimer1 = 0;
				ucMode = MODE_RS232C;
				PORTAbits.RA5 = 1;//RN42 off
 			}
			if ((iTimer1 > 3000)||(iPowerSW_Low > 6)) {//wait approx 13 min to sleep, or sense continuous SW pressing
				if (BLUETOOTH_CONNECTED) {
					while(!PIR1bits.TXIF);  TXREG = '$';
					while(!PIR1bits.TXIF);  TXREG = '$';
					while(!PIR1bits.TXIF);  TXREG = '$';
					wait(10000);
					while(!PIR1bits.TXIF);  TXREG = 'K';
					while(!PIR1bits.TXIF);  TXREG = ',';
					while(!PIR1bits.TXIF);  TXREG = 0x0d;
					wait(30000);
				}
				enterSleepMode();
				while (POWER_SW == 0) {}//wait for SW released
				wait(20);
				//while (POWER_SW) {}//sleep
				while (POWER_SW && (BLUETOOTH_CONNECTED == 0)) {}//sleep
				initMCU();
			}
		} else {//MODE_RS232C
			if (RS232C_CONNECTED == 0) {
				enterSleepMode();
				//while (POWER_SW == 0) {}//wait for SW released
				wait(20);
				while (POWER_SW && (RS232C_CONNECTED == 0)) {}//sleep
				initMCU();
			}
		}

	}

}

