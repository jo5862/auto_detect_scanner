#include "func.h"

// 전역 변수 정의
const float PULSES_PER_REVOLUTION = 2000.0;
const float CIRCUMFERENCE = 62.8;
volatile long encoder_count = 0;
volatile uint8_t unit_mode = 0;
volatile float saved_distance = 0.0;
volatile uint8_t is_saved = 0;
volatile uint32_t system_timer = 0;
volatile uint8_t viewing_saved = 0;
char buffer[16];

// I2C 함수들
void i2c_init(void) {
	TWSR = 0x00;
	TWBR = 72;
}

void i2c_start(void) {
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
}

void i2c_stop(void) {
	TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
	_delay_ms(1);
}

void i2c_write(uint8_t data) {
	TWDR = data;
	TWCR = (1<<TWINT)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
}

// LCD 함수들
void lcd_cmd(uint8_t cmd) {
	uint8_t data_u, data_l;
	data_u = (cmd & 0xF0);
	data_l = ((cmd << 4) & 0xF0);
	
	i2c_start();
	i2c_write(LCD_ADDR << 1);
	
	i2c_write(data_u | LCD_EN | LCD_BACKLIGHT);
	_delay_us(1);
	i2c_write(data_u | LCD_BACKLIGHT);
	_delay_us(200);
	
	i2c_write(data_l | LCD_EN | LCD_BACKLIGHT);
	_delay_us(1);
	i2c_write(data_l | LCD_BACKLIGHT);
	_delay_ms(2);
	
	i2c_stop();
}

void lcd_data(uint8_t data) {
	uint8_t data_u, data_l;
	data_u = (data & 0xF0);
	data_l = ((data << 4) & 0xF0);
	
	i2c_start();
	i2c_write(LCD_ADDR << 1);
	
	i2c_write(data_u | LCD_RS | LCD_EN | LCD_BACKLIGHT);
	_delay_us(1);
	i2c_write(data_u | LCD_RS | LCD_BACKLIGHT);
	_delay_us(200);
	
	i2c_write(data_l | LCD_RS | LCD_EN | LCD_BACKLIGHT);
	_delay_us(1);
	i2c_write(data_l | LCD_RS | LCD_BACKLIGHT);
	_delay_ms(2);
	
	i2c_stop();
}

void lcd_init(void) {
	_delay_ms(20);
	lcd_cmd(0x33);
	_delay_ms(5);
	lcd_cmd(0x32);
	_delay_ms(1);
	lcd_cmd(0x28);
	_delay_ms(1);
	lcd_cmd(0x0C);
	_delay_ms(1);
	lcd_cmd(0x06);
	_delay_ms(1);
	lcd_cmd(0x01);
	_delay_ms(2);
}

void lcd_string(const char *str) {
	while(*str) lcd_data(*str++);
}

void lcd_clear(void) {
	lcd_cmd(0x01);
	_delay_ms(2);
}

void lcd_gotoxy(uint8_t x, uint8_t y) {
	uint8_t addr = (y == 0) ? 0x80 : 0xC0;
	addr += x;
	lcd_cmd(addr);
}

// 타이머 초기화
void timer_init(void) {
	TCCR2 = (1 << WGM21) | (1 << CS22);
	OCR2 = 249;
	TIMSK |= (1 << OCIE2);
}

// 버튼 초기화 함수
void button_init(void) {
	DDRD &= ~((1 << BUTTON1) | (1 << BUTTON2) | (1 << BUTTON3));
	PORTD |= (1 << BUTTON1) | (1 << BUTTON2) | (1 << BUTTON3);
}

// 엔코더 초기화 함수
void encoder_init(void) {
	DDRE &= ~((1 << ENC_A) | (1 << ENC_B));
	PORTE |= (1 << ENC_A) | (1 << ENC_B);
	
	EICRB |= (1 << ISC41) | (1 << ISC40);
	EICRB |= (1 << ISC51) | (1 << ISC50);
	EIMSK |= (1 << INT4) | (1 << INT5);
	
	sei();
}

// 버튼 체크 함수들
void check_button1(void) {
	static uint8_t last_state = 1;
	static uint32_t last_change_time = 0;
	uint8_t current_state = (PIND & (1 << BUTTON1));
	
	if (current_state != last_state && (system_timer - last_change_time) > DEBOUNCE_DELAY) {
		if (!current_state) {
			encoder_count = 0;
			is_saved = 0;
			lcd_gotoxy(0, 1);
			lcd_string("Count: 0        ");
			lcd_gotoxy(0, 0);
			switch(unit_mode) {
				case 0:
				lcd_string("Distance: 0.0cm ");
				break;
				case 1:
				lcd_string("Distance: 0.0m  ");
				break;
				case 2:
				lcd_string("Distance: 0.0km ");
				break;
			}
		}
		last_state = current_state;
		last_change_time = system_timer;
	}
}

void check_button2(void) {
	static uint8_t last_state = 1;
	static uint32_t last_change_time = 0;
	static uint16_t press_duration = 0;
	uint8_t current_state = (PIND & (1 << BUTTON2));
	
	if (current_state != last_state && (system_timer - last_change_time) > DEBOUNCE_DELAY) {
		if (!current_state) {
			if (viewing_saved) {
				viewing_saved = 0;
				float distance_cm = (encoder_count / PULSES_PER_REVOLUTION) * CIRCUMFERENCE;
				char dist_str[8];
				
				switch(unit_mode) {
					case 0:
					dtostrf(distance_cm, 3, 1, dist_str);
					sprintf(buffer, "Distance:%scm  ", dist_str);
					break;
					case 1:
					dtostrf(distance_cm / 100.0, 3, 1, dist_str);
					sprintf(buffer, "Distance:%sm   ", dist_str);
					break;
					case 2:
					dtostrf(distance_cm / 100000.0, 3, 2, dist_str);
					sprintf(buffer, "Distance:%skm  ", dist_str);
					break;
				}
				lcd_gotoxy(0, 0);
				lcd_string(buffer);
				sprintf(buffer, "Count: %ld   %c", encoder_count, is_saved ? 'S' : ' ');
				lcd_gotoxy(0, 1);
				lcd_string(buffer);
				} else {
				press_duration = 0;
				uint32_t press_start = system_timer;
				
				while (!(PIND & (1 << BUTTON2))) {
					if (system_timer - press_start > LONG_PRESS_TIME) {
						viewing_saved = 1;
						char saved_str[8];
						switch(unit_mode) {
							case 0:
							dtostrf(saved_distance, 3, 1, saved_str);
							sprintf(buffer, "Saved: %scm    ", saved_str);
							break;
							case 1:
							dtostrf(saved_distance / 100.0, 3, 1, saved_str);
							sprintf(buffer, "Saved: %sm     ", saved_str);
							break;
							case 2:
							dtostrf(saved_distance / 100000.0, 3, 2, saved_str);
							sprintf(buffer, "Saved: %skm    ", saved_str);
							break;
						}
						lcd_gotoxy(0, 0);
						lcd_string(buffer);
						return;
					}
					_delay_ms(10);
					press_duration++;
				}
				
				if (press_duration <= LONG_PRESS_TIME / 10) {
					saved_distance = (encoder_count / PULSES_PER_REVOLUTION) * CIRCUMFERENCE;
					is_saved = 1;
					
					for(uint8_t i = 0; i < 2; i++) {
						lcd_gotoxy(13, 1);
						lcd_string("S");
						_delay_ms(300);
						lcd_gotoxy(13, 1);
						lcd_string(" ");
						_delay_ms(300);
					}
					lcd_gotoxy(13, 1);
					lcd_string("S");
				}
			}
		}
		last_state = current_state;
		last_change_time = system_timer;
	}
}

void check_button3(void) {
	static uint8_t last_state = 1;
	static uint32_t last_change_time = 0;
	uint8_t current_state = (PIND & (1 << BUTTON3));
	
	if (current_state != last_state && (system_timer - last_change_time) > DEBOUNCE_DELAY) {
		if (!current_state) {
			unit_mode = (unit_mode + 1) % 3;
		}
		last_state = current_state;
		last_change_time = system_timer;
	}
}