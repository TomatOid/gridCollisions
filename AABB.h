#ifndef AABB_H_INCLUDED
#define AABB_H_INCLUDED

typedef struct Box
{
    double X0, Y0; // top left corner
    double X1, Y1; // bottom right corner
} Box;

int isBoxOverlap(Box* box0, Box* box1);
int isBoxOverlap0(Box box0, Box box1);
int isPtOverlap(double Pt, double X0, double X1);
Box* makeBox(double AX, double AY, double BX, double BY);
void translateBox(Box* B, int X, int Y);

#endif // AABB_H_INCLUDED
