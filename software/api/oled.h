#define OLED_CMD_0 (1<<8)
#define OLED_0(B)  (B)

void oled_init()
{
  // reset
  *(OLED_RST) = 0;
  // wait > 100 msec
  pause(4*2500000);
  // reset
  *(OLED_RST) = 1;
  // wait > 300us
  pause(2*25000);
  // reset
  *(OLED_RST) = 0;
  // wait > 300 msec
  pause(4*2500000);

#if 0 // SSD1351
  // send screen on
  *(OLED) = OLED_0(0xAF) | OLED_CMD_0;
  // wait > 300 msec
  pause(3*2500000);
  // select auto horiz. increment, 666 RGB
  *(OLED) = OLED_0(0xA0) | OLED_CMD_0;
  pause(32);
  *(OLED) = OLED_0(0x20); // 666 RGB: 0xA0, 565 RGB: 0x20
  pause(32);
  // unlock
  *(OLED) = OLED_0(0xFD) | OLED_CMD_0;
  pause(32);
  *(OLED) = OLED_0(0xB1);
  pause(32);
  // vertical scroll to zero
  *(OLED) = OLED_0(0xA2) | OLED_CMD_0;
  pause(32);
  *(OLED) = OLED_0(0x00);
  pause(32);
#endif

#if 1 // ST7789
  // software reset
  *(OLED) = OLED_0(0x01) | OLED_CMD_0;
  // long wait
  pause(2*2500000);
  // sleep out
  *(OLED) = OLED_0(0x11) | OLED_CMD_0;
  // long wait
  pause(2*2500000);
  // colmod
  *(OLED) = OLED_0(0x3A) | OLED_CMD_0;
  pause(32);
  *(OLED) = OLED_0(0x55);
  // long wait
  pause(2*2500000);
  // madctl
  *(OLED) = OLED_0(0x36) | OLED_CMD_0;
  pause(32);
  *(OLED) = OLED_0(0x40);
  pause(32);
  // long wait
  pause(2*2500000);
  // invon
  *(OLED) = OLED_0(0x21) | OLED_CMD_0;
  pause(2*2500000);
  // noron
  *(OLED) = OLED_0(0x13) | OLED_CMD_0;
  pause(2*2500000);
  // brightness
  *(OLED) = OLED_0(0x51) | OLED_CMD_0;
  pause(32);
  *(OLED) = OLED_0(0xFF);
  pause(32);
  // display on
  *(OLED) = OLED_0(0x29) | OLED_CMD_0;
  pause(2*2500000);
#endif

  // done!
}

void oled_fullscreen()
{
  // NOTE: we add pauses as the OLED fifo is now very small
#if 0
  // set col addr
  *(OLED) = OLED_0(0x15) | OLED_CMD_0;
  pause(32);
  *(OLED) = OLED_0(   0);
  pause(32);
  *(OLED) = OLED_0( 127);
  pause(32);
  // set row addr
  *(OLED) = OLED_0(0x75) | OLED_CMD_0;
  pause(32);
  *(OLED) = OLED_0(   0);
  pause(32);
  *(OLED) = OLED_0( 127);
  pause(32);
  // initiate write
  *(OLED) = OLED_0(0x5c) | OLED_CMD_0;
  pause(32);
#endif

#if 1
  // set col addr
  *(OLED) = OLED_0(0x2A) | OLED_CMD_0;
  pause(32);
  *(OLED) = OLED_0(   0);
  pause(32);
  *(OLED) = OLED_0(   0);
  pause(32);
  *(OLED) = OLED_0( (((SCREEN_HEIGHT-1)>>8)&255) );
  pause(32);
  *(OLED) = OLED_0(   (SCREEN_HEIGHT-1)  &255 );
  pause(32);
  // set row addr
  *(OLED) = OLED_0(0x2B) | OLED_CMD_0;
  pause(32);
  *(OLED) = OLED_0(   0);
  pause(32);
  *(OLED) = OLED_0(   0);
  pause(32);
  *(OLED) = OLED_0( (((SCREEN_WIDTH-1)>>8)&255) );
  pause(32);
  *(OLED) = OLED_0(   (SCREEN_WIDTH-1)    &255 );
  pause(32);
  // initiate write
  *(OLED) = OLED_0(0x2C) | OLED_CMD_0;
  pause(32);
#endif
}
