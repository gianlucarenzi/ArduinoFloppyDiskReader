/*
 * Simple image viewer with timeout or event
 *
 * Written by Gianluca Renzi <gianlucarenzi@eurekelettronica.it>
 *
 * Licensed under LGPLv2: the complete text of the GNU Lesser General
 * Public License can be found in COPYING file of the nilfs-utils
 * package.
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <termio.h>
#include <time.h>
#include <libgen.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/mount.h>

#include <linux/fb.h>

#include "SDL.h"
#include "SDL_image.h"

static int vv = 0; // Verbose level

// Variabili statiche al modulo, servono fondamentalmente per chiudere
// correttamente l'applicazione tramite SIGNALS da fuori...

static SDL_Surface *image = NULL;
static void sighandler(int a);

#define dbg(fmt, args...)	\
if (vv) {\
	fprintf(stdout, "%s: " fmt "\n", __func__, ## args);\
}

// Per qualsiasi segnale (SIGTERM o SIGINT) esco in maniera corretta
static void sighandler(int a)
{
	// ...ed esce in maniera pulita...
	if (image != NULL)
		SDL_FreeSurface(image);
	SDL_Quit();
	dbg("Exiting from SIGHANDLER by signal: %d...\n", a);
	exit(0);
}

static void fb_list_modes(void)
{
	int i;
	SDL_Rect **modes;

	/* Get available fullscreen modes */
	modes = SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_ANYFORMAT);
	/* Check is there are any modes available */
	if (modes == (SDL_Rect **)0)
	{
		fprintf(stderr, "No modes available!\n");
		SDL_Quit();
		exit(1);
	}
	/* Check if our resolution is restricted */
	if (modes == (SDL_Rect **)-1)
	{
		fprintf(stderr, "All resolutions available.\n");
	}
	else
	{
		/* Print valid modes */
		fprintf(stderr, "Available Modes\n");
		for (i = 0; modes[i]; ++i)
			fprintf(stderr, "  %d x %d\n", modes[i]->w, modes[i]->h);
	}
}

#define TIMER_TICK 40

int main(int argc, char **argv)
{
	int loop = 1;
	char * imagefile;
	SDL_Surface *screen;
	SDL_Event event;
	SDL_Rect rect;
	int screen_x;
	int screen_y;
	int screen_bpp;
	const SDL_VideoInfo* vinfo;
	int screenSupported;
	uint32_t sdlflags;
	int duration;
	struct timeval start, end;

	atexit(SDL_Quit);
	signal(SIGTERM, sighandler);
	signal(SIGINT, sighandler);

	if (argc > 1)
	{
		imagefile = argv[1];
	}
	else
	{
		imagefile = "/etc/splash.png";
	}

	if (argc > 2)
	{
		duration = strtoul(argv[2], NULL, 0);
	}
	else
	{
		duration = 10;
	}

	// startup SDL
	if (SDL_Init(SDL_INIT_VIDEO) == -1)
	{
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}

	vinfo = SDL_GetVideoInfo();

	if (vinfo != NULL)
	{
		screen_x = vinfo->current_w;
		screen_y = vinfo->current_h;
		screen_bpp = vinfo->vfmt->BitsPerPixel;
	}
	else
	{
		fprintf(stderr, "SDL_GetVideoInfo() Error %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	sdlflags = SDL_ANYFORMAT;
	screenSupported = SDL_VideoModeOK(screen_x, screen_y, screen_bpp, sdlflags);

	if (screenSupported == 0)
	{
		fprintf(stderr, "Unable to set screen: %dx%d-%d\n",
			screen_x, screen_y, screen_bpp);
		sdlflags |= SDL_FULLSCREEN;
		/* Try fullscreen */
		screenSupported = SDL_VideoModeOK(screen_x, screen_y, screen_bpp, sdlflags);
		if (screenSupported == 0)
		{
			fprintf(stderr, "FATAL: Unable to set screen: %dx%d-%d FULLSCREEN\n",
				screen_x, screen_y, screen_bpp);
			fb_list_modes();
			SDL_Quit();
			return 1;
		}
	}

	screen = SDL_SetVideoMode(screen_x, screen_y, screen_bpp, sdlflags);

	image = IMG_Load(imagefile);
	if (!image)
	{
		fprintf(stderr, "Error on loading image %s\n", imagefile);
		SDL_Quit();
		return 1;
	}

	// Centriamo l'immagine nello schermo
	rect.w = image->w;
	rect.h = image->h;
	rect.x = (screen_x - rect.w) / 2;
	rect.y = (screen_y - rect.h) / 2;
	dbg("IMAGE Coord: X: %d, Y: %d, W: %d, H:%d", rect.x, rect.y, rect.w, rect.h);

	// Impostiamo il nome della caption della finestra
	// (se fosse presente un window manager)
	SDL_WM_SetCaption(argv[0], argv[0]);

	// Disabilitiamo il cursore (se presente)
	SDL_ShowCursor(0);
	SDL_BlitSurface(image, 0, screen, &rect);
	SDL_Flip(screen);

	// start timer
	gettimeofday(&start, NULL);

	SDL_PollEvent(&event);
	do
	{
		dbg("SDL Event: type: %d", event.type);
		switch(event.type)
		{
			case SDL_QUIT:
				// Fine degli eventi, esco dal loop eventi
				loop=0;
				break;
			default:
				break;
		}
		// Verifico che sia passato il tempo impostato...
		if (duration > 0)
		{
			// Con una durata minore o uguale a zero, significa
			// esco solo per evento SDL_QUIT o signal handler...
			double time_spent;
			gettimeofday(&end, NULL);
			// Convertiamo tutto in msec poi successivamente in secondi!
			time_spent = (end.tv_sec - start.tv_sec) * 1000.0;    // sec -> ms
			time_spent += (end.tv_usec - start.tv_usec) / 1000.0; // us -> ms
			time_spent /= 1000; // ms -> sec
			if (time_spent >= (double) duration)
			{
				// al timeout usciamo in maniera corretta
				loop = 1;
				dbg("TIMEOUT\n");
				break;
			}
			dbg("Duration: %f -- Elapsed: %f\n", (double) duration, time_spent);
		}
		// Non consumiamo troppa CPU in busy poll SDL!
		usleep(TIMER_TICK * 1000L);
	} while (loop == 1 && SDL_PollEvent(&event) != -1);

	// ...ed usciamo in maniera corretta
	if (image != NULL)
		SDL_FreeSurface(image);
	// finiamo-shutdown SDL
	SDL_Quit();

	dbg("Exiting from %s", argv[0]);
	return 0;
}
