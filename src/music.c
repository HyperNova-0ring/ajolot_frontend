#include <pthread.h>
#include <time.h>
#include <string.h>
#include <raylib.h>
#include "ajolot.h"

typedef struct {
    pthread_t       thread;
    pthread_mutex_t mutex;
    Music           music;
    bool            owns_music;
    char            path[512];
    volatile bool   running;
    // independent flags — all can be set simultaneously
    bool            do_play;
    bool            do_stop;
    bool            do_fade;
    float           fade_target;
    float           fade_duration;
} MusicCtx;

static MusicCtx g_music;

static void *music_thread(void *arg)
{
    MusicCtx *ctx = arg;

    Music music = ctx->owns_music ? LoadMusicStream(ctx->path) : ctx->music;
    music.looping = true;
    SetMusicVolume(music, 0.0f);

    bool  playing   = false;
    float volume    = 0.0f;
    bool  fading    = false;
    float fade_from = 0.0f;
    float fade_to   = 0.0f;
    float fade_dur  = 0.0f;
    float fade_t    = 0.0f;

    const float TICK = 0.016f;

    while (ctx->running) {
        bool  do_play, do_stop, do_fade;
        float ftarget, fdur;

        pthread_mutex_lock(&ctx->mutex);
        do_play  = ctx->do_play;  ctx->do_play  = false;
        do_stop  = ctx->do_stop;  ctx->do_stop  = false;
        do_fade  = ctx->do_fade;  ctx->do_fade  = false;
        ftarget  = ctx->fade_target;
        fdur     = ctx->fade_duration;
        pthread_mutex_unlock(&ctx->mutex);

        // stop takes priority
        if (do_stop) {
            StopMusicStream(music);
            playing = false;
            fading  = false;
        }

        if (do_fade) {
            fade_from = volume;
            fade_to   = ftarget;
            fade_dur  = fdur > 0.0f ? fdur : 0.001f;
            fade_t    = 0.0f;
            fading    = true;
            // fade-in implicitly starts playback
            if (!playing && fade_to > 0.0f) {
                PlayMusicStream(music);
                playing = true;
            }
        }

        if (do_play && !playing) {
            PlayMusicStream(music);
            playing = true;
        }

        if (fading) {
            fade_t += TICK / fade_dur;
            if (fade_t >= 1.0f) { fade_t = 1.0f; fading = false; }
            volume = fade_from + (fade_to - fade_from) * fade_t;
            SetMusicVolume(music, volume);
            // auto-stop after fade-out
            if (!fading && fade_to <= 0.0f && playing) {
                StopMusicStream(music);
                playing = false;
            }
        }

        if (playing) UpdateMusicStream(music);

        struct timespec ts = { .tv_sec = 0, .tv_nsec = (long)(TICK * 1e9f) };
        nanosleep(&ts, NULL);
    }

    if (ctx->owns_music) UnloadMusicStream(music);
    return NULL;
}

void music_init(const char *path)
{
    memset(&g_music, 0, sizeof(g_music));
    strncpy(g_music.path, path, sizeof(g_music.path) - 1);
    g_music.owns_music = true;
    pthread_mutex_init(&g_music.mutex, NULL);
    g_music.running = true;
    pthread_create(&g_music.thread, NULL, music_thread, &g_music);
}

void music_init_from_track(Music track)
{
    memset(&g_music, 0, sizeof(g_music));
    g_music.music      = track;
    g_music.owns_music = false;
    pthread_mutex_init(&g_music.mutex, NULL);
    g_music.running = true;
    pthread_create(&g_music.thread, NULL, music_thread, &g_music);
}

void music_cleanup(void)
{
    g_music.running = false;
    pthread_join(g_music.thread, NULL);
    pthread_mutex_destroy(&g_music.mutex);
}

void music_start(void)
{
    pthread_mutex_lock(&g_music.mutex);
    g_music.do_play = true;
    pthread_mutex_unlock(&g_music.mutex);
}

void music_stop(void)
{
    pthread_mutex_lock(&g_music.mutex);
    g_music.do_stop = true;
    pthread_mutex_unlock(&g_music.mutex);
}

void music_fade(float target_vol, float duration_sec)
{
    pthread_mutex_lock(&g_music.mutex);
    g_music.do_fade       = true;
    g_music.fade_target   = target_vol;
    g_music.fade_duration = duration_sec;
    pthread_mutex_unlock(&g_music.mutex);
}
