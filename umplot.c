#include <stddef.h>
#include <float.h>
#include <math.h>

#include "raylib.h"
#include "umka_api.h"
#include "font.h"


#ifdef _WIN32
    #define UMPLOT_API __declspec(dllexport)
#else
    #define UMPLOT_API __attribute__((visibility("default")))
#endif


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
    UmkaDynArray(Point) points;
    char *name;
    Style style;
} Series;


typedef struct
{
    int64_t xNumLines, yNumLines;
    uint32_t color;
    int64_t fontSize;
    bool visible, labelled;
} Grid;


typedef struct
{
    char *x, *y, *graph;
    uint32_t color;
    int64_t fontSize;
    bool visible;
} Titles;


typedef struct
{
    bool visible;
} Legend;


typedef struct
{
    UmkaDynArray(Series) series;
    Grid grid;
    Titles titles;
    Legend legend;
} Plot;


typedef struct
{
    double dx, dy;
    double xScale, yScale;
} ScreenTransform;


static Rectangle getClientRectWithLegend()
{
    const int width = GetScreenWidth(), height = GetScreenHeight();
    return (Rectangle){0.15 * width, 0.05 * height, 0.8 * width, 0.8 * height};
}


static Rectangle getLegendRect(const Plot *plot, UmkaAPI *api)
{
    const int dashLength = 20, margin = 20;
    Rectangle legendRect = {0};

    if (!plot->legend.visible)
        return legendRect;

    for (int iSeries = 0; iSeries < api->umkaGetDynArrayLen(&plot->series); iSeries++)
    {
        const int labelWidth = MeasureText(plot->series.data[iSeries].name, plot->grid.fontSize);
        if (labelWidth > legendRect.width)
            legendRect.width = labelWidth;
    }
    
    legendRect.width += dashLength + 2 * margin;

    const Rectangle clientRectWithLegend = getClientRectWithLegend();

    if (legendRect.width > clientRectWithLegend.width / 2)
        legendRect.width = clientRectWithLegend.width / 2;

    legendRect.height = clientRectWithLegend.height;
    legendRect.x = clientRectWithLegend.x + clientRectWithLegend.width - legendRect.width;
    legendRect.y = clientRectWithLegend.y;

    return legendRect;
}


static Rectangle getClientRect(const Plot *plot, UmkaAPI *api)
{
    const Rectangle clientRectWithLegend = getClientRectWithLegend();
    const Rectangle legendRect = getLegendRect(plot, api);

    Rectangle clientRect = clientRectWithLegend;
    clientRect.width -= legendRect.width;

    return clientRect;
}


static Vector2 getScreenPoint(const Point point, const ScreenTransform *transform)
{
    return (Vector2){transform->xScale * (point.x - transform->dx), transform->yScale * (point.y - transform->dy)};
}


static Point getGraphPoint(const Vector2 point, const ScreenTransform *transform)
{
    return (Point){point.x / transform->xScale + transform->dx, point.y / transform->yScale + transform->dy};
}


static void setTransformToMinMax(const Plot *plot, ScreenTransform *transform, const Point *minPt, const Point *maxPt, UmkaAPI *api)
{
    Rectangle rect = getClientRect(plot, api);

    transform->xScale = (maxPt->x > minPt->x) ?  (rect.width  / (maxPt->x - minPt->x)) : 1.0;
    transform->yScale = (maxPt->y > minPt->y) ? -(rect.height / (maxPt->y - minPt->y)) : 1.0; 

    transform->dx = minPt->x - rect.x / transform->xScale;
    transform->dy = maxPt->y - rect.y / transform->yScale;
}


static void resetTransform(const Plot *plot, ScreenTransform *transform, UmkaAPI *api)
{
    Point minPt = (Point){ DBL_MAX,  DBL_MAX};
    Point maxPt = (Point){-DBL_MAX, -DBL_MAX};

    for (int iSeries = 0; iSeries < api->umkaGetDynArrayLen(&plot->series); iSeries++)
    {
        Series *series = &plot->series.data[iSeries];
        for (int iPt = 0; iPt < api->umkaGetDynArrayLen(&series->points); iPt++)
        {
            const Point *pt = &series->points.data[iPt];
            if (pt->x > maxPt.x)  maxPt.x = pt->x;
            if (pt->x < minPt.x)  minPt.x = pt->x;
            if (pt->y > maxPt.y)  maxPt.y = pt->y;
            if (pt->y < minPt.y)  minPt.y = pt->y;
        }
    }

    setTransformToMinMax(plot, transform, &minPt, &maxPt, api);
}


static void resizeTransform(const Plot *plot, ScreenTransform *transform, const Rectangle *rect, UmkaAPI *api)
{
    const Point minPt = getGraphPoint((Vector2){rect->x, rect->y + rect->height}, transform);
    const Point maxPt = getGraphPoint((Vector2){rect->x + rect->width, rect->y}, transform);

    setTransformToMinMax(plot, transform, &minPt, &maxPt, api);
}


static void panTransform(const Plot *plot, ScreenTransform *transform, const Vector2 *delta)
{
    transform->dx -= delta->x / transform->xScale;
    transform->dy -= delta->y / transform->yScale;
}


static void zoomTransform(const Plot *plot, ScreenTransform *transform, const Rectangle *zoomRect, UmkaAPI *api)
{
    if (zoomRect->width == 0 && zoomRect->height == 0)
        return;

    if (zoomRect->width < 0 || zoomRect->height < 0)
    {
        resetTransform(plot, transform, api);
        return;
    }

    resizeTransform(plot, transform, zoomRect, api);
}


static void drawGraph(const Plot *plot, const ScreenTransform *transform, UmkaAPI *api)
{
    Rectangle clientRect = getClientRect(plot, api);
    BeginScissorMode(clientRect.x, clientRect.y, clientRect.width, clientRect.height);

    for (int iSeries = 0; iSeries < api->umkaGetDynArrayLen(&plot->series); iSeries++)
    {
        Series *series = &plot->series.data[iSeries];
        switch (series->style.kind)
        {
            case STYLE_LINE:
            {
                if (api->umkaGetDynArrayLen(&series->points) > 1)
                {
                    Vector2 prevPt = getScreenPoint(series->points.data[0], transform);                
                    
                    for (int iPt = 1; iPt < api->umkaGetDynArrayLen(&series->points); iPt++)
                    {
                        Vector2 pt = getScreenPoint(series->points.data[iPt], transform); 
                        DrawLineEx(prevPt, pt, series->style.width, *(Color *)&series->style.color);
                        prevPt = pt;
                    }
                }                
                break;
            }

            case STYLE_SCATTER:
            {
                for (int iPt = 0; iPt < api->umkaGetDynArrayLen(&series->points); iPt++)
                {
                    Vector2 pt = getScreenPoint(series->points.data[iPt], transform); 
                    DrawCircleV(pt, series->style.width, *(Color *)&series->style.color);
                }
                break;
            }

            default: break;
        }
    }

    EndScissorMode();    
}


static void drawGrid(const Plot *plot, const ScreenTransform *transform, const Font *font, int *maxYLabelWidth, UmkaAPI *api)
{
    if (maxYLabelWidth)
        *maxYLabelWidth = 0;

    if (plot->grid.xNumLines <= 0 || plot->grid.yNumLines <= 0)
        return;

    const Rectangle clientRect = getClientRect(plot, api);

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
        if (plot->grid.visible)
            DrawLineEx((Vector2){x, clientRect.y}, (Vector2){x, clientRect.y + clientRect.height}, 1, *(Color *)&plot->grid.color);

        // Label
        if (plot->grid.labelled)
        {
            const char *label = TextFormat((xStep > 0.01) ? "%.2f" : "%.4f", startPt.x + i * xStep);
            const int labelWidth = MeasureTextEx(*font, label, plot->grid.fontSize, 1).x;

            const int labelX = x - labelWidth / 2;
            const int labelY = clientRect.y + clientRect.height + plot->grid.fontSize;

            DrawTextEx(*font, label, (Vector2){labelX, labelY}, plot->grid.fontSize, 1, *(Color *)&plot->grid.color);            
        }
    }

    // Horizontal grid
    for (int j = 0, y = startPtScreen.y; y > clientRect.y; j++, y = startPtScreen.y + j * yStep * transform->yScale)
    {
        // Line
        if (plot->grid.visible)
            DrawLineEx((Vector2){clientRect.x, y}, (Vector2){clientRect.x + clientRect.width, y}, 1, *(Color *)&plot->grid.color);

        // Label
        if (plot->grid.labelled)
        {
            const char *label = TextFormat((yStep > 0.01) ? "%.2f" : "%.4f", startPt.y + j * yStep);
            const int labelWidth = MeasureTextEx(*font, label, plot->grid.fontSize, 1).x;

            const int labelX = clientRect.x - labelWidth - plot->grid.fontSize;
            const int labelY = y - plot->grid.fontSize / 2;

            DrawTextEx(*font, label, (Vector2){labelX, labelY}, plot->grid.fontSize, 1, *(Color *)&plot->grid.color);

            if (maxYLabelWidth && labelWidth > *maxYLabelWidth)
                *maxYLabelWidth = labelWidth;                       
        }
    }
}


static void drawTitles(const Plot *plot, const ScreenTransform *transform, const Font *font, int maxYLabelWidth, UmkaAPI *api)
{
    if (!plot->titles.visible)
        return;

    Rectangle clientRect = getClientRect(plot, api);

    // Horizontal axis
    if (plot->titles.x && TextLength(plot->titles.x) > 0)
    {
        const int titleWidth = MeasureTextEx(*font, plot->titles.x, plot->titles.fontSize, 1).x;

        const int titleX = clientRect.x + clientRect.width / 2 - titleWidth / 2;
        const int titleY = clientRect.y + clientRect.height + 2 * plot->grid.fontSize + plot->titles.fontSize;

        DrawTextEx(*font, plot->titles.x, (Vector2){titleX, titleY}, plot->titles.fontSize, 1, *(Color *)&plot->titles.color);
    }

    // Vertical axis
    if (plot->titles.y && TextLength(plot->titles.y) > 0)
    {
        const int titleWidth = MeasureTextEx(*font, plot->titles.y, plot->titles.fontSize, 1).x;

        const int titleX = clientRect.x - 2 * plot->grid.fontSize - plot->titles.fontSize - maxYLabelWidth;
        const int titleY = clientRect.y + clientRect.height / 2 + titleWidth / 2;

        DrawTextPro(*font, plot->titles.y, (Vector2){titleX, titleY}, (Vector2){0, 0}, -90.0, plot->titles.fontSize, 1, *(Color *)&plot->titles.color);
    }

    // Graph
    if (plot->titles.graph && TextLength(plot->titles.graph) > 0)
    {
        const int titleWidth = MeasureTextEx(*font, plot->titles.graph, plot->titles.fontSize, 1).x;

        const int titleX = clientRect.x + clientRect.width / 2 - titleWidth / 2;
        const int titleY = clientRect.y - 2 * plot->titles.fontSize;

        DrawTextEx(*font, plot->titles.graph, (Vector2){titleX, titleY}, plot->titles.fontSize, 1, *(Color *)&plot->titles.color);
    }        
}


static void drawLegend(const Plot *plot, const Font *font, UmkaAPI *api)
{
    if (!plot->legend.visible)
        return;    

    const int dashLength = 20, margin = 20;

    const Rectangle legendRect = getLegendRect(plot, api);

    for (int iSeries = 0; iSeries < api->umkaGetDynArrayLen(&plot->series); iSeries++)
    {
        Series *series = &plot->series.data[iSeries];
        
        // Legend mark
        switch (series->style.kind)
        {
            case STYLE_LINE:
            {
                Vector2 dashPt1 = (Vector2){legendRect.x + margin, legendRect.y + plot->grid.fontSize / 2 + iSeries * (plot->grid.fontSize + margin)};
                Vector2 dashPt2 = dashPt1;
                dashPt2.x += dashLength;

                DrawLineEx(dashPt1, dashPt2, series->style.width, *(Color *)&series->style.color);
                break;
            }

            case STYLE_SCATTER:
            {
                Vector2 pt = (Vector2){legendRect.x + margin + dashLength / 2, legendRect.y + plot->grid.fontSize / 2 + iSeries * (plot->grid.fontSize + margin)};

                DrawCircleV(pt, series->style.width, *(Color *)&series->style.color);
                break;
            }

            default: break;
        }

        // Legend text
        const int labelX = legendRect.x + dashLength + 2 * margin;
        const int labelY = legendRect.y + iSeries * (plot->grid.fontSize + margin);

        DrawTextEx(*font, series->name, (Vector2){labelX, labelY}, plot->grid.fontSize, 1, *(Color *)&plot->grid.color);  

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


UMPLOT_API void umplot_plot(UmkaStackSlot *params, UmkaStackSlot *result)
{
    Plot *plot = (Plot *) params[0].ptrVal;

    void *umka = result->ptrVal;
    UmkaAPI *api = umkaGetAPI(umka);

    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "UmPlot");
    SetTargetFPS(30);

    Font gridFont = LoadFontFromMemory(".ttf", liberationFont, sizeof(liberationFont), plot->grid.fontSize, NULL, 256);
    Font titlesFont = LoadFontFromMemory(".ttf", liberationFont, sizeof(liberationFont), plot->titles.fontSize, NULL, 256);

    Rectangle clientRect = getClientRect(plot, api);
    Rectangle zoomRect = clientRect;
    bool showZoomRect = false;
    
    ScreenTransform transform;
    resetTransform(plot, &transform, api);    

    while (!WindowShouldClose())
    {
        // Handle input
        const Vector2 pos = GetMousePosition();
        const Vector2 delta = GetMouseDelta();

        // Resizing
        if (IsWindowResized())
        {
            resizeTransform(plot, &transform, &clientRect, api);
            clientRect = zoomRect = getClientRect(plot, api);
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
            zoomTransform(plot, &transform, &zoomRect, api);
            zoomRect = getClientRect(plot, api);
            showZoomRect = false;
        }        

        // Panning
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(pos, clientRect))  
        {    
            panTransform(plot, &transform, &delta);
        }            

        // Draw
        BeginDrawing();
        ClearBackground(WHITE);
        
        // Border
        DrawRectangleLinesEx(clientRect, 1, BLACK);

        // Grid
        int maxYLabelWidth = 0;
        drawGrid(plot, &transform, &gridFont, &maxYLabelWidth, api);

        // Graph
        drawGraph(plot, &transform, api);

        // Titles
        drawTitles(plot, &transform, &titlesFont, maxYLabelWidth, api);

        // Legend
        drawLegend(plot, &gridFont, api);

        // Zoom rectangle
        if (showZoomRect)
            drawZoomRect(zoomRect, &transform);

        EndDrawing();
    }

    CloseWindow();
    result->intVal = 1;
}
