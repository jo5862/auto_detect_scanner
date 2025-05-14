//최최최종 코드
//기능정리.


#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

// LCD I2C 관련 정의
#define LCD_ADDR 0x27
#define LCD_EN 0x04
#define LCD_RW 0x02
#define LCD_RS 0x01
#define LCD_BACKLIGHT 0x08

// 엔코더 핀 정의
#define ENC_A PE4    // A상 (INT4)
#define ENC_B PE5    // B상 (INT5)

// 버튼 핀 정의
#define BUTTON1 PD1  // 리셋 버튼
#define BUTTON2 PD0  // 저장/불러오기 버튼
#define BUTTON3 PD2  // 단위 변환 버튼

// 버튼 관련 상수
#define DEBOUNCE_DELAY 100       // 디바운스 딜레이 (ms)
#define LONG_PRESS_TIME 100    // 긴 누름 감지 시간 (10ms * 100 = 1초)
#define DISPLAY_TIME 2000      // 표시 지속 시간 (ms)

// 엔코더 관련 상수
const float PULSES_PER_REVOLUTION = 2000.0;  // 한 바퀴당 펄스 수
const float CIRCUMFERENCE = 62.8;            // 바퀴 둘레(cm)

// 전역 변수
volatile long encoder_count = 0;    // 엔코더 카운트 값
volatile uint8_t unit_mode = 0;   // 0: cm, 1: m, 2: km
volatile float saved_distance = 0.0;    // 저장된 거리 값
volatile uint8_t is_saved = 0;         // 저장 상태 표시
volatile uint32_t system_timer = 0;    // 시스템 타이머
volatile uint8_t viewing_saved = 0;    // 저장값 확인 중 상태 플래그
char buffer[16];                       // LCD 출력용 버퍼

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
	TCCR2 = (1 << WGM21) |     // CTC 모드
	(1 << CS22);        // 분주비 64
	OCR2 = 249;                // 1ms 주기
	TIMSK |= (1 << OCIE2);     // 비교일치 인터럽트 활성화
}

// 타이머 인터럽트 처리
ISR(TIMER2_COMP_vect) {
	system_timer++;
}

// 버튼 초기화 함수
void button_init(void) {
	DDRD &= ~((1 << BUTTON1) | (1 << BUTTON2) | (1 << BUTTON3));
	PORTD |= (1 << BUTTON1) | (1 << BUTTON2) | (1 << BUTTON3);
}

// 버튼 상태 체크 함수들
void check_button1(void) {
	static uint8_t last_state = 1;
	static uint32_t last_change_time = 0;
	uint8_t current_state = (PIND & (1 << BUTTON1));
	
	if (current_state != last_state && (system_timer - last_change_time) > DEBOUNCE_DELAY) {
		if (!current_state) {  // 버튼이 눌렸을 때
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
		if (!current_state) {  // 버튼이 눌렸을 때
			if (viewing_saved) {
				// 저장값 확인 중이었다면, 측정 모드로 복귀
				viewing_saved = 0;
				
				// 현재 단위에 맞는 화면으로 복귀
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
						// 저장된 값 표시 모드 시작
						viewing_saved = 1;  // 저장값 확인 중 플래그 설정
						
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
					
					// S 깜빡임 효과
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

// 엔코더 초기화 함수
void encoder_init(void) {
	DDRE &= ~((1 << ENC_A) | (1 << ENC_B));
	PORTE |= (1 << ENC_A) | (1 << ENC_B);
	
	EICRB |= (1 << ISC41) | (1 << ISC40);  // INT4 rising edge
	EICRB |= (1 << ISC51) | (1 << ISC50);  // INT5 rising edge
	EIMSK |= (1 << INT4) | (1 << INT5);    // 인터럽트 활성화
	
	sei();
}

// 인터럽트 핸들러
ISR(INT4_vect) {
	if (!viewing_saved) {  // 저장값 확인 중이 아닐 때만 카운트
		uint8_t a = (PINE & (1 << ENC_A)) != 0;
		uint8_t b = (PINE & (1 << ENC_B)) != 0;
		
		if (a != b) {
			encoder_count++;
			} else {
			encoder_count--;
		}
	}
}

ISR(INT5_vect) {
	if (!viewing_saved) {  // 저장값 확인 중이 아닐 때만 카운트
		uint8_t a = (PINE & (1 << ENC_A)) != 0;
		uint8_t b = (PINE & (1 << ENC_B)) != 0;
		
		if (a == b) {
			encoder_count++;
			} else {
			encoder_count--;
		}
	}
}

int main(void) {
	// 초기화
	cli();
	i2c_init();
	lcd_init();
	encoder_init();
	button_init();
	timer_init();
	sei();
	
	// 초기 화면 설정
	lcd_clear();
	lcd_gotoxy(0, 0);
	lcd_string("Distance: 0.0cm ");
	lcd_gotoxy(0, 1);
	lcd_string("Count: 0        ");
	
	uint32_t last_display_update = 0;
	uint32_t last_button_check = 0;
	static uint8_t last_unit_mode = 255;  // 초기값을 무효한 값으로 설정
	
	while(1) {
		uint32_t current_time = system_timer;
		
		// 버튼 체크 (20ms 마다)
		if (current_time - last_button_check >= 20) {
			check_button1();
			check_button2();
			check_button3();
			last_button_check = current_time;
		}
		
		// viewing_saved가 false일 때만 디스플레이 업데이트 (50ms 마다)
		if (!viewing_saved && current_time - last_display_update >= 50) {
			float distance_cm = (encoder_count / PULSES_PER_REVOLUTION) * CIRCUMFERENCE;
			char dist_str[8];
			
			// 단위가 변경되었을 때만 Distance 문자열 전체를 업데이트
			if (last_unit_mode != unit_mode) {
				switch(unit_mode) {
					case 0:  // cm 단위
					dtostrf(distance_cm, 3, 1, dist_str);
					sprintf(buffer, "Distance:%scm  ", dist_str);
					break;
					case 1:  // m 단위
					dtostrf(distance_cm / 100.0, 3, 1, dist_str);
					sprintf(buffer, "Distance:%sm   ", dist_str);
					break;
					case 2:  // km 단위
					dtostrf(distance_cm / 100000.0, 3, 2, dist_str);
					sprintf(buffer, "Distance:%skm  ", dist_str);
					break;
				}
				lcd_gotoxy(0, 0);
				lcd_string(buffer);
				last_unit_mode = unit_mode;
				} else {
				// 단위가 같을 때는 숫자만 업데이트
				lcd_gotoxy(9, 0);
				switch(unit_mode) {
					case 0:
					dtostrf(distance_cm, 3, 1, dist_str);
					break;
					case 1:
					dtostrf(distance_cm / 100.0, 3, 1, dist_str);
					break;
					case 2:
					dtostrf(distance_cm / 100000.0, 3, 2, dist_str);
					break;
				}
				lcd_string(dist_str);
			}
			
			// Count 값 업데이트
			lcd_gotoxy(7, 1);
			sprintf(buffer, "%ld", encoder_count);
			lcd_string(buffer);
			lcd_string("   ");  // 이전 숫자의 잔상을 지우기 위해
			
			// 저장 상태 표시 ('S' 또는 공백)
			lcd_gotoxy(13, 1);
			lcd_string(is_saved ? "S" : " ");
			
			last_display_update = current_time;
		}
	}
	
	return 0;
}

