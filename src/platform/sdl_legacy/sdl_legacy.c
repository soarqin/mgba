#include "sdl_legacy.h"

#include <mgba/core/core.h>
#include <mgba/core/log.h>

#include <SDL.h>
#include <mgba/core/thread.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/core/blip_buf.h>
#include <mgba/internal/gba/input.h>

mLOG_DEFINE_CATEGORY(SDL_AUDIO, "SDL Audio", "platform.sdl.audio");

#define SDL_BINDING_KEY 0x53444C4BU

static struct mInputMap *input = NULL;
static SDL_Surface *screen = NULL;

bool sdlInit(struct mCore *core, unsigned width, unsigned height) {
    uint32_t flag;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Could not initialize video: %s\n", SDL_GetError());
        return false;
    }
    flag = SDL_HWSURFACE;
#ifndef USE_SINGLEBUF
#ifdef SDL_TRIPLEBUF
    flag |= SDL_TRIPLEBUF;
#else
    flag |= SDL_DOUBLEBUF;
#endif
#endif
    screen = SDL_SetVideoMode(width, height, 16, flag);
    SDL_WM_SetCaption("mGBA", "");

    if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);

    core->setVideoBuffer(core, screen->pixels, screen->pitch / 2);
    return true;
}

void sdlDeinit() {
    sdlDeinitAudio();
    if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
    SDL_Quit();
}

void sdlBindKeys(struct mInputMap *inputMap) {
#ifdef DINGOO
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_LCTRL, GBA_KEY_A);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_LALT, GBA_KEY_B);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_TAB, GBA_KEY_L);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_BACKSPACE, GBA_KEY_R);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_ESCAPE, GBA_KEY_SELECT);
#else
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_x, GBA_KEY_A);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_z, GBA_KEY_B);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_a, GBA_KEY_L);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_s, GBA_KEY_R);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_BACKSPACE, GBA_KEY_SELECT);
#endif
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_RETURN, GBA_KEY_START);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_UP, GBA_KEY_UP);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_DOWN, GBA_KEY_DOWN);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_LEFT, GBA_KEY_LEFT);
    mInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_RIGHT, GBA_KEY_RIGHT);
    input = inputMap;
}

// State
SDL_AudioSpec desiredSpec;
SDL_AudioSpec obtainedSpec;

static struct mCore* core_ = NULL;
static struct mCoreSync* sync_ = NULL;
static int32_t clockRate_ = GBA_ARM7TDMI_FREQUENCY;

static void _sdlAudioCallback(void* context, Uint8* data, int len) {
    (void)context;

    blip_t* left = core_->getAudioChannel(core_, 0);
    blip_t* right = core_->getAudioChannel(core_, 1);

    mCoreSyncLockAudio(sync_);

    len >>= 2; // len /= 2 * 2
    int available = blip_samples_avail(left);
    if (available > len) {
        available = len;
    }
    blip_read_samples(left, (short*) data, available, 1);
    blip_read_samples(right, ((short*) data) + 1, available, 1);

    mCoreSyncConsumeAudio(sync_);
    if (available < len) {
        memset(((short*) data) + 2 * available, 0, (len - available) * 2 * sizeof(short));
    }
}

bool sdlInitAudio(struct mCoreThread *thread, size_t samples, unsigned sampleRate) {
    if (!thread) return false;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        mLOG(SDL_AUDIO, ERROR, "Could not initialize SDL sound system: %s", SDL_GetError());
        return false;
    }

    desiredSpec.freq = sampleRate;
    desiredSpec.format = AUDIO_S16SYS;
    desiredSpec.channels = 2;
    desiredSpec.samples = samples;
    desiredSpec.callback = _sdlAudioCallback;
    desiredSpec.userdata = NULL;

    if (SDL_OpenAudio(&desiredSpec, &obtainedSpec) < 0) {
        mLOG(SDL_AUDIO, ERROR, "Could not open SDL sound system");
        return false;
    }

    core_ = thread->core;
    sync_ = &thread->impl->sync;

	core_->setAudioBufferSize(core_, samples);

    clockRate_ = core_->frequency(core_);
	double ratio = sync_->fpsTarget > 0 ? GBAAudioCalculateRatio(1, sync_->fpsTarget, 1) : 1;
	blip_set_rates(core_->getAudioChannel(core_, 0), clockRate_, sampleRate * ratio);
	blip_set_rates(core_->getAudioChannel(core_, 1), clockRate_, sampleRate * ratio);

    SDL_PauseAudio(0);

    return true;
}

void sdlDeinitAudio() {
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void sdlPauseAudio() {
    SDL_PauseAudio(1);
}

void sdlResumeAudio() {
    SDL_PauseAudio(0);
}

void sdlLoop(struct mCoreThread *thread) {
    SDL_Event event;

    while (mCoreThreadIsActive(thread)) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    mCoreThreadEnd(thread);
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    int key = -1;
                    if (!(event.key.keysym.mod & ~(KMOD_NUM | KMOD_CAPS))) {
                        key = mInputMapKey(input, SDL_BINDING_KEY, event.key.keysym.sym);
                    }
                    if (key != -1) {
                        mCoreThreadInterrupt(thread);
                        if (event.type == SDL_KEYDOWN) {
                            thread->core->addKeys(thread->core, 1 << key);
                        } else {
                            thread->core->clearKeys(thread->core, 1 << key);
                        }
                        mCoreThreadContinue(thread);
                        break;
                    }
                    break;
                }
            }
        }

        if (mCoreSyncWaitFrameStart(&thread->impl->sync)) {
            if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
            SDL_Flip(screen);
            if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
#ifndef USE_SINGLEBUF
            thread->core->setVideoBuffer(thread->core, screen->pixels, screen->pitch / 2);
#endif
        }
        mCoreSyncWaitFrameEnd(&thread->impl->sync);
    }
}
