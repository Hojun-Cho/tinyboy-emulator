#include "gb.h"
#define SDL_DISABLE_IMMINTRIN_H
#include <SDL2/SDL.h>

int in;
u8 keys;
void* window;
u8* pic;
SDL_Renderer* renderer;
SDL_Texture* bitmapTex;

static void
render()
{
  SDL_UpdateTexture(bitmapTex, nil, pic, 160 * sizeof(u32));
  SDL_RenderCopy(renderer, bitmapTex, nil, nil);
  SDL_RenderPresent(renderer);
}

void
joypadevent(void*_)
{
  SDL_Event evt;

  addevent(&evjoypad, JOYPAD_CYCLE);
  if (SDL_PollEvent(&evt) == 0)
    return;
  switch (evt.type) {
    case SDL_KEYUP: keys = 0; break;
    case SDL_KEYDOWN:
      switch (evt.key.keysym.scancode) {
        case SDL_SCANCODE_X: keys = GB_KEY_A; break;
        case SDL_SCANCODE_Z: keys = GB_KEY_B; break;
        case SDL_SCANCODE_UP: keys = GB_KEY_UP; break;
        case SDL_SCANCODE_DOWN: keys = GB_KEY_DOWN; break;
        case SDL_SCANCODE_LEFT: keys = GB_KEY_LEFT; break;
        case SDL_SCANCODE_RIGHT: keys = GB_KEY_RIGHT; break;
        case SDL_SCANCODE_RETURN: keys = GB_KEY_START; break;
        case SDL_SCANCODE_ESCAPE: break;
        default: return;
      }
      break;
    case SDL_QUIT: exit(1);
  }
  return;
}

void
flush()
{
  render();
}

void
initwindow(int scale)
{
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL Initialization Fail: %s\n", SDL_GetError());
    return;
  }
  pic = xalloc(PICH * PICW * sizeof(u32));
  window = SDL_CreateWindow("gb",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            PICW * scale,
                            PICH * scale,
                            SDL_WINDOW_SHOWN);
  if (!window)
    panic("SDL Initialization Fail: %s\n", SDL_GetError());
  SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  renderer = SDL_GetRenderer(window);
  bitmapTex = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                PICW,
                                PICH);
}
