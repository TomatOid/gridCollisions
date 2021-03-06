#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
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
        #define omp_init_lock(lck) 0
        #define _ESCAPE_OPENMP
    #endif
#endif
#ifndef CACHE_LINE_SIZE
    #define CACHE_LINE_SIZE 64
#endif
#include "AABB.h"
#include "Cells.h"
#include "Grid.h"
#define DO_LOGGING
//#define ISOTHERMAL_RESIZE
#define OUTPUT_CSV

Grid* mainGrid;
SDL_Texture* GridTex;
int NUM_BALLS = 500;
// acceleration due to gravity
double gravity_acceleration = 2;
// velocity loss per interaction, multiplicitive
double restitution_coefficient = 1;
double max_ball_size = 10;
int CYCLES_PER_FRAME = 3;
int DO_DEBUG = 0;
int DO_UNEQUAL = 0;
int OPACITY = 255;
int MAX_FPS = 60;
const int XRES = 960;
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
int ballCollide(Ball* a, const Ball* b)
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
        a->vx -= restitution_coefficient * mul * deltax;
        a->vy -= restitution_coefficient * mul * deltay;
        return 1;
    }
    else { return 0; }
}

uint32_t threadSafeXorShift(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
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
        gravity_acceleration = atof(argv[2]);
        max_ball_size = atof(argv[3]);
        // if there are more than the required arguments, scan for valid options
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
                    if (f >= 0 && f <= 1) { restitution_coefficient = f; }
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
    //printf("max_ball_size: %f\n", max_ball_size);
    //fflush(stdout);
    if (SDL_Init(SDL_INIT_EVERYTHING)) { printf("error."); return 0; }

    SDL_Window* win;
    SDL_Renderer* ren;

    SDL_CreateWindowAndRenderer(XRES, YRES, SDL_WINDOW_RESIZABLE, &win, &ren);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    int grid_cell_size = ceil(2.0 * max_ball_size);
    while (XRES % grid_cell_size) grid_cell_size++;
    //printf("grid_cell_size: %d\n", grid_cell_size);
    //fflush(stdout);

    // declare random seeds
    srand(18698238);
    static uint32_t randr_state;
    #pragma omp threadprivate(randr_state)

    // make the grid so grid coords equal screen coords
    mainGrid = makeGrid(1 + (int)XRES / grid_cell_size, 1 + (int)YRES / grid_cell_size, (double)grid_cell_size);
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
        #ifdef __unix__
        if (posix_memalign((void**)&hTable, CACHE_LINE_SIZE, sizeof(hashTable))) exit(1);
        if (posix_memalign((void**)&hTable->items, CACHE_LINE_SIZE, NUM_BALLS * sizeof(hashItem))) exit(1);
        #else
        hTable = calloc(1, sizeof(hashTable));
        hTable->items = calloc(NUM_BALLS, sizeof(hashItem));
        #endif
        hTable->len = NUM_BALLS;
    }
    for (int i = 0; i < NUM_BALLS; i++)
    {
	    // Initalize the sprites on the heap at random positions
	    // I should really position them so they don't overlap each other,
	    // but it'll be fine, right?
        double r = randi(max_ball_size * 1.0, max_ball_size);
        makeBall(r, randi(r, XRES - r), randi(r, YRES - r), randi(-100, 100), randi(-100, 100), 3.14 * r * r, &objects[i], &buf[i]);
        makeBallTex(&objects[i], ren);
        if (DO_UNEQUAL && !(objects[i].cx < XRES / 2)) { objects[i].vx *= 3; objects[i].vy *= 3; }
        insertToGrid(mainGrid, objects[i].collide2d, curr_update);
    }

    // Now make the grid texture for debugging
    GridTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, XRES, YRES);
    SDL_SetRenderTarget(ren, GridTex);
    SDL_SetRenderDrawColor(ren, 12, 32, 89, 255);
    SDL_Rect bb;
    bb.w = grid_cell_size;
    bb.h = grid_cell_size;
    for (bb.x = 0; bb.x < XRES; bb.x += grid_cell_size)
    {
        for (bb.y = 0; bb.y < YRES; bb.y += grid_cell_size)
        {
            SDL_RenderDrawRect(ren, &bb);
        }
    }
    SDL_SetRenderTarget(ren, NULL);

	// The minimum amount of time each cycle can take on average
    double deltat = 1.0 / (double)(MAX_FPS * CYCLES_PER_FRAME);
    SDL_Event e;
    SDL_Rect winbox = { 0, 0, XRES, YRES };
    SDL_SetWindowMaximumSize(win, XRES, YRES);
    int window_width, window_height, last_window_width, last_window_height;
    SDL_GetWindowPosition(win, &window_width, &window_height);
    double right_edge_velocity = 0;
    double bottom_edge_velocity = 0;
    double accumualted_impulse = 0;
    // Setup window pos variables
    int winposX0, winposY0, winposX1, winposY1, winposX2, winposY2;
    SDL_GetWindowPosition(win, &winposX0, &winposY0);
    // set them all to be the same
    winposX1 = winposX0;
    winposY1 = winposY0;
    winposX2 = winposX1;
    winposX2 = winposY1;
    startTime = SDL_GetTicks();
#ifdef OUTPUT_CSV
    puts("left energy (rms), right energy (rms), total energy, average kinetic energy, average pressure, window area, P * V");
#endif
    while (1)
    {
        // Check to see if the user is trying to close the program, to prevent hanging
		while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
                case SDL_QUIT:
                    exit(0);
                case SDL_WINDOWEVENT:
                {
                    if (e.window.event == SDL_WINDOWEVENT_RESIZED)
                    {
                        winbox.w = e.window.data1;
                        winbox.h = e.window.data2;
                    } 
                }
            }
        }
        // clear the background
        if ((curr_update + 1) % CYCLES_PER_FRAME == 0) { SDL_SetRenderDrawColor(ren, 0, 0, 0, OPACITY); SDL_RenderFillRect(ren, &winbox); }

        // copy to the buffer, which is acts as a static backup used when checking collisions,
        // and is pointed to in each collider
        memcpy(buf, objects, NUM_BALLS * sizeof(Ball));
        
        // now, if debugging, and it is a draw cycle, draw the grid
        if (DO_DEBUG && (curr_update + 1) % CYCLES_PER_FRAME == 0)
        {
            SDL_RenderCopy(ren, GridTex, NULL, NULL);
        }

        // loop to adjust velocities according to window movement
        if ((curr_update + 1) % CYCLES_PER_FRAME == 0)
        {
            // record the window sizes
            last_window_width = window_width;
            last_window_height = window_height;
            SDL_GetWindowSize(win, &window_width, &window_height);
#ifndef ISOTHERMAL_RESIZE
            right_edge_velocity = -(double)(last_window_width - window_width) / (deltat * CYCLES_PER_FRAME);
            bottom_edge_velocity = -(double)(last_window_height - window_height) / (deltat * CYCLES_PER_FRAME);
#endif
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
            int nresults = queryBox(mainGrid, objects[i].collide2d->hitbox, ret, hTable, NUM_BALLS, curr_update, htable_use, threadSafeXorShift(&randr_state) % 2, 0);

            // now, handle the physics for each collision by looping over returned values
            int r = threadSafeXorShift(&randr_state) % 2;
            for (int j = 0; j < nresults; j++)
            {
                // set object[i]'s velocity according to elastic collision
                if (r) ballCollide(&objects[i], ret[j]->sprite);
                else ballCollide(&objects[i], ret[nresults - j - 1]->sprite);
                #ifndef _OPENMP
                if (DO_DEBUG && (curr_update + 1) % CYCLES_PER_FRAME == 0)
                {
                    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                    SDL_RenderDrawLine(ren, objects[i].cx, objects[i].cy, ((Ball*)ret[j]->sprite)->cx, ((Ball*)ret[j]->sprite)->cy);
                }
                #endif
            }

            // bounce off the walls
            Box hit = objects[i].collide2d->hitbox;
            if (hit.X0 < 0 && objects[i].vx < 0) 
            { 
                accumualted_impulse += abs(objects[i].m * objects[i].vx);
                objects[i].vx = objects[i].vx * -restitution_coefficient; //- (1 + restitution_coefficient) * right_edge_velocity;
                objects[i].vy *= restitution_coefficient;  
                accumualted_impulse += abs(objects[i].m * objects[i].vx);
            }
            if (hit.X1 > window_width && objects[i].vx - right_edge_velocity > 0) 
            { 
                accumualted_impulse += abs(objects[i].m * objects[i].vx);
                objects[i].vx = objects[i].vx * -restitution_coefficient + (1 + restitution_coefficient) * right_edge_velocity;
                objects[i].vy *= restitution_coefficient; 
                accumualted_impulse += abs(objects[i].m * objects[i].vx);
            }
            if (hit.Y0 < 0 && objects[i].vy < 0) 
            { 
                accumualted_impulse += abs(objects[i].m * objects[i].vy);
                objects[i].vy = objects[i].vy * -restitution_coefficient; //- (1 + restitution_coefficient) * bottom_edge_velocity;
                objects[i].vx *= restitution_coefficient;  
                accumualted_impulse += abs(objects[i].m * objects[i].vy);
            }
            if (hit.Y1 > window_height && objects[i].vy - bottom_edge_velocity > 0) 
            {
                accumualted_impulse += abs(objects[i].m * objects[i].vy);
                objects[i].vy = objects[i].vy * -restitution_coefficient + (1 + restitution_coefficient) * bottom_edge_velocity;
                objects[i].vx *= restitution_coefficient; objects[i].isColliding = 1;
                accumualted_impulse += abs(objects[i].m * objects[i].vy);
            }
            // do gravity if not colliding
            if (!objects[i].isColliding) { objects[i].vy += gravity_acceleration * deltat; }
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
                double energy = 0.0001 * objects[i].m / (max_ball_size * max_ball_size) * (objects[i].vx * objects[i].vx + objects[i].vy * objects[i].vy);
                // using a squishification function to limit sig to a range of 0 - 1
                float sig = 1 - 1 / (1 + energy + energy * energy / 2);
                // now adjust the hue according to sig
                float colormin = 46;
                float colormax = 234;
                int r = (int)((sig > 0.5) ? colormax : colormin + (colormax - colormin) * 2 * sig);
                int g = (int)((sig < 0.5) ? colormax : colormin - (colormax - colormin) * 2 * (sig - 1));

                // draw the ball
                SDL_SetTextureColorMod(objects[i].texture, r, g, 36);
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
            if (DO_DEBUG && curr_update % (CYCLES_PER_FRAME * MAX_FPS) == 0) printf("\x1b[2JFPS: %f\n", (dt < milis ? MAX_FPS : 1000.0 / (double)dt));
            startTime = SDL_GetTicks();
        }

        #ifdef DO_LOGGING
        if ((curr_update % (CYCLES_PER_FRAME * MAX_FPS)) == 0)
        {
            double left_energy = 0;
            int left_count = 0;
            double right_energy = 0;
            int right_count = 0;
            double total_energy = 0;
            for (int i = 0; i < NUM_BALLS; i++)
            {
                double particle_energy = objects[i].m * (objects[i].vx * objects[i].vx + objects[i].vy * objects[i].vy + objects[i].cy);
                total_energy += particle_energy;
                if (objects[i].cx < window_width / 2)
                {
                    left_energy += particle_energy;
                    left_count++;
                }
                else
                {
                    right_energy += particle_energy;
                    right_count++;
                }
            }
            /*
            double left_chi = 0;
            double left_mean = left_energy * left_energy / left_count * left_count;
            double right_chi = 0;
            double right_mean = right_energy / right_count;
            for (int i = 0; i < NUM_BALLS; i++)
            {
                double particle_energy = objects[i].m * (objects[i].vx * objects[i].vx + objects[i].vy * objects[i].vy);
                if (objects[i].cx < window_width / 2)
                {
                     left_chi += (left_mean - particle_energy) * (left_mean - particle_energy);
                }
                else
                {
                     right_chi += (right_mean - particle_energy) * (right_mean - particle_energy);
                }
            }
            */
            // delta t == 1
            double average_pressure = accumualted_impulse / (2 * (window_width + window_height)); 
            accumualted_impulse = 0;
#ifndef OUTPUT_CSV
            puts("--------------------------------------");
            printf("left energy (rms): %f\n", left_energy / left_count);
            //printf("left chi: %f\n", sqrt(left_chi / left_count));
            printf("right energy (rms): %f\n", right_energy / right_count);
            //printf("right chi: %f\n", sqrt(right_chi / right_count));
            printf("total energy: %f\n", total_energy);
            printf("average kinetic energy (rms): %f\n", (left_energy + right_energy) / NUM_BALLS);
            printf("average pressure: %f\n", average_pressure);
            printf("window area: %d\n", window_width * window_height);
            printf("P * V: %f\n", average_pressure * window_width * window_height);
#else 
            printf("%f, ", left_energy / left_count);
            //printf("%f, ", sqrt(left_chi / left_count));
            printf("%f, ", right_energy / right_count);
            //printf("%f, ", sqrt(right_chi / left_count));
            printf("%f, ", left_energy + right_energy);
            printf("%f, ", (left_energy + right_energy) / NUM_BALLS);
            printf("%f, ", average_pressure);
            printf("%d, ", window_width * window_height);
            printf("%f\n", average_pressure * window_width * window_height);
#endif
        }
        #endif
    }
    return 0;
}
