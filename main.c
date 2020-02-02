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
const int NUM_BALLS = 1000;
float g = 0;
float eff = 1;
float vthresh = 0;

typedef struct Ball
{
    Collider* collide2d;
    float r;
    float cx;
    float cy;
    float vx;
    float vy;
    float m;
} Ball;

void makeBall(float r, float cx, float cy, float vx, float vy, float m, Ball* addr, Ball* buff)
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

void drawBall(Ball* ball, SDL_Renderer* ren)
{
    float cx = ball->cx;
    float cy = ball->cy;
    float r = ball->r;
    float oldx = cx + r;
    float oldy = cy;
    float step = 6.283185307 / 16;
    for (float i = 0; i < 6.283185307; i += step)
    {
        float newx = cx + r * SDL_cosf(i);
        float newy = cy + r * SDL_sinf(i);
        SDL_RenderDrawLine(ren, newx, newy, oldx, oldy);
        oldx = newx;
        oldy = newy;
    }
}

/* Ball a will be mutated, and should not point to the buffer, b is in the buffer */
void ballCollide(Ball* a, Ball* b)
{
    float deltax = a->cx - b->cx;
    float deltay = a->cy - b->cy;
    float dsquared = deltax * deltax + deltay * deltay;
    float rsum = a->r + b->r;
    float deltavx = a->vx - b->vx;
    float deltavy = a->vy - b->vy;
    float vdotdisp = (deltax * deltavx + deltay * deltavy);
    // only handle the collision if they are touching and moving towards each other
    if ((dsquared <= rsum * rsum) && (vdotdisp < 0))
    {
        float mul = (2 * b->m * vdotdisp) / ((a->m + b->m) * dsquared);
        a->vx -= mul * deltax * eff;
        a->vy -= mul * deltay * eff;
    }
}

float randi(float a, float b)
{
    return ((float)rand() / RAND_MAX) * (b - a) + a;
}

Ball* objects;
Ball* buf;
Collider** ret;
int curr_update = 0;
int htable_use = 0;

int main(int argc, char* argv[])
{
    if (SDL_Init(SDL_INIT_EVERYTHING)) { printf("error."); return 0; }
    //SDL_Window* win; = SDL_CreateWindow("Collisions test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 600, 600, 0);
    SDL_Window* win;
    SDL_Surface* surf;
    SDL_Renderer* ren;

    SDL_CreateWindowAndRenderer(600, 600, 0, &win, &ren);

    srand(18698238);

    // make the grid so grid coords equal screen coords
    mainGrid = makeGrid(120, 120, 5.0);
    // allocate the sprite memory
    objects = calloc(NUM_BALLS, sizeof(Ball));
    buf = calloc(NUM_BALLS, sizeof(Ball));
    ret = calloc(NUM_BALLS, sizeof(Collider*));
    // initialize the hash table
    hashTable* hTable = malloc(sizeof(hashTable));
    hTable->items = calloc(NUM_BALLS, sizeof(hashItem));
    hTable->len = NUM_BALLS;

    for (int i = 0; i < NUM_BALLS; i++)
    {
        // try to add a sprite, repeat untill there is an available slot
        float r = randi(4, 5);
        makeBall(r, randi(r, 600 - r), randi(r, 600 - r), randi(-10, 10), randi(-10, 10), r * r, &objects[i], &buf[i]);
//        while (queryBox(mainGrid, objects[i].collide2d->hitbox, ret, hTable, NUM_BALLS, curr_update, htable_use) > 0)
//        {
//            free(objects[i].collide2d->memPool);
//            free(objects[i].collide2d->hitbox);
//            free(objects[i].collide2d);
//            // reset the hash table
//            hTable->num = 0;
//            htable_use++;
//
//            r = randi(5, 10);
//            makeBall(r, randi(r, 600 - r), randi(r, 600 - r), randi(-10, 10), randi(-10, 10), r * r, &objects[i], &buf[i]);
//        }
        insertToGrid(mainGrid, objects[i].collide2d, curr_update);
    }

    SDL_Rect* bb = malloc(sizeof(SDL_Rect));
    SDL_Event e;
    while (1)
    {
        SDL_PollEvent(&e);
        if (e.type == SDL_QUIT) { break; }
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
        if (curr_update % 10 == 0) { SDL_RenderClear(ren); }
        float deltat = 0.01;
        // copy to the buffer
        memcpy(buf, objects, NUM_BALLS * sizeof(Ball));
        SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
        for (int i = 0; i < NUM_BALLS; i++)
        {

            objects[i].vy += g * deltat;
            hTable->num = 0;
            htable_use++;

            insertHashItem(hTable, (intptr_t)(buf + i), htable_use); // exclude itself



            int nresults = queryBox(mainGrid, objects[i].collide2d->hitbox, ret, hTable, NUM_BALLS, curr_update, htable_use);
            //printf("%d ", nresults);
            // now, handle the physics for each collision

            for (int j = 0; j < nresults; j++)
            {
                ballCollide(&objects[i], (Ball*)ret[j]->sprite);
                if (1)
                {
                    bb->x = ret[j]->hitbox.X0;
                    bb->y = ret[j]->hitbox.Y0;
                    bb->w = ret[j]->hitbox.X1 - bb->x;
                    bb->h = ret[j]->hitbox.Y1 - bb->y;
                }

                //SDL_RenderDrawRect(ren, bb);
            }

            int w, h;
            Box hit = objects[i].collide2d->hitbox;
            SDL_GetWindowSize(win, &w, &h);
            if (hit.X0 < 1 && objects[i].vx < 0) { objects[i].vx *= -eff; objects[i].vy *= eff; }
            if (hit.X1 > w && objects[i].vx > 0) { objects[i].vx *= -eff; objects[i].vy *= eff; }
            if (hit.Y0 < 1 && objects[i].vy < 0) { objects[i].vy *= -eff; objects[i].vx *= eff; }
            if (hit.Y1 > h && objects[i].vy > 0)
            {
                objects[i].vy *= -eff;
                objects[i].vx *= eff;
                //if (fabs(objects[i].vx) < vthresh) { objects[i].vx = 0; }
                //if (fabs(objects[i].vy) < vthresh) { objects[i].vy = 0; }
            }
            //if (objects[i].vx * objects[i].vx + objects[i].vy * objects[i].vy < vthresh * vthresh && objects[i].vy > 0) { objects[i].vx = 0; }
        }
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 128);
        curr_update++;

        for (int i = 0; i < NUM_BALLS; i++)
        {
            // before we do anything, we must check that our objects have not been corrupted

            if (objects[i].collide2d != buf[i].collide2d)
            {
                printf("Corruption detected at index %d", i);
                break;
            }

            // update the sprite
            float dx = objects[i].vx * deltat;
            float dy = objects[i].vy * deltat;

            if (1)
            {
                objects[i].collide2d->hitbox.X0 += dx;
                objects[i].collide2d->hitbox.Y0 += dy;
                objects[i].collide2d->hitbox.X1 += dx;
                objects[i].collide2d->hitbox.Y1 += dy;
                (objects + i)->cx += dx;
                (objects + i)->cy += dy;
                // draw the sprite to the buffer
            }
            objects[497].collide2d->memPool;
            if (curr_update == 3269 && i >= 497)
            {
                i += 1;
                i -= 1;
            }
            insertToGrid(mainGrid, objects[i].collide2d, curr_update);

            int r = (int)objects[i].r;
            bb->x = (int)objects[i].cx - r;
            bb->y = (int)objects[i].cy - r;
            bb->h = 2 * r;
            bb->w = 2 * r;
            SDL_RenderDrawRect(ren, bb);

            float energy = 0.5 * objects[i].m * (objects[i].vx * objects[i].vx + objects[i].vy * objects[i].vy);
            int sig = floor(255 / (1 + exp(-0.0005 * energy + 1)));
            //printf("%d ", sig);
            SDL_SetRenderDrawColor(ren, (Uint8)sig, 255 - (Uint8)sig, 0, 0);
            if (i == 498 || i == 497) { SDL_SetRenderDrawColor(ren, 255, 255, 255, 0); }
            if (curr_update % 10 == 0 ) drawBall(objects + i, ren);
        }
        //memset(hTable->items, 0, hTable->len * sizeof(hashItem));
        if (curr_update % 10 == 0) { SDL_RenderPresent(ren); SDL_Delay(10); }
    }
    return 0;
}
