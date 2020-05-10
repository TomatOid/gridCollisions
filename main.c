#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_image.h>
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
double tick_prob = 0.05;
int CYCLES_PER_FRAME = 2;
int DO_DEBUG = 0;
int DO_ARROWS = 0;
int DO_UNEQUAL = 0;
int OPACITY = 255;
const int XRES = 720;
const int YRES = 720;

typedef struct Ball
{
    Collider* collide2d;
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

/* TODO: Fix this garbage. */
void drawBall(Ball* ball, SDL_Renderer* ren)
{
    double cx = ball->cx;
    double cy = ball->cy;
    double r = ball->r;
    double oldx = cx + r;
    double oldy = cy;
    double step = 6.283185307 / 12;
    for (double i = 0; i <= 6.283185307; i += step)
    {
        double newx = cx + r * SDL_cos(i);
        double newy = cy + r * SDL_sin(i);
        SDL_RenderDrawLine(ren, newx, newy, oldx, oldy);
        oldx = newx;
        oldy = newy;
    }
    if (!DO_ARROWS) return;
    // draw line for vector
    SDL_SetRenderDrawColor(ren, 255, 10, 10, 128);
    double tailx = cx + ball->vx;
    double taily = cy + ball->vy;
    double mag = sqrt(ball->vx * ball->vx + ball->vy * ball->vy);
    if (mag == 0.0f) return;
    double cosx = ball->vx / mag;
    double sinx = ball->vy / mag;
    double arrs = sqrt(mag);
    SDL_RenderDrawLine(ren, cx, cy, tailx, taily);
    // draw vector tail
    SDL_RenderDrawLine(ren, tailx, taily, tailx + (-arrs) * cosx - arrs * sinx, taily + (-arrs) * sinx + arrs * cosx);
    SDL_RenderDrawLine(ren, tailx, taily, tailx + (-arrs) * cosx + arrs * sinx, taily + (-arrs) * sinx - arrs * cosx);
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
Collider** ret;
Uint32 startTime = 0;
Uint32 endTime = 0;
uint32_t curr_update = 0;
uint32_t htable_use = 0;

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
                if (strcmp(argv[i], "-a") == 0) { DO_ARROWS = 1; }
                else if (strcmp(argv[i], "-d") == 0) { DO_DEBUG = 1; }
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
        " -a        Adds velocity arrows\n"
        " -d        Super secret debug mode\n"
        " -u        Spawn particles unbalenced with more velocity on one side\n"
        " -c[int]   Physics cycles per frame, must be nonzero\n"
        " -e[float] Coefficient of restitution / efficiency, must be on [0, 1]\n"
        " -o[int]   Clearing opacity, lower values create trails, must be between 0 and 255\n",
        argv[0]);
        exit(1);
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

    // make the grid so grid coords equal screen coords
    mainGrid = makeGrid(1 + (int)XRES / csize, 1 + (int)YRES / csize, (double)csize);
    // allocate the sprite memory, using calloc because debuging has made me paranoid
    objects = calloc(NUM_BALLS, sizeof(Ball));
    buf = calloc(NUM_BALLS, sizeof(Ball));
    ret = calloc(NUM_BALLS, sizeof(Collider*));
    // initialize the hash table
    hashTable* hTable = malloc(sizeof(hashTable));
    hTable->items = calloc(NUM_BALLS * 4 + 1, sizeof(hashItem));
    hTable->len = NUM_BALLS * 4 + 1;

    for (int i = 0; i < NUM_BALLS; i++)
    {
	    // Initalize the sprites on the heap at random positions
	    // I should really position them so they don't overlap each other,
	    // but it'll be fine, right?
        double r = randi(0.9 * bSize, bSize);
        makeBall(r, randi(r, XRES - r), randi(r, YRES - r), randi(-10, 10), randi(-10, 10), r, &objects[i], &buf[i]);
        if (DO_UNEQUAL && (objects[i].cx < XRES / 2)) { objects[i].vx *= 3; objects[i].vy *= 3; }
        insertToGrid(mainGrid, objects[i].collide2d, curr_update);
    }
	// The
    double deltat = 0.1 / CYCLES_PER_FRAME;
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
            SDL_SetRenderDrawColor(ren, 12, 32, 89, 0);
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
            int nresults = queryBox(mainGrid, objects[i].collide2d->hitbox, ret, hTable, NUM_BALLS, curr_update, htable_use, rand() % 2, 0);

            if (DO_DEBUG && 0)
            {
                // print how many colliders were returned
                printf("%d ", nresults);
                fflush(stdout);
            }

            // now, handle the physics for each collision by looping over returned values
            int r = rand() % 2;
            for (int j = 0; j < nresults; j++)
            {
                // set object[i]'s velocity according to elastic collision
                if (r) ballCollide(&objects[i], ret[j]->sprite);
                else ballCollide(&objects[i], ret[nresults - j - 1]->sprite);
                if (DO_DEBUG)
                {
                    SDL_SetRenderDrawColor(ren, 255, 255, 255, 0);
                    SDL_RenderDrawLine(ren, objects[i].cx, objects[i].cy, ((Ball*)ret[j]->sprite)->cx, ((Ball*)ret[j]->sprite)->cy);
                }
                if (DO_DEBUG && (curr_update + 1) % CYCLES_PER_FRAME == 0 && 0)
                {
                    // draw a bounding box around the sprite
                    SDL_SetRenderDrawColor(ren, 255, 255, 255, 0);
                    bb->x = ret[j]->hitbox.X0;
                    bb->y = ret[j]->hitbox.Y0;
                    bb->w = ret[j]->hitbox.X1 - bb->x;
                    bb->h = ret[j]->hitbox.Y1 - bb->y;
                    SDL_RenderDrawRect(ren, bb);
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
            if (curr_update % CYCLES_PER_FRAME == 0 )
            {
                // part of the purpose of this demo is to visualize
                // energy transfer using colors, where more red means
                // more energy, and more green means less energy
                double energy = 0.01 * objects[i].m / bSize * (objects[i].vx * objects[i].vx + objects[i].vy * objects[i].vy);
                // using a squishification function to limit sig to a range of 0 - 1
                float sig = 1 - 1 / (1 + energy + energy * energy / 2);
                // now adjust the hue according to sig
                float colormin = 46;
                float colormax = 234;
                int r = (int)((sig > 0.5) ? colormax : colormin + (colormax - colormin) * 2 * sig);
                int g = (int)((sig < 0.5) ? colormax : colormin - (colormax - colormin) * 2 * (sig - 1));

                SDL_SetRenderDrawColor(ren, r, g, 26, 255);

                // draw the ball
                drawBall(objects + i, ren);
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
            if (dt < 20) { SDL_Delay(20 - dt); }
            if (DO_DEBUG) printf("FPS: %f\n", 1000 / (double)(dt));
            startTime = SDL_GetTicks();
        }
    }
    return 0;
}
