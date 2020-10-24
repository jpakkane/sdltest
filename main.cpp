/*
 * Copyright (c) 2015 Jussi Pakkanen.
 *
 * This file is part of mcdemo.
 *
 * Mcdemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * Mcdemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Mcdemo; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include<SDL.h>
#include<SDL_image.h>
#include<cstdio>
#include<cstdlib>
#include<cassert>
#include<math.h>

const int SCREEN_WIDTH = 1920/2;
const int SCREEN_HEIGHT = 1080/2;
const uint32_t FTIME = (1000/60);
SDL_AudioSpec want, have;

// For simplicity.
const int texw = 100;
const int texh = 100;

const char blue_file[] = "res/blue.png";
const char green_file[] = "res/green.jpg";
const char red_file[] = "res/red.tif";
const char shoot_file[] = "res/shoot.wav";
const char startup_file[] = "res/startup.wav";
const char explode_file[] = "res/explode.wav";

SDL_Texture* unpack_image(SDL_Renderer *rend, const char* fname) {
    SDL_Surface *s = IMG_Load(fname);
    if(!s) {
        printf("IMG_Load: %s\n", IMG_GetError());
        std::abort();
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, s);
    SDL_FreeSurface(s);
    return tex;
}

void unpack_wav(const char *fname, Uint8 **audio_buf, Uint32 *audio_len) {
    SDL_AudioSpec *res = SDL_LoadWAV(fname, &want, audio_buf, audio_len);
    assert(res);
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

    void play_sample(Uint8 *new_sample, Uint32 new_size) {
        SDL_LockMutex(m);
        sample = new_sample;
        sample_size = new_size;
        played_bytes = 0;
        SDL_UnlockMutex(m);
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

    Uint8* startup_sound;
    Uint32 startup_size;
    Uint8* shoot_sound;
    Uint32 shoot_size;
    Uint8* explode_sound;
    Uint32 explode_size;

    resources(SDL_Renderer *rend) : blue_tex(unpack_image(rend, blue_file)),
            red_tex(unpack_image(rend, red_file)),
            green_tex(unpack_image(rend, green_file)) {
        assert(blue_tex);
        assert(red_tex);
        assert(green_tex);
        Uint8 *tmp = nullptr;
        unpack_wav(startup_file, &tmp, &startup_size);
        startup_sound = tmp;
        tmp = nullptr;
        unpack_wav(shoot_file, &tmp, &shoot_size);
        shoot_sound = tmp;
        tmp=nullptr;
        unpack_wav(explode_file, &tmp, &explode_size);
        explode_sound = tmp;
    }

    ~resources() {
        SDL_FreeWAV(explode_sound);
        SDL_FreeWAV(shoot_sound);
        SDL_FreeWAV(startup_sound);
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
    control.play_sample(res.startup_sound, res.startup_size);
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
                control.play_sample(res.explode_sound, res.explode_size);
            } else if(e.type == SDL_JOYBUTTONDOWN || e.type == SDL_MOUSEBUTTONDOWN) {
                control.play_sample(res.shoot_sound, res.shoot_size);
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

#ifdef __cplusplus
extern "C"
#endif

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
    // All our wav files are in this format so hardcode it and have SDL do all conversions.
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    want.samples = 4096;
    want.callback = audiocallback;
    want.userdata = &control;
    control.dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    assert(control.dev);

    mainloop(win, rend, control);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    return 0;
}
