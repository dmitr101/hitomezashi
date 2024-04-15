#include "stdint.h"
#include "stdlib.h"
#include "math.h"
#include "assert.h"

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// TODO: add emscripten back

typedef enum
{
    UPDATE_REGENERATE,
    UPDATE_SHIFT,
    UPDATE_SCROLL,
} UpdateType;

typedef struct AppState_t
{
    int windowWidth;
    int windowHeight;

    float verticalProbability;
    float horizontalProbability;

    int cellSize;
    int gridWidth;
    int gridHeight;
    bool* verticalSequence;
    bool* horizontalSequence;
    int* islands;

    double lastUpdateTime;
    float updateSpeed; // How many time per second to update
    int updateType;
    bool updateTypeEditMode;
    bool showFPS;
    bool colored;
} AppState;

typedef struct UIUpdateResult_t
{
	int renderAreaWidth;
    bool shouldRegenerate;
} UIUpdateResult;

static float Uniform01Rand() { return GetRandomValue(0, 1000) / 1000.0f; }
static void UpdateDrawFrame(AppState* state);
static UIUpdateResult UpdateDrawUI(AppState* state); // Returns the x coordinate of the beginning of the UI blockhorizontalSequence
static void RegenerateSequences(AppState* state)
{
    state->windowWidth = GetRenderWidth();
    state->windowHeight = GetRenderHeight();

	if (state->verticalSequence)
	{
		free(state->verticalSequence);
	}
	if (state->horizontalSequence)
	{
		free(state->horizontalSequence);
	}

    state->gridWidth = state->windowWidth / state->cellSize;
    state->gridHeight = state->windowHeight / state->cellSize;

	state->horizontalSequence = (bool*)malloc(state->gridWidth * sizeof(bool));
    assert(state->horizontalSequence != NULL);
	for (int i = 0; i < state->gridWidth; ++i)
	{
		state->horizontalSequence[i] = Uniform01Rand() < state->horizontalProbability;
	}

    state->verticalSequence = (bool*)malloc(state->gridHeight * sizeof(bool));
    assert(state->verticalSequence != NULL);
    for (int i = 0; i < state->gridHeight; ++i)
    {
        state->verticalSequence[i] = Uniform01Rand() < state->verticalProbability;
    }

    if (state->islands)
	{
		free(state->islands);
	}
    state->islands = (int*)calloc(state->gridWidth * state->gridHeight, sizeof(int));
}

static void UniformShift(AppState* state)
{
	for (int i = 1; i < state->gridWidth; ++i)
	{
		state->horizontalSequence[state->gridWidth - i] = state->horizontalSequence[state->gridWidth - 1 - i];
	}
	state->horizontalSequence[0] = Uniform01Rand() < state->horizontalProbability;

	for (int i = 1; i < state->gridHeight; ++i)
	{
		state->verticalSequence[state->gridHeight - i] = state->verticalSequence[state->gridHeight - 1 - i];
	}
	state->verticalSequence[0] = Uniform01Rand() < state->verticalProbability;
}

static void Scroll(AppState* state)
{
    for (int i = 1; i < state->gridWidth; ++i)
    {
        state->horizontalSequence[state->gridWidth - i] = state->horizontalSequence[state->gridWidth - 1 - i];
    }
    state->horizontalSequence[0] = Uniform01Rand() < state->horizontalProbability;

	for (int i = 0; i < state->gridHeight; ++i)
	{
		state->verticalSequence[i] = !state->verticalSequence[i];
	}
}

int main(void)
{
    srand(1023);
    AppState appState = {
        .windowWidth = 640,
        .windowHeight = 480,
        .verticalProbability = 0.5f,
        .horizontalProbability = 0.5f,
        .cellSize = 20,
        .gridWidth = 0,
        .gridHeight = 0,
        .horizontalSequence = NULL,
        .verticalSequence = NULL,
        .islands = NULL,
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
    RegenerateSequences(&appState);
    while (!WindowShouldClose())
    {
        UpdateDrawFrame(&appState);
    }

    CloseWindow();
    return 0;
}

#define FLATTEN(x, y) ((y) * (state->gridWidth) + (x))
#define FREE(x, y) (state->islands[FLATTEN(x, y)] == 0)
#define INB(x, y) ((x) >= 0 && (x) < state->gridWidth && (y) >= 0 && (y) < state->gridHeight)
#define H(x, y) ((((y) & 1) == 1) && (!state->horizontalSequence[(x)]) || (((y) & 1) != 1) && (state->horizontalSequence[(x)]))
#define V(x, y) ((((x) & 1) == 1) && (!state->verticalSequence[(y)]) || (((x) & 1) != 1) && (state->verticalSequence[(y)]))

typedef bool(*wallCheckFunc)(int x, int y, AppState* state);
bool horizontalCheckLeft(int x, int y, AppState* state) { return H(x, y); }
bool horizontalCheckRight(int x, int y, AppState* state) { return H(x + 1, y); }
bool verticalCheckUp(int x, int y, AppState* state) { return V(x, y); }
bool verticalCheckDown(int x, int y, AppState* state) { return V(x, y + 1); }

typedef struct FloodFillCheck_t
{
	int x;
	int y;
    wallCheckFunc f;
} FloodFillCheck;

static void FloodFill(AppState* state, int x, int y, int currentIsland)
{
    if (!FREE(x, y) || !INB(x, y))
	{
		return;
	}

    FloodFillCheck checks[] = {
		{ x + 1, y, &horizontalCheckRight },
		{ x - 1, y, &horizontalCheckLeft },
		{ x, y + 1, &verticalCheckDown },
		{ x, y - 1, &verticalCheckUp },
	};

    state->islands[FLATTEN(x, y)] = currentIsland;
    for (int i = 0; i < 4; ++i)
	{
		if (INB(checks[i].x, checks[i].y) && FREE(checks[i].x, checks[i].y))
		{
            if (checks[i].f(x, y, state))
            {
                FloodFill(state, checks[i].x, checks[i].y, currentIsland);
            }
			else
			{
				FloodFill(state, checks[i].x, checks[i].y, currentIsland ^ 6);
			}
		}
	}
}

void UpdateDrawFrame(AppState* state)
{
    if (IsWindowResized())
    {
		RegenerateSequences(state);
	}

    double currentTime = GetTime();
    bool updatePattern = state->updateSpeed != 0.0 && currentTime > (state->lastUpdateTime + (1.0f / state->updateSpeed));
    if (updatePattern)
    {
        state->lastUpdateTime = currentTime;
        switch (state->updateType)
        {
        case UPDATE_REGENERATE:
            RegenerateSequences(state);
            break;
        case UPDATE_SHIFT:
            UniformShift(state);
            break;
        case UPDATE_SCROLL:
            Scroll(state);
			break;
        }
    }

    if (state->colored)
    {
        // Flood-fill the islands
        memset(state->islands, 0, state->gridWidth * state->gridHeight * sizeof(int));
        FloodFill(state, 0, 0, 2);
    }

    BeginDrawing();
    {
        ClearBackground(WHITE);
        const UIUpdateResult uiUpdate = UpdateDrawUI(state);
        if (uiUpdate.shouldRegenerate)
		{
			RegenerateSequences(state);
		}

        const int cappedGridWidth = (uiUpdate.renderAreaWidth / state->cellSize) < state->gridWidth ? (uiUpdate.renderAreaWidth / state->cellSize - 1) : state->gridWidth;
        
        if (!state->colored)
        {
            // Horizontal pass
            for (int i = 0; i < cappedGridWidth; ++i)
            {
                const int offset = state->horizontalSequence[i] ? state->cellSize : 0;
                int x = i * state->cellSize;
                for (int j = 0; j < state->gridHeight; j += 2)
                {
                    int y = j * state->cellSize + offset;
                    DrawLine(x, y, x, y + state->cellSize, BLACK);
                }
            }

            // Vertical pass
            for (int i = 0; i < state->gridHeight; ++i)
            {
                int offset = state->verticalSequence[i] ? state->cellSize : 0;
                int y = i * state->cellSize;
                for (int j = 0; j < cappedGridWidth; j += 2)
                {
                    int x = j * state->cellSize + offset;
                    DrawLine(x, y, x + state->cellSize, y, BLACK);
                }
            }
        }
        else
        {
			for (int i = 0; i < state->gridHeight; ++i)
			{
				for (int j = 0; j < cappedGridWidth; ++j)
				{
                    Color color = state->islands[i * state->gridWidth + j] == 2 ? RED : GREEN;
					DrawRectangle(j * state->cellSize, i * state->cellSize, state->cellSize, state->cellSize, color);
				}
			}
        }
    }
    EndDrawing();
}

// Simplest possible layout system, top to bottom, possibly with half-width controls
typedef struct UILayout_t
{
    int controlWidth;
    int controlRectXStart;

    int currentY;
    bool isHalfFilled;
    int lastHalfHeight;
} UILayout;

#define TEXT_HEIGHT 20
#define FAT_CONTROL_HEIGHT 40

static Rectangle LayoutFull(UILayout* layout, bool isText)
{
    if (layout->isHalfFilled)
	{
		layout->currentY += layout->lastHalfHeight;
        layout->isHalfFilled = false;
    }

    const int height = isText ? TEXT_HEIGHT : FAT_CONTROL_HEIGHT;
    Rectangle result = { layout->controlRectXStart, layout->currentY, layout->controlWidth, height };
	layout->currentY += height;
	return result;
}

static Rectangle LayoutHalf(UILayout* layout, bool isText)
{
    const int height = isText ? TEXT_HEIGHT : FAT_CONTROL_HEIGHT;
    const int xStart = layout->isHalfFilled ? (layout->controlRectXStart + layout->controlWidth / 2) : layout->controlRectXStart;
    Rectangle result = { xStart, layout->currentY, layout->controlWidth / 2, height };
	layout->isHalfFilled = !layout->isHalfFilled;
	layout->lastHalfHeight = height;
	if (!layout->isHalfFilled)
	{
		layout->currentY += height;
	}
	return result;
}

static Rectangle LayoutCheckbox(UILayout* layout)
{
    if (layout->isHalfFilled)
    {
        layout->currentY += layout->lastHalfHeight;
        layout->isHalfFilled = false;
    }

    Rectangle result = { layout->controlRectXStart, layout->currentY, FAT_CONTROL_HEIGHT, FAT_CONTROL_HEIGHT };
    layout->currentY += FAT_CONTROL_HEIGHT;
    return result;
}

UIUpdateResult UpdateDrawUI(AppState* state)
{
    UILayout layout = {
		.controlWidth = 250,
		.controlRectXStart = state->windowWidth - 250,
	};


    GuiLabel(LayoutFull(&layout, true), "Cell size");
    float floatCellSize = (float)state->cellSize;
    const bool cellSizeChanged = GuiSlider(
        LayoutFull(&layout, false),
        NULL, NULL, &floatCellSize, 3, 60
    );
    state->cellSize = (int)floatCellSize;

    GuiLabel(LayoutHalf(&layout, true), "HP");
    GuiLabel(LayoutHalf(&layout, true), "VP");
    const bool hpChanged = GuiSlider(LayoutHalf(&layout, false), NULL, NULL, &state->horizontalProbability, 0.0, 1.0);
    const bool vpChanged = GuiSlider(LayoutHalf(&layout, false), NULL, NULL, &state->verticalProbability, 0.0, 1.0);

    Rectangle updateTypeLblRect = LayoutFull(&layout, true);
    Rectangle updateTypeRect = LayoutFull(&layout, false);
    GuiLabel(LayoutFull(&layout, true), "Update frequency");
    GuiSlider(LayoutFull(&layout, false), NULL, NULL, &state->updateSpeed, 0.0, 60.0);

    GuiLabel(updateTypeLblRect, "Update type");
    if (GuiDropdownBox(updateTypeRect, "REGENERATE;SHIFT;SCROLL", &state->updateType, state->updateTypeEditMode))
        state->updateTypeEditMode = !state->updateTypeEditMode;

    GuiCheckBox(LayoutCheckbox(&layout), "Colored", &state->colored);
    GuiCheckBox(LayoutCheckbox(&layout), "Show FPS", &state->showFPS);

    if (state->showFPS)
	{
		DrawFPS(layout.controlRectXStart + layout.controlWidth / 3 * 2, state->windowHeight - TEXT_HEIGHT);
	}

    return (UIUpdateResult) {
        .renderAreaWidth = layout.controlRectXStart,
        .shouldRegenerate = cellSizeChanged || hpChanged || vpChanged,
    };
}
