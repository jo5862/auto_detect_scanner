#ifndef FUNC_H_
#define FUNC_H_

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
#define DEBOUNCE_DELAY 50      // 디바운스 딜레이 (ms)
#define LONG_PRESS_TIME 100    // 긴 누름 감지 시간 (10ms * 100 = 1초)
#define DISPLAY_TIME 2000      // 표시 지속 시간 (ms)

// 엔코더 관련 상수
extern const float PULSES_PER_REVOLUTION;  // 한 바퀴당 펄스 수
extern const float CIRCUMFERENCE;          // 바퀴 둘레(cm)

// 전역 변수
extern volatile long encoder_count;    // 엔코더 카운트 값
extern volatile uint8_t unit_mode;     // 0: cm, 1: m, 2: km
extern volatile float saved_distance;  // 저장된 거리 값
extern volatile uint8_t is_saved;      // 저장 상태 표시
extern volatile uint32_t system_timer; // 시스템 타이머
extern volatile uint8_t viewing_saved; // 저장값 확인 중 상태 플래그
extern char buffer[16];               // LCD 출력용 버퍼

// 함수 선언
void i2c_init(void);
void i2c_start(void);
void i2c_stop(void);
void i2c_write(uint8_t data);

void lcd_cmd(uint8_t cmd);
void lcd_data(uint8_t data);
void lcd_init(void);
void lcd_string(const char *str);
void lcd_clear(void);
void lcd_gotoxy(uint8_t x, uint8_t y);

void timer_init(void);
void button_init(void);
void encoder_init(void);

void check_button1(void);
void check_button2(void);
void check_button3(void);

#endif /* FUNC_H_ */