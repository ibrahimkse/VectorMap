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

struct Rtree* buildTree(Shape* shape, struct Rect* rectArray)
{
    struct Rtree* tree = rtree_new();

    for (int i = 0; i < shape->count; i++) {
        Way* way = &shape->ways[i];
        struct Rect MBR = convertWayToRect(way);

        rectArray[i] = MBR;

        rtree_insert(tree, MBR.min, MBR.max, way);
    }

    return tree;
}

void writeNodeToFile(FILE* file, struct Node* node)
{
    fwrite(node, sizeof(node), 1, file);
    fwrite(node->rects, sizeof(struct Rect), node->count, file);

    if (node->kind == LEAF) {
        fwrite(node->datas, sizeof(struct Item), node->count, file);
    }
    else {
        for (int i = 0; i < node->count; i++) {
            writeNodeToFile(file, node->nodes[i]);
        }
    }

}

void writeTreeToFile(const char* filename, struct Rtree* tree){
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("File opening failed");
        return;
    }

    fwrite(tree, sizeof(struct Rtree), 1, file);

    writeNodeToFile(file, tree->root);

    fclose(file);
}


struct Rtree* readTree(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("File opening failed");
        return;
    }

    struct Rtree* tree = malloc(sizeof(struct Rtree));
    if (!tree) {
        fclose(file);
        return NULL;
    }

    // Read the rtree struct
    if (fread(tree, sizeof(struct Rtree), 1, file) != 1) {
        free(tree);
        fclose(file);
        return NULL;
    }

    // Read the nodes recursively
    tree->root = readTreeNode(file, tree);
    if (!tree->root) {
        free(tree);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return tree;
}

struct Node* readTreeNode(FILE* file, struct Rtree* tree) {
    struct Node* node = tree->malloc(sizeof(struct Node));
    if (!node) {
        return NULL;
    }

    // Read the node struct
    if (fread(node, sizeof(struct Node), 1, file) != 1) {
        tree->free(node);
        return NULL;
    }

    // Read the node data
    if (fread(node->rects, sizeof(struct Rect), node->count, file) != node->count) {
        tree->free(node);
        return NULL;
    }

    if (node->kind == LEAF) {
        if (fread(node->datas, sizeof(struct Item), node->count, file) != node->count) {
            tree->free(node);
            return NULL;
        }
        // Clone the data items
        for (int i = 0; i < node->count; i++) {
            DATATYPE cloned_data;
            if (!tree->item_clone(node->datas[i].data, &cloned_data, tree->udata)) {
                // Clean up previously cloned items
                for (int j = 0; j < i; j++) {
                    tree->item_free(node->datas[j].data, tree->udata);
                }
                tree->free(node);
                return NULL;
            }
            node->datas[i].data = cloned_data;
        }
    }
    else {
        for (int i = 0; i < node->count; i++) {
            node->nodes[i] = readTreeNode(file, tree);
            if (!node->nodes[i]) {
                // Clean up previously read nodes
                for (int j = 0; j < i; j++) {
                    //free_node(node->nodes[j], tree);
                }
                tree->free(node);
                return NULL;
            }
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

    SetTargetFPS(60);  
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

    struct Rtree* rt;
    struct Rect* rectArray = (struct Rect*)malloc(turkiyeBorders.count * sizeof(struct Rect));
    rt = buildTree(&turkiyeBorders, rectArray);

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

        
        DrawShape(&italyBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShape(&greeceBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShape(&bulgariaBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShape(&cyprusBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShape(&russiaBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        DrawShape(&provinces, screenWidth, screenHeight, offset, zoom, &totalLineCount, RAYWHITE);
        DrawShape(&rivers, screenWidth, screenHeight, offset, zoom, &totalLineCount, riverColor);

        //turkiye border
        DrawShape(&turkiyeBorders, screenWidth, screenHeight, offset, zoom, &totalLineCount, RED);
        DrawMBR(turkiyeBorders.count, rectArray, screenWidth, screenHeight, offset, zoom);

        printf("Total line count is: %d\n", totalLineCount);

        DrawFPS(10, 10);
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
