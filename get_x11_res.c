#include <X11/Xlib.h>
#include <stdio.h>

int get_screen_size(int *w, int *h) {

 Display* dsp;
 Screen* scr;

 dsp = XOpenDisplay(NULL);
 if (!dsp) {
  fprintf(stderr, "Failed to open default display.\n");
  return -1;
 }

 scr = DefaultScreenOfDisplay(dsp);
 if (!scr) {
  fprintf(stderr, "Failed to obtain the default screen of given display.\n");
  return -1;
 }

 *w = scr->width;
 *h = scr->height;
 XCloseDisplay(dsp);
 return 0;
}

int main() {
 int w, h;
 get_screen_size(&w, &h);
 printf ("Screen: width = %d, height = %d \n", w, h);
 return 0;
}

