#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <time.h>

#include <raylib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define JC_VORONOI_IMPLEMENTATION
#include "jc_voronoi.h"

#define NUM_POINTS 2000

// Get a random float between 0 and 1
float randomFloat() {
    return (float)rand() / RAND_MAX;
}

// Test if a point is contained within a polygon, https://wrfranklin.org/Research/Short_Notes/pnpoly.html
int pnpoly(int nvert, float *vertx, float *verty, float testx, float testy)
{
  int i, j, c = 0;
  for (i = 0, j = nvert-1; i < nvert; j = i++) {
    if ( ((verty[i]>testy) != (verty[j]>testy)) &&
	 (testx < (vertx[j]-vertx[i]) * (testy-verty[i]) / (verty[j]-verty[i]) + vertx[i]) )
       c = !c;
  }
  return c;
}

float lerp(float a, float b, float f)
{
    return (a * (1.0 - f)) + (b * f);
}

// Gets the bounding box coords for each poly
jcv_rect get_site_bounds(const jcv_site* site) {
    const jcv_graphedge* edge = site->edges;
    jcv_point p = edge->pos[0];

    jcv_real min_x = p.x, min_y = p.y, max_x = p.x, max_y = p.y;
    edge = edge->next;

    while( edge )
    {
        p = edge->pos[0];
        if (p.x > max_x) {
            max_x = p.x;
        }
        else if (p.x < min_x) {
            min_x = p.x;
        }
        if (p.y > max_y) {
            max_y = p.y;
        }
        else if (p.y < min_y) {
            min_y = p.y;
        }
        edge = edge->next;
    }

    jcv_rect bbox = {
        .min = {.x = min_x, .y = min_y},
        .max = {.x = max_x, .y = max_y}
    };

    return bbox;
}

// Remaps the point from the input space to image space
static inline jcv_point remap(const jcv_point* pt, const jcv_point* min, const jcv_point* max, const jcv_point* scale)
{
    jcv_point p;
    p.x = (pt->x - min->x)/(max->x - min->x) * scale->x;
    p.y = (pt->y - min->y)/(max->y - min->y) * scale->y;
    return p;
}

// Unmaps the point from the image space to input space
static inline jcv_point unmap(const jcv_point* pt, const jcv_point* min, const jcv_point* max, const jcv_point* scale)
{
    jcv_point p;
    p.x = (pt->x / scale->x) * (max->x - min->x) + min->x;
    p.y = (pt->y / scale->y) * (max->y - min->y) + min->y;
    return p;
}

// Relax algorithm from jcv impl
static void relax_points(const jcv_diagram* diagram, jcv_point* points, jcv_point* dimensions, unsigned char* img_data)
{
    const int img_width = (int)round(dimensions->x);
    const int img_height = (int)round(dimensions->y);
    const jcv_site* sites = jcv_diagram_get_sites(diagram);
    for( int i = 0; i < diagram->numsites; ++i )
    {
        const jcv_site* site = &sites[i];

        // Get all site x and y
        int nvert = 0;
        float site_xs[20];
        float site_ys[20];

        const jcv_graphedge* edge = site->edges;

        while( edge )
        {
            jcv_point img_space_p = remap(&edge->pos[0], &diagram->min, &diagram->max, dimensions);
            site_xs[nvert] = img_space_p.x;
            site_ys[nvert] = img_space_p.y;
            ++nvert;
            edge = edge->next;
        }

        // Get voronoi bounds
        jcv_rect site_bbox = get_site_bounds(site);
        jcv_point site_point = site->p;

        // Convert to image space
        jcv_point min_p = remap(&site_bbox.min, &diagram->min, &diagram->max, dimensions);
        jcv_point max_p = remap(&site_bbox.max, &diagram->min, &diagram->max, dimensions);

        // Convert to int
        int minx = (int)round(min_p.x), miny = (int)round(min_p.y), maxx = (int)round(max_p.x), maxy = (int)round(max_p.y);

        // nested for, loop through all possible ints and pos check them
        float centroid_x = 0, centroid_y = 0;
        float total_weight = 0;
        for (int x = minx; x < maxx; x++)
        {
            for (int y = miny; y < maxy; y++)
            {
                if (x < 0 || x >= img_width || y < 0 || y >= img_height)
                    continue;  // Skip invalid pixels
                if ( pnpoly(nvert, site_xs, site_ys, x, y) )
                {
                    unsigned char* pixelOffset = img_data + (x + img_width * y) * 3;
                    unsigned char r = *(pixelOffset + 0);
                    unsigned char g = *(pixelOffset + 1);
                    unsigned char b = *(pixelOffset + 2);

                    float brightness = (r + g + b) / 3.0f;
                    float weight = 1.0f - (brightness / 255.0f); // Go towards darkness
                    // float weight = brightness / 255.0f; // Go towards light

                    jcv_point img_space = {
                        .x = x,
                        .y = y,
                    };
                    jcv_point point_space = unmap(&img_space, &diagram->min, &diagram->max, dimensions);

                    // Do weighted sum here
                    if (weight > 0) {
                        centroid_x += point_space.x * weight;
                        centroid_y += point_space.y * weight;
                        total_weight += weight;
                    }
                }
            }
        }

        if ( total_weight > 0 ) {
            // printf("point %d with x: %f, y: %f, has centroid: x: %f, y: %f\n",
            //     site->index,
            //     points[site->index].x,
            //     points[site->index].y,
            //     centroid_x / total_weight,
            //     centroid_y / total_weight
            // );
            points[site->index].x = lerp(points[site->index].x, centroid_x / total_weight, 0.1);
            points[site->index].y = lerp(points[site->index].y, centroid_y / total_weight, 0.1);
        }

        // count how many are contained and store that info
        // printf("minx: %f maxx: %f miny: %f maxy: %f\n", min_p.x, max_p.x, min_p.y, max_p.y);
        // jcv_point sum = site->p;
        // int count = 1;

        // const jcv_graphedge* edge = site->edges;

        // while( edge )
        // {
        //     sum.x += edge->pos[0].x;
        //     sum.y += edge->pos[0].y;
        //     ++count;
        //     edge = edge->next;
        // }

        // points[site->index].x = sum.x / (jcv_real)count;
        // points[site->index].y = sum.y / (jcv_real)count;
    }
}

int main(void) {
    srand(time(NULL));

    // const int screen_width = 1000;
    // const int screen_height = 1000;
    const int numrelaxations = 250;

    int img_width, img_height, components;
    // unsigned char *data = stbi_load("./000/groundtruth/first000_gt.png", &img_width, &img_height, &components, 3);
    unsigned char *data = stbi_load("./images/fisk.jpg", &img_width, &img_height, &components, 3);
    if( !data )
    {
        puts("Could not open file");
        return 1;
    }
    const int screen_width = img_width * 2;
    const int screen_height = img_height * 2;


    // Calculate scale factors to fill the screen
    float textureWidth = (float)img_width;
    float textureHeight = (float)img_height;
    float screenWidthFloat = (float)screen_width;
    float screenHeightFloat = (float)screen_height;

    float scaleX = screenWidthFloat / textureWidth;
    float scaleY = screenHeightFloat / textureHeight;

    // To maintain aspect ratio, use the larger scale factor
    float scale = (scaleX > scaleY) ? scaleX : scaleY;

    InitWindow(screen_width, screen_height, "Voronoi path planning app");
    SetTargetFPS(120); // Controls how many voronoi steps per second target

    // Create raylib Texture2D
    // Texture2D texture = LoadTexture("./000/groundtruth/first000_gt.png");
    Texture2D texture = LoadTexture("./images/fisk.jpg");

    jcv_diagram diagram;
    memset(&diagram, 0, sizeof(jcv_diagram));

    jcv_point* points = 0;
    jcv_point dimensions = {
        .x = screen_width,
        .y = screen_height
    };
    jcv_point img_dimensions = {
        .x = img_width,
        .y = img_height
    };

    points = (jcv_point*)malloc(sizeof(jcv_point) * NUM_POINTS);
    if( !points )
    {
        puts("Could not malloc points");
        return 1;
    }

    // Generate points for voronoi
    for (int i = 0; i < NUM_POINTS; ++i)
    {
        points[i].x = rand() * img_width;
        points[i].y = rand() * img_height;
    }

    // int p_idx = 0;
    // while ( p_idx < NUM_POINTS )
    // {
    //     float r_x = rand() % img_width;
    //     float r_y = rand() % img_height;

    //     unsigned char* pixelOffset = data + ((int)round(r_x) + img_width * (int)round(r_y)) * 3;
    //     unsigned char r = *(pixelOffset + 0);
    //     unsigned char g = *(pixelOffset + 1);
    //     unsigned char b = *(pixelOffset + 2);

    //     float brightness = (r + g + b) / 3.0f;
    //     // float weight = 1.0f - (brightness / 255.0f); // Go towards darkness
    //     float weight = brightness / 255.0f; // Go towards light

    //     if ( randomFloat() < weight ) {
    //         points[p_idx].x = r_x;
    //         points[p_idx].y = r_y;
    //         ++p_idx;
    //     }
    // }

    int relaxationcount = 0;

    bool showtexture = true;

    while (!WindowShouldClose())
    {
        // Generate Diagram, resets the Voro state
        jcv_diagram_generate(NUM_POINTS, points, 0, 0, &diagram);

        // Get all edges for all Voronois
        const jcv_edge* edge = jcv_diagram_get_edges( &diagram );
        if( !edge )
        {
            puts("No edges");
            return 1;
        }

        jcv_delauney_iter delauney;
        jcv_delauney_begin( &diagram, &delauney );
        jcv_delauney_edge delauney_edge;

        BeginDrawing();
            ClearBackground(RAYWHITE);

            if ( IsKeyPressed(KEY_P) )
                showtexture = !showtexture;

            if ( showtexture )
                DrawTextureEx(texture, (Vector2){ 0, 0 }, 0, scale, WHITE);

            while (edge)
            {
                jcv_point p0 = remap(&edge->pos[0], &diagram.min, &diagram.max, &dimensions);
                jcv_point p1 = remap(&edge->pos[1], &diagram.min, &diagram.max, &dimensions);

                DrawLine((int)round(p0.x), (int)round(p0.y), (int)round(p1.x), (int)round(p1.y), WHITE);
                edge = jcv_diagram_get_next_edge(edge);
            }

            while (jcv_delauney_next( &delauney, &delauney_edge ))
            {
                jcv_point p0 = remap(&delauney_edge.pos[0], &diagram.min, &diagram.max, &dimensions);
                jcv_point p1 = remap(&delauney_edge.pos[1], &diagram.min, &diagram.max, &dimensions);

                DrawLine((int)round(p0.x), (int)round(p0.y), (int)round(p1.x), (int)round(p1.y), BLUE);
            }

            for (int i = 0; i < NUM_POINTS; ++i)
            {
                jcv_point p0 = remap(&points[i], &diagram.min, &diagram.max, &dimensions);

                DrawCircle((int)round(p0.x), (int)round(p0.y), 2.0, PINK);
            }
        EndDrawing();

        // if (relaxationcount < numrelaxations)
        // {
        //    relax_points(&diagram, points, &img_dimensions, data);
        //    relaxationcount += 1;
        // }
        relax_points(&diagram, points, &img_dimensions, data);
        relaxationcount += 1;
    }

    printf("img width: %d, height: %d, components: %d\n", img_width, img_height, components);
    printf("texture height: %d, width: %d\n", texture.height, texture.width);

    // stbi_image_free(data);
    UnloadTexture(texture);
    jcv_diagram_free(&diagram);
    free(points);
    CloseWindow();

    return 0;
}
