#include <Arduino.h>
//THERMOSTAT_canon_a_air.ino
/*
                                      +-----+
         +----[PWR]-------------------| USB |--+
         |                            +-----+  |
         |         GND/RST2  [ ][ ]            |
         |       MOSI2/SCK2  [ ][ ]  A5/SCL[ ] |
         |          5V/MISO2 [ ][ ]  A4/SDA[ ] |
         |                             AREF[ ] |
         |                              GND[ ] |
         | [ ]N/C                    SCK/13[ ] |
         | [ ]IOREF                 MISO/12[ ] |
         | [ ]RST                   MOSI/11[ ]~|
         | [ ]3V3    +---+               10[ ]~|
         | [ ]5v    -| A |-               9[ ]~|
         | [ ]GND   -| R |-               8[ ] |
         | [ ]GND   -| D |-                    |
         | [ ]Vin   -| U |-               7[ ] |
         |          -| I |-               6[ ]~|
         | [ ]A0    -| N |-               5[X]~|   signal Servo
         | [ ]A1    -| O |-               4[X] |   bouton
         | [ ]A2     +---+           INT1/3[X]~|   GND bouton.
         | [ ]A3                     INT0/2[ ] |
         | [X]A4/SDA  RST SCK MISO     TX>1[ ] |
         | [X]A5/SCL  [ ] [ ] [ ]      RX<0[ ] |
         |            [ ] [ ] [ ]              |
         |  UNO_R3    GND MOSI 5V  ____________/
          \_______________________/

		  http://busyducks.com/ascii-art-arduinos
*/
//#define DEBUG
#include <OneWire.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#define bouton 4
#define pinservo 5

//Servo RC
Servo monservo;
int posservo=0;
int prevpos=0;
//ecran lcd
LiquidCrystal_I2C lcd(0x27,16,2);

//thermometre DS18B20
OneWire  ds(10);  // on pin 10 -<4.7k>- VCC
float celsius;
byte present;

//tableau de selection des temperature consigne
float ordre_celsius[]={10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25};
int ordre_celsius_steps=(sizeof(ordre_celsius)/sizeof(ordre_celsius[1]));
int step_ordre=7;

void lcd_pr(){
	lcd.setCursor(0,0);
	lcd.print("Temp:");
	lcd.setCursor(5,0);
	lcd.print(celsius);
	lcd.setCursor(0,1);
	lcd.print("ordre:");
	lcd.setCursor(6,1);
	lcd.print(ordre_celsius[step_ordre],0);

}
void pos_servo(){
	if(posservo==prevpos){delay(100);}else{
		prevpos=posservo;
		monservo.attach(pinservo);
		monservo.write(posservo);
		delay(250);
		monservo.detach();
	}
}
void ordre_canon(){
	if(celsius>ordre_celsius[step_ordre]){
		posservo=120;
		lcd.setCursor(12,1);
		lcd.print("off");
	}
	if(celsius<ordre_celsius[step_ordre]-3){
		posservo=15;
		lcd.setCursor(12,1);
		lcd.print("on ");
	}
	pos_servo();
}

void setup() {
	//init servo
	monservo.attach(pinservo);
	posservo=120;
	pos_servo();
	//init bouton selecteur ordre
	pinMode(bouton, INPUT_PULLUP);
	pinMode(3, OUTPUT);
	digitalWrite(3,LOW);//masse pour le bouton
	//init lcd
	lcd.init();
	lcd.backlight();
	lcd.setCursor(0,0);
	lcd.print("Thermostat");
	lcd.setCursor(0,1);
	lcd.print("www.alnoa.fr");
	delay(2000);
	lcd.clear();

	#ifdef DEBUG
		Serial.begin(9600);
	#endif // DEBUG
}
void loop() {
	byte i;
	byte type_s;
	byte data[12];
	byte addr[8];

	//selecteur d ordre
	if(!digitalRead(bouton)){
		step_ordre++;
		if(step_ordre>=ordre_celsius_steps){
			step_ordre=0;
		}
	}

	//acquisition du capteur
	if ( !ds.search(addr)) {
		ds.reset_search();
		delay(250);
		return;
	}

	#ifdef DEBUG
	Serial.print("ROM =");
	for( i = 0; i < 8; i++) {
			Serial.write(' ');
			Serial.print(addr[i], HEX);
	}
	#endif // DEBUG

	if (OneWire::crc8(addr, 7) != addr[7]) {
		#ifdef DEBUG
			Serial.println("CRC invalide!");
		#endif // DEBUG
		return;
	}
	#ifdef DEBUG
	Serial.println();
	#endif // DEBUG

	// indicateur du type de capteur
	switch (addr[0]) {
	case 0x10:
		#ifdef DEBUG
			Serial.println("  Chip = DS18S20");  // or old DS1820
		#endif // DEBUG
		type_s = 1;
		break;
	case 0x28:
		#ifdef DEBUG
			Serial.println("  Chip = DS18B20");
		#endif // DEBUG
		type_s = 0;
		break;
	case 0x22:
		#ifdef DEBUG
			Serial.println("  Chip = DS1822");
		#endif // DEBUG
		type_s = 0;
		break;
	default:
		#ifdef DEBUG
		Serial.println("Device is not a DS18x20 family device.");
		#endif // DEBUG
		return;
	}

	ds.reset();
	ds.select(addr);
	ds.write(0x44,1);

	delay(100);

	present = ds.reset();
	ds.select(addr);
	ds.write(0xBE);
	for ( i = 0; i < 9; i++) {
		data[i] = ds.read();
	}
	unsigned int raw = (data[1] << 8) | data[0];
	if (type_s) {
    	raw = raw << 3; // 9 bit resolution default
    	if (data[7] == 0x10) {
     		// count remain gives full 12 bit resolution
     		raw = (raw & 0xFFF0) + 12 - data[6];
    	}
  	} else {
		byte cfg = (data[4] & 0x60);
		if (cfg == 0x00) raw = raw << 3;  // 9 bit resolution, 93.75 ms
		else if (cfg == 0x20) raw = raw << 2; // 10 bit res, 187.5 ms
		else if (cfg == 0x40) raw = raw << 1; // 11 bit res, 375 ms
		// default is 12 bit resolution, 750 ms conversion time
  	}

	celsius = (float)raw / 16.0;
	#ifdef DEBUG
		Serial.print("  Temperature = ");
		Serial.print(celsius);
		Serial.print(" Celsius, ");
	#endif // DEBUG

	if(celsius>40){
		//nada , protection contre les valeurs erronées (parasites)
	}
	else
	{
		lcd_pr();
		ordre_canon();
	}

}
