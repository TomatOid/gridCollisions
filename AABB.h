#ifndef AABB_H_INCLUDED
#define AABB_H_INCLUDED

typedef struct Box
{
    float X0, Y0; // top left corner
    float X1, Y1; // bottom right corner
} Box;

int isBoxOverlap(Box* box0, Box* box1);
int isBoxOverlap0(Box box0, Box box1);
int isPtOverlap(float Pt, float X0, float X1);
Box* makeBox(float AX, float AY, float BX, float BY);
void translateBox(Box* B, int X, int Y);

#endif // AABB_H_INCLUDED
