# Integration Test - Flappy Bird STM32

Board: STM32F103C8T6
Display: OLED SSD1306
Operating System: FreeRTOS

## Build
- Build ARM GCC: PASS
- Flash STM32: PASS

## Functional Tests
- OLED display: PASS
- PA0 button: PASS
- PA1 Pause/Resume: PASS
- Button debounce: PASS
- Bird movement and gravity: PASS
- Pipe movement and collision: PASS
- SCORE and BEST display: PASS
- Passive buzzer PWM: PASS
- Game restart: PASS

## FreeRTOS
- Heap measurement: Not recorded
- Stack high-water marks: Not recorded
