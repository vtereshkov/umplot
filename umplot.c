#include <float.h>
#include <math.h>

#include "raylib.h"
#include "umka_api.h"


enum
{
    STYLE_LINE = 1,
    STYLE_SCATTER
};


typedef struct 
{
    double x, y;
} Point;


typedef struct 
{
    int64_t kind;
    uint32_t color;
    double width;
} Style;


typedef struct
{
    Point *points;
    int64_t numPoints;
    Style style;
} Series;


typedef struct
{
    int64_t xNumLines, yNumLines;
    uint32_t color;
    bool labelled;
    int64_t fontSize;
} Grid;


typedef struct
{
    Series *series;
    int64_t numSeries;
    Grid grid;
} Plot;


typedef struct
{
    double dx, dy;
    double xScale, yScale;
} ScreenTransform;


static Rectangle getClientRect()
{
    const int width = GetScreenWidth(), height = GetScreenHeight();
    return (Rectangle){0.15 * width, 0.05 * height, 0.8 * width, 0.8 * height};
}


static Vector2 getScreenPoint(const Point point, const ScreenTransform *transform)
{
    return (Vector2){transform->xScale * (point.x - transform->dx), transform->yScale * (point.y - transform->dy)};
}


static Point getGraphPoint(const Vector2 point, const ScreenTransform *transform)
{
    return (Point){point.x / transform->xScale + transform->dx, point.y / transform->yScale + transform->dy};
}


static void setTransformToMinMax(ScreenTransform *transform, const Point *minPt, const Point *maxPt)
{
    Rectangle rect = getClientRect();

    transform->xScale = (maxPt->x > minPt->x) ?  (rect.width  / (maxPt->x - minPt->x)) : 1.0;
    transform->yScale = (maxPt->y > minPt->y) ? -(rect.height / (maxPt->y - minPt->y)) : 1.0; 

    transform->dx = minPt->x - rect.x / transform->xScale;
    transform->dy = maxPt->y - rect.y / transform->yScale;
}


static void resetTransform(ScreenTransform *transform, const Plot *plot)
{
    Point minPt = (Point){ DBL_MAX,  DBL_MAX};
    Point maxPt = (Point){-DBL_MAX, -DBL_MAX};

    for (int iSeries = 0; iSeries < plot->numSeries; iSeries++)
    {
        Series *series = &plot->series[iSeries];
        for (int iPt = 0; iPt < series->numPoints; iPt++)
        {
            const Point *pt = &series->points[iPt];
            if (pt->x > maxPt.x)  maxPt.x = pt->x;
            if (pt->x < minPt.x)  minPt.x = pt->x;
            if (pt->y > maxPt.y)  maxPt.y = pt->y;
            if (pt->y < minPt.y)  minPt.y = pt->y;
        }
    }

    setTransformToMinMax(transform, &minPt, &maxPt);
}


static void resizeTransform(ScreenTransform *transform, const Rectangle *rect)
{
    const Point minPt = getGraphPoint((Vector2){rect->x, rect->y + rect->height}, transform);
    const Point maxPt = getGraphPoint((Vector2){rect->x + rect->width, rect->y}, transform);

    setTransformToMinMax(transform, &minPt, &maxPt);
}


static void panTransform(ScreenTransform *transform, const Vector2 *delta)
{
    transform->dx -= delta->x / transform->xScale;
    transform->dy -= delta->y / transform->yScale;
}


static void zoomTransform(ScreenTransform *transform, const Plot *plot, const Rectangle *zoomRect)
{
    if (zoomRect->width == 0 && zoomRect->height == 0)
        return;

    if (zoomRect->width < 0 || zoomRect->height < 0)
    {
        resetTransform(transform, plot);
        return;
    }

    resizeTransform(transform, zoomRect);
}


static void drawGraph(const Plot *plot, const ScreenTransform *transform)
{
    Rectangle clientRect = getClientRect();
    BeginScissorMode(clientRect.x, clientRect.y, clientRect.width, clientRect.height);

    for (int iSeries = 0; iSeries < plot->numSeries; iSeries++)
    {
        Series *series = &plot->series[iSeries];

        switch (series->style.kind)
        {
            case STYLE_LINE:
            {
                if (series->numPoints > 1)
                {
                    Vector2 prevPt = getScreenPoint(series->points[0], transform);                
                    
                    for (int iPt = 1; iPt < series->numPoints; iPt++)
                    {
                        Vector2 pt = getScreenPoint(series->points[iPt], transform); 
                        DrawLineEx(prevPt, pt, series->style.width, *(Color *)&series->style.color);
                        prevPt = pt;
                    }
                }                
                break;
            }

            case STYLE_SCATTER:
            {
                for (int iPt = 0; iPt < series->numPoints; iPt++)
                {
                    Vector2 pt = getScreenPoint(series->points[iPt], transform); 
                    DrawCircleV(pt, series->style.width, *(Color *)&series->style.color);
                }
                break;
            }

            default: break;
        }
    }

    EndScissorMode();    
}


static void drawGrid(const Plot *plot, const ScreenTransform *transform)
{
    if (plot->grid.xNumLines <= 0 || plot->grid.yNumLines <= 0)
        return;

    const Rectangle clientRect = getClientRect();

    const double xSpan =  clientRect.width  / transform->xScale;
    const double ySpan = -clientRect.height / transform->yScale;

    double xStep = pow(10.0, floor(log10(xSpan / plot->grid.xNumLines)));
    double yStep = pow(10.0, floor(log10(ySpan / plot->grid.yNumLines)));

    while (xSpan / xStep > 2.0 * plot->grid.xNumLines)
        xStep *= 2.0;

    while (ySpan / yStep > 2.0 * plot->grid.yNumLines)
        yStep *= 2.0;    

    const Point minPt   = getGraphPoint((Vector2){clientRect.x, clientRect.y + clientRect.height}, transform);
    const Point startPt = (Point){ceil(minPt.x / xStep) * xStep, ceil(minPt.y / yStep) * yStep};

    Vector2 startPtScreen = getScreenPoint(startPt, transform);

    // Vertical grid
    for (int i = 0, x = startPtScreen.x; x < clientRect.x + clientRect.width; i++, x = startPtScreen.x + i * xStep * transform->xScale)
    {
        // Line
        DrawLineEx((Vector2){x, clientRect.y}, (Vector2){x, clientRect.y + clientRect.height}, 1, *(Color *)&plot->grid.color);

        // Label
        if (plot->grid.labelled)
        {
            const char *label = TextFormat((xStep > 0.01) ? "%.2f" : "%.4f", startPt.x + i * xStep);
            const int labelWidth = MeasureText(label, plot->grid.fontSize);

            const int labelX = x - labelWidth / 2;
            const int labelY = clientRect.y + clientRect.height + plot->grid.fontSize;

            DrawText(label, labelX, labelY, plot->grid.fontSize, *(Color *)&plot->grid.color);            
        }
    }

    // Horizontal grid
    for (int j = 0, y = startPtScreen.y; y > clientRect.y; j++, y = startPtScreen.y + j * yStep * transform->yScale)
    {
        // Line
        DrawLineEx((Vector2){clientRect.x, y}, (Vector2){clientRect.x + clientRect.width, y}, 1, *(Color *)&plot->grid.color);

        // Label
        if (plot->grid.labelled)
        {
            const char *label = TextFormat((yStep > 0.01) ? "%.2f" : "%.4f", startPt.y + j * yStep);
            const int labelWidth = MeasureText(label, plot->grid.fontSize);

            const int labelX = clientRect.x - labelWidth - plot->grid.fontSize;
            const int labelY = y - plot->grid.fontSize / 2;

            DrawText(label, labelX, labelY, plot->grid.fontSize, *(Color *)&plot->grid.color);           
        }
    }
}


static void drawZoomRect(Rectangle zoomRect, const ScreenTransform *transform)
{
    if (zoomRect.width < 0)
    {
        zoomRect.x += zoomRect.width;
        zoomRect.width *= -1;
    }
    if (zoomRect.height < 0)
    {
        zoomRect.y += zoomRect.height;
        zoomRect.height *= -1;
    }

    DrawRectangleLinesEx(zoomRect, 1, GRAY);
}


void umplot_plot(UmkaStackSlot *params, UmkaStackSlot *result)
{
    Plot *plot = (Plot *) params[0].ptrVal;

    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(640, 480, "UmPlot");
    SetTargetFPS(60);

    Rectangle clientRect = getClientRect();
    Rectangle zoomRect = clientRect;
    bool showZoomRect = false;
    
    ScreenTransform transform;
    resetTransform(&transform, plot);    

    while (!WindowShouldClose())
    {
        // Handle input
        const Vector2 pos = GetMousePosition();
        const Vector2 delta = GetMouseDelta();

        // Resizing
        if (IsWindowResized())
        {
            resizeTransform(&transform, &clientRect);
            clientRect = zoomRect = getClientRect();
        }

        // Zooming
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(pos, clientRect))  
        {            
            zoomRect = (Rectangle){pos.x, pos.y, 0, 0};
            showZoomRect = true;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(pos, clientRect))  
        {            
            zoomRect.width  = pos.x - zoomRect.x;
            zoomRect.height = pos.y - zoomRect.y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))  
        {            
            zoomTransform(&transform, plot, &zoomRect);
            zoomRect = getClientRect();
            showZoomRect = false;
        }        

        // Panning
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(pos, clientRect))  
        {    
            panTransform(&transform, &delta);
        }            

        // Draw
        BeginDrawing();
        ClearBackground(WHITE);
        
        // Graph border
        DrawRectangleLinesEx(clientRect, 1, BLACK);

        // Grid
        drawGrid(plot, &transform);

        // Graph
        drawGraph(plot, &transform);

        // Zoom rectangle
        if (showZoomRect)
            drawZoomRect(zoomRect, &transform);

        EndDrawing();
    }

    CloseWindow();
    result->intVal = 1;
}
