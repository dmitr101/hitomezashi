#include "stdint.h"
#include "stdlib.h"
#include "math.h"

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// TODO: add emscripten back

#define CELL_SIZE 40
#define UPDATE_CADENCE 0.1

typedef enum
{
    UPDATE_REGENERATE,
    UPDATE_SHIFT,
} UpdateType;

typedef struct AppState_t
{
    int windowWidth;
    int windowHeight;

    uint32_t horizontalSequence;
    uint32_t verticalSequence;

    double lastUpdateTime;
    float updateSpeed; // How many time per second to update
    int updateType;
    bool updateTypeEditMode;
    bool colored;
} AppState;

static void UpdateDrawFrame(AppState* state);
static int UpdateDrawUI(AppState* state); // Returns the x coordinate of the beginning of the UI block

int main(void)
{
    srand(1023);
    AppState appState = {
        .windowWidth = 1920,
        .windowHeight = 1080,
        .horizontalSequence = (uint32_t)rand(),
        .verticalSequence = (uint32_t)rand(),
        .lastUpdateTime = 0.0,
        .updateSpeed = 10.0,
        .updateType = UPDATE_REGENERATE,
        .updateTypeEditMode = false,
        .colored = false,
    };
    InitWindow(appState.windowWidth, appState.windowHeight, "hitomezashi pattern generator");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowMinSize(480, 480);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);

    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        UpdateDrawFrame(&appState);
    }

    CloseWindow();
    return 0;
}

void UpdateDrawFrame(AppState* state)
{
    double currentTime = GetTime();
    bool updatePattern = state->updateSpeed != 0.0 && currentTime > (state->lastUpdateTime + (1.0f / state->updateSpeed));
    if (updatePattern)
    {
        state->lastUpdateTime = currentTime;
        state->horizontalSequence = GetRandomValue(0, INT_MAX);
        state->verticalSequence = GetRandomValue(0, INT_MAX);
    }

    BeginDrawing();
    {
        ClearBackground(WHITE);
        const int controlRectXStart = UpdateDrawUI(state);
        const int maxXSteps = (controlRectXStart / CELL_SIZE) < 32 ? (controlRectXStart / CELL_SIZE - 1) : 32;

        // Vertical pass
        for (int i = 0; i < maxXSteps; ++i)
        {
            bool offset = (state->verticalSequence & ((uint32_t)1 << i)) != 0;
            int offset_pixels = offset ? CELL_SIZE : 0;
            int x = i * CELL_SIZE;
            for (int j = 0; j < 32; j += 2)
            {
                int y = j * CELL_SIZE + offset_pixels;
                DrawLine(x, y, x, y + CELL_SIZE, BLACK);
            }
        }

        // Horizontal pass
        for (int i = 0; i < 32; ++i)
        {
            bool offset = (state->horizontalSequence & ((uint32_t)1 << i)) != 0;
            int offset_pixels = offset ? CELL_SIZE : 0;
            int y = i * CELL_SIZE;
            for (int j = 0; j < maxXSteps; j += 2)
            {
                int x = j * CELL_SIZE + offset_pixels;
                DrawLine(x, y, x + CELL_SIZE, y, BLACK);
            }
        }
    }
    EndDrawing();
}

int UpdateDrawUI(AppState* state)
{
    const int controlWidth = 250;
    const int currentWindowWidth = GetRenderWidth();
    const int controlRectXStart = currentWindowWidth - controlWidth;

    const int textHeight = 20;
    const int fatControlHeight = 40;

    const int dropdownLabelY = 0;
    const int dropdownY = dropdownLabelY + textHeight;
    const int frequencyLabelY = dropdownY + fatControlHeight;
    const int frequencySliderY = frequencyLabelY + textHeight;

    GuiLabel((Rectangle) { controlRectXStart, frequencyLabelY, controlWidth, textHeight }, "Update frequency");
    GuiSlider((Rectangle) { controlRectXStart, frequencySliderY, controlWidth, fatControlHeight }, NULL, NULL, & state->updateSpeed, 0.0, 60.0);

    // It's actually higher, but needs to be lower to draw the dropdown above the slider 
    GuiLabel((Rectangle) { controlRectXStart, dropdownLabelY, controlWidth, textHeight }, "Update type");
    if (GuiDropdownBox((Rectangle) { controlRectXStart, dropdownY, controlWidth, fatControlHeight }, "REGENERATE;SHIFT", & state->updateType, state->updateTypeEditMode))
        state->updateTypeEditMode = !state->updateTypeEditMode;
    return controlRectXStart;
}
