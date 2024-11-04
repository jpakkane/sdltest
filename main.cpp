// SPDX-License-Identifier: Apache-2.0
// Copyright 2019-2024 Jussi Pakkanen.

#include<SDL.h>
#include<SDL_image.h>
#include<SDL_mixer.h>
#include<cstdio>
#include<cstdlib>
#include<cassert>
#include<math.h>

const int SCREEN_WIDTH = 1920/2;
const int SCREEN_HEIGHT = 1080/2;
const uint32_t FTIME = (1000/60);

// For simplicity.
const int texw = 100;
const int texh = 100;

const char blue_file[] = "res/blue.png";
const char green_file[] = "res/green.jpg";
const char red_file[] = "res/red.tif";
const char shoot_file[] = "res/shoot.wav";
const char startup_file[] = "res/startup.ogg";
const char explode_file[] = "res/explode.wav";

SDL_Texture* unpack_image(SDL_Renderer *rend, const char* fname) {
    SDL_Surface *s = IMG_Load(fname);
    if(!s) {
        printf("IMG_Load %s: %s\n", fname, IMG_GetError());
        std::abort();
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, s);
    SDL_FreeSurface(s);
    return tex;
}

Mix_Chunk* unpack_wav(const char *fname) {
    Mix_Chunk *res = Mix_LoadWAV(fname);
    assert(res);
    return res;
}

template<typename T>
T min(const T &t1, const T &t2) {
   return t1 < t2 ? t1 : t2;
}

struct audiocontrol {
    SDL_mutex *m;
    SDL_AudioDeviceID dev;
    Uint8 *sample;
    int sample_size;
    int played_bytes;

    audiocontrol() : dev(0), sample(nullptr), sample_size(0), played_bytes(0) {
        m = SDL_CreateMutex();
    }

    ~audiocontrol() {
        if(dev) {
            SDL_CloseAudioDevice(dev);
        }
    }

    void play_sample(Mix_Chunk *sample) {
        Mix_PlayChannel(-1, sample, 0);
    }

    void produce(Uint8 *stream, int len) {
        SDL_LockMutex(m);
        int written_bytes = 0;
        if(played_bytes < sample_size) {
            written_bytes = min(len, sample_size - played_bytes);
            SDL_memmove(stream, sample, written_bytes);
            played_bytes += written_bytes;
        }
        if(written_bytes < len) {
            SDL_memset(stream + written_bytes, 0, len - written_bytes);
        }
        SDL_UnlockMutex(m);
    }
};

struct resources {
    SDL_Texture*blue_tex;
    SDL_Texture*red_tex;
    SDL_Texture*green_tex;

    Mix_Chunk* startup_sound;
    Mix_Chunk* shoot_sound;
    Mix_Chunk* explode_sound;

    resources(SDL_Renderer *rend) : blue_tex(unpack_image(rend, blue_file)),
            red_tex(unpack_image(rend, red_file)),
            green_tex(unpack_image(rend, green_file)) {
        assert(blue_tex);
        assert(red_tex);
        assert(green_tex);
        startup_sound = unpack_wav(startup_file);
        shoot_sound = unpack_wav(shoot_file);
        explode_sound = unpack_wav(explode_file);
    }

    ~resources() {
        Mix_FreeChunk(explode_sound);
        Mix_FreeChunk(shoot_sound);
        Mix_FreeChunk(startup_sound);
    }
};


void audiocallback(void *data, Uint8* stream, int len) {
    auto control = reinterpret_cast<audiocontrol*>(data);
    control->produce(stream, len);
}

void draw_single(SDL_Renderer *rend, SDL_Texture *tex, const double ratio) {
    SDL_Rect r;
    int centerx = SCREEN_WIDTH/2;
    int centery = SCREEN_HEIGHT/2;
    r.x = int(centerx - texw/2 + SCREEN_WIDTH*0.4*sin(2*M_PI*ratio));
    r.y = int(centery - texh/2 + SCREEN_HEIGHT*0.4*cos(2*2*M_PI*(ratio) + M_PI/2));
    r.w = texw;
    r.h = texh;
    auto rc = SDL_RenderCopy(rend, tex, nullptr, &r);
    assert(!rc);
}

void render(SDL_Renderer *rend, const resources &res, const double ratio) {
    assert(!SDL_SetRenderDrawColor(rend, 0, 0, 0, 0));
    SDL_RenderClear(rend);
    assert(!SDL_SetRenderDrawColor(rend, 255, 255, 255, 0));
    draw_single(rend, res.blue_tex, ratio);
    draw_single(rend, res.red_tex, ratio + 0.3333);
    draw_single(rend, res.green_tex, ratio + 0.6666);
    SDL_RenderPresent(rend);
}

void mainloop(SDL_Window *win, SDL_Renderer *rend, audiocontrol &control) {
    SDL_Event e;
    resources res(rend);
    auto start_time = SDL_GetTicks();
    const int cycle = 2000;
    auto last_frame = SDL_GetTicks();
    SDL_RendererInfo f;
    SDL_GetRendererInfo(rend, &f);
    int has_vsync = f.flags & SDL_RENDERER_PRESENTVSYNC;
    control.play_sample(res.startup_sound);
    SDL_PauseAudioDevice(control.dev, 0);
    while(true) {
        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) {
                return;
            } else if(e.type == SDL_KEYDOWN) {
                switch(e.key.keysym.sym) {
                case  SDLK_ESCAPE:
                case  SDLK_q:
                    return;
                }
                control.play_sample(res.explode_sound);
            } else if(e.type == SDL_JOYBUTTONDOWN || e.type == SDL_MOUSEBUTTONDOWN) {
                control.play_sample(res.shoot_sound);
            }
        }
        render(rend, res, ((SDL_GetTicks() - start_time) % cycle)/double(cycle));
        if(!has_vsync) {
            auto time_spent = SDL_GetTicks() - last_frame;
            if(time_spent < FTIME) {
                SDL_Delay(FTIME - time_spent);
            }
        }
        last_frame = SDL_GetTicks(); // Not accurate, but good enough.
    }
}


int main(int argc, char *argv[]) {
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER |
            SDL_INIT_JOYSTICK | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    int flags=IMG_INIT_PNG;
    int initted=IMG_Init(flags);
    if((initted&flags) != flags) {
        printf("IMG_Init: Failed to init required image support!\n");
        printf("IMG_Init: %s\n", IMG_GetError());
        return 1;
    }
    flags=MIX_INIT_OGG;
    initted=Mix_Init(flags);
    if((initted&flags) != flags) {
        printf("Mix_Init: Failed to init required ogg and mod support!\n");
        printf("Mix_Init: %s\n", Mix_GetError());
        return 1;
    }
    if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024)==-1) {
        printf("Mix_OpenAudio: %s\n", Mix_GetError());
        return 1;
    }
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
    atexit(SDL_Quit);
#if defined(_MSC_VER)
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "direct3d11", SDL_HINT_OVERRIDE);
    SDL_Window *win = SDL_CreateWindow("SDL test app", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
#else
    SDL_Window *win = SDL_CreateWindow("SDL test app", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
#endif
    if(!win) {
        printf("Window creation failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer *rend(SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC));
    if(!rend) {
        printf("Falling back to sw rendering: %s\n", SDL_GetError());
        rend = SDL_CreateRenderer(win, -1, 0);
        if(!rend) {
            printf("Renderer setup failed: %s\n", SDL_GetError());
            return 1;
        }
    }
    audiocontrol control;

    mainloop(win, rend, control);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    return 0;
}
