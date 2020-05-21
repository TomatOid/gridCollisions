#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_image.h>
#ifdef _OPENMP
    #include <omp.h>
#else
    #ifndef _ESCAPE_OPENMP
        #define omp_get_num_threads() 1
        #define omp_get_thread_num() 0
        #define omp_get_max_threads() 0
        #define omp_lock_t int
        #define omp_set_lock(lck) 0
        #define omp_unset_lock(lck) 0
        #define _ESCAPE_OMPENMP
    #endif
#endif
#ifndef CACHE_LINE_SIZE
    #define CACHE_LINE_SIZE 64
#endif
#include "AABB.h"
#include "Cells.h"
#include "Grid.h"

Grid* mainGrid;
int NUM_BALLS = 500;
// acceleration due to gravity
double g = 2;
// velocity loss per interaction, multiplicitive
double eff = 1;
double vthresh = 0;
double bSize = 10;
int CYCLES_PER_FRAME = 3;
int DO_DEBUG = 0;
int DO_UNEQUAL = 0;
int OPACITY = 255;
int MAX_FPS = 60;
const int XRES = 720;
const int YRES = 720;

typedef struct Ball
{
    Collider* collide2d;
    SDL_Texture* texture;
    int isColliding;
    double r;
    double cx;
    double cy;
    double vx;
    double vy;
    double m;
} Ball;

void makeBall(double r, double cx, double cy, double vx, double vy, double m, Ball* addr, Ball* buff)
{
    Box hitbox;
    hitbox.X0 = cx - r;
    hitbox.X1 = cx + r;
    hitbox.Y0 = cy - r;
    hitbox.Y1 = cy + r;
    addr->collide2d = makeCollider(hitbox, mainGrid, buff);
    addr->r = r;
    addr->cx = cx;
    addr->cy = cy;
    addr->vx = vx;
    addr->vy = vy;
    addr->m = m;
}

int makeBallTex(Ball* ball, SDL_Renderer* ren)
{
    // calculate the center pixel of the texture
    int cx = ceil(ball->r);
    int cy = cx;
    // initialize the texture
    ball->texture = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 2 * cx + 1, 2 * cy + 1);
    SDL_SetTextureBlendMode(ball->texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(ren, ball->texture);
    // now set the texture's backgrund to transparent
    SDL_SetRenderDrawColor(ren, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(ren);
    // now fill in a circle in white, we will worry about coloring later
    SDL_SetRenderDrawColor(ren, 0xff, 0xff, rand() % 256, 0xff);
    for (int x = 0; x <= 2 * cx; x++)
    {
        for (int y = 0; y <= 2 * cy; y++)
        {
            // check if this point is within the circle
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < cx * cx)
            {
                SDL_RenderDrawPoint(ren, x, y);
            }
        }
    }
    SDL_SetRenderTarget(ren, NULL);
    return 1;
}

/* Ball a will be mutated, and should not point to the buffer, b is in the buffer */
int ballCollide(Ball* a, Ball* b)
{
    double deltax = a->cx - b->cx;
    double deltay = a->cy - b->cy;
    double dsquared = deltax * deltax + deltay * deltay;
    double rsum = a->r + b->r;
    double deltavx = a->vx - b->vx;
    double deltavy = a->vy - b->vy;
    double vdotdisp = (deltax * deltavx + deltay * deltavy);
    // only handle the collision if they are touching and moving towards each other (vdotdisp < 0)
    if ((dsquared <= rsum * rsum) && vdotdisp < 0)
    {
        a->isColliding = 1;
        double mul = (2 * b->m * vdotdisp) / ((a->m + b->m) * dsquared);
        a->vx -= eff * mul * deltax;
        a->vy -= eff * mul * deltay;
        return 1;
    }
    else { return 0; }
}

/* pick a random double from a to b */
double randi(double a, double b)
{
    return ((double)rand() / RAND_MAX) * (b - a) + a;
}

Ball* objects;
Ball* buf;
static Collider** ret;
Uint32 startTime = 0;
Uint32 endTime = 0;
uint32_t curr_update = 0;
static uint32_t htable_use;

int main(int argc, char* argv[])
{
    // Argument parsing
    if (argc > 3)
    {
        // the first argument determines the number of sprites,
        // the second determines the gravity acceleration
        NUM_BALLS = atoi(argv[1]);
        if (NUM_BALLS < 0) { printf("NUM_BALLS cannot be negitive, abort!\n"); return -1; }
        g = atof(argv[2]);
        bSize = atof(argv[3]);
        if (argc > 4)
        {
            for (int i = 4; i < argc; i++)
            {
                if (strcmp(argv[i], "-d") == 0) { DO_DEBUG = 1; }
                else if (strcmp(argv[i], "-u") == 0) { DO_UNEQUAL = 1; }
                else if (strncmp(argv[i], "-c", 2) == 0)
                {
                    int t = atoi(argv[i] + 2);
                    if (t > 0) { CYCLES_PER_FRAME = t; }
                }
                else if (strncmp(argv[i], "-e", 2) == 0)
                {
                    float f = atof(argv[i] + 2);
                    if (f >= 0 && f <= 1) { eff = f; }
                }
                else if (strncmp(argv[i], "-o", 2) == 0)
                {
                    int f = atoi(argv[i] + 2);
                    if (f >= 0 && f <= 255) { OPACITY = f; }
                } 
            }
        }
    }
    // Display the help page
    else
    {
        printf(
        "gridCollisions help: \n\n"
        "Usage: %s particles gravity radius [flags]\n\n"
        " -d        Super secret debug mode\n"
        " -u        Spawn particles unbalenced with more velocity on one side\n"
        " -c[int]   Physics cycles per frame, must be nonzero\n"
        " -e[float] Coefficient of restitution / efficiency, must be on [0, 1]\n"
        " -o[int]   Clearing opacity, lower values create trails, must be between 0 and 255\n",
        argv[0]);
        exit(0);
    }
    printf("bSize: %f\n", bSize);
    fflush(stdout);
    if (SDL_Init(SDL_INIT_EVERYTHING)) { printf("error."); return 0; }

    SDL_Window* win;
    SDL_Renderer* ren;

    SDL_CreateWindowAndRenderer(XRES, YRES, 0, &win, &ren);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    srand(18698238);
    
    int csize = ceil(2.0 * bSize);
    while (XRES % csize) csize++;
    printf("csize: %d\n", csize);
    fflush(stdout);

    // declare random seeds
    static unsigned int randr_state;
    #pragma omp threadprivate(randr_state)

    // make the grid so grid coords equal screen coords
    mainGrid = makeGrid(1 + (int)XRES / csize, 1 + (int)YRES / csize, (double)csize);
    // allocate the sprite memory, using calloc because debuging has made me paranoid
    objects = calloc(NUM_BALLS, sizeof(Ball));
    buf = calloc(NUM_BALLS, sizeof(Ball));
    #pragma omp threadprivate(ret)
    #pragma omp parallel
    ret = calloc(NUM_BALLS, sizeof(Collider*));
    // initialize the hash table as well as other threadprivates once per thread
    static hashTable* hTable;
    #pragma omp threadprivate(hTable)
    #pragma omp threadprivate(htable_use)
    #pragma omp parallel
    {
        randr_state = 0xe7425723 ^ omp_get_thread_num();
        htable_use = 0;
        if (posix_memalign((void**)&hTable, CACHE_LINE_SIZE, sizeof(hashTable))) exit(1);
        if (posix_memalign((void**)&hTable->items, CACHE_LINE_SIZE, NUM_BALLS * sizeof(hashItem))) exit(1);
        hTable->len = NUM_BALLS;
    }
    for (int i = 0; i < NUM_BALLS; i++)
    {
	    // Initalize the sprites on the heap at random positions
	    // I should really position them so they don't overlap each other,
	    // but it'll be fine, right?
        double r = randi(0.5 * bSize, bSize);
        makeBall(r, randi(r, XRES - r), randi(r, YRES - r), randi(-100, 100), randi(-100, 100), 3.14 * r * r, &objects[i], &buf[i]);
        makeBallTex(&objects[i], ren);
        if (DO_UNEQUAL && (objects[i].cx < XRES / 2)) { objects[i].vx *= 3; objects[i].vy *= 3; }
        insertToGrid(mainGrid, objects[i].collide2d, curr_update);
    }
	// The minimum amount of time each cycle can take on average
    double deltat = 1.0 / (double)(MAX_FPS * CYCLES_PER_FRAME);
    SDL_Rect* bb = malloc(sizeof(SDL_Rect));
    SDL_Event e;
    SDL_Rect winbox = { 0, 0, XRES, YRES };
    // Setup window pos variables
    int winposX0, winposY0, winposX1, winposY1, winposX2, winposY2;
    SDL_GetWindowPosition(win, &winposX0, &winposY0);
    // set them all to be the same
    winposX1 = winposX0;
    winposY1 = winposY0;
    winposX2 = winposX1;
    winposX2 = winposY1;
    startTime = SDL_GetTicks();
    while (1)
    {
        // Check to see if the user is trying to close the program, to prevent hanging
		SDL_PollEvent(&e);
        if (e.type == SDL_QUIT) { break; }

        // clear the background
        if ((curr_update + 1) % CYCLES_PER_FRAME == 0) { SDL_SetRenderDrawColor(ren, 0, 0, 0, OPACITY); SDL_RenderFillRect(ren, &winbox); }

        // copy to the buffer, which is acts as a static backup used when checking collisions,
        // and is pointed to in each collider
        memcpy(buf, objects, NUM_BALLS * sizeof(Ball));
        
        // now, if debugging, and it is a draw cycle, draw the grid
        if (DO_DEBUG && (curr_update + 1) % CYCLES_PER_FRAME == 0)
        {
            SDL_SetRenderDrawColor(ren, 12, 32, 89, 255);
            bb->w = csize;
            bb->h = csize;
            for (bb->x = 0; bb->x < XRES; bb->x += csize)
            {
                for (bb->y = 0; bb->y < YRES; bb->y += csize)
                {
                    SDL_RenderDrawRect(ren, bb);
                }
            }
        }

        // loop to adjust velocities according to window movement
        if ((curr_update + 1) % CYCLES_PER_FRAME == 0)
        {
            int tmpx, tmpy;
            SDL_GetWindowPosition(win, &tmpx, &tmpy);
            winposX2 = winposX1;
            winposY2 = winposY1;
            winposX1 = winposX0;
            winposY1 = winposY0;
            winposX0 = tmpx;
            winposY0 = tmpy;
            int winaccelX = (winposX2 - 2 * winposX1 + winposX0) / (deltat * deltat);
            int winaccelY = (winposY2 - 2 * winposY1 + winposY0) / (deltat * deltat);
            if (curr_update > 5 * CYCLES_PER_FRAME)
            {
                for (int i = 0; i < NUM_BALLS; i++)
                {
                    objects[i].vx -= winaccelX * deltat / CYCLES_PER_FRAME;
                    objects[i].vy -= winaccelY * deltat / CYCLES_PER_FRAME;
                }
            }
        }

        // collision loop
        #pragma omp parallel for
        for (int i = 0; i < NUM_BALLS; i++)
        {
            objects[i].isColliding = 0;
            // reset the hash table to be effectively empty
            hTable->num = 0;
            htable_use++;

            // we don't want our search to return this current collider,
            // so add it to the hash table, which is used inside the
            // queryBox function to avoid reduntant results
            insertHashItem(hTable, (intptr_t)(buf + i), htable_use);

            // search for the other colliders (in buf) which are near this
            // collider's hitbox, and put them in ret
            int nresults = queryBox(mainGrid, objects[i].collide2d->hitbox, ret, hTable, NUM_BALLS, curr_update, htable_use, rand_r(&randr_state) % 2, 0);

            if (DO_DEBUG && 0)
            {
                // print how many colliders were returned
                printf("%d ", nresults);
                fflush(stdout);
            }

            // now, handle the physics for each collision by looping over returned values
            int r = rand_r(&randr_state) % 2;
            #pragma omp critical
            for (int j = 0; j < nresults; j++)
            {
                // set object[i]'s velocity according to elastic collision
                if (r) ballCollide(&objects[i], ret[j]->sprite);
                else ballCollide(&objects[i], ret[nresults - j - 1]->sprite);
                if (DO_DEBUG && (curr_update + 1) % CYCLES_PER_FRAME == 0)
                {
                    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                    SDL_RenderDrawLine(ren, objects[i].cx, objects[i].cy, ((Ball*)ret[j]->sprite)->cx, ((Ball*)ret[j]->sprite)->cy);
                }
            }

            // bounce off the walls
            int w, h;
            Box hit = objects[i].collide2d->hitbox;
            SDL_GetWindowSize(win, &w, &h);
            if (hit.X0 < 0 && objects[i].vx < 0) { objects[i].vx *= -eff; objects[i].vy *= eff;  }
            if (hit.X1 > w && objects[i].vx > 0) { objects[i].vx *= -eff; objects[i].vy *= eff;  }
            if (hit.Y0 < 0 && objects[i].vy < 0) { objects[i].vy *= -eff; objects[i].vx *= eff;  }
            if (hit.Y1 > h && objects[i].vy > 0) { objects[i].vy *= -eff; objects[i].vx *= eff; objects[i].isColliding = 1; }
            // do gravity if not colliding
            if (!objects[i].isColliding) { objects[i].vy += g * deltat; }
        }

        curr_update++;

        #pragma omp parallel for
        for (int i = 0; i < NUM_BALLS; i++)
        {
            // calculate the position step
            double dx = objects[i].vx * deltat;
            double dy = objects[i].vy * deltat;

            // update the hitbox
            objects[i].collide2d->hitbox.X0 += dx;
            objects[i].collide2d->hitbox.Y0 += dy;
            objects[i].collide2d->hitbox.X1 += dx;
            objects[i].collide2d->hitbox.Y1 += dy;

            // update the circle center
            objects[i].cx += dx;
            objects[i].cy += dy;

            // re-insert this sprite into the grid
            insertToGrid(mainGrid, objects[i].collide2d, curr_update);
        }

        if (curr_update % CYCLES_PER_FRAME == 0 )
        {
            for (int i = 0; i < NUM_BALLS; i++)
            {
                // part of the purpose of this demo is to visualize
                // energy transfer using colors, where more red means
                // more energy, and more green means less energy
                double energy = 0.0001 * objects[i].m / (bSize * bSize) * (objects[i].vx * objects[i].vx + objects[i].vy * objects[i].vy);
                // using a squishification function to limit sig to a range of 0 - 1
                float sig = 1 - 1 / (1 + energy + energy * energy / 2);
                // now adjust the hue according to sig
                float colormin = 46;
                float colormax = 234;
                int r = (int)((sig > 0.5) ? colormax : colormin + (colormax - colormin) * 2 * sig);
                int g = (int)((sig < 0.5) ? colormax : colormin - (colormax - colormin) * 2 * (sig - 1));

                // draw the ball
                //drawBall(objects + i, ren);
                SDL_SetTextureColorMod(objects[i].texture, r, g, 26);
                SDL_Rect boundbox = { round(objects[i].cx - objects[i].r) - 1, round(objects[i].cy - objects[i].r) - 1, 2 * ceil(objects[i].r) + 2, 2 * ceil(objects[i].r) + 2 }; 
                SDL_RenderCopy(ren, objects[i].texture, NULL, &boundbox);
            }
        }
        
        // clear the hash table to make me feel better
        memset(hTable->items, 0, hTable->len * sizeof(hashItem));
        if (curr_update % CYCLES_PER_FRAME == 0)
        {
            SDL_RenderPresent(ren);
            endTime = SDL_GetTicks();
            double dt = endTime - startTime;
            // limit framerate
            int milis = 1000 / MAX_FPS;
            if (dt < milis) { SDL_Delay(milis - dt); }
            if (DO_DEBUG || 1) printf("FPS: %f\n", (dt < milis ? MAX_FPS : 1000.0 / (double)dt));
            startTime = SDL_GetTicks();
        }
    }
    return 0;
}
