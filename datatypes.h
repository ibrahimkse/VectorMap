#ifndef DATATYPES_H
#define DATATYPES_H

#include <stdint.h>

typedef struct {
    double latitude;
    double longitude;
} LatLon64;

/*typedef struct
{
    Vector2 leftBot;
    Vector2 rightTop;
    float rotation;
}MBR;
*/
typedef struct {
    LatLon64* points;
    int32_t count;
    int32_t id; //? 
    //MBR mbr;
} Way;

typedef struct {
    Way* ways;
    int32_t count;
} Shape;

#endif // !DATATYPES_H

