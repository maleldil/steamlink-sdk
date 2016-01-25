/*
*/

#include "minimal.h"
#include "driver.h"

#include <SDL.h>
#include <assert.h>

static SDL_Window *window;
static int window_width = 640;
static int window_height = 480;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_Rect displayRect;
SDL_Joystick* myjoy[4];


uint32_t *gp2x_screen32 = NULL;

int rotate_controls=0;

static int surface_width;
static int surface_height;

extern void keyprocess(SDL_Scancode inkey, bool pressed);
extern void joyprocess(Uint8 button, SDL_bool pressed, Uint8 njoy);
extern void mouse_motion_process(int x, int y);
extern void mouse_button_process(Uint8 button, SDL_bool pressed);

unsigned long gp2x_joystick_read()
{
	// Reset mouse incase there is no motion
	mouse_motion_process(0,0);

	SDL_Event event;
	while (SDL_PollEvent(&event) > 0) {
		switch(event.type)
		{
			case SDL_KEYDOWN:
				keyprocess(event.key.keysym.scancode, true);
				break;
			case SDL_KEYUP:
				keyprocess(event.key.keysym.scancode, false);
				break;
			case SDL_JOYBUTTONDOWN:
				joyprocess(event.jbutton.button, SDL_TRUE, event.jbutton.which);
				break;
			case SDL_JOYBUTTONUP:
				joyprocess(event.jbutton.button, SDL_FALSE, event.jbutton.which);
				break;
			case SDL_MOUSEMOTION:
				mouse_motion_process(event.motion.xrel, event.motion.yrel);
				break;
			case SDL_MOUSEBUTTONDOWN:
				mouse_button_process(event.button.button, SDL_TRUE);
				break;
			case SDL_MOUSEBUTTONUP:
				mouse_button_process(event.button.button, SDL_FALSE);
				break;
			case SDL_QUIT:
				gp2x_exit();
				break;
			default:
				break;
		}
	}
	return 0;
}

void gp2x_timer_delay(unsigned long ticks)
{
	unsigned long ini=gp2x_timer_read();
	while (gp2x_timer_read()-ini<ticks);
}

unsigned long gp2x_timer_read(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);

	return ((unsigned long long)now.tv_sec * 1000000LL + (now.tv_nsec / 1000LL));

}

void exitfunc()
{
	SDL_Quit();
}

static void CalculateDisplayRect(int nScreenWidth, int nScreenHeight, int nGameWidth, int nGameHeight, SDL_Rect *pDisplayRect)
{
        float flScreenAspect = static_cast<float>( nScreenWidth ) / nScreenHeight;
        float flGameAspect = static_cast<float>( nGameWidth ) / nGameHeight;
	const float flDisplayScale = 1.0f;
        if ( flScreenAspect <= flGameAspect )
        {   
            pDisplayRect->w = static_cast<int>( ceilf( nScreenWidth * flDisplayScale ) );
            pDisplayRect->h = static_cast<int>( ceilf( nGameHeight * flDisplayScale * nScreenWidth / nGameWidth ) );
        }
        else
        {   
            pDisplayRect->w = static_cast<int>( ceilf( nGameWidth * flDisplayScale * nScreenHeight / nGameHeight ) );
            pDisplayRect->h = static_cast<int>( ceilf( nScreenHeight * flDisplayScale ) );
        }
        pDisplayRect->x = ( nScreenWidth - pDisplayRect->w ) / 2;
        pDisplayRect->y = ( nScreenHeight - pDisplayRect->h ) / 2;
}

int init_SDL(void)
{
	myjoy[0]=0;
	myjoy[1]=0;
	myjoy[2]=0;
	myjoy[3]=0;

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_JOYSTICK) < 0) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		return(0);
	}

	window = SDL_CreateWindow("MAME4ALL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_FULLSCREEN);
	if ( !window ) {
		fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
		return(0);
	}
	SDL_GetWindowSize(window, &window_width, &window_height);

	renderer = SDL_CreateRenderer(window, -1, 0);
	if (!renderer) {
		fprintf(stderr, "Unable to create renderer: %s\n", SDL_GetError());
		return(0);
	}

	// Set the draw color to black for the letterboxing
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);

	// Use linear interpolation when stretching to fill the screen
	SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "linear" );

	// We handle up to four joysticks
	if (SDL_NumJoysticks() > 0) 
	{
		int i;
		SDL_JoystickEventState(SDL_ENABLE);
		
		for ( i=0; i < SDL_arraysize(myjoy) && i<SDL_NumJoysticks(); i++) {	
			myjoy[i]=SDL_JoystickOpen(i);
		}
		if (myjoy[0]) 
			logerror("Found %d joysticks\n",SDL_NumJoysticks());
	}
	SDL_ShowCursor(SDL_DISABLE);

	//Clean exits, hopefully!
	atexit(exitfunc);
    
    return(1);
}

void deinit_SDL(void)
{
	if (renderer)
	{
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
	}

	if (window)
	{
		SDL_DestroyWindow(window);
		window = NULL;
	}

	SDL_Quit();
}


void gp2x_deinit(void)
{
	if (texture) SDL_DestroyTexture(texture);
	texture = NULL;

	if(gp2x_screen32) free(gp2x_screen32);
	gp2x_screen32 = NULL;
}

void gp2x_exit(void)
{
	remove("frontend/mame.lst");
	sync();
	gp2x_deinit();
	deinit_SDL();
	exit(0);
}

void gp2x_set_video_mode(struct osd_bitmap *bitmap, int bpp,int width,int height)
{
	surface_width = width;
	surface_height = height;

	gp2x_screen32 = (uint32_t *) calloc(1, surface_width*surface_height*4);

	if (texture) SDL_DestroyTexture(texture);
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, surface_width, surface_height);

	CalculateDisplayRect(window_width, window_height, surface_width, surface_height, &displayRect);
}

void gp2x_video_flip()
{
#if 0
static int nFrame;
if ( ( ++nFrame % 2 ) == 0 ) return;
#endif
	if (displayRect.x)
	{
		SDL_Rect blank;
		blank.x = 0;
		blank.y = 0;
		blank.w = displayRect.x;
		blank.h = window_height;

		SDL_RenderFillRect(renderer, &blank);
		blank.x = (window_width - displayRect.x - 1);
		SDL_RenderFillRect(renderer, &blank);
	}
	else if (displayRect.y)
	{
		SDL_Rect blank;
		blank.x = 0;
		blank.y = 0;
		blank.w = window_width;
		blank.h = displayRect.y;

		SDL_RenderFillRect(renderer, &blank);
		blank.y = (window_height - displayRect.y - 1);
		SDL_RenderFillRect(renderer, &blank);
	}

	SDL_UpdateTexture(texture, NULL, gp2x_screen32, surface_width*4);
	SDL_RenderCopy(renderer, texture, NULL, &displayRect);
	SDL_RenderPresent(renderer);
}


void gp2x_frontend_init(void)
{
	gp2x_set_video_mode( NULL, 16, 640, 480 );
}

void gp2x_frontend_deinit(void)
{
	gp2x_deinit();
}

void FE_DisplayScreen(void)
{
	gp2x_video_flip();
}


static unsigned char fontdata8x8[] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x3C,0x42,0x99,0xBD,0xBD,0x99,0x42,0x3C,0x3C,0x42,0x81,0x81,0x81,0x81,0x42,0x3C,
	0xFE,0x82,0x8A,0xD2,0xA2,0x82,0xFE,0x00,0xFE,0x82,0x82,0x82,0x82,0x82,0xFE,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x64,0x74,0x7C,0x38,0x00,0x00,
	0x80,0xC0,0xF0,0xFC,0xF0,0xC0,0x80,0x00,0x01,0x03,0x0F,0x3F,0x0F,0x03,0x01,0x00,
	0x18,0x3C,0x7E,0x18,0x7E,0x3C,0x18,0x00,0xEE,0xEE,0xEE,0xCC,0x00,0xCC,0xCC,0x00,
	0x00,0x00,0x30,0x68,0x78,0x30,0x00,0x00,0x00,0x38,0x64,0x74,0x7C,0x38,0x00,0x00,
	0x3C,0x66,0x7A,0x7A,0x7E,0x7E,0x3C,0x00,0x0E,0x3E,0x3A,0x22,0x26,0x6E,0xE4,0x40,
	0x18,0x3C,0x7E,0x3C,0x3C,0x3C,0x3C,0x00,0x3C,0x3C,0x3C,0x3C,0x7E,0x3C,0x18,0x00,
	0x08,0x7C,0x7E,0x7E,0x7C,0x08,0x00,0x00,0x10,0x3E,0x7E,0x7E,0x3E,0x10,0x00,0x00,
	0x58,0x2A,0xDC,0xC8,0xDC,0x2A,0x58,0x00,0x24,0x66,0xFF,0xFF,0x66,0x24,0x00,0x00,
	0x00,0x10,0x10,0x38,0x38,0x7C,0xFE,0x00,0xFE,0x7C,0x38,0x38,0x10,0x10,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x1C,0x18,0x00,0x18,0x18,0x00,
	0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0x7C,0x28,0x7C,0x28,0x00,0x00,
	0x10,0x38,0x60,0x38,0x0C,0x78,0x10,0x00,0x40,0xA4,0x48,0x10,0x24,0x4A,0x04,0x00,
	0x18,0x34,0x18,0x3A,0x6C,0x66,0x3A,0x00,0x18,0x18,0x20,0x00,0x00,0x00,0x00,0x00,
	0x30,0x60,0x60,0x60,0x60,0x60,0x30,0x00,0x0C,0x06,0x06,0x06,0x06,0x06,0x0C,0x00,
	0x10,0x54,0x38,0x7C,0x38,0x54,0x10,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x04,0x08,0x10,0x20,0x40,0x00,0x00,
	0x38,0x4C,0xC6,0xC6,0xC6,0x64,0x38,0x00,0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00,
	0x7C,0xC6,0x0E,0x3C,0x78,0xE0,0xFE,0x00,0x7E,0x0C,0x18,0x3C,0x06,0xC6,0x7C,0x00,
	0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x00,0xFC,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00,
	0x3C,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00,0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00,
	0x78,0xC4,0xE4,0x78,0x86,0x86,0x7C,0x00,0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00,
	0x00,0x00,0x18,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x18,0x18,0x30,
	0x1C,0x38,0x70,0xE0,0x70,0x38,0x1C,0x00,0x00,0x7C,0x00,0x00,0x7C,0x00,0x00,0x00,
	0x70,0x38,0x1C,0x0E,0x1C,0x38,0x70,0x00,0x7C,0xC6,0xC6,0x1C,0x18,0x00,0x18,0x00,
	0x3C,0x42,0x99,0xA1,0xA5,0x99,0x42,0x3C,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00,
	0xFC,0xC6,0xC6,0xFC,0xC6,0xC6,0xFC,0x00,0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00,
	0xF8,0xCC,0xC6,0xC6,0xC6,0xCC,0xF8,0x00,0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xFE,0x00,
	0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xC0,0x00,0x3E,0x60,0xC0,0xCE,0xC6,0x66,0x3E,0x00,
	0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,
	0x06,0x06,0x06,0x06,0xC6,0xC6,0x7C,0x00,0xC6,0xCC,0xD8,0xF0,0xF8,0xDC,0xCE,0x00,
	0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00,
	0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,
	0xFC,0xC6,0xC6,0xC6,0xFC,0xC0,0xC0,0x00,0x7C,0xC6,0xC6,0xC6,0xDE,0xCC,0x7A,0x00,
	0xFC,0xC6,0xC6,0xCE,0xF8,0xDC,0xCE,0x00,0x78,0xCC,0xC0,0x7C,0x06,0xC6,0x7C,0x00,
	0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,
	0xC6,0xC6,0xC6,0xEE,0x7C,0x38,0x10,0x00,0xC6,0xC6,0xD6,0xFE,0xFE,0xEE,0xC6,0x00,
	0xC6,0xEE,0x3C,0x38,0x7C,0xEE,0xC6,0x00,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00,
	0xFE,0x0E,0x1C,0x38,0x70,0xE0,0xFE,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,
	0x60,0x60,0x30,0x18,0x0C,0x06,0x06,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,
	0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,
	0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x06,0x3E,0x66,0x66,0x3C,0x00,
	0x60,0x7C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x3C,0x66,0x60,0x60,0x66,0x3C,0x00,
	0x06,0x3E,0x66,0x66,0x66,0x66,0x3E,0x00,0x00,0x3C,0x66,0x66,0x7E,0x60,0x3C,0x00,
	0x1C,0x30,0x78,0x30,0x30,0x30,0x30,0x00,0x00,0x3E,0x66,0x66,0x66,0x3E,0x06,0x3C,
	0x60,0x7C,0x76,0x66,0x66,0x66,0x66,0x00,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x00,
	0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x0C,0x38,0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00,
	0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0xEC,0xFE,0xFE,0xFE,0xD6,0xC6,0x00,
	0x00,0x7C,0x76,0x66,0x66,0x66,0x66,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x3C,0x00,
	0x00,0x7C,0x66,0x66,0x66,0x7C,0x60,0x60,0x00,0x3E,0x66,0x66,0x66,0x3E,0x06,0x06,
	0x00,0x7E,0x70,0x60,0x60,0x60,0x60,0x00,0x00,0x3C,0x60,0x3C,0x06,0x66,0x3C,0x00,
	0x30,0x78,0x30,0x30,0x30,0x30,0x1C,0x00,0x00,0x66,0x66,0x66,0x66,0x6E,0x3E,0x00,
	0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x7C,0x6C,0x00,
	0x00,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x06,0x3C,
	0x00,0x7E,0x0C,0x18,0x30,0x60,0x7E,0x00,0x0E,0x18,0x0C,0x38,0x0C,0x18,0x0E,0x00,
	0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00,0x70,0x18,0x30,0x1C,0x30,0x18,0x70,0x00,
	0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x10,0x28,0x10,0x54,0xAA,0x44,0x00,0x00,
};

static void gp2x_text(uint32_t *screen, int x, int y, char *text, int color)
{
	unsigned int i,l;
	screen=screen+(x*2)+(y*2)*640;

	for (i=0;i<strlen(text);i++) {
		
		for (l=0;l<16;l=l+2) {
			screen[l*640+0]=(fontdata8x8[((text[i])*8)+l/2]&0x80)?color:screen[l*640+0];
			screen[l*640+1]=(fontdata8x8[((text[i])*8)+l/2]&0x80)?color:screen[l*640+1];

			screen[l*640+2]=(fontdata8x8[((text[i])*8)+l/2]&0x40)?color:screen[l*640+2];
			screen[l*640+3]=(fontdata8x8[((text[i])*8)+l/2]&0x40)?color:screen[l*640+3];

			screen[l*640+4]=(fontdata8x8[((text[i])*8)+l/2]&0x20)?color:screen[l*640+4];
			screen[l*640+5]=(fontdata8x8[((text[i])*8)+l/2]&0x20)?color:screen[l*640+5];

			screen[l*640+6]=(fontdata8x8[((text[i])*8)+l/2]&0x10)?color:screen[l*640+6];
			screen[l*640+7]=(fontdata8x8[((text[i])*8)+l/2]&0x10)?color:screen[l*640+7];

			screen[l*640+8]=(fontdata8x8[((text[i])*8)+l/2]&0x08)?color:screen[l*640+8];
			screen[l*640+9]=(fontdata8x8[((text[i])*8)+l/2]&0x08)?color:screen[l*640+9];

			screen[l*640+10]=(fontdata8x8[((text[i])*8)+l/2]&0x04)?color:screen[l*640+10];
			screen[l*640+11]=(fontdata8x8[((text[i])*8)+l/2]&0x04)?color:screen[l*640+11];

			screen[l*640+12]=(fontdata8x8[((text[i])*8)+l/2]&0x02)?color:screen[l*640+12];
			screen[l*640+13]=(fontdata8x8[((text[i])*8)+l/2]&0x02)?color:screen[l*640+13];

			screen[l*640+14]=(fontdata8x8[((text[i])*8)+l/2]&0x01)?color:screen[l*640+14];
			screen[l*640+15]=(fontdata8x8[((text[i])*8)+l/2]&0x01)?color:screen[l*640+15];
		}
		for (l=1;l<16;l=l+2) {
			screen[l*640+0]=(fontdata8x8[((text[i])*8)+l/2]&0x80)?color:screen[l*640+0];
			screen[l*640+1]=(fontdata8x8[((text[i])*8)+l/2]&0x80)?color:screen[l*640+1];

			screen[l*640+2]=(fontdata8x8[((text[i])*8)+l/2]&0x40)?color:screen[l*640+2];
			screen[l*640+3]=(fontdata8x8[((text[i])*8)+l/2]&0x40)?color:screen[l*640+3];

			screen[l*640+4]=(fontdata8x8[((text[i])*8)+l/2]&0x20)?color:screen[l*640+4];
			screen[l*640+5]=(fontdata8x8[((text[i])*8)+l/2]&0x20)?color:screen[l*640+5];

			screen[l*640+6]=(fontdata8x8[((text[i])*8)+l/2]&0x10)?color:screen[l*640+6];
			screen[l*640+7]=(fontdata8x8[((text[i])*8)+l/2]&0x10)?color:screen[l*640+7];

			screen[l*640+8]=(fontdata8x8[((text[i])*8)+l/2]&0x08)?color:screen[l*640+8];
			screen[l*640+9]=(fontdata8x8[((text[i])*8)+l/2]&0x08)?color:screen[l*640+9];

			screen[l*640+10]=(fontdata8x8[((text[i])*8)+l/2]&0x04)?color:screen[l*640+10];
			screen[l*640+11]=(fontdata8x8[((text[i])*8)+l/2]&0x04)?color:screen[l*640+11];

			screen[l*640+12]=(fontdata8x8[((text[i])*8)+l/2]&0x02)?color:screen[l*640+12];
			screen[l*640+13]=(fontdata8x8[((text[i])*8)+l/2]&0x02)?color:screen[l*640+13];

			screen[l*640+14]=(fontdata8x8[((text[i])*8)+l/2]&0x01)?color:screen[l*640+14];
			screen[l*640+15]=(fontdata8x8[((text[i])*8)+l/2]&0x01)?color:screen[l*640+15];
		}
		screen+=16;
	} 
}

void gp2x_gamelist_text_out(int x, int y, char *eltexto, int color)
{
	char texto[33];
	strncpy(texto,eltexto,32);
	texto[32]=0;
	gp2x_text(gp2x_screen32,x,y,texto,color);
}

static int pflog=0;

void gp2x_printf_init(void)
{
	pflog=0;
}

void gp2x_text_log(char *texto)
{
    if (!pflog)
    {
        memset(gp2x_screen32,0,320*240*4);
    }
    gp2x_text(gp2x_screen32,0,pflog,texto,gp2x_color32(255,255,255));
    pflog+=8;
    if(pflog>239) pflog=0;
}

/* Variadic functions guide found at http://www.unixpapa.com/incnote/variadic.html */
void gp2x_printf(char* fmt, ...)
{
	int i,c;
	char strOut[4096];
	char str[41];
	va_list marker;

	va_start(marker, fmt);
	vsprintf(strOut, fmt, marker);
	va_end(marker);

	gp2x_frontend_init();

	c=0;
	for (i=0;i<strlen(strOut);i++)
	{
	    str[c]=strOut[i];
	    if (str[c]=='\n')
	    {
	        str[c]=0;
	        gp2x_text_log(str);
	        c=0;
	    }
	    else if (c==39)
	    {
	        str[40]=0;
	        gp2x_text_log(str);
	        c=0;
	    }
	    else
	    {
	        c++;
	    }
	}

	FE_DisplayScreen();
	sleep(6);	
	gp2x_frontend_deinit();

	pflog=0;

}