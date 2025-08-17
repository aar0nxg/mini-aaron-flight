// main.c (SDL + Emscripten) — Mini Aaron Flight ✈️
// Robust renderer creation (accelerated → vsync → software) + helpful logs.

#include <SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

typedef struct { float x,y,w,h,speed; int type; } Obstacle;

static const int W = 960, H = 540;
static const int GROUND_Y = 460;
static const float KM_TOTAL = 12000.0f;

static Obstacle obs[128];
static int obsCount = 0;

static void addObstacle(float speed) {
    if (obsCount >= 128) return;
    float r = (float)rand()/RAND_MAX;
    Obstacle o;
    if (r < 0.55f) { // cloud
        o.type = 0; o.w = 90 + rand()%80; o.h = 50 + rand()%30;
        o.y = 60 + rand()%240;
        o.speed = speed * 1.0f;
    } else if (r < 0.85f) { // bird
        o.type = 1; o.w = 44; o.h = 28;
        o.y = 100 + rand()%260;
        o.speed = speed * 1.35f;
    } else { // storm column
        o.type = 2; o.w = 50; o.h = 120 + rand()%120;
        bool top = (rand()%2)==0;
        o.y = top ? 40 : (GROUND_Y - o.h);
        o.speed = speed * 1.1f;
    }
    o.x = (float)W + 40.0f + (rand()%120);
    obs[obsCount++] = o;
}

static bool hit(float ax,float ay,float aw,float ah, Obstacle *b) {
    float shrink = 0.8f;
    float aox = ax - (aw*(1-shrink)/2), aoy = ay - (ah*(1-shrink)/2);
    float aow = aw*shrink, aoh = ah*shrink;
    return aox < b->x + b->w && aox + aow > b->x && aoy < b->y + b->h && aoy + aoh > b->y;
}

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Prefer WebGL/GLES2 in the browser; you can force software if needed:
    // SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

    SDL_Window* win = SDL_CreateWindow(
        "Mini Aaron Flight ✈️",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H, 0
    );
    if (!win) { SDL_Log("CreateWindow failed: %s", SDL_GetError()); return 1; }

    // Renderer fallbacks
    SDL_Renderer* r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!r) {
        SDL_Log("Accel renderer failed: %s", SDL_GetError());
        r = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    }
    if (!r) {
        SDL_Log("VSYNC renderer failed: %s", SDL_GetError());
        r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!r) { SDL_Log("Software renderer failed: %s", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(r, W, H);

    float px = W*0.22f, py = H*0.5f, vy = 0.0f;
    const float gravity = 0.38f, thrust = 0.8f, maxVy = 7.0f;
    bool holding = false, running = true, victory = false, gameOver = false;

    float kmLeft = KM_TOTAL;
    float baseSpeed = 4.2f;
    float difficulty = 1.0f;
    float spawnMs = 1100.0f, spawnTimer = 0.0f;

    Uint32 last = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_SPACE || e.key.keysym.sym == SDLK_UP) holding = true;
                if (e.key.keysym.sym == SDLK_RETURN && (gameOver || victory)) {
                    obsCount = 0; py = H*0.5f; vy = 0; kmLeft = KM_TOTAL;
                    difficulty = 1.0f; spawnMs = 1100.0f; spawnTimer = 0.0f;
                    victory = false; gameOver = false;
                }
            }
            if (e.type == SDL_KEYUP) {
                if (e.key.keysym.sym == SDLK_SPACE || e.key.keysym.sym == SDLK_UP) holding = false;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) holding = true;
            if (e.type == SDL_MOUSEBUTTONUP)   holding = false;
        }

        Uint32 now = SDL_GetTicks();
        float dtms = (float)(now - last);
        float dt = fminf(48.0f, dtms) / 16.6667f;
        float dts = dtms / 1000.0f;
        last = now;

        if (!gameOver && !victory) {
            difficulty += 0.00045f * dtms;
            float worldSpeed = baseSpeed * (1.0f + (difficulty-1.0f)*0.35f);
            spawnMs = fmaxf(580.0f, 1100.0f - (difficulty-1.0f)*160.0f);

            vy += gravity * dt;
            if (holding) vy -= thrust * dt;
            if (vy >  maxVy) vy =  maxVy;
            if (vy < -maxVy) vy = -maxVy;
            py += vy * 3.2f;
            if (py < 60) py = 60;
            if (py > GROUND_Y-20) py = GROUND_Y-20;

            spawnTimer += dtms;
            if (spawnTimer >= spawnMs) { spawnTimer = 0.0f; addObstacle(worldSpeed); }

            for (int i=0;i<obsCount;i++) obs[i].x -= obs[i].speed * 3.2f;

            int wix = 0; for (int i=0;i<obsCount;i++) if (obs[i].x + obs[i].w > -10) obs[wix++] = obs[i];
            obsCount = wix;

            for (int i=0;i<obsCount;i++) if (hit(px-30, py-18, 60, 36, &obs[i])) { gameOver = true; break; }

            float startSpd = 120.0f, endSpd = 160.0f;
            float prog = (KM_TOTAL - kmLeft) / KM_TOTAL;
            float t = prog*prog*(3 - 2*prog);
            float kmPerSec = startSpd + (endSpd - startSpd) * t;
            kmLeft -= kmPerSec * dts;
            if (kmLeft <= 0) { kmLeft = 0; victory = true; }
        }

        SDL_SetRenderDrawColor(r, 16,10,32,255); SDL_RenderClear(r);

        SDL_SetRenderDrawColor(r, 255,255,255,70);
        for (int i=0;i<80;i++){
            int sx = (i*97)%W, sy = (i*53)%(H-160)+20;
            SDL_RenderDrawPoint(r, sx, sy);
            SDL_RenderDrawPoint(r, sx+1, sy);
        }

        for (int i=0;i<obsCount;i++) {
            Obstacle *o = &obs[i];
            if (o->type == 0) SDL_SetRenderDrawColor(r, 255,255,255,46);
            if (o->type == 1) SDL_SetRenderDrawColor(r, 255,216,225,255);
            if (o->type == 2) SDL_SetRenderDrawColor(r, 184,123,255,64);
            SDL_Rect box = { (int)o->x, (int)o->y, (int)o->w, (int)o->h };
            SDL_RenderFillRect(r, &box);
        }

        SDL_SetRenderDrawColor(r, 230,212,255,255);
        SDL_Rect body = { (int)(W*0.22f - 30), (int)(py - 14), 60, 28 }; SDL_RenderFillRect(r, &body);
        SDL_SetRenderDrawColor(r, 184,123,255,255);
        SDL_Rect nose = { (int)(W*0.22f + 18), (int)(py - 12), 20, 24 }; SDL_RenderFillRect(r, &nose);
        SDL_SetRenderDrawColor(r, 59,44,95,255);
        SDL_Rect winr = { (int)(W*0.22f + 2), (int)(py - 6), 18, 12 }; SDL_RenderFillRect(r, &winr);

        float p = (KM_TOTAL - kmLeft)/KM_TOTAL;
        SDL_SetRenderDrawColor(r, 255,255,255,30);
        SDL_Rect track = { 20, H-28, W-40, 10 }; SDL_RenderFillRect(r, &track);
        SDL_SetRenderDrawColor(r, 184,123,255,255);
        SDL_Rect progBar = { 20, H-28, (int)((W-40)*p), 10 }; SDL_RenderFillRect(r, &progBar);

        if (gameOver) {
            SDL_SetRenderDrawColor(r, 0,0,0,130);
            SDL_Rect overlay = {0,0,W,H}; SDL_RenderFillRect(r, &overlay);
        } else if (victory) {
            SDL_SetRenderDrawColor(r, 43,23,79,255); SDL_RenderClear(r);
            SDL_SetRenderDrawColor(r, 255,134,166,180);
            for (int i=0;i<40;i++){
                int x = (i*137 + (int)(SDL_GetTicks()/30)) % W;
                int y = (i*71 % (H-200)) + 40;
                SDL_RenderDrawPoint(r, x, y);
            }
            SDL_SetRenderDrawColor(r, 90,58,133,255);
            SDL_Rect a = { W/2 - 60, H/2 - 20, 44, 64 }; SDL_RenderFillRect(r, &a);
            SDL_SetRenderDrawColor(r, 255,143,177,255);
            SDL_Rect z = { W/2 + 16, H/2 - 20, 44, 64 }; SDL_RenderFillRect(r, &z);
            SDL_SetRenderDrawColor(r, 255,214,234,255);
            SDL_Rect arm1 = { W/2 - 56, H/2 + 6, 56, 10 }; SDL_RenderFillRect(r, &arm1);
            SDL_Rect arm2 = { W/2 + 4,  H/2 + 6, 56, 10 }; SDL_RenderFillRect(r, &arm2);
        }

        SDL_RenderPresent(r);
        SDL_Delay(0);
    }

    SDL_DestroyRenderer(r); SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
