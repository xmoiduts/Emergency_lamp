//辣鸡Arduino IDE
//Scrappy Arduino IDE
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif
#include <avr/wdt.h>
#include <avr/sleep.h>

#define PIN 11							//NeoPixel data Pin
#define IR_PIN A3						//IR sensor Pin
#define NEO_SWITCH 12
#define NUM_LEDS 4
#define BRIGHTNESS 255

#define DARKER_TO_IGNITE 350   			//When light value **falls** below this value , lamp will light up ;
#define LIGHTER_TO_SHUTDOWN_INIT 450	//When light value **rises** above this value , lamp will turn off ;
#define TOLERANCE_RANGE 15 
int LIGHTER_TO_SHUTDOWN = LIGHTER_TO_SHUTDOWN_INIT;


class lamp {
public:
	Adafruit_NeoPixel strip;
	int stat;								//Indicates if the lamp is on .
	int cur_red, cur_green, cur_blue;		//State machine for this NEOPIXEL alternative LED strip ;
	float k_red, k_green, k_blue;			//For white balance . if LED outputs (255,255,255),the "white" is unconfortable.
	int redmax, greenmax, bluemax;			//255*k_color  for each color
	int ttl;
	lamp(int timetolive, float a = 1.0, float b = 0.7, float c = 0.4) :ttl(timetolive), k_red(a), k_green(b), k_blue(c)
		//
	{
		strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);
		stat = 0;
		cur_red = cur_green = cur_blue = 0;
		redmax = 255 * k_red;
		greenmax = 255 * k_green;
		bluemax = 255 * k_blue;
	}
	void lightUp() {
		if (stat) {
			while (cur_blue<bluemax) {
				if (cur_red <redmax)
					cur_red++;
				else if (cur_green <greenmax)
					cur_green++;
				else if (cur_blue <bluemax)
					cur_blue++;
				for (int j = 0; j < 4; j++) {
					strip.setPixelColor(j, strip.Color(cur_red, cur_green, cur_blue));
				}
				strip.show();
				delay(2);
			}

		}//A procedure when lamp is on , the lamp will show red->orange->yellow->warm white->white , depending on which output level it is when triggering this METHOD.
		else {
			digitalWrite(NEO_SWITCH, HIGH);//ctrl pin
			for (int i = 0; i < 256; i++) {
				for (int j = 0; j < 4; j++) {
					strip.setPixelColor(j, strip.Color(i*k_red, i*k_green, i*k_blue));
				}
				strip.show();
				delay(2);
			}
			cur_red = 255 * k_red;
			cur_green = 255 * k_green;
			cur_blue = 255 * k_blue;
			stat = 1;
			delay(500);
			LIGHTER_TO_SHUTDOWN = max(1023 - analogRead(A0) + TOLERANCE_RANGE, LIGHTER_TO_SHUTDOWN_INIT);
		}//A procedure to light up the lamp when lamp is off ,lamp shows dark->light gradually , after that , lamp gets a new LIGHTER_TO_SHUTDOWN lightvalue in order not to loop between lightup and shutdown......
		ttl = 40;
	}
	void comeOver() {
		ttl--;
	}//When lamp is at its maxium output while IR is LOW (Nobody in its detecting range),ttl will - 1 .
	void dimDown() {
		if (cur_blue > 0) {
			cur_blue -= 2;
			if (cur_blue < 0)
				cur_blue = 0;
		}
		else if (cur_green > 0) {
			cur_green -= 4;
			if (cur_green < 0)
				cur_green = 0;
		}
		else if (cur_red > 0) {
			cur_red -= 8;
			if (cur_red < 0)
				cur_red = 0;
		}

		if (cur_red == 0) {
			shutDown();
		}//dim to full dark

		for (int j = 0; j < 4; j++) {
			strip.setPixelColor(j, strip.Color(cur_red, cur_green, cur_blue));
		}
		strip.show();
	}//When nobody is moving near the lamp for a while (ttl time + "IR pin high" time),lamp will dimdown ,shows  light->warm white->yellow->orange->red->off.
	void shutDown() {
		ttl = 0;
		stat = 0;
		for (int j = 0; j < 4; j++) {
			strip.setPixelColor(j, 0x00000000);
		}
		strip.show();
		LIGHTER_TO_SHUTDOWN = LIGHTER_TO_SHUTDOWN_INIT;
		delay(2);
		digitalWrite(NEO_SWITCH, LOW);
	}//shutdown this light
};

//Global variables
lamp lamp1(60);
int lightValue[2] = { 1023,1023 };

//flags
int flag = 0;//shows if the light is in a light or a dark environment.
int rise = 0;//shows if flag rises in the last loop
int drop = 0;//shows if flag dropped in the last loop
int IR = 0;//shows if IR pin is HIGH
char bitmap = 0b00000000;//abandoned

void setup()
{
	//Serial.begin(115200);

	pinMode(A0, INPUT_PULLUP);//lightsensor
	pinMode(IR_PIN,INPUT_PULLUP);
	pinMode(NEO_SWITCH, OUTPUT);//neo power
	pinMode(PIN, INPUT);//neo data
	//pinMode(13, OUTPUT);
	
	digitalWrite(IR_PIN, LOW);
	digitalWrite(NEO_SWITCH, LOW);

	lamp1.strip.setBrightness(BRIGHTNESS);
	lamp1.strip.begin();
	lamp1.strip.show();
	delay(1);
	digitalWrite(PIN, HIGH);//reduce neopixel idle current.
	/*Energy saver using WDT */
	//setTime(5);//配置每秒2次的中断唤醒


	//ACSR |= _BV(ACD);//OFF ACD
}

void loop()
{

	//ADCSRA = 1;
	/********业务代码开始*********/
	getSensors();
	generateBitmap();

	//outputStat();
	//Serial.println(ADCSRA, BIN);
	//delay(1);

	updateStrip();
	
	/********业务代码结束*********/
	/**********节能器*************/

	ADCSRA = 0;										// disable ADC	
	MCUSR = 0;										// clear various "reset" flags	
	WDTCSR = bit(WDCE) | bit(WDE);					// allow changes, disable reset
													// set interrupt mode and an interval 
	WDTCSR = bit(WDIE) | bit(WDP2) | bit(WDP0);		// set WDIE, and 0.5 seconds delay
	wdt_reset();									// pat the dog

	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	noInterrupts();									// timed sequence follows
	sleep_enable();

	// turn off brown-out enable in software
	MCUCR = bit(BODS) | bit(BODSE);
	MCUCR = bit(BODS);
	interrupts();									// guarantees next instruction executed
	sleep_cpu();

	// cancel sleep as a precaution
	sleep_disable();
	/*********节能器结束**********/
	ADCSRA = 0b10010111;							//|= bit(ADSC);  // start conversion
}

void getSensors() {
	lightValue[1] = lightValue[0];
	lightValue[0] = 1023 - analogRead(A0);//stores last two loop 's light value for calculating flag , rise and drop.
	if (flag == 0) {
		if (lightValue[0]<DARKER_TO_IGNITE && lightValue[1]>DARKER_TO_IGNITE) {
			flag = 1;
			rise = 1;
			drop = 0;
		}
		else {
			rise = 0;
			drop = 0;
		}
	}
	else {
		if (lightValue[0] > LIGHTER_TO_SHUTDOWN && lightValue[1] < LIGHTER_TO_SHUTDOWN) {
			flag = 0;
			drop = 1;
			rise = 0;
		}
		else {
			drop = 0;
			rise = 0;
		}
	}
	IR = digitalRead(IR_PIN);
}//calculate new values of lightValue,flag,rise,drop,IR.

 /*
 * 7    6    5    4
 * Preserve  Flag   Rise   Drop
 * 3    2    1    0
 * IR    .Stat   .TTL   Preserve
 */
void generateBitmap() {
	bitmap = 0;
	bitmap |= flag << 6 | rise << 5 | drop << 4 | IR << 3 | lamp1.stat << 2 | (lamp1.ttl == 0 ? 0 : 1) << 1;
}


void outputStat() {
	Serial.print(lightValue[0]);
	Serial.print('\t');
	Serial.print(bitmap, BIN);
	Serial.print('\t');
	Serial.println(IR);
}

void updateStrip() {
	/*
	abandoned because it does NOT reduces execute time.
	if (((bitmap & 0b01110100) == 0b00000000) || ((bitmap & 0b01111100) == 0b01000000)) {
	//nop
	//Serial.println("NOP");
	}
	else if (((bitmap & 0b01001000) == 0b01001000) || ((bitmap & 0b01100000) == 0b01100000)) {
	lamp1.lightUp();
	//Serial.println("Lightup");
	}
	else if ((bitmap & 0b01010100) == 0b00010100) {
	lamp1.shutDown();
	//Serial.println("Shutdown");
	}
	else if ((bitmap & 0b01001110) == 0b01000100) {
	lamp1.dimDown();
	//Serial.println("Dimdown");
	}
	else if ((bitmap & 0b01001110) == 0b01000110) {
	lamp1.comeOver();
	//Serial.println("comeOver");
	}

	}
	*/

	if ((!flag && !rise && !drop && !lamp1.stat) || (flag && !rise && !drop && !IR && !lamp1.stat)) {
		//Serial.println("NOP");
		
	}//nop
	else if (flag && (IR || rise)) {

		digitalWrite(PIN, LOW);
		delay(1);
		lamp1.lightUp();
		//Serial.println("Lightup");
	}
	else if (!flag && drop && lamp1.stat) {
		lamp1.shutDown();
		delay(1);
		digitalWrite(PIN, HIGH);
		
		//Serial.println("Shutdown");
	}
	else if (flag && !IR && lamp1.stat) {
		if (!lamp1.ttl) {
			lamp1.dimDown();
			
			//Serial.println("dimDown");
		}
		else {
			lamp1.comeOver();
			//Serial.println("comeOver");
		}
	}
}

void setTime(int mode) {

	byte bb;

	if (mode > 9)
		mode = 9;
	bb = mode & 7;
	if (mode > 7)
		bb |= (1 << 5);
	bb |= (1 << WDCE);

	MCUSR &= ~(1 << WDRF);
	// start timed sequence
	WDTCSR |= (1 << WDCE) | (1 << WDE);
	// set new watchdog timeout value
	WDTCSR = bb;
	WDTCSR |= _BV(WDIE);
}//mode: 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms, 6=1 sec,7=2 sec, 8=4 sec, 9= 8sec ;every [mode] will call ISR(WDT_vect) once .

void powerdown_avr() {
	set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
	sleep_enable();
	sleep_mode();                        // System sleeps here
}
ISR(WDT_vect) {
	wdt_disable();  // disable watchdog
}

