FLAPPY BIRD STM32 - BAN DA SUA

Phan cung:
 PA0 -> nut FLAP/START/RESTART -> GND
 PA1 -> nut PAUSE/RESUME -> GND
 GPIO pull-up noi, EXTI falling, debounce 20ms.
 PB6 SCL, PB7 SDA -> OLED SSD1306 I2C 0x3C (7-bit).
 PA8 TIM1_CH1 -> buzzer (can mach driver phu hop voi buzzer 5V).

Da sua:
 - Hai nut PA0/PA1, xu ly ngat EXTI va debounce.
 - Ve chim kich thuoc 7x7 va ong day du thay vi 2 diem pixel.
 - Man cho, dang choi, pause, game over; diem va best score (RAM).
 - Trong luc, flap, ong di chuyen, khe ong ngau nhien, va cham, tinh diem.
 - Khoi tao SSD1306 day du va timeout I2C 100ms.
 - Game task va display task FreeRTOS 33ms.

Chu y:
 - Chua nap thu len board; may kiem tra khong co arm-none-eabi-gcc.
 - Kiem tra OLED dia chi 0x3C va dien tro pull-up I2C.
 - Best score chi luu RAM, mat khi reset nguon.
 - Mo file .ioc trong CubeMX de kiem tra cau hinh ngat falling truoc khi generate lai.
 - Neu OLED cham, can toi uu driver I2C (DMA) de dam bao 30FPS thuc te.
