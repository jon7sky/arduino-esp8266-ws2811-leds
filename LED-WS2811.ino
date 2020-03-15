// The WS2811 spec says that the protocol has this timing:
// _______
// |     |__________|       Logic 0: Time On: 220-380ns, Time Off: 580-1000ns
//
// ____________
// |          |__________|  Logic 1: Time On: 580-1000ns, Time Off: 580-1000ns
//
// It looks like we can use four UART bits at 300ns each to send both 0 and 1 WS2811 bits like this:
//
// UART bits:     | Bit 0 | Bit 1 | Bit 2 | Bit 3 |
//                _________
// WS2811 0 bit:  |       |_______________________|
//
//                | 300ns |        900ns          |
//
//                _________________
// WS2811 1 bit:  |               |_______________|
//
//                |     600ns     |     600ns     |
//
// For a bit time of 300ns, the baud rate is 1/300ns = 3,333,333.
//
// The WS2811 signal idles low, but the UART normally idles high, so we'll need to invert it.
// That can be done by setting the uart_txd_inv bit in the ESP8266 UART1 CONF0 register.
//
// We will send two WS2811 bits with one 6-bit write to the UART, like this:
//
// WS2811 signal:  |            0 bit          |            1 bit           |
//                 ________                    _______________
//                 |      |____________________|             |_______________
//
// UART signal:    | Strt |  D0  |  D1  |  D2  |  D3  |  D4  |   D5  | Stop |


// the setup function runs once when you press reset or power the board
void setup() {
  uint32_t *UART1_CONF0 = (uint32_t *)(0x60000F00 + 0x20);
  
  Serial.begin(115200);
  Serial1.begin(3333333, SERIAL_6N1);
  
  Serial.println("WS2811 LEDs");
  
  // Set the uart_txd_inv bit in the UART_CONF0 register for UART1 to invert the TX output.
  *UART1_CONF0 |= (1 << 22);
}


#if 0
// This is just a simple loop I used to test the UART.
void loop() {
  Serial1.write(0x55);
  delay(1);
}
#endif


void ws2811SendRGB(uint32_t rgb) {
  uint32_t bit;
  uint8_t byteToSend;

  // Output the bits starting at the high bit of red, ending with the low bit of blue.
  bit = (1 << 23);
  while (bit) {
    // D3 is always high, the rest are low (for now).
    byteToSend = (1 << 3);
    // If the WS2811 next bit is 1, set UART data bit 0.
    if (rgb & bit) {
      byteToSend |= (1 << 0);
    }
    bit >>= 1;
    // If the next WS2811 bit is 1, set UART data bit 4.
    if (rgb & bit) {
      byteToSend |= (1 << 4);
    }
    bit >>= 1;
    // Send these two WS2811 bits. Since we're inverting the output, we will invert what we write.
    Serial1.write(~byteToSend);
  }
}


void ws2811SendRGB(uint8_t r, uint8_t g, uint8_t b) {
  ws2811SendRGB((r << 16) | (g << 8) | (b << 0));
}


#define MAX_COLOR 10
#define MIN_COLOR 0
#define LED_CNT   50

// the loop function runs over and over again forever
void loop() {
  enum {
    INC_RED,
    DEC_BLUE,
    INC_GREEN,
    DEC_RED,
    INC_BLUE,
    DEC_GREEN,
    NUM_STATES
  } state;
  uint8_t r, g, b;
  int i, j, k;
  uint32_t rgb;

  // Make all black, then red, then green, then blue. 0xFF would be full brightness, but that might use too much current
  // if we're powering this from a USB port. So keep it dim with 0x11.
  for (rgb = 0x11000000; rgb != 0; rgb >>= 8) {
    for (i = 0; i < LED_CNT; i++) {
      ws2811SendRGB(rgb);
    }
    delay(1000);
  }

  // Ping-pong blue, then green, then red.
  for (i = 0; i < 3; i++) {
    // Ping
    for (j = 0; j < LED_CNT; j++) {
      for (k = 0; k < LED_CNT; k++) {
        ws2811SendRGB((k > j-3) && (k < j+3) ? (0x11 << ((i % 3) * 8)) : 0);
      }
      delay(5);
    }
    // Pong
    for (j = LED_CNT - 1; j >= 0; j--) {
      for (k = 0; k < LED_CNT; k++) {
        ws2811SendRGB((k > j-3) && (k < j+3) ? (0x11 << ((i % 3) * 8)) : 0);
      }
      delay(5);
    }
  }

  // Moving rainbow
  r = MIN_COLOR;
  g = MIN_COLOR;
  b = MAX_COLOR;
  state = INC_RED;
  for (j = 0; j < 200; j++) {
    for (i = 0; i < (MAX_COLOR - MIN_COLOR) * NUM_STATES + 1; i++) {
      ws2811SendRGB(r, g, b);
      switch (state) {
        case INC_RED:   if (++r >= MAX_COLOR) { state = DEC_BLUE;   } break;
        case DEC_BLUE:  if (--b == MIN_COLOR) { state = INC_GREEN;  } break;
        case INC_GREEN: if (++g >= MAX_COLOR) { state = DEC_RED;    } break;
        case DEC_RED:   if (--r == MIN_COLOR) { state = INC_BLUE;   } break;
        case INC_BLUE:  if (++b >= MAX_COLOR) { state = DEC_GREEN;  } break;
        case DEC_GREEN: if (--g == MIN_COLOR) { state = INC_RED;    } break;
      }
    }
    delay(50);
  }
}
