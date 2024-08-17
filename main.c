#define _CRT_SECURE_NO_DEPRECATE
#include "raylib.h"
#include "lxml.h"
#include <stdlib.h>
#include "DataManagement.h"
#include <stdint.h>
//#include "Types.h"



void setDetailAmount(float zoom, int* detailDivideCoeff);

Metadata LoadGeoDataFromXML(const char* filePath) {
    XMLDocument doc;
    Relations relationList = { 0 };
    Ways2 wayList = { 0 };
    Nodes nodes = { 0 };
    Metadata metadata = { 0 };

    if (!XMLDocument_load(&doc, filePath)) {
        fprintf(stderr, "Failed to load XML file\n");
        return metadata;
    }

    XMLNode* osm = XMLNodeList_at(&doc.root->children, 0);

    //Relation operations Start
    XMLNodeList* relationNodeList = XMLNode_children(osm, "relation");
    if (relationNodeList->size == 0) {
        fprintf(stderr, "No 'relation' elements found\n");
        XMLDocument_free(&doc);
        //return relationList;
    }

    relationList.count = relationNodeList->size;
    relationList.relations = (Relation*)malloc(relationList.count * sizeof(Relation));

    for (int i = 0; i < relationList.count; i++) {//member ve tag için yapılacak
        XMLNode* relation = XMLNodeList_at(relationNodeList, i);
        XMLNodeList* memberNodes = XMLNode_children(relation, "member");
        XMLNodeList* tagNodes = XMLNode_children(relation, "tag");
        //relationList.relations[i].id= atoi(XMLNode_attr_val(relation, "id"));
        relationList.relations[i].memberCount = memberNodes->size;
        relationList.relations[i].members = (Member*)malloc(memberNodes->size * sizeof(Member));
        for (int j = 0; j < memberNodes->size; j++) {
            XMLNode* member = XMLNodeList_at(memberNodes, j);
            relationList.relations[i].members[j].ref = atoi(XMLNode_attr_val(member, "ref"));
            relationList.relations[i].members[j].role = XMLNode_attr_val(member, "role");
            relationList.relations[i].members[j].type = XMLNode_attr_val(member, "type");
        }

        relationList.relations[i].tagCount = tagNodes->size;
        relationList.relations[i].tags = (Tag*)malloc(tagNodes->size * sizeof(Tag));
        for (int t = 0; t < tagNodes->size; t++) {
            XMLNode* tag = XMLNodeList_at(tagNodes, t);
            relationList.relations[i].tags[t].k= XMLNode_attr_val(tag, "k");
            relationList.relations[i].tags[t].v= XMLNode_attr_val(tag, "v");
        }

        XMLNodeList_free(memberNodes);
        XMLNodeList_free(tagNodes);
    }
    //Relation operations End

    XMLNodeList* wayNodeList = XMLNode_children(osm, "way");
    if (wayNodeList->size == 0) {
        fprintf(stderr, "No 'way' elements found\n");
        XMLDocument_free(&doc);
        //return relationList;
    }
    wayList.count = wayNodeList->size;
    wayList.ways = (Way2*)malloc(wayList.count * sizeof(Way2));

    for (int i = 0; i < wayList.count; i++) {
        XMLNode* wayNode = XMLNodeList_at(wayNodeList, i);
        XMLNodeList* ndNodeList = XMLNode_children(wayNode, "nd");
        XMLNodeList* tagNodeList = XMLNode_children(wayNode, "tag");
        wayList.ways[i].id= atoi(XMLNode_attr_val(wayNode, "id"));

        wayList.ways[i].countNd = ndNodeList->size;
        wayList.ways[i].countTag = tagNodeList->size;
        wayList.ways[i].nd_ids = (int64_t*)malloc(wayList.ways[i].countNd * sizeof(int64_t));
        wayList.ways[i].tags = (Tag*)malloc(wayList.ways[i].countTag * sizeof(Tag));
        for (int j = 0; j < wayList.ways[i].countTag; j++) {
            XMLNode* tag = XMLNodeList_at(tagNodeList, j);
            wayList.ways[i].tags[j].k= XMLNode_attr_val(tag, "k");
            wayList.ways[i].tags[j].v= XMLNode_attr_val(tag, "v");
        }
        for (int j = 0; j < wayList.ways[i].countNd; j++) {
            XMLNode* nd= XMLNodeList_at(ndNodeList, j);
            wayList.ways[i].nd_ids[j]=atoll(XMLNode_attr_val(nd, "ref"));
        }
    }

    metadata.relations = relationList;
    metadata.ways = wayList;
    XMLNodeList* nodeNodeList = XMLNode_children(osm, "node");
    if (nodeNodeList->size == 0) {
        fprintf(stderr, "No 'way' elements found\n");
        XMLDocument_free(&doc);
        //return relationList;
    }
    nodes.count = nodeNodeList->size;
    nodes.nodes = (Node*)malloc(nodes.count * sizeof(Node));
    for (int i = 0; i < nodes.count; i++) {
        XMLNode* nodeNode = XMLNodeList_at(nodeNodeList, i);
        nodes.nodes[i].id = atoll(XMLNode_attr_val(nodeNode, "id"));
        nodes.nodes[i].lon = atof(XMLNode_attr_val(nodeNode, "lon"));
        nodes.nodes[i].lat = atof(XMLNode_attr_val(nodeNode, "lat"));
    }
    metadata.nodes = nodes;
    //XMLNodeList_free(ways);

    XMLDocument_free(&doc);
    return metadata;
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
    printf("Read op has been started\n");
    Metadata shape = LoadGeoDataFromXML("turkiye.xml");
    loadRivers(&shape);
    printf("Read op has been finished\n");


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

        //DrawCountryBoundaries(&shape1, screenWidth, screenHeight, offset, zoom, &totalLineCount, GREEN);
        //DrawCountryBoundaries(&shape2, screenWidth, screenHeight, offset, zoom, &totalLineCount, BLUE);
        //DrawCountryBoundaries(&shape3, screenWidth, screenHeight, offset, zoom, &totalLineCount, MAGENTA);
        //DrawCountryBoundaries(&shape4, screenWidth, screenHeight, offset, zoom, &totalLineCount, RAYWHITE);
        //DrawCountryBoundaries(&shape5, screenWidth, screenHeight, offset, zoom, &totalLineCount, RAYWHITE);
        //DrawCountryBoundaries(&shape, screenWidth, screenHeight, offset, zoom, &totalLineCount, RED);

        //printf("Total line count is: %d\n", totalLineCount);
        printf("%d\n",shape.relations.count);

        DrawFPS(10, 10);
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    //freeShape(&shape);
    //freeShape(&shape1);
    //freeShape(&shape2);
    //freeShape(&shape3);
    //freeShape(&shape4);
    //freeShape(&shape5);
    CloseWindow();     // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
