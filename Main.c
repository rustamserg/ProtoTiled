#include "raylib.h"

#include "rlgl.h"
#include "raymath.h"
#include "resource_dir.h"

#include "Map.h"

#include <math.h>


typedef enum ZoomMode : uint8_t
{
    ZOOM_MODE_WHEEL,
    ZOOM_MODE_MOVE,
} ZoomMode;

int main(void)
{
    constexpr int screenWidth = 1920;
    constexpr int screenHeight = 1080;
    constexpr float minZoom = 0.125f;
    constexpr float maxZoom = 64.0f;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - 2d camera mouse zoom");

    SearchAndSetResourceDir("resources");

    Camera2D camera = { .zoom = 1.0f };

    ZoomMode zoomMode = ZOOM_MODE_WHEEL;
    bool showObjects = false;

    SetTargetFPS(60);       // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    Map map;
    if (!LoadMap(&map, "map.tmx"))
    {
        TraceLog(LOG_ERROR, "Failed to load map.tmx");
        CloseWindow();
        return 1;
    }

    // Main game loop
    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_ONE)) zoomMode = ZOOM_MODE_WHEEL;
        else if (IsKeyPressed(KEY_TWO)) zoomMode = ZOOM_MODE_MOVE;
        if (IsKeyPressed(KEY_O)) showObjects = !showObjects;

        UpdateMap(&map, GetFrameTime());

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            Vector2 delta = GetMouseDelta();
            delta = Vector2Scale(delta, -1.0f / camera.zoom);
            camera.target = Vector2Add(camera.target, delta);
        }

        switch (zoomMode)
        {
        case ZOOM_MODE_WHEEL:
        {
            const float wheel = GetMouseWheelMove();
            if (wheel != 0)
            {
                const Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
                camera.offset = GetMousePosition();
                camera.target = mouseWorldPos;

                const float scale = 0.2f * wheel;
                camera.zoom = Clamp(expf(logf(camera.zoom) + scale), minZoom, maxZoom);
            }
            break;
        }
        case ZOOM_MODE_MOVE:
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            {
                const Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
                camera.offset = GetMousePosition();
                camera.target = mouseWorldPos;
            }

            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
            {
                const float deltaX = GetMouseDelta().x;
                const float scale = 0.005f * deltaX;
                camera.zoom = Clamp(expf(logf(camera.zoom) + scale), minZoom, maxZoom);
            }
            break;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode2D(camera);
        // Draw the 3d grid, rotated 90 degrees and centered around 0,0
        // just so we have something in the XY plane
        rlPushMatrix();
        rlTranslatef(0, 25 * 50, 0);
        rlRotatef(90, 1, 0, 0);
        DrawGrid(100, 50);
        rlPopMatrix();

        DrawMap(&map);
        if (showObjects) DrawMapObjectsDebug(&map);

        EndMode2D();

        DrawCircleV(GetMousePosition(), 4, DARKGRAY);
        DrawTextEx(GetFontDefault(), TextFormat("[%i, %i]", GetMouseX(), GetMouseY()),
            Vector2Add(GetMousePosition(), (Vector2){ -44, -24 }), 20, 2, BLACK);

        DrawText("[1][2] Select mouse zoom mode (Wheel or Move)", 20, 20, 20, DARKGRAY);
        if (zoomMode == ZOOM_MODE_WHEEL) DrawText("Mouse left button drag to move, mouse wheel to zoom", 20, 50, 20, DARKGRAY);
        else DrawText("Mouse left button drag to move, mouse press and move to zoom", 20, 50, 20, DARKGRAY);
        DrawText("[O] Toggle objects", 20, 80, 20, DARKGRAY);

        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadMap(&map);
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
