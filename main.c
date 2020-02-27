#include <stdlib.h>
#include <stdio.h>
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
double eff = 0.9;
double vthresh = 0;
double bSize = 10;
const int CYCLES_PER_FRAME = 2;
const int DO_DEBUG = 0;
const int DO_ARROWS = 0;
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
    double cosx = ball->vx / mag;
    double sinx = ball->vy / mag;
    double arrs = sqrt(mag);
    SDL_RenderDrawLine(ren, cx, cy, tailx, taily);
    // draw vector tail
    SDL_RenderDrawLine(ren, tailx, taily, tailx + (-arrs) * cosx - arrs * sinx, taily + (-arrs) * sinx + arrs * cosx);
    SDL_RenderDrawLine(ren, tailx, taily, tailx + (-arrs) * cosx + arrs * sinx, taily + (-arrs) * sinx - arrs * cosx);
}

/* Ball a will be mutated, and should not point to the buffer, b is in the buffer */
void ballCollide(Ball* a, Ball* b)
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
        a->vx -= mul * deltax * eff;
        a->vy -= mul * deltay * eff;
    }
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
int curr_update = 0;
int htable_use = 0;

int main(int argc, char* argv[])
{
    if (argc > 3)
    {
        // the first argument determines the number of sprites,
        // the second determines the gravity acceleration
        NUM_BALLS = atoi(argv[1]);
        g = atof(argv[2]);
        bSize = atof(argv[3]);
    }
    printf("bSize: %f\n", bSize);
    fflush(stdout);
    if (SDL_Init(SDL_INIT_EVERYTHING)) { printf("error."); return 0; }

    SDL_Window* win;
    SDL_Renderer* ren;

    SDL_CreateWindowAndRenderer(XRES, YRES, 0, &win, &ren);

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
    hTable->items = calloc(NUM_BALLS, sizeof(hashItem));
    hTable->len = NUM_BALLS;

    for (int i = 0; i < NUM_BALLS; i++)
    {
	    // Initalize the sprites on the heap at random positions
	    // I should really position them so they don't overlap each other,
	    // but it'll be fine, right?
        double r = randi(0.9 * bSize, bSize);
        makeBall(r, randi(r, XRES - r), randi(r, YRES - r), randi(-10, 10), randi(-10, 10), r, &objects[i], &buf[i]);
        if (objects[i].cx < XRES / 2) { objects[i].vx *= 2; objects[i].vy *= 2; }
        insertToGrid(mainGrid, objects[i].collide2d, curr_update);
    }
	// The
    double deltat = 0.1 / CYCLES_PER_FRAME;
    SDL_Rect* bb = malloc(sizeof(SDL_Rect));
    SDL_Event e;
    while (1)
    {
        startTime = SDL_GetTicks();
        // Check to see if the user is trying to close the program, to prevent hanging
		SDL_PollEvent(&e);
        if (e.type == SDL_QUIT) { break; }

        // clear the background
        if ((curr_update + 1) % CYCLES_PER_FRAME == 0) { SDL_SetRenderDrawColor(ren, 0, 0, 0, 0); SDL_RenderClear(ren); }

        // copy to the buffer, which is acts as a static backup used when checking collisions,
        // and is pointed to in each collider
        memcpy(buf, objects, NUM_BALLS * sizeof(Ball));
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
            int nresults = queryBox(mainGrid, objects[i].collide2d->hitbox, ret, hTable, NUM_BALLS, curr_update, htable_use);

            if (DO_DEBUG)
            {
                // print how many colliders were returned
                printf("%d ", nresults);
                fflush(stdout);
            }

            // now, handle the physics for each collision by looping over returned values
            for (int j = 0; j < nresults; j++)
            {
                // set object[i]'s velocity according to elastic collision
                ballCollide(&objects[i], ret[j]->sprite);

                if (DO_DEBUG && (curr_update + 1) % CYCLES_PER_FRAME == 0)
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
            if (hit.X0 < 0 && objects[i].vx < 0) { objects[i].vx *= -eff; objects[i].vy *= eff; objects[i].isColliding = 1; }
            if (hit.X1 > w && objects[i].vx > 0) { objects[i].vx *= -eff; objects[i].vy *= eff; objects[i].isColliding = 1; }
            if (hit.Y0 < 0 && objects[i].vy < 0) { objects[i].vy *= -eff; objects[i].vx *= eff; objects[i].isColliding = 1; }
            if (hit.Y1 > h && objects[i].vy > 0) { objects[i].vy *= -eff; objects[i].vx *= eff; objects[i].isColliding = 1; }
            // do gravity if not colliding
            if (!objects[i].isColliding) { objects[i].vy += g * deltat; }
        }

        curr_update++;

        for (int i = 0; i < NUM_BALLS; i++)
        {
            // before we do anything, we must check that our objects have not been corrupted

            if (objects[i].collide2d != buf[i].collide2d)
            {
                printf("Corruption detected at index %d", i);
                break;
            }

            double dx = objects[i].vx * deltat;
            double dy = objects[i].vy * deltat;

            // update the hitbox
            objects[i].collide2d->hitbox.X0 += dx;
            objects[i].collide2d->hitbox.Y0 += dy;
            objects[i].collide2d->hitbox.X1 += dx;
            objects[i].collide2d->hitbox.Y1 += dy;

            // update the circle center
            (objects + i)->cx += dx;
            (objects + i)->cy += dy;

            // re-insert this sprite into the grid
            insertToGrid(mainGrid, objects[i].collide2d, curr_update);

            // part of the purpose of this demo is to visualize
            // energy transfer using colors, where more red means
            // more energy, and more green means less energy
            double energy = 0.001 * objects[i].m * (objects[i].vx * objects[i].vx + objects[i].vy * objects[i].vy);
            // using the sigmoid function to limit sig to a range of 0 - 255
            int sig = floor(255 - 255 / (1 + energy + energy * energy / 2));

            SDL_SetRenderDrawColor(ren, (Uint8)(sig / 4) + 64 * 3, (Uint8)(sig / 4) + 64 * 3, (255 - sig) / 2, 0);

            // draw the ball
            if (curr_update % CYCLES_PER_FRAME == 0 ) { drawBall(objects + i, ren); }
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
            if (DO_DEBUG) printf("FPS: %f", 1000 / (double)(dt));
        }
    }
    return 0;
}
