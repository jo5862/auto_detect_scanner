#include "func.h"

// 타이머 인터럽트 처리
ISR(TIMER2_COMP_vect) {
	system_timer++;
}

// 인터럽트 핸들러
ISR(INT4_vect) {
	if (!viewing_saved) {
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
	if (!viewing_saved) {
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
	static uint8_t last_unit_mode = 255;
	
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