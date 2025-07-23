#ifndef PCH_H
#define PCH_H
#include <iostream>
#include <vector>
union ViewContent {
    enum {
        Marker = 0,
        Skeleton = 1,
        Mesh_Polygon = 2,
        Nurbs = 3,
        Patch = 4,
        Camera = 5,
        Light = 6,
        LODGroup = 7,
        Mesh_Texture = 8,
        Mesh_ControlPoint = 9,
    };
    int id;
};
extern bool view[128];
extern std::vector<std::string> VisibleProperties;
#endif // !PCH_H