#define _CRT_SECURE_NO_DEPRECATE
#include "raylib.h"
#include "lxml.h"
#include <stdlib.h>

typedef struct {
    double latitude;
    double longitude;
} LatLon64;

typedef struct {
    LatLon64* points;
    int count;
} Way;

typedef struct {
    Way* ways;
    int count;
} CountryBorder;

void setDetailAmount(float zoom, int* detailDivideCoeff);

CountryBorder LoadGeoDataFromXML(const char* filePath) {
    CountryBorder turkiyeBorder = { NULL, 0 };
    XMLDocument doc;

    if (!XMLDocument_load(&doc, filePath)) {
        fprintf(stderr, "Failed to load XML file\n");
        return turkiyeBorder;
    }

    XMLNode* osm = XMLNodeList_at(&doc.root->children, 0);

    // Find all way elements
    XMLNodeList* ways = XMLNode_children(osm, "way");
    if (ways->size == 0) {
        fprintf(stderr, "No 'way' elements found\n");
        XMLDocument_free(&doc);
        return turkiyeBorder;
    }

    turkiyeBorder.count = ways->size;
    turkiyeBorder.ways = (Way*)malloc(turkiyeBorder.count * sizeof(Way));

    // Fill turkiye struct
    for (int i = 0; i < ways->size; i++) {
        XMLNode* way = XMLNodeList_at(ways, i);
        XMLNodeList* nodes = XMLNode_children(way, "node");

        turkiyeBorder.ways[i].count = nodes->size;
        turkiyeBorder.ways[i].points = (LatLon64*)malloc(nodes->size * sizeof(LatLon64));

        for (int j = 0; j < nodes->size; j++) {
            XMLNode* node = XMLNodeList_at(nodes, j);
            turkiyeBorder.ways[i].points[j].latitude = atof(XMLNode_attr_val(node, "lat"));
            turkiyeBorder.ways[i].points[j].longitude = atof(XMLNode_attr_val(node, "lon"));
        }

        XMLNodeList_free(nodes);
    }

    XMLNodeList_free(ways);
    XMLDocument_free(&doc);
    return turkiyeBorder;
}

// Function to convert geographic coordinates to screen coordinates
Vector2 GeoToScreen(LatLon64 point, float screenWidth, float screenHeight, Vector2 offset, float zoom) {
    float x = ((point.longitude + 180.0) * (screenWidth / 360.0) - offset.x) * zoom;
    float y = ((90.0 - point.latitude) * (screenHeight / 180.0) - offset.y) * zoom;
    return (Vector2) { x, y };
}

void DrawWorldBoundaries(float screenWidth, float screenHeight, Vector2 offset, float zoom) {

    Vector2 screenTop = { 0.0f, 0.0f };
    Vector2 screenBottom = { 0.0f, 0.0f };
    Vector2 screenLeft = { 0.0f, 0.0f };
    Vector2 screenRight = { 0.0f, 0.0f };
    LatLon64 lonGridTop = { 90.0f, 0.0f };
    LatLon64 lonGridBottom = { -90.0f, 0.0f };
    LatLon64 latGridLeft = { 0.0f, -180.0f };
    LatLon64 latGridRight = { 0.0f, 180.0f };

    Color gridColor = DARKGRAY;

    if (zoom < 10) {
        gridColor = (Color){ 80, 80, 80, zoom * 25 };
    }

    for (int lon = -180; lon <= 180; lon += 1) {
        lonGridTop.longitude = lon;
        lonGridBottom.longitude = lon;
        screenTop = GeoToScreen(lonGridTop, screenWidth, screenHeight, offset, zoom);
        screenBottom = GeoToScreen(lonGridBottom, screenWidth, screenHeight, offset, zoom);
        DrawLineV(screenTop, screenBottom, gridColor);
    }

    for (int lat = -90; lat <= 90; lat += 1) {
        latGridLeft.latitude = lat;
        latGridRight.latitude = lat;
        screenLeft = GeoToScreen(latGridLeft, screenWidth, screenHeight, offset, zoom);
        screenRight = GeoToScreen(latGridRight, screenWidth, screenHeight, offset, zoom);
        DrawLineV(screenLeft, screenRight, gridColor);
    }

    LatLon64 topLeft = { 90.0, -180.0 };
    LatLon64 topRight = { 90.0, 180.0 };
    LatLon64 bottomLeft = { -90.0, -180.0 };
    LatLon64 bottomRight = { -90.0, 180.0 };

    Vector2 screenTopLeft = GeoToScreen(topLeft, screenWidth, screenHeight, offset, zoom);
    Vector2 screenTopRight = GeoToScreen(topRight, screenWidth, screenHeight, offset, zoom);
    Vector2 screenBottomLeft = GeoToScreen(bottomLeft, screenWidth, screenHeight, offset, zoom);
    Vector2 screenBottomRight = GeoToScreen(bottomRight, screenWidth, screenHeight, offset, zoom);

    DrawLineV(screenTopLeft, screenTopRight, RED);
    DrawLineV(screenTopRight, screenBottomRight, RED);
    DrawLineV(screenBottomRight, screenBottomLeft, RED);
    DrawLineV(screenBottomLeft, screenTopLeft, RED);
}

void DrawCountryBoundaries(CountryBorder* shape,float screenWidth, float screenHeight, Vector2 offset, float zoom, int* totalLineCount, Color color) {

    int detailDivideCoeff;
    setDetailAmount(zoom, &detailDivideCoeff);
    
    // Draw each way
    for (int i = 0; i < shape->count; i++) {
        for (int j = 0; j < shape->ways[i].count - 1; j += detailDivideCoeff) {

            if (j + detailDivideCoeff >= shape->ways[i].count) {
                Vector2 start = GeoToScreen(shape->ways[i].points[j], screenWidth, screenHeight, offset, zoom);
                Vector2 end = GeoToScreen(shape->ways[i].points[shape->ways[i].count - 1], screenWidth, screenHeight, offset, zoom);
                if (((start.x >= 0 && start.x <= screenWidth) && (start.y >= 0 && start.y <= screenHeight)) ||
                    ((end.x >= 0 && end.x <= screenWidth) && (end.y >= 0 && end.y <= screenHeight))) {
                    DrawLineV(start, end, color);
                    *totalLineCount += 1;
                }
            }
            else {
                Vector2 start = GeoToScreen(shape->ways[i].points[j], screenWidth, screenHeight, offset, zoom);
                Vector2 end = GeoToScreen(shape->ways[i].points[(j + detailDivideCoeff)], screenWidth, screenHeight, offset, zoom);
                if (((start.x >= 0 && start.x <= screenWidth) && (start.y >= 0 && start.y <= screenHeight)) ||
                    ((end.x >= 0 && end.x <= screenWidth) && (end.y >= 0 && end.y <= screenHeight))) {
                    DrawLineV(start, end, color);
                    *totalLineCount += 1;
                }   
            }
        }
    }
}

void freeShape(CountryBorder* shape) {
    for (int i = 0; i < shape->count; i++) {
        free(shape->ways[i].points);  // Free the allocated memory for each way's points
    }
    free(shape->ways);  // Free the allocated memory for the ways array
}

void setDetailAmount(float zoom, int* detailDivideCoeff) {
    if (zoom <= 1.0f) {
        *detailDivideCoeff = 100;
    }
    else if (zoom <= 5.0f) {
        *detailDivideCoeff = 50;
    }
    else if (zoom <= 10.0f) {
        *detailDivideCoeff = 20;
    }
    else if (zoom <= 20.0f) {
        *detailDivideCoeff = 10;
    }
    else if (zoom <= 50.0f) {
        *detailDivideCoeff = 5;
    }
    else if (zoom <= 100.0f) {
        *detailDivideCoeff = 2;
    }
    else  {
        *detailDivideCoeff = 1;
    }
}

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void) {
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 1200;
    const int screenHeight = 675;
    int totalLineCount = 0;

    InitWindow(screenWidth, screenHeight, "Vector Map");

    //SetTargetFPS(60);  
    //--------------------------------------------------------------------------------------

    // Load geographic vector data from XML file
    CountryBorder shape = LoadGeoDataFromXML("turkey_border1.xml");
    CountryBorder shape1 = LoadGeoDataFromXML("italy_border1.xml");
    CountryBorder shape2 = LoadGeoDataFromXML("greece_border1.xml");
    CountryBorder shape3 = LoadGeoDataFromXML("bulgaria_border1.xml");
    CountryBorder shape4 = LoadGeoDataFromXML("cyprus_border1.xml");

    // Initialize pan and zoom
    Vector2 offset = { 0.0f, 0.0f };
    float zoom = 1.0f;

    // Variables for mouse dragging
    bool isDragging = false;
    Vector2 lastMousePosition = { 0.0f, 0.0f };
    Vector2 currentMousePosition = { 0.0f, 0.0f };
    Vector2 delta = { 0.0f, 0.0f };

    // Main game loop
    while (!WindowShouldClose()) {  // Detect window close button or ESC key
        totalLineCount = 0;
        // Update
        //----------------------------------------------------------------------------------
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isDragging = true;
            lastMousePosition = GetMousePosition();
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            isDragging = false;
        }
        if (isDragging) {
            currentMousePosition = GetMousePosition();
            delta.x = currentMousePosition.x - lastMousePosition.x;
            delta.y = currentMousePosition.y - lastMousePosition.y;
            lastMousePosition = currentMousePosition;

            offset.x -= delta.x / zoom;
            offset.y -= delta.y / zoom;
        }

        if (GetMouseWheelMove() > 0) zoom *= 1.1f;
        if (GetMouseWheelMove() < 0) zoom /= 1.1f;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(BLACK);

        DrawWorldBoundaries(screenWidth, screenHeight, offset, zoom);
        //printf("offset.x = %f\n", offset.x);
        //printf("offset.y = %f\n", offset.y);

        DrawCountryBoundaries(&shape1, screenWidth, screenHeight, offset, zoom, &totalLineCount, GREEN);
        DrawCountryBoundaries(&shape2, screenWidth, screenHeight, offset, zoom, &totalLineCount, BLUE);
        DrawCountryBoundaries(&shape3, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawCountryBoundaries(&shape4, screenWidth, screenHeight, offset, zoom, &totalLineCount, RAYWHITE);
        DrawCountryBoundaries(&shape, screenWidth, screenHeight, offset, zoom, &totalLineCount, RED);

        printf("Total line count is: %d\n", totalLineCount);

        DrawFPS(10, 10);
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    freeShape(&shape);
    freeShape(&shape1);
    freeShape(&shape2);
    freeShape(&shape3);
    freeShape(&shape4);
    CloseWindow();     // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
