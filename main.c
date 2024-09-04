#define _CRT_SECURE_NO_DEPRECATE
#include "raylib.h"
#include "lxml.h"
#include "rtree.h"
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <math.h>

#include "datatypes.h"

void setDetailAmount(float zoom, int* detailDivideCoeff);
struct Node* readTreeNode(FILE* file, struct Rtree* tree);

Shape LoadGeoDataFromXML(const char* filePath) {
    Shape shapeBorder = { NULL, 0 };
    XMLDocument doc;
    int totalNodeCount = 0;

    if (!XMLDocument_load(&doc, filePath)) {
        fprintf(stderr, "Failed to load XML file\n");
        return shapeBorder;
    }

    XMLNode* osm = XMLNodeList_at(&doc.root->children, 0);

    // Find all way elements
    XMLNodeList* ways = XMLNode_children(osm, "way");
    if (ways->size == 0) {
        fprintf(stderr, "No 'way' elements found\n");
        XMLDocument_free(&doc);
        return shapeBorder;
    }

    shapeBorder.count = ways->size;
    shapeBorder.ways = (Way*)malloc(shapeBorder.count * sizeof(Way));

    // Fill shape struct
    for (int i = 0; i < ways->size; i++) {
        XMLNode* way = XMLNodeList_at(ways, i);
        XMLNodeList* nodes = XMLNode_children(way, "node");

        shapeBorder.ways[i].count = nodes->size;
        shapeBorder.ways[i].points = (LatLon64*)malloc(nodes->size * sizeof(LatLon64));

        for (int j = 0; j < nodes->size; j++) {
            XMLNode* node = XMLNodeList_at(nodes, j);
            shapeBorder.ways[i].points[j].latitude = atof(XMLNode_attr_val(node, "lat"));
            shapeBorder.ways[i].points[j].longitude = atof(XMLNode_attr_val(node, "lon"));
            totalNodeCount++;
        }

        XMLNodeList_free(nodes);
    }

    printf("total node count: %d\n", totalNodeCount);
    XMLNodeList_free(ways);
    XMLDocument_free(&doc);
    return shapeBorder;
}

struct Rect convertWayToRect(Way* way) {
    struct Rect mbr = { DBL_MAX, DBL_MAX, -DBL_MAX, -DBL_MAX };
    for (int i = 0; i < way->count; i++) {
        if (way->points[i].latitude < mbr.min[1]) mbr.min[1] = way->points[i].latitude;
        if (way->points[i].longitude < mbr.min[0]) mbr.min[0] = way->points[i].longitude;
        if (way->points[i].latitude > mbr.max[1]) mbr.max[1] = way->points[i].latitude;
        if (way->points[i].longitude > mbr.max[0]) mbr.max[0] = way->points[i].longitude;
    }
    return mbr;
}

void writeBinaryFile(const char* filename, Shape* shape) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("File opening failed");
        return;
    }

    // Write the way count in the Shape
    fwrite(&shape->count, sizeof(int32_t), 1, file);
    // Write the data for all ways
    for (int i = 0; i < shape->count; i++) {
        // Write the node count for the way
        fwrite(&shape->ways[i].count, sizeof(int32_t), 1, file);
        // Write LatLon64 array
        fwrite(shape->ways[i].points, sizeof(LatLon64), shape->ways[i].count, file);
    }

    fclose(file);
}

void readBinaryFile(const char* filename, Shape* shape) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("File opening failed");
        return;
    }

    fread(&shape->count, sizeof(int), 1, file);
    shape->ways = (Way*)malloc(shape->count * sizeof(Way));
    for (int32_t i = 0; i < shape->count; i++) {
        fread(&shape->ways[i].count, sizeof(int32_t), 1, file);
        shape->ways[i].points = (LatLon64*)malloc(shape->ways[i].count * sizeof(LatLon64));
        fread(shape->ways[i].points, sizeof(LatLon64), shape->ways[i].count, file);
    }
    fclose(file);
}

struct Rtree* buildTree(Shape* shape)
{
    struct Rtree* tree = rtree_new();

    for (int i = 0; i < shape->count; i++) {
        Way* way = &shape->ways[i];
        struct Rect MBR = convertWayToRect(way);
        rtree_insert(tree, &MBR.min, &MBR.max, way);
    }

    return tree;
}

// Forward declaration of the serialize functions
void writeNodeToFile(FILE* file, const struct Node* node, const struct Rtree* rtree);

void writeRtreeToFile(const char* filename, const struct Rtree* rtree) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Error opening file");
        return;
    }

    // Write the basic Rtree structure
    fwrite(&rtree->rect, sizeof(struct Rect), 1, file);
    fwrite(&rtree->count, sizeof(size_t), 1, file);
    fwrite(&rtree->height, sizeof(size_t), 1, file);

#ifdef USE_PATHHINT
    fwrite(rtree->path_hint, sizeof(int), 16, file);
#endif

    fwrite(&rtree->relaxed, sizeof(bool), 1, file);

    // Write the root node
    writeNodeToFile(file, rtree->root, rtree);

    // Close the file
    fclose(file);
}

// Helper function to serialize a Node
void writeNodeToFile(FILE* file, const struct Node* node, const struct Rtree* rtree) {
    if (node == NULL) {
        // Indicate that the node is NULL
        int null_marker = -1;
        fwrite(&null_marker, sizeof(int), 1, file);
        return;
    }

    // Write the node structure
    fwrite(&node->rc, sizeof(rc_t), 1, file);
    fwrite(&node->kind, sizeof(enum Kind), 1, file);
    fwrite(&node->count, sizeof(int), 1, file);
    fwrite(node->rects, sizeof(struct Rect), node->count, file);

    // Write node-specific data
    if (node->kind == LEAF) {
        // Serialize leaf nodes
        for (int i = 0; i < node->count; ++i) {
            // Serialize the Item structure
            fwrite(&node->datas[i].data, sizeof(DATATYPE), 1, file);
        }
    }
    else {
        // Serialize branch nodes
        for (int i = 0; i < node->count; ++i) {
            writeNodeToFile(file, node->nodes[i], rtree);
        }
    }
}

#include <stdio.h>
#include <stdlib.h>

// Forward declaration of the deserialization functions
struct Node* readNodeFromFile(FILE* file, struct Rtree* rtree);

void readRtreeFromFile(const char* filename, struct Rtree* rtree) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        return;
    }

    // Read the basic Rtree structure
    fread(&rtree->rect, sizeof(struct Rect), 1, file);
    fread(&rtree->count, sizeof(size_t), 1, file);
    fread(&rtree->height, sizeof(size_t), 1, file);

#ifdef USE_PATHHINT
    fread(rtree->path_hint, sizeof(int), 16, file);
#endif

    fread(&rtree->relaxed, sizeof(bool), 1, file);

    // Read the root node
    rtree->root = readNodeFromFile(file, rtree);

    // Close the file
    fclose(file);
}

// Helper function to read a Node
struct Node* readNodeFromFile(FILE* file, struct Rtree* rtree) {
    struct Node* node = malloc(sizeof(struct Node));
    if (!node) {
        perror("Error allocating memory for Node");
        return NULL;
    }

    // Read the node structure
    fread(&node->rc, sizeof(rc_t), 1, file);
    fread(&node->kind, sizeof(enum Kind), 1, file);
    fread(&node->count, sizeof(int), 1, file);
    fread(node->rects, sizeof(struct Rect), node->count, file);

    // Check if the node is NULL
    int null_marker;
    fread(&null_marker, sizeof(int), 1, file);
    if (null_marker == -1) {
        free(node);
        return NULL;
    }
    fseek(file, sizeof(int), SEEK_CUR); // Move the file pointer back

    // Read node-specific data
    if (node->kind == LEAF) {
        // Deserialize leaf nodes
        for (int i = 0; i < node->count; ++i) {
            // Deserialize the Item structure
            // Adjust based on how you serialized DATATYPE
            fread(&node->datas[i].data, sizeof(DATATYPE), 1, file);
        }
    }
    else {
        // Deserialize branch nodes
        for (int i = 0; i < node->count; ++i) {
            node->nodes[i] = readNodeFromFile(file, rtree);
        }
    }

    return node;
}

// Function to convert geographic coordinates to screen coordinates
Vector2 GeoToScreen(LatLon64 point, float screenWidth, float screenHeight, Vector2 offset, float zoom) {
    float x = ((point.longitude + 180.0) * (screenWidth / 360.0) - offset.x) * zoom;
    float y = ((90.0 - point.latitude) * (screenHeight / 180.0) - offset.y) * zoom;
    return (Vector2) { x, y };
}

LatLon64 ScreenToGeo(Vector2 point, float screenWidth, float screenHeight, Vector2 offset, float zoom) {
    double longitude = (point.x / zoom + offset.x) / (screenWidth / 360) - 180;
    double latitude = -((point.y / zoom + offset.y) / (screenHeight / 180) - 90);
    return (LatLon64) { latitude, longitude };
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

    for (int32_t lon = -180; lon <= 180; lon += 1) {
        lonGridTop.longitude = lon;
        lonGridBottom.longitude = lon;
        screenTop = GeoToScreen(lonGridTop, screenWidth, screenHeight, offset, zoom);
        screenBottom = GeoToScreen(lonGridBottom, screenWidth, screenHeight, offset, zoom);
        DrawLineV(screenTop, screenBottom, gridColor);
    }

    for (int32_t lat = -90; lat <= 90; lat += 1) {
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

void DrawShape(Shape* shape,float screenWidth, float screenHeight, Vector2 offset, float zoom, int* totalLineCount, Color color) {

    int32_t detailDivideCoeff;
    setDetailAmount(zoom, &detailDivideCoeff);
    
    // Draw each way
    for (int32_t i = 0; i < shape->count; i++) {
        for (int32_t j = 0; j < shape->ways[i].count - 1; j += detailDivideCoeff) {

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

bool WayIter(const double* min, const double* max, const Way* item, Shape* udata) {
    const Way* way = item;
    udata->count++; udata->ways = realloc(udata->ways, udata->count * sizeof(Way));

    udata->ways[udata->count - 1] = *way;
    return true;
}


void DrawShapeFromTree(struct Rtree* tree, float screenWidth, float screenHeight, Vector2 offset, float zoom, int* totalLineCount, Color color) {

    int32_t detailDivideCoeff;
    setDetailAmount(zoom, &detailDivideCoeff);

    Vector2 start = { 0, screenHeight };
    LatLon64 startPoint = ScreenToGeo(start, screenWidth, screenHeight, offset, zoom);
    Vector2 end = { screenWidth, 0 };
    LatLon64 endPoint = ScreenToGeo(end, screenWidth, screenHeight, offset, zoom);

    double min[2] = { startPoint.longitude, startPoint.latitude };
    double max[2] = { endPoint.longitude, endPoint.latitude };

    Shape queryResult = { NULL, 0 };
    queryResult.ways = (Way*) malloc(sizeof(Way));
    rtree_search(tree, min, max, WayIter, &queryResult);

    // Draw each way
    for (int32_t i = 0; i < queryResult.count; i++) {
        for (int32_t j = 0; j < queryResult.ways[i].count - 1; j += detailDivideCoeff) {

            if (j + detailDivideCoeff >= queryResult.ways[i].count) {
                Vector2 start = GeoToScreen(queryResult.ways[i].points[j], screenWidth, screenHeight, offset, zoom);
                Vector2 end = GeoToScreen(queryResult.ways[i].points[queryResult.ways[i].count - 1], screenWidth, screenHeight, offset, zoom);
                if (((start.x >= 0 && start.x <= screenWidth) && (start.y >= 0 && start.y <= screenHeight)) ||
                    ((end.x >= 0 && end.x <= screenWidth) && (end.y >= 0 && end.y <= screenHeight))) {
                    DrawLineV(start, end, color);
                    *totalLineCount += 1;
                }
            }
            else {
                Vector2 start = GeoToScreen(queryResult.ways[i].points[j], screenWidth, screenHeight, offset, zoom);
                Vector2 end = GeoToScreen(queryResult.ways[i].points[(j + detailDivideCoeff)], screenWidth, screenHeight, offset, zoom);
                if (((start.x >= 0 && start.x <= screenWidth) && (start.y >= 0 && start.y <= screenHeight)) ||
                    ((end.x >= 0 && end.x <= screenWidth) && (end.y >= 0 && end.y <= screenHeight))) {
                    DrawLineV(start, end, color);
                    *totalLineCount += 1;
                }
            }
        }
    }
}

void DrawMBR(int count, struct Rect* rectArray, float screenWidth, float screenHeight, Vector2 offset, float zoom) {
    for (int i = 0; i < count; i++) {
        LatLon64 topLeft = { rectArray[i].max[1], rectArray[i].min[0]};
        LatLon64 bottomRight = { rectArray[i].min[1], rectArray[i].max[0]};

        Color mbrColor = YELLOW;

        Vector2 start = GeoToScreen(topLeft, screenWidth, screenHeight, offset, zoom);
        Vector2 end = GeoToScreen(bottomRight, screenWidth, screenHeight, offset, zoom);

        DrawRectangleLines(start.x, start.y, end.x-start.x, end.y-start.y, YELLOW);
    }

}

void freeShape(Shape* shape) {
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
    *detailDivideCoeff = 1.0;

}

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void) {
    // Initialization
    //--------------------------------------------------------------------------------------
    const int32_t screenWidth = 1200;
    const int32_t screenHeight = 675;
    int32_t totalLineCount = 0;

    InitWindow(screenWidth, screenHeight, "Vector Map");

    //SetTargetFPS(60);  
    //--------------------------------------------------------------------------------------

    // Convert geographical XML data files to binary files
    /*Shape turkiyeBorders = LoadGeoDataFromXML("appData\\turkey_border1.xml");
    writeBinaryFile("turkiye_border.bin", &turkiyeBorders);
    Shape italyBorders = LoadGeoDataFromXML("appData\\italy_border1.xml");
    writeBinaryFile("italy_border.bin", &italyBorders);
    Shape greeceBorders = LoadGeoDataFromXML("appData\\greece_border1.xml");
    writeBinaryFile("greece_border.bin", &greeceBorders);
    Shape bulgariaBorders = LoadGeoDataFromXML("appData\\bulgaria_border1.xml");
    writeBinaryFile("bulgaria_border.bin", &bulgariaBorders);
    Shape cyprusBorders = LoadGeoDataFromXML("appData\\cyprus_border1.xml");
    writeBinaryFile("cyprus_border.bin", &cyprusBorders);
    Shape russiaBorders = LoadGeoDataFromXML("appData\\russia_border1.xml");
    writeBinaryFile("russia_border.bin", &russiaBorders);
    Shape provinces = LoadGeoDataFromXML("appData\\provinces.xml");
    writeBinaryFile("provinces.bin", &provinces);
    Shape rivers = LoadGeoDataFromXML("appData\\rivers.xml");
    writeBinaryFile("rivers.bin", &rivers);*/

    // Read every binary file
    Shape turkiyeBorders;
    readBinaryFile("turkiye_border.bin", &turkiyeBorders);
    Shape italyBorders;
    readBinaryFile("italy_border.bin", &italyBorders);
    Shape greeceBorders;
    readBinaryFile("greece_border.bin", &greeceBorders);
    Shape bulgariaBorders;
    readBinaryFile("bulgaria_border.bin", &bulgariaBorders);
    Shape cyprusBorders;
    readBinaryFile("cyprus_border.bin", &cyprusBorders);
    Shape russiaBorders;
    readBinaryFile("russia_border.bin", &russiaBorders);
    Shape provinces;
    readBinaryFile("provinces.bin", &provinces);
    Shape rivers;
    readBinaryFile("rivers.bin", &rivers);

    struct Rtree* turkey_tree;
    turkey_tree = buildTree(&turkiyeBorders);
    struct Rtree* italy_tree;
    italy_tree = buildTree(&italyBorders);
    struct Rtree* greece_tree;
    greece_tree = buildTree(&greeceBorders);
    struct Rtree* bulgaria_tree;
    bulgaria_tree = buildTree(&bulgariaBorders);
    struct Rtree* cyprus_tree;
    cyprus_tree = buildTree(&cyprusBorders);
    struct Rtree* russia_tree;
    russia_tree = buildTree(&russiaBorders);
    struct Rtree* provinces_tree;
    provinces_tree = buildTree(&provinces);
    struct Rtree* rivers_tree;
    rivers_tree = buildTree(&rivers);

    // Initialize pan and zoom
    Vector2 offset = { 0.0f, 0.0f };
    float zoom = 1.0f;

    // Variables for mouse dragging
    bool isDragging = false;
    Vector2 lastMousePosition = { 0.0f, 0.0f };
    Vector2 currentMousePosition = { 0.0f, 0.0f };
    Vector2 delta = { 0.0f, 0.0f };
    Color riverColor = { 0, 121, 241, 30 };
    int32_t riverAlpha;

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

        if ((10 * zoom) < 127) {
            riverColor.a = 10 * zoom;
        }
        else {
            riverColor.a = 127;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(BLACK);

        DrawWorldBoundaries(screenWidth, screenHeight, offset, zoom);
        printf("offset.x = %f\n", offset.x);
        printf("offset.y = %f\n", offset.y);

        //DrawShapeFromTree(rt, screenWidth, screenHeight, offset, zoom, &totalLineCount, RED);
        /*DrawShape(&italyBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShape(&greeceBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShape(&bulgariaBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShape(&cyprusBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShape(&russiaBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShape(&provinces, screenWidth, screenHeight, offset, zoom, &totalLineCount, RAYWHITE);
        DrawShape(&rivers, screenWidth, screenHeight, offset, zoom, &totalLineCount, riverColor);*/
        //DrawShape(&turkiyeBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, RED);

        DrawShapeFromTree(turkey_tree, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShapeFromTree(greece_tree, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShapeFromTree(bulgaria_tree, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShapeFromTree(cyprus_tree, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShapeFromTree(russia_tree, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShapeFromTree(provinces_tree, screenWidth, screenHeight, offset, zoom, &totalLineCount, RAYWHITE);
        DrawShapeFromTree(rivers_tree, screenWidth, screenHeight, offset, zoom, &totalLineCount, riverColor);
        

        printf("Total line count is: %d\n", totalLineCount);

        float ms = GetFrameTime() * 1e3;
        DrawText( TextFormat("%f" , ms ) , 100,100, 32 , GREEN);
        
        //DrawFPS(10, 10);
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    freeShape(&turkiyeBorders);
    freeShape(&italyBorders);
    freeShape(&greeceBorders);
    freeShape(&bulgariaBorders);
    freeShape(&cyprusBorders);
    freeShape(&russiaBorders);
    freeShape(&provinces);
    freeShape(&rivers);
    CloseWindow();     // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
