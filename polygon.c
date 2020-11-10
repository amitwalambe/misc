#include <stdio.h>
#include <stdbool.h>

#define uint8_t int

typedef struct vertice_s {
        float x;
        float y;
} vertice_t;

#define MAX_VERTICES 4

typedef struct polygon_s {
        uint8_t vertices_cnt;
        float m[MAX_VERTICES];        // slope of the line
        float c[MAX_VERTICES];
        vertice_t vertices[MAX_VERTICES];
} polygon_t;

polygon_t polygons[] =    {
        { MAX_VERTICES, {}, {}, { {10, 10}, {20, 20}, {40, 30}, {40, 5} } },
        { MAX_VERTICES, {}, {}, { {-25, 0}, {-30, -10}, {-40, -20}, {-10, -30} } },
        { MAX_VERTICES, {}, {}, { {-10, 10}, {-20, 10}, {-30, 30}, {-10, 20} } },
        { MAX_VERTICES, {}, {}, { {0.10, 0.56}, {0.26, 0.54}, {0.32, 0.68}, {0.02, 0.90} } }
};

uint8_t polygons_cnt = (sizeof(polygons) / sizeof(polygons[0]));

// For line equation y = mx + c, we pre-calculate m and c for all edges of a given polygon.
// This will save compute time for each iteration.
void precalc_values()
{
        uint8_t p = 0;
        int i, j = (polygons[p].vertices_cnt - 1);
        float x1, x2, y1, y2;

        for (p = 0; p < polygons_cnt; p++) {
                for (i = 0; i < polygons[p].vertices_cnt; i++) {
                        x1 = polygons[p].vertices[i].x;
                        x2 = polygons[p].vertices[j].x;
                        y1 = polygons[p].vertices[i].y;
                        y2 = polygons[p].vertices[j].y;
                        if(y2 == y1) {
                                polygons[p].c[i] = x1;
                                polygons[p].m[i] = 0;
                        } else {
                                polygons[p].c[i] = x1 - (y1 * x2) / (y2 - y1) + (y1 * x1) / (y2 - y1);
                                polygons[p].m[i] = (x2 - x1) / (y2 - y1);
                        }
                        j = i;
                }
        }
}


/* Algorithm :
 * To find if a point lies within a convex polygon, we draw an imaginary line passing through
 * the point and parallel to x axis. On either side of the point, if the line intersects
 * polygon edges odd number of times, the point is inside. For even number, it is outside.
 *      _ _ _
 *     /     /
 * .--/-----/---
 *   /     /
 *  /  .--/---
 *  \    /
 *   \  /
 *    \/
 *
 * Ref:
 * http://alienryderflex.com/polygon/
 * https://www.geeksforgeeks.org/how-to-check-if-a-given-point-lies-inside-a-polygon/
 *
 * The function will return true if the point x,y is inside the polygon, or
 * false if it is not.  If the point is exactly on the edge of the polygon,
 * then the function may return true or false.
 * Need to fix that (or just live with it as it is a border condition or define polygon so that entire range is within it)
 */
bool point_in_polygon(float x, float y)
{
        bool odd_nodes = false;
        uint8_t p = 0;
        int i, j = (polygons[p].vertices_cnt - 1);
        float x1, x2, y1, y2;

        for (p = 0; p < polygons_cnt; p++) {
                odd_nodes = false;
                for (i = 0; i < polygons[p].vertices_cnt; i++) {
                        x1 = polygons[p].vertices[i].x;
                        x2 = polygons[p].vertices[j].x;
                        y1 = polygons[p].vertices[i].y;
                        y2 = polygons[p].vertices[j].y;
                        if ((y1 < y && y2 >= y) || (y2 < y && y1 >= y)) {
                                odd_nodes ^= ((y * polygons[p].m[i] + polygons[p].c[i]) < x);
                        }
                        j = i;
                }
                if(odd_nodes) {
                        printf("Point %f %f is inside polygon %d\n", x, y, p);
                        break;
                }
        }
        if (p == polygons_cnt) {
                printf("Point %f %f does not lie within any polygon\n", x, y);
        }

        return odd_nodes;
}

int main()
{
        vertice_t ver[] = { {0.25, 0.15}, {0.1, 0.7}, {-15, -25}, {15, 25}, {-20, 15} };
        precalc_values();

        for (uint8_t i = 0; i < 3; i++) {
                point_in_polygon(ver[i].x, ver[i].y);
        }
}
