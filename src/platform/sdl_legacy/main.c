#include "sdl_legacy.h"

#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/feature/commandline.h>
#include <mgba/core/cheats.h>
#include <mgba-util/vfs.h>
#include <mgba/internal/gba/input.h>
#include <mgba/core/thread.h>
#include <mgba/core/serialize.h>

static struct VFile* _state = NULL;

static void _loadState(struct mCoreThread* thread) {
    mCoreLoadStateNamed(thread->core, _state, SAVESTATE_RTC);
}

int main(int argc, char* argv[]) {
    struct mCoreOptions opts = {
        .useBios = true,
        .rewindEnable = false,
        .rewindBufferCapacity = 300,
        .audioBuffers = 1536,
        .sampleRate = 44100,
        .videoSync = false,
        .audioSync = true,
        .volume = 0x100,
    };

    struct mArguments args;
    struct mGraphicsOpts graphicsOpts;

    struct mSubParser subparser;

    struct mCore* core;
    unsigned width, height;
    int ratio;
    struct mCoreThread thread = {};

    int ret;

    initParserForGraphics(&subparser, &graphicsOpts);
    bool parsed = parseArguments(&args, argc, argv, &subparser);
    if (!args.fname && !args.showVersion) {
        parsed = false;
    }
    if (!parsed || args.showHelp) {
        usage(argv[0], subparser.usage);
        freeArguments(&args);
        return !parsed;
    }
    if (args.showVersion) {
        version(argv[0]);
        freeArguments(&args);
        return 0;
    }

    core = mCoreFind(args.fname);
    if (!core) {
        printf("Could not run game. Are you sure the file exists and is a compatible game?\n");
        freeArguments(&args);
        return 1;
    }
    if (!core->init(core)) {
        freeArguments(&args);
        return 1;
    }
    core->desiredVideoDimensions(core, &width, &height);

    ratio = graphicsOpts.multiplier;
    if (ratio == 0) {
        ratio = 1;
    }
    opts.width = width * ratio;
    opts.height = height * ratio;

    struct mCheatDevice* device = NULL;
    if (args.cheatsFile && (device = core->cheatDevice(core))) {
        struct VFile* vf = VFileOpen(args.cheatsFile, O_RDONLY);
        if (vf) {
            mCheatDeviceClear(device);
            mCheatParseFile(device, vf);
            vf->close(vf);
        }
    }

    mInputMapInit(&core->inputMap, &GBAInputInfo);
    mCoreInitConfig(core, "sdl");
    applyArguments(&args, &subparser, &core->config);

    mCoreConfigLoadDefaults(&core->config, &opts);
    mCoreLoadConfig(core);

    if (!mCoreLoadFile(core, args.fname)) {
        return 1;
    }

    if (!sdlInit(core, width, height)) {
        freeArguments(&args);
        mCoreConfigDeinit(&core->config);
        core->deinit(core);
        return 1;
    }
    sdlBindKeys(&core->inputMap);

    mCoreAutoloadSave(core);
    mCoreAutoloadCheats(core);

    if (args.patch) {
        struct VFile* patch = VFileOpen(args.patch, O_RDONLY);
        if (patch) {
            core->loadPatch(core, patch);
        }
    } else {
        mCoreAutoloadPatch(core);
    }
    thread.core = core;
    ret = 0;
    if (mCoreThreadStart(&thread)) {
        if (sdlInitAudio(&thread, core->opts.audioBuffers, opts.sampleRate)) {
            if (args.savestate) {
                struct VFile* state = VFileOpen(args.savestate, O_RDONLY);
                if (state) {
                    _state = state;
                    mCoreThreadRunFunction(&thread, _loadState);
                    _state = NULL;
                    state->close(state);
                }
            }
            sdlLoop(&thread);
            sdlPauseAudio();
            if (mCoreThreadHasCrashed(&thread)) {
                ret = 1;
                printf("The game crashed!\n");
            }
        } else {
            printf("Could not initialize audio.\n");
            ret = 1;
        }
        mCoreThreadJoin(&thread);
    } else {
        ret = 1;
    }

    mInputMapDeinit(&core->inputMap);
    if (device) {
        mCheatDeviceDestroy(device);
    }

    sdlDeinit();

    freeArguments(&args);
    mCoreConfigFreeOpts(&opts);
    mCoreConfigDeinit(&core->config);
    core->deinit(core);

    return ret;
}

#if defined(_WIN32) && !defined(_UNICODE)
#include <mgba-util/string.h>

int wmain(int argc, wchar_t* argv[]) {
    char** argv8 = malloc(sizeof(char*) * argc);
    int i;
    for (i = 0; i < argc; ++i) {
        argv8[i] = utf16to8((uint16_t*) argv[i], wcslen(argv[i]) * 2);
    }
    int ret = main(argc, argv8);
    for (i = 0; i < argc; ++i) {
        free(argv8[i]);
    }
    free(argv8);
    return ret;
}

#endif
