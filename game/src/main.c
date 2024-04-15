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

    int old00Island;

    double lastUpdateTime;
    float updateSpeed; // How many time per second to update
    int updateType;
    bool updateTypeEditMode;
    bool showFPS;
    bool colored;

    int diagonalScrollDirection;
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

static void GenericScroll(bool* primarySequence, int primaryLength, float primaryProbability, bool* secondarySequence, int secondaryLength)
{
    for (int i = 1; i < primaryLength; ++i)
    {
        primarySequence[primaryLength - i] = primarySequence[primaryLength - 1 - i];
    }
    primarySequence[0] = Uniform01Rand() < primaryProbability;

    for (int i = 0; i < secondaryLength; ++i)
    {
        secondarySequence[i] = !secondarySequence[i];
    }
}

static void Scroll(AppState* state)
{
    GenericScroll(state->horizontalSequence, state->gridWidth, state->horizontalProbability, state->verticalSequence, state->gridHeight);
}

static void DiagonalScroll(AppState* state)
{
	if (state->diagonalScrollDirection == 0)
	{
		GenericScroll(state->horizontalSequence, state->gridWidth, state->horizontalProbability, state->verticalSequence, state->gridHeight);
	}
	else
	{
		GenericScroll(state->verticalSequence, state->gridHeight, state->verticalProbability, state->horizontalSequence, state->gridWidth);
	}
    state->diagonalScrollDirection = !state->diagonalScrollDirection;
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
        .old00Island = 0,
        .lastUpdateTime = 0.0,
        .updateSpeed = 10.0,
        .updateType = UPDATE_REGENERATE,
        .updateTypeEditMode = false,
        .colored = false,
        .diagonalScrollDirection = 0,
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

static void IterativeFill(AppState* state)
{
    memset(state->islands, 0, state->gridWidth * state->gridHeight * sizeof(int));

    int currentIsland = 2;
    if (state->old00Island != 0)
    {
        switch (state->updateType)
        {
        case UPDATE_SCROLL:
            currentIsland = state->horizontalSequence[1] ? state->old00Island : state->old00Island ^ 6;
            break;
        case UPDATE_SHIFT:
            const bool keep = state->diagonalScrollDirection == 1 && state->horizontalSequence[1] || state->diagonalScrollDirection == 0 && state->verticalSequence[1];
            currentIsland = keep ? state->old00Island : state->old00Island ^ 6;
            break;
        }
    }

    for (int y = 0; y < state->gridHeight; ++y)
	{
        const bool yOdd = (y & 1) == 1;
		for (int x = 0; x < state->gridWidth; ++x)
		{
			state->islands[y * state->gridWidth + x] = currentIsland;
            if (x + 1 < state->gridWidth)
            {
                const bool nextShifted = state->horizontalSequence[x + 1];
                const bool keep = yOdd && !nextShifted || !yOdd && nextShifted;
                currentIsland = keep ? currentIsland : currentIsland ^ 6;
            }
		}

        currentIsland = state->islands[y * state->gridWidth];
        if (y + 1 < state->gridHeight)
		{
			currentIsland = state->verticalSequence[y + 1] ? currentIsland : currentIsland ^ 6;
		}
	}

	state->old00Island = state->islands[0];
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
            DiagonalScroll(state);
            break;
        case UPDATE_SCROLL:
            Scroll(state);
			break;
        }
        if (state->colored)
        {
            IterativeFill(state);
        }
    }

    BeginDrawing();
    {
        ClearBackground(WHITE);
        const UIUpdateResult uiUpdate = UpdateDrawUI(state);
        if (uiUpdate.shouldRegenerate)
		{
			RegenerateSequences(state);
            state->old00Island = 0;
            if (state->colored)
            {
                IterativeFill(state);
            }
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
