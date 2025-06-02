#include"u_gpio.h" // basic gpio
#include"u_uart.h"

#include"u_display.h" // basic display
#include"u_rand.h" // basic random
#include"u_heap.h"
#include"u_cstr_util.h"
#include"u_fs.h"

Pixel565* mainDisplayBuffer;
u64 elapsedMS;
time_point start, end;
size_t freeBytes;

void main() {
    uart_init();
    uart_print("kernel wake up!\n");
    delay_ms(1000);
    
    srand(0x330633);
    lcd_init();
    lcd_clear_screen(0xffff);

    _u_heap_init(); // init heap
    
    uart_print("creating frame buffer...\n");
    mainDisplayBuffer = (Pixel565*)u_malloc(sizeof(Pixel565) * 128 * 128);
    lcd_clear_screen(0x0000);
    uart_print("created frame buffer!\n");

    uart_print("sd...\n");

    if(sd_init() != 0){
        uart_print("sd fail\n");
    }else{
        uart_print("sd success\n");
    }

    uart_print("sd end.\n");

    lcd_clear_screen(0x00ff);
   
    while(1){
        start = get_now();
        memset(mainDisplayBuffer, 0x00, sizeof(Pixel565) * 128 * 128);
        lcd_draw_string("uOS v1.0", 0, 0, 0x00f8, mainDisplayBuffer);
        /*
        freeBytes = _u_free_heap();
        lcd_draw_string(ulltoa(freeBytes), 0, 9, 0xf800, mainDisplayBuffer);
        lcd_draw_string(ulltoa(_u_allocated_block_count()), 0, 18, 0xf800, mainDisplayBuffer);
        lcd_draw_string(ulltoa(elapsedMS), 0, 27, 0xf800, mainDisplayBuffer);
        */
        lcd_draw_pixel(100, 100, 0x0000);
       // lcd_draw_string(ulltoa(entCount), 0, 9, 0xf800, mainDisplayBuffer);
       // lcd_draw_string(ulltoa(entries[0].sector_count), 0, 18, 0xf800, mainDisplayBuffer);
      //  lcd_draw_string(ulltoa(entries[0].type), 0, 27, 0xf800, mainDisplayBuffer);

        lcd_draw_bitmap(0, 0, mainDisplayBuffer, 128, 128);
        lcd_draw_pixel(100, 100, 0x0f0f);
        end = get_now();
        elapsedMS = tp_to_ms(end - start);

        delay_ms(10);
    }

    while (1);
}
