#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <mpsse.h>
#include <endian.h>
#include <unistd.h>

#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF

#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA

#define SSD1306_SETVCOMDETECT 0xDB

#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9

#define SSD1306_SETMULTIPLEX 0xA8

#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10

#define SSD1306_SETSTARTLINE 0x40

#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR   0x22

#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8

#define SSD1306_SEGREMAP 0xA0

#define SSD1306_CHARGEPUMP 0x8D

#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2

// Scrolling #defines
#define SSD1306_ACTIVATE_SCROLL 0x2F
#define SSD1306_DEACTIVATE_SCROLL 0x2E
#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3
#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A

#define WIDTH		128
#define HEIGHT		32

static struct mpsse_context *ctx = NULL;

static bool write_once(char cmd, const char *data, size_t len)
{
	const char addr = 0x78;
	Start(ctx);
	Write(ctx, &addr, 1);
	if (GetAck(ctx)) goto end;
	Write(ctx, &cmd, 1);
	if (GetAck(ctx)) goto end;
	Write(ctx, data, len);
	if (GetAck(ctx)) goto end;
	Stop(ctx);
	return true;
end:
	// got NACK
	Stop(ctx);
	return false;
}

static bool write_reg_once(char data)
{
	return write_once(0, &data, 1);
}

static bool write_data_once(const char *data)
{
	return write_once(0x40, data, 16);
}

static void write_reg(char data)
{
	while (!write_reg_once(data));
}

static void write_data(const char *data)
{
	write_data_once(data);
}

/* Init sequence taken from the Adafruit SSD1306 Arduino library */
static bool init_display()
{
	if (!write_reg_once(SSD1306_DISPLAYOFF)) {// 0xAE
		return false;
	}

	/* Set Display Clock Divide Ratio/ Oscillator Frequency */
	write_reg(SSD1306_SETDISPLAYCLOCKDIV);            // 0xD5
	write_reg(0x80);                                  // the suggested ratio 0x80

	/* Set Multiplex Ratio */
	write_reg(SSD1306_SETMULTIPLEX);                  // 0xA8
	write_reg(HEIGHT - 1);

	/* Set Display Offset */
	write_reg(SSD1306_SETDISPLAYOFFSET);              // 0xD3
	write_reg(0x0);                                   // no offset

	/* Set Display Start Line */
	write_reg(SSD1306_SETSTARTLINE | 0x0);            // line #0

	/* Charge Pump Setting */
	write_reg(0x8D);
	/* A[2] = 1b, Enable charge pump during display on */
	write_reg(0x14);

	/* Set Memory Addressing Mode */
	write_reg(SSD1306_MEMORYMODE);                    // 0x20
#if 0
	/* Vertical addressing mode  */
	write_reg(0x01);
#else
	write_reg(0x00);                                  // 0x0 act like ks0108
#endif

	/*Set Segment Re-map */
	/* column address 127 is mapped to SEG0 */
	write_reg(0xA0 | 0x1);

	/* Set COM Output Scan Direction */
	/* remapped mode. Scan from COM[N-1] to COM0 */
	write_reg(SSD1306_COMSCANDEC);

	/* Set COM Pins Hardware Configuration */
	write_reg(SSD1306_SETCOMPINS);                    // 0xDA
	write_reg(0x02);

	write_reg(SSD1306_SETCONTRAST);                   // 0x81
	write_reg(0x8F);

	/* Set Pre-charge Period */
	write_reg(SSD1306_SETPRECHARGE);                  // 0xd9
	write_reg(0xF1);

	/* Set VCOMH Deselect Level */
	write_reg(SSD1306_SETVCOMDETECT);                 // 0xDB
	/* according to the datasheet, this value is out of bounds */
	write_reg(0x40);

	/* Entire Display ON */
	/* Resume to RAM content display. Output follows RAM content */
	write_reg(SSD1306_DISPLAYALLON_RESUME);           // 0xA4

	/* Set Normal Display
	 * 0 in RAM: OFF in display panel
	 * 1 in RAM: ON in display panel
	 */
	write_reg(SSD1306_NORMALDISPLAY);                 // 0xA6

	write_reg(SSD1306_DEACTIVATE_SCROLL);

	/* Set Display ON */
	write_reg(SSD1306_DISPLAYON);//--turn on oled panel

	return true;
}

#if 0
static int blank(struct fbtft_par *par, bool on)
{
	fbtft_par_dbg(DEBUG_BLANK, par, "%s(blank=%s)\n",
		__func__, on ? "true" : "false");

	if (on)
		write_reg(par, 0xAE);
	else
		write_reg(par, 0xAF);
	return 0;
}
#endif

#if 0
/* Gamma is used to control Contrast */
static int set_gamma(struct fbtft_par *par, unsigned long *curves)
{
	/* apply mask */
	curves[0] &= 0xFF;

	/* Set Contrast Control for BANK0 */
	write_reg(par, 0x81);
	write_reg(par, curves[0]);

	return 0;
}
#endif

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;
	int retval = EXIT_FAILURE;

	if ((ctx = MPSSE(I2C, 400000, MSB)) != NULL && ctx->open) {
		if (!init_display()) {
			fprintf(stderr, "The SSD1306 doesn't ACK, check the wiring!\n");
			return retval;
		}
		write_reg(SSD1306_COLUMNADDR);
		write_reg(0);   // Column start address (0 = reset)
		write_reg(WIDTH-1); // Column end address (127 = reset)

		write_reg(SSD1306_PAGEADDR);
		write_reg(0); // Page start address (0 = reset)
		write_reg(3); // Page end address
		while (1) {
			for (uint16_t i=0; i<(WIDTH*HEIGHT/8/16); i++) {
				char data[16] = { 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0, 0, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7 };
				usleep(100000);
				write_data(data);
			}
			for (uint16_t i=0; i<(WIDTH*HEIGHT/8/16); i++) {
				char data[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
				usleep(100000);
				write_data(data);
			}
		}
	} else {
		fprintf(stderr, "Couldn't open a FTDI device!\n");
	}
	return retval;
}
