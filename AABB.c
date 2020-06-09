#include <stdlib.h>
#include <math.h>
#include "AABB.h"

void translateBox(Box* B, int X, int Y)
{
    B->X0 += X;
    B->X1 += X;
    B->Y0 += Y;
    B->Y1 += Y;
}

int isPtOverlap(double Pt, double X0, double X1)
{
    return (X0 > Pt) ^ (X1 > Pt);
}

//int isBoxOverlap(Box* box0, Box* box1)
//{
//    // first, check for X overlap, then y
//    if (!box0) { return 0; }
//    return (isPtOverlap(box0->X0, box1->X0, box1->X1) | isPtOverlap(box1->X1, box0->X0, box0->X1)) & (isPtOverlap(box0->Y0, box1->Y0, box1->Y1) | isPtOverlap(box1->Y1, box1->Y0, box0->Y1));
//}

int isBoxOverlap(Box* box0, Box* box1)
{
    return fabs(box1->X1 + box1->X0 - box0->X1 + box0->X0) <= (box1->X1 + box1->X0 + box0->X1 - box0->X0) && fabs(box1->Y1 + box1->Y0 - box0->Y1 + box0->Y0) <= (box1->Y1 + box1->Y0 + box0->Y1 - box0->Y0);
}

int isBoxOverlap0(Box box0, Box box1)
{
    // first, check for X overlap, then y
    return (isPtOverlap(box0.X0, box1.X0, box1.X1) | isPtOverlap(box1.X1, box0.X0, box0.X1)) & (isPtOverlap(box0.Y0, box1.Y0, box1.Y1) | isPtOverlap(box1.Y1, box1.Y0, box0.Y1));
}

Box* makeBox(double AX, double AY, double BX, double BY)
{
    Box* res = malloc(sizeof(Box));
    if (!res) { return res; }
    res->X0 = AX;
    res->X1 = BX;
    res->Y0 = AY;
    res->Y1 = BY;
    return res;
}
