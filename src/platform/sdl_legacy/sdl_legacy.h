#ifndef SDL_LEGACY_H
#define SDL_LEGACY_H

#include <stdbool.h>
#include <stddef.h>

struct mCore;
struct mCoreThread;
struct mInputMap;

bool sdlInit(struct mCore *core, unsigned width, unsigned height);
void sdlDeinit();

void sdlBindKeys(struct mInputMap *inputMap);

bool sdlInitAudio(struct mCoreThread *thread, size_t samples, unsigned sampleRate);
void sdlDeinitAudio();
void sdlPauseAudio();
void sdlResumeAudio();

void sdlLoop(struct mCoreThread *thread);

#endif // SDL_LEGACY_H
