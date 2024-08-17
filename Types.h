#include <stdint.h>
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

typedef struct {
    char* k;
    char* v;
}Tag;

typedef struct {
    char* type;
    int   ref;
    char* role;
}Member;

typedef struct {
    int id;
    Member* members;
    Tag* tags;
    int tagCount;
    int memberCount;
}Relation;

typedef struct {
    int id;
    int64_t* nd_ids;
    Tag* tags;
    int countNd;
    int countTag;
}Way2;

typedef struct {
    Way2* ways;
    int count;
}Ways2;

typedef struct {
    int64_t id;
    double lat;
    double lon;
}Node;

typedef struct {
    Node* nodes;
    int count;
}Nodes;

typedef struct {
    Relation* relations;
    int count;
}Relations;

typedef struct {
    Relations relations;
    Ways2 ways;
    Nodes nodes;
}Metadata;

typedef struct {//<tag k="water" v="lake"/>
    Node* lakeCoordinates;
    int count;
}Lake;

typedef struct {//<tag k="region:type" v="mountain_area"/>
    Node* mountCoordinates;
    int count;
}Mount;

typedef struct {//<tag k="waterway" v="river"/>
    Node* riverCoordinates;
    int count;
}River;

typedef struct {//<tag k="landuse" v="military"/>
    Node* MilitaryCoordinates;
    int count;
}Military;

typedef struct {// <tag k="aeroway" v="aerodrome"/>
    Node* AerodromeCoordinates;
    int count;
}Aerodrome;

typedef struct {
    Lake* lakeList;
    int count;
}Lakes;

typedef struct {
    Mount* mountList;
    int count;
}Mounts;

typedef struct {
    River* riverList;
    int count;
}Rivers;

typedef struct {
    Military* militaryList;
    int count;
}Militaries;

typedef struct {
    Aerodrome* aerodromeList;
    int count;
}Aerodromes;
