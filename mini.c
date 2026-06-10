#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
// Fallback non-blocking getch implementation for Unix systems
int getch(void) {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif
#define KEY_UP    1000
#define KEY_DOWN  1001
#define KEY_LEFT  1002
#define KEY_RIGHT 1003
#define KEY_ENTER 13
#define KEY_SPACE 32
#define KEY_ESC   27
int get_key(void) {
    int ch = getch();
#ifdef _WIN32
    if (ch == 0 || ch == 224) {
        int next_ch = getch();
        switch (next_ch) {
            case 72: return KEY_UP;   // Arrow Up
            case 80: return KEY_DOWN; // Arrow Down
            case 75: return KEY_LEFT; // Arrow Left
            case 77: return KEY_RIGHT;// Arrow Right
            default: return -1;
        }
    }
#else
    if (ch == 27) { // ESC sequence on Unix
        // We use non-blocking check or short read. For termios fallback, we can do direct nested reads.
        int next1 = getch();
        if (next1 == 91) {
            int next2 = getch();
            switch (next2) {
                case 65: return KEY_UP;
                case 66: return KEY_DOWN;
                case 67: return KEY_RIGHT;
                case 68: return KEY_LEFT;
                default: return -1;
            }
        }
        return KEY_ESC;
    }
#endif
    // Convert Unix newline (10) to KEY_ENTER (13) for consistency
    if (ch == 10) return KEY_ENTER;
    
    return ch;
}
#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif
#define WIDTH 50
#define HEIGHT 20
#define MAX_SHAPES 100
typedef enum {
    SHAPE_LINE = 1,
    SHAPE_RECTANGLE,
    SHAPE_CIRCLE,
    SHAPE_TRIANGLE
} ShapeType;
const char* shape_type_name(ShapeType t) {
    switch(t) {
        case SHAPE_LINE:      return "Line";
        case SHAPE_RECTANGLE: return "Rectangle";
        case SHAPE_CIRCLE:    return "Circle";
        case SHAPE_TRIANGLE:  return "Triangle";
        default:              return "Unknown";
    }
}
typedef struct {
    int x1, y1;
    int x2, y2;
} LineData;
typedef struct {
    int x, y;
    int w, h;
} RectData;
typedef struct {
    int cx, cy;
    int r;
} CircleData;
typedef struct {
    int x1, y1;
    int x2, y2;
    int x3, y3;
} TriangleData;
typedef struct {
    ShapeType type;
    char draw_char;
    union {
        LineData line;
        RectData rect;
        CircleData circle;
        TriangleData triangle;
    } data;
} Shape;
Shape shapes[MAX_SHAPES];
int shape_count = 0;
char canvas[HEIGHT][WIDTH];
int cursor_x = WIDTH / 2;
int cursor_y = HEIGHT / 2;
int show_cursor = 1;
char canvas_color[HEIGHT][WIDTH];
int current_draw_color = 1; // 1 = cyan, 2 = yellow, 3 = green/magenta
int selected_shape_idx = -1;
Shape preview_shape;
int show_preview = 0;
// Enable virtual terminal processing for ANSI escape support in Windows consoles
void enable_ansi_escapes() {
#ifdef _WIN32
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}
// Clear console screen using ANSI escape sequences
void clear_screen() {
    printf("\033[H\033[J");
}
// Set a pixel in the canvas with boundary checks
void set_pixel(char canvas[HEIGHT][WIDTH], int px, int py, char ch) {
    if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
        canvas[py][px] = ch;
        canvas_color[py][px] = current_draw_color;
    }
}
// Bresenham's Line Algorithm
void draw_line(char canvas[HEIGHT][WIDTH], int x1, int y1, int x2, int y2, char ch) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    while (1) {
        set_pixel(canvas, x1, y1, ch);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}
// Outline Rectangle drawing
void draw_rectangle(char canvas[HEIGHT][WIDTH], int x, int y, int w, int h, char ch) {
    if (w <= 0 || h <= 0) return;
    // Draw horizontal borders
    for (int i = 0; i < w; i++) {
        set_pixel(canvas, x + i, y, ch);
        set_pixel(canvas, x + i, y + h - 1, ch);
    }
    // Draw vertical borders
    for (int i = 0; i < h; i++) {
        set_pixel(canvas, x, y + i, ch);
        set_pixel(canvas, x + w - 1, y + i, ch);
    }
}
// Midpoint Circle Drawing Algorithm
void draw_circle(char canvas[HEIGHT][WIDTH], int cx, int cy, int r, char ch) {
    if (r < 0) return;
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        set_pixel(canvas, cx + x, cy + y, ch);
        set_pixel(canvas, cx - x, cy + y, ch);
        set_pixel(canvas, cx + x, cy - y, ch);
        set_pixel(canvas, cx - x, cy - y, ch);
        set_pixel(canvas, cx + y, cy + x, ch);
        set_pixel(canvas, cx - y, cy + x, ch);
        set_pixel(canvas, cx + y, cy - x, ch);
        set_pixel(canvas, cx - y, cy - x, ch);
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}
// Triangle outline drawing by connecting vertices
void draw_triangle(char canvas[HEIGHT][WIDTH], int x1, int y1, int x2, int y2, int x3, int y3, char ch) {
    draw_line(canvas, x1, y1, x2, y2, ch);
    draw_line(canvas, x2, y2, x3, y3, ch);
    draw_line(canvas, x3, y3, x1, y1, ch);
}
void draw_shape_to_canvas(char cv[HEIGHT][WIDTH], Shape* s) {
    switch (s->type) {
        case SHAPE_LINE:
            draw_line(cv, s->data.line.x1, s->data.line.y1, s->data.line.x2, s->data.line.y2, s->draw_char);
            break;
        case SHAPE_RECTANGLE:
            draw_rectangle(cv, s->data.rect.x, s->data.rect.y, s->data.rect.w, s->data.rect.h, s->draw_char);
            break;
        case SHAPE_CIRCLE:
            draw_circle(cv, s->data.circle.cx, s->data.circle.cy, s->data.circle.r, s->draw_char);
            break;
        case SHAPE_TRIANGLE:
            draw_triangle(cv, s->data.triangle.x1, s->data.triangle.y1, s->data.triangle.x2, s->data.triangle.y2, s->data.triangle.x3, s->data.triangle.y3, s->draw_char);
            break;
    }
}
// Render all active shapes from shapes array into canvas
void render_all_shapes() {
    // Clear canvas first
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            canvas[y][x] = '_';
            canvas_color[y][x] = 0;
        }
    }
    // Rasterize each shape
    for (int i = 0; i < shape_count; i++) {
        if (i == selected_shape_idx) {
            current_draw_color = 2; // Yellow for selected shape
        } else {
            current_draw_color = 1; // Cyan for normal shape
        }
        draw_shape_to_canvas(canvas, &shapes[i]);
    }
    // Rasterize preview shape if active
    if (show_preview) {
        current_draw_color = 3; // Magenta/Green for preview shape
        draw_shape_to_canvas(canvas, &preview_shape);
    }
}
// Display the canvas with row/col rulers and double-width scaling to keep 1:1 aspect ratio
void display_canvas(char cv[HEIGHT][WIDTH]) {
    // Print tens column guide
    printf("     ");
    for (int x = 0; x < WIDTH; x++) {
        if (x % 10 == 0) {
            printf("%d ", x / 10);
        } else {
            printf("  ");
        }
    }
    printf("\n");
    // Print units column guide
    printf("     ");
    for (int x = 0; x < WIDTH; x++) {
        printf("%d ", x % 10);
    }
    printf("\n");
    // Print top border (ASCII)
    printf("   +-");
    for (int x = 0; x < WIDTH * 2; x++) printf("-");
    printf("+\n");
    // Print each row
    for (int y = 0; y < HEIGHT; y++) {
        printf("%02d | ", y);
        for (int x = 0; x < WIDTH; x++) {
            if (show_cursor && x == cursor_x && y == cursor_y) {
                // Interactive cursor: bold yellow plus '+'
                printf("\033[1;33m+\033[0m ");
            } else {
                char ch = cv[y][x];
                int col = canvas_color[y][x];
                if (ch == '_') {
                    // Dim gray for background character
                    printf("\033[90m_\033[0m ");
                } else {
                    if (col == 2) {
                        // Selected: bold yellow
                        printf("\033[1;33m%c\033[0m ", ch);
                    } else if (col == 3) {
                        // Preview: bold green
                        printf("\033[1;32m%c\033[0m ", ch);
                    } else {
                        // Normal shape: cyan
                        printf("\033[36m%c\033[0m ", ch);
                    }
                }
            }
        }
        printf("|\n");
    }
    // Print bottom border (ASCII)
    printf("   +-");
    for (int x = 0; x < WIDTH * 2; x++) printf("-");
    printf("+\n");
}
// Safe integer input function using fgets to prevent stream crashes
int get_int_input(const char* prompt, int min_val, int max_val) {
    int val;
    char buffer[128];
    while (1) {
        printf("%s", prompt);
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            continue;
        }
        buffer[strcspn(buffer, "\n")] = 0; // trim newline
        
        if (strlen(buffer) == 0) {
            printf("\033[31mInput cannot be empty.\033[0m\n");
            continue;
        }
        
        char* endptr;
        val = (int)strtol(buffer, &endptr, 10);
        if (endptr == buffer || *endptr != '\0') {
            printf("\033[31mInvalid integer. Please try again.\033[0m\n");
            continue;
        }
        
        if (val < min_val || val > max_val) {
            printf("\033[31mOut of bounds [%d, %d]. Please try again.\033[0m\n", min_val, max_val);
            continue;
        }
        
        return val;
    }
}
// Safe character input function
char get_char_input(const char* prompt, char default_char) {
    char buffer[128];
    while (1) {
        printf("%s (default '%c'): ", prompt, default_char);
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            return default_char;
        }
        buffer[strcspn(buffer, "\n")] = 0;
        
        if (strlen(buffer) == 0) {
            return default_char;
        }
        
        if (strlen(buffer) > 1) {
            printf("\033[31mPlease enter a single character.\033[0m\n");
            continue;
        }
        
        if (buffer[0] < 33 || buffer[0] > 126) {
            printf("\033[31mMust be a printable non-whitespace character.\033[0m\n");
            continue;
        }
        
        return buffer[0];
    }
}
// Print text descriptions of all current shapes
void print_shape_list() {
    printf("\n\033[1;33m--- Shapes List ---\033[0m\n");
    if (shape_count == 0) {
        printf("\033[90m(Empty canvas)\033[0m\n");
        return;
    }
    for (int i = 0; i < shape_count; i++) {
        Shape* s = &shapes[i];
        printf("  [%d] \033[1m%s\033[0m ('%c') -> ", i + 1, shape_type_name(s->type), s->draw_char);
        switch (s->type) {
            case SHAPE_LINE:
                printf("Line from (%d, %d) to (%d, %d)\n", s->data.line.x1, s->data.line.y1, s->data.line.x2, s->data.line.y2);
                break;
            case SHAPE_RECTANGLE:
                printf("Rectangle top-left (%d, %d), size %dx%d\n", s->data.rect.x, s->data.rect.y, s->data.rect.w, s->data.rect.h);
                break;
            case SHAPE_CIRCLE:
                printf("Circle center (%d, %d), radius %d\n", s->data.circle.cx, s->data.circle.cy, s->data.circle.r);
                break;
            case SHAPE_TRIANGLE:
                printf("Triangle vertices: (%d, %d), (%d, %d), (%d, %d)\n", s->data.triangle.x1, s->data.triangle.y1, s->data.triangle.x2, s->data.triangle.y2, s->data.triangle.x3, s->data.triangle.y3);
                break;
        }
    }
}
// Dialog loop to add a shape
void add_shape_menu() {
    printf("\n\033[1;36m=== Add Shape ===\033[0m\n");
    printf("  1. Line\n");
    printf("  2. Rectangle\n");
    printf("  3. Circle\n");
    printf("  4. Triangle\n");
    printf("  5. Cancel (Back)\n");
    
    int choice = get_int_input("Choose option [1-5]: ", 1, 5);
    if (choice == 5) return;
    
    if (shape_count >= MAX_SHAPES) {
        printf("\033[31mError: Maximum shape count reached (%d).\033[0m\n", MAX_SHAPES);
        printf("Press Enter to return...");
        getchar();
        return;
    }
    
    Shape s;
    s.type = (ShapeType)choice;
    s.draw_char = get_char_input("Drawing character", '*');
    
    switch (s.type) {
        case SHAPE_LINE: {
            int x1 = cursor_x, y1 = cursor_y;
            
            // Step 1: Select start point
            while (1) {
                clear_screen();
                printf("\033[1;36m=== DRAW LINE: Select Start Point ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Arrows] Move Cursor    [Space] Set Start Point    [Q/Esc] Cancel\033[0m\n");
                printf("Cursor at: (%d, %d)\n", cursor_x, cursor_y);
                
                int key = get_key();
                if (key == KEY_UP && cursor_y > 0) cursor_y--;
                else if (key == KEY_DOWN && cursor_y < HEIGHT - 1) cursor_y++;
                else if (key == KEY_LEFT && cursor_x > 0) cursor_x--;
                else if (key == KEY_RIGHT && cursor_x < WIDTH - 1) cursor_x++;
                else if (key == KEY_SPACE) {
                    x1 = cursor_x;
                    y1 = cursor_y;
                    break;
                }
                else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    return;
                }
            }
            
            // Step 2: Select end point (with preview)
            show_preview = 1;
            preview_shape.type = SHAPE_LINE;
            preview_shape.draw_char = s.draw_char;
            preview_shape.data.line.x1 = x1;
            preview_shape.data.line.y1 = y1;
            
            while (1) {
                preview_shape.data.line.x2 = cursor_x;
                preview_shape.data.line.y2 = cursor_y;
                render_all_shapes();
                
                clear_screen();
                printf("\033[1;36m=== DRAW LINE: Select End Point ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Arrows] Move Cursor (Preview updates)    [Enter] Place Line    [Q/Esc] Cancel\033[0m\n");
                printf("Line from (%d, %d) to (%d, %d)\n", x1, y1, cursor_x, cursor_y);
                
                int key = get_key();
                if (key == KEY_UP && cursor_y > 0) cursor_y--;
                else if (key == KEY_DOWN && cursor_y < HEIGHT - 1) cursor_y++;
                else if (key == KEY_LEFT && cursor_x > 0) cursor_x--;
                else if (key == KEY_RIGHT && cursor_x < WIDTH - 1) cursor_x++;
                else if (key == KEY_ENTER) {
                    s.data.line.x1 = x1;
                    s.data.line.y1 = y1;
                    s.data.line.x2 = cursor_x;
                    s.data.line.y2 = cursor_y;
                    break;
                }
                else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    show_preview = 0;
                    render_all_shapes();
                    return;
                }
            }
            show_preview = 0;
            break;
        }
            
        case SHAPE_RECTANGLE: {
            int rx = cursor_x, ry = cursor_y;
            
            // Step 1: Select top-left corner
            while (1) {
                clear_screen();
                printf("\033[1;36m=== DRAW RECTANGLE: Select Top-Left Corner ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Arrows] Move Cursor    [Space] Set Top-Left Corner    [Q/Esc] Cancel\033[0m\n");
                printf("Cursor at: (%d, %d)\n", cursor_x, cursor_y);
                
                int key = get_key();
                if (key == KEY_UP && cursor_y > 0) cursor_y--;
                else if (key == KEY_DOWN && cursor_y < HEIGHT - 1) cursor_y++;
                else if (key == KEY_LEFT && cursor_x > 0) cursor_x--;
                else if (key == KEY_RIGHT && cursor_x < WIDTH - 1) cursor_x++;
                else if (key == KEY_SPACE) {
                    rx = cursor_x;
                    ry = cursor_y;
                    break;
                }
                else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    return;
                }
            }
            
            // Step 2: Select bottom-right corner (with preview)
            show_preview = 1;
            preview_shape.type = SHAPE_RECTANGLE;
            preview_shape.draw_char = s.draw_char;
            preview_shape.data.rect.x = rx;
            preview_shape.data.rect.y = ry;
            
            while (1) {
                int x_start = min(rx, cursor_x);
                int y_start = min(ry, cursor_y);
                int w = abs(cursor_x - rx) + 1;
                int h = abs(cursor_y - ry) + 1;
                
                preview_shape.data.rect.x = x_start;
                preview_shape.data.rect.y = y_start;
                preview_shape.data.rect.w = w;
                preview_shape.data.rect.h = h;
                
                render_all_shapes();
                
                clear_screen();
                printf("\033[1;36m=== DRAW RECTANGLE: Select Bottom-Right Corner ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Arrows] Move Cursor (Preview updates)    [Enter] Place Rectangle    [Q/Esc] Cancel\033[0m\n");
                printf("Rectangle: top-left (%d, %d), size %dx%d\n", x_start, y_start, w, h);
                
                int key = get_key();
                if (key == KEY_UP && cursor_y > 0) cursor_y--;
                else if (key == KEY_DOWN && cursor_y < HEIGHT - 1) cursor_y++;
                else if (key == KEY_LEFT && cursor_x > 0) cursor_x--;
                else if (key == KEY_RIGHT && cursor_x < WIDTH - 1) cursor_x++;
                else if (key == KEY_ENTER) {
                    s.data.rect.x = x_start;
                    s.data.rect.y = y_start;
                    s.data.rect.w = w;
                    s.data.rect.h = h;
                    break;
                }
                else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    show_preview = 0;
                    render_all_shapes();
                    return;
                }
            }
            show_preview = 0;
            break;
        }
            
        case SHAPE_CIRCLE: {
            int cx = cursor_x, cy = cursor_y;
            
            // Step 1: Select center
            while (1) {
                clear_screen();
                printf("\033[1;36m=== DRAW CIRCLE: Select Center Point ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Arrows] Move Cursor    [Space] Set Center Point    [Q/Esc] Cancel\033[0m\n");
                printf("Cursor at: (%d, %d)\n", cursor_x, cursor_y);
                
                int key = get_key();
                if (key == KEY_UP && cursor_y > 0) cursor_y--;
                else if (key == KEY_DOWN && cursor_y < HEIGHT - 1) cursor_y++;
                else if (key == KEY_LEFT && cursor_x > 0) cursor_x--;
                else if (key == KEY_RIGHT && cursor_x < WIDTH - 1) cursor_x++;
                else if (key == KEY_SPACE) {
                    cx = cursor_x;
                    cy = cursor_y;
                    break;
                }
                else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    return;
                }
            }
            
            // Step 2: Select radius point (with preview)
            show_preview = 1;
            preview_shape.type = SHAPE_CIRCLE;
            preview_shape.draw_char = s.draw_char;
            preview_shape.data.circle.cx = cx;
            preview_shape.data.circle.cy = cy;
            
            while (1) {
                int dx = cursor_x - cx;
                int dy = cursor_y - cy;
                int r = (int)round(sqrt(dx*dx + dy*dy));
                
                preview_shape.data.circle.r = r;
                render_all_shapes();
                
                clear_screen();
                printf("\033[1;36m=== DRAW CIRCLE: Set Radius ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Arrows] Move Cursor (Preview updates)    [Enter] Place Circle    [Q/Esc] Cancel\033[0m\n");
                printf("Circle: center (%d, %d), radius %d\n", cx, cy, r);
                
                int key = get_key();
                if (key == KEY_UP && cursor_y > 0) cursor_y--;
                else if (key == KEY_DOWN && cursor_y < HEIGHT - 1) cursor_y++;
                else if (key == KEY_LEFT && cursor_x > 0) cursor_x--;
                else if (key == KEY_RIGHT && cursor_x < WIDTH - 1) cursor_x++;
                else if (key == KEY_ENTER) {
                    s.data.circle.cx = cx;
                    s.data.circle.cy = cy;
                    s.data.circle.r = r;
                    break;
                }
                else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    show_preview = 0;
                    render_all_shapes();
                    return;
                }
            }
            show_preview = 0;
            break;
        }
            
        case SHAPE_TRIANGLE: {
            int tx1 = cursor_x, ty1 = cursor_y;
            int tx2 = cursor_x, ty2 = cursor_y;
            
            // Step 1: Select Vertex 1
            while (1) {
                clear_screen();
                printf("\033[1;36m=== DRAW TRIANGLE: Select Vertex 1 ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Arrows] Move Cursor    [Space] Set Vertex 1    [Q/Esc] Cancel\033[0m\n");
                printf("Cursor at: (%d, %d)\n", cursor_x, cursor_y);
                
                int key = get_key();
                if (key == KEY_UP && cursor_y > 0) cursor_y--;
                else if (key == KEY_DOWN && cursor_y < HEIGHT - 1) cursor_y++;
                else if (key == KEY_LEFT && cursor_x > 0) cursor_x--;
                else if (key == KEY_RIGHT && cursor_x < WIDTH - 1) cursor_x++;
                else if (key == KEY_SPACE) {
                    tx1 = cursor_x;
                    ty1 = cursor_y;
                    break;
                }
                else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    return;
                }
            }
            
            // Step 2: Select Vertex 2 (with line preview)
            show_preview = 1;
            preview_shape.type = SHAPE_LINE;
            preview_shape.draw_char = s.draw_char;
            preview_shape.data.line.x1 = tx1;
            preview_shape.data.line.y1 = ty1;
            
            while (1) {
                preview_shape.data.line.x2 = cursor_x;
                preview_shape.data.line.y2 = cursor_y;
                render_all_shapes();
                
                clear_screen();
                printf("\033[1;36m=== DRAW TRIANGLE: Select Vertex 2 ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Arrows] Move Cursor    [Space] Set Vertex 2    [Q/Esc] Cancel\033[0m\n");
                printf("Vertex 1 at (%d, %d), Current cursor at (%d, %d)\n", tx1, ty1, cursor_x, cursor_y);
                
                int key = get_key();
                if (key == KEY_UP && cursor_y > 0) cursor_y--;
                else if (key == KEY_DOWN && cursor_y < HEIGHT - 1) cursor_y++;
                else if (key == KEY_LEFT && cursor_x > 0) cursor_x--;
                else if (key == KEY_RIGHT && cursor_x < WIDTH - 1) cursor_x++;
                else if (key == KEY_SPACE) {
                    tx2 = cursor_x;
                    ty2 = cursor_y;
                    break;
                }
                else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    show_preview = 0;
                    render_all_shapes();
                    return;
                }
            }
            
            // Step 3: Select Vertex 3 (with triangle preview)
            preview_shape.type = SHAPE_TRIANGLE;
            preview_shape.data.triangle.x1 = tx1;
            preview_shape.data.triangle.y1 = ty1;
            preview_shape.data.triangle.x2 = tx2;
            preview_shape.data.triangle.y2 = ty2;
            
            while (1) {
                preview_shape.data.triangle.x3 = cursor_x;
                preview_shape.data.triangle.y3 = cursor_y;
                render_all_shapes();
                
                clear_screen();
                printf("\033[1;36m=== DRAW TRIANGLE: Select Vertex 3 ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Arrows] Move Cursor (Preview updates)    [Enter] Place Triangle    [Q/Esc] Cancel\033[0m\n");
                printf("Vertices: V1(%d, %d), V2(%d, %d), V3(%d, %d)\n", tx1, ty1, tx2, ty2, cursor_x, cursor_y);
                
                int key = get_key();
                if (key == KEY_UP && cursor_y > 0) cursor_y--;
                else if (key == KEY_DOWN && cursor_y < HEIGHT - 1) cursor_y++;
                else if (key == KEY_LEFT && cursor_x > 0) cursor_x--;
                else if (key == KEY_RIGHT && cursor_x < WIDTH - 1) cursor_x++;
                else if (key == KEY_ENTER) {
                    s.data.triangle.x1 = tx1;
                    s.data.triangle.y1 = ty1;
                    s.data.triangle.x2 = tx2;
                    s.data.triangle.y2 = ty2;
                    s.data.triangle.x3 = cursor_x;
                    s.data.triangle.y3 = cursor_y;
                    break;
                }
                else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    show_preview = 0;
                    render_all_shapes();
                    return;
                }
            }
            show_preview = 0;
            break;
        }
    }
    
    shapes[shape_count++] = s;
    render_all_shapes();
    printf("\033[32mShape added successfully!\033[0m\n");
    printf("Press Enter to continue...");
    getchar();
}
// Dialog loop to delete a shape
void delete_shape_menu() {
    if (shape_count == 0) {
        printf("\nNo shapes available to delete. Press Enter to return...");
        fflush(stdout);
        getchar();
        return;
    }
    
    selected_shape_idx = 0;
    
    while (1) {
        render_all_shapes();
        clear_screen();
        printf("\033[1;31m=== DELETE SHAPE: Select Shape to Delete ===\033[0m\n");
        display_canvas(canvas);
        
        printf("\n\033[1;32m[Left/Right] Cycle Shapes    [Enter] Select to Delete    [Q/Esc] Cancel\033[0m\n");
        Shape* s = &shapes[selected_shape_idx];
        printf("\033[1;33mCurrently highlighted shape: [%d] %s ('%c')\033[0m -> ", selected_shape_idx + 1, shape_type_name(s->type), s->draw_char);
        switch (s->type) {
            case SHAPE_LINE:
                printf("Line from (%d, %d) to (%d, %d)\n", s->data.line.x1, s->data.line.y1, s->data.line.x2, s->data.line.y2);
                break;
            case SHAPE_RECTANGLE:
                printf("Rectangle top-left (%d, %d), size %dx%d\n", s->data.rect.x, s->data.rect.y, s->data.rect.w, s->data.rect.h);
                break;
            case SHAPE_CIRCLE:
                printf("Circle center (%d, %d), radius %d\n", s->data.circle.cx, s->data.circle.cy, s->data.circle.r);
                break;
            case SHAPE_TRIANGLE:
                printf("Triangle vertices: (%d, %d), (%d, %d), (%d, %d)\n", s->data.triangle.x1, s->data.triangle.y1, s->data.triangle.x2, s->data.triangle.y2, s->data.triangle.x3, s->data.triangle.y3);
                break;
        }
        
        int key = get_key();
        if (key == KEY_LEFT || key == KEY_DOWN) {
            selected_shape_idx = (selected_shape_idx - 1 + shape_count) % shape_count;
        } else if (key == KEY_RIGHT || key == KEY_UP) {
            selected_shape_idx = (selected_shape_idx + 1) % shape_count;
        } else if (key == KEY_ENTER) {
            printf("\nAre you sure you want to delete shape [%d]? (y/n): ", selected_shape_idx + 1);
            fflush(stdout);
            int confirm = getch();
            if (confirm == 'y' || confirm == 'Y') {
                int idx = selected_shape_idx;
                for (int i = idx; i < shape_count - 1; i++) {
                    shapes[i] = shapes[i + 1];
                }
                shape_count--;
                selected_shape_idx = -1;
                render_all_shapes();
                printf("\n\033[32mShape deleted successfully!\033[0m\n");
                printf("Press Enter to continue...");
                fflush(stdout);
                getchar();
                return;
            }
        } else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
            selected_shape_idx = -1;
            render_all_shapes();
            return;
        }
    }
}
// Dialog loop to modify details of an existing shape
void modify_shape_menu() {
    printf("\n\033[1;33m=== Modify Shape ===\033[0m\n");
    if (shape_count == 0) {
        printf("No shapes available to modify. Press Enter to return...\n");
        fflush(stdout);
        getchar();
        return;
    }
    
    selected_shape_idx = 0;
    
    // Step 1: Select shape to modify
    while (1) {
        render_all_shapes();
        clear_screen();
        printf("\033[1;33m=== MODIFY SHAPE: Select Shape to Modify ===\033[0m\n");
        display_canvas(canvas);
        
        printf("\n\033[1;32m[Left/Right] Cycle Shapes    [Enter] Select to Modify    [Q/Esc] Cancel\033[0m\n");
        Shape* s = &shapes[selected_shape_idx];
        printf("\033[1;33mCurrently highlighted shape: [%d] %s ('%c')\033[0m -> ", selected_shape_idx + 1, shape_type_name(s->type), s->draw_char);
        switch (s->type) {
            case SHAPE_LINE:
                printf("Line from (%d, %d) to (%d, %d)\n", s->data.line.x1, s->data.line.y1, s->data.line.x2, s->data.line.y2);
                break;
            case SHAPE_RECTANGLE:
                printf("Rectangle top-left (%d, %d), size %dx%d\n", s->data.rect.x, s->data.rect.y, s->data.rect.w, s->data.rect.h);
                break;
            case SHAPE_CIRCLE:
                printf("Circle center (%d, %d), radius %d\n", s->data.circle.cx, s->data.circle.cy, s->data.circle.r);
                break;
            case SHAPE_TRIANGLE:
                printf("Triangle vertices: (%d, %d), (%d, %d), (%d, %d)\n", s->data.triangle.x1, s->data.triangle.y1, s->data.triangle.x2, s->data.triangle.y2, s->data.triangle.x3, s->data.triangle.y3);
                break;
        }
        
        int key = get_key();
        if (key == KEY_LEFT || key == KEY_DOWN) {
            selected_shape_idx = (selected_shape_idx - 1 + shape_count) % shape_count;
        } else if (key == KEY_RIGHT || key == KEY_UP) {
            selected_shape_idx = (selected_shape_idx + 1) % shape_count;
        } else if (key == KEY_ENTER) {
            break;
        } else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
            selected_shape_idx = -1;
            render_all_shapes();
            return;
        }
    }
    
    int index = selected_shape_idx;
    selected_shape_idx = -1; // stop selection highlight for sub-actions
    Shape* s = &shapes[index];
    Shape original_shape = *s;
    
    // Step 2: Show modification action menu
    clear_screen();
    printf("\nModifying Shape [%d] (%s, char '%c'):\n", index + 1, shape_type_name(s->type), s->draw_char);
    printf("  1. Translate (Move shape)\n");
    printf("  2. Scale (Resize shape)\n");
    printf("  3. Change drawing character\n");
    printf("  4. Cancel\n");
    int choice = get_int_input("Choose option [1-4]: ", 1, 4);
    if (choice == 4) {
        render_all_shapes();
        return;
    }
    
    if (choice == 3) {
        s->draw_char = get_char_input("New character", s->draw_char);
        render_all_shapes();
        printf("\033[32mCharacter changed successfully!\033[0m\n");
        printf("Press Enter to continue...");
        getchar();
        return;
    }
    
    if (choice == 1) {
        // Translation loop
        while (1) {
            selected_shape_idx = index; // highlight it during move
            render_all_shapes();
            clear_screen();
            printf("\033[1;33m=== MODIFY SHAPE: Translate (Move) ===\033[0m\n");
            display_canvas(canvas);
            printf("\n\033[1;32m[Arrows] Move Shape    [Enter] Save Changes    [Q/Esc] Cancel\033[0m\n");
            
            int key = get_key();
            int dx = 0, dy = 0;
            if (key == KEY_UP) dy = -1;
            else if (key == KEY_DOWN) dy = 1;
            else if (key == KEY_LEFT) dx = -1;
            else if (key == KEY_RIGHT) dx = 1;
            else if (key == KEY_ENTER) {
                break;
            } else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                *s = original_shape;
                break;
            }
            
            if (dx != 0 || dy != 0) {
                if (s->type == SHAPE_LINE) {
                    int tx1 = s->data.line.x1 + dx; int ty1 = s->data.line.y1 + dy;
                    int tx2 = s->data.line.x2 + dx; int ty2 = s->data.line.y2 + dy;
                    if (tx1 >= 0 && tx1 < WIDTH && ty1 >= 0 && ty1 < HEIGHT &&
                        tx2 >= 0 && tx2 < WIDTH && ty2 >= 0 && ty2 < HEIGHT) {
                        s->data.line.x1 = tx1; s->data.line.y1 = ty1;
                        s->data.line.x2 = tx2; s->data.line.y2 = ty2;
                    }
                } else if (s->type == SHAPE_RECTANGLE) {
                    int rx = s->data.rect.x + dx; int ry = s->data.rect.y + dy;
                    if (rx >= 0 && rx + s->data.rect.w <= WIDTH &&
                        ry >= 0 && ry + s->data.rect.h <= HEIGHT) {
                        s->data.rect.x = rx; s->data.rect.y = ry;
                    }
                } else if (s->type == SHAPE_CIRCLE) {
                    int cx = s->data.circle.cx + dx; int cy = s->data.circle.cy + dy;
                    if (cx >= 0 && cx < WIDTH && cy >= 0 && cy < HEIGHT) {
                        s->data.circle.cx = cx; s->data.circle.cy = cy;
                    }
                } else if (s->type == SHAPE_TRIANGLE) {
                    int tx1 = s->data.triangle.x1 + dx; int ty1 = s->data.triangle.y1 + dy;
                    int tx2 = s->data.triangle.x2 + dx; int ty2 = s->data.triangle.y2 + dy;
                    int tx3 = s->data.triangle.x3 + dx; int ty3 = s->data.triangle.y3 + dy;
                    if (tx1 >= 0 && tx1 < WIDTH && ty1 >= 0 && ty1 < HEIGHT &&
                        tx2 >= 0 && tx2 < WIDTH && ty2 >= 0 && ty2 < HEIGHT &&
                        tx3 >= 0 && tx3 < WIDTH && ty3 >= 0 && ty3 < HEIGHT) {
                        s->data.triangle.x1 = tx1; s->data.triangle.y1 = ty1;
                        s->data.triangle.x2 = tx2; s->data.triangle.y2 = ty2;
                        s->data.triangle.x3 = tx3; s->data.triangle.y3 = ty3;
                    }
                }
            }
        }
    }
    
    if (choice == 2) {
        // Scaling / Resizing loop
        if (s->type == SHAPE_CIRCLE) {
            while (1) {
                selected_shape_idx = index;
                render_all_shapes();
                clear_screen();
                printf("\033[1;33m=== MODIFY CIRCLE: Resize ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[+/Right/Up] Increase Radius    [-/Left/Down] Decrease Radius (Current R: %d)    [Enter] Save    [Q/Esc] Cancel\033[0m\n", s->data.circle.r);
                
                int key = get_key();
                if (key == KEY_UP || key == KEY_RIGHT || key == '+' || key == '=') {
                    s->data.circle.r++;
                } else if (key == KEY_DOWN || key == KEY_LEFT || key == '-' || key == '_') {
                    if (s->data.circle.r > 0) s->data.circle.r--;
                } else if (key == KEY_ENTER) {
                    break;
                } else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    *s = original_shape;
                    break;
                }
            }
        } else if (s->type == SHAPE_RECTANGLE) {
            while (1) {
                selected_shape_idx = index;
                render_all_shapes();
                clear_screen();
                printf("\033[1;33m=== MODIFY RECTANGLE: Resize ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Right/Left] Width (%d)    [Up/Down] Height (%d)    [Enter] Save    [Q/Esc] Cancel\033[0m\n", s->data.rect.w, s->data.rect.h);
                
                int key = get_key();
                if (key == KEY_UP) {
                    if (s->data.rect.h > 1) s->data.rect.h--;
                } else if (key == KEY_DOWN) {
                    if (s->data.rect.y + s->data.rect.h < HEIGHT) s->data.rect.h++;
                } else if (key == KEY_LEFT) {
                    if (s->data.rect.w > 1) s->data.rect.w--;
                } else if (key == KEY_RIGHT) {
                    if (s->data.rect.x + s->data.rect.w < WIDTH) s->data.rect.w++;
                } else if (key == KEY_ENTER) {
                    break;
                } else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    *s = original_shape;
                    break;
                }
            }
        } else if (s->type == SHAPE_LINE) {
            int active_vertex = 0; // 0 = start, 1 = end
            show_cursor = 1;
            while (1) {
                selected_shape_idx = index;
                cursor_x = (active_vertex == 0) ? s->data.line.x1 : s->data.line.x2;
                cursor_y = (active_vertex == 0) ? s->data.line.y1 : s->data.line.y2;
                render_all_shapes();
                clear_screen();
                printf("\033[1;33m=== MODIFY LINE: Move Vertex ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Tab/Space] Cycle Vertices (Active: Vertex %d)    [Arrows] Move Active Vertex    [Enter] Save    [Q/Esc] Cancel\033[0m\n", active_vertex + 1);
                
                int key = get_key();
                if (key == KEY_SPACE || key == '\t') {
                    active_vertex = (active_vertex + 1) % 2;
                } else if (key == KEY_UP) {
                    if (active_vertex == 0) { if (s->data.line.y1 > 0) s->data.line.y1--; }
                    else { if (s->data.line.y2 > 0) s->data.line.y2--; }
                } else if (key == KEY_DOWN) {
                    if (active_vertex == 0) { if (s->data.line.y1 < HEIGHT - 1) s->data.line.y1++; }
                    else { if (s->data.line.y2 < HEIGHT - 1) s->data.line.y2++; }
                } else if (key == KEY_LEFT) {
                    if (active_vertex == 0) { if (s->data.line.x1 > 0) s->data.line.x1--; }
                    else { if (s->data.line.x2 > 0) s->data.line.x2--; }
                } else if (key == KEY_RIGHT) {
                    if (active_vertex == 0) { if (s->data.line.x1 < WIDTH - 1) s->data.line.x1++; }
                    else { if (s->data.line.x2 < WIDTH - 1) s->data.line.x2++; }
                } else if (key == KEY_ENTER) {
                    break;
                } else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    *s = original_shape;
                    break;
                }
            }
        } else if (s->type == SHAPE_TRIANGLE) {
            int active_vertex = 0; // 0 = V1, 1 = V2, 2 = V3
            show_cursor = 1;
            while (1) {
                selected_shape_idx = index;
                cursor_x = (active_vertex == 0) ? s->data.triangle.x1 : (active_vertex == 1) ? s->data.triangle.x2 : s->data.triangle.x3;
                cursor_y = (active_vertex == 0) ? s->data.triangle.y1 : (active_vertex == 1) ? s->data.triangle.y2 : s->data.triangle.y3;
                render_all_shapes();
                clear_screen();
                printf("\033[1;33m=== MODIFY TRIANGLE: Move Vertex ===\033[0m\n");
                display_canvas(canvas);
                printf("\n\033[1;32m[Tab/Space] Cycle Vertices (Active: Vertex %d)    [Arrows] Move Active Vertex    [Enter] Save    [Q/Esc] Cancel\033[0m\n", active_vertex + 1);
                
                int key = get_key();
                if (key == KEY_SPACE || key == '\t') {
                    active_vertex = (active_vertex + 1) % 3;
                } else if (key == KEY_UP) {
                    if (active_vertex == 0) { if (s->data.triangle.y1 > 0) s->data.triangle.y1--; }
                    else if (active_vertex == 1) { if (s->data.triangle.y2 > 0) s->data.triangle.y2--; }
                    else { if (s->data.triangle.y3 > 0) s->data.triangle.y3--; }
                } else if (key == KEY_DOWN) {
                    if (active_vertex == 0) { if (s->data.triangle.y1 < HEIGHT - 1) s->data.triangle.y1++; }
                    else if (active_vertex == 1) { if (s->data.triangle.y2 < HEIGHT - 1) s->data.triangle.y2++; }
                    else { if (s->data.triangle.y3 < HEIGHT - 1) s->data.triangle.y3++; }
                } else if (key == KEY_LEFT) {
                    if (active_vertex == 0) { if (s->data.triangle.x1 > 0) s->data.triangle.x1--; }
                    else if (active_vertex == 1) { if (s->data.triangle.x2 > 0) s->data.triangle.x2--; }
                    else { if (s->data.triangle.x3 > 0) s->data.triangle.x3--; }
                } else if (key == KEY_RIGHT) {
                    if (active_vertex == 0) { if (s->data.triangle.x1 < WIDTH - 1) s->data.triangle.x1++; }
                    else if (active_vertex == 1) { if (s->data.triangle.x2 < WIDTH - 1) s->data.triangle.x2++; }
                    else { if (s->data.triangle.x3 < WIDTH - 1) s->data.triangle.x3++; }
                } else if (key == KEY_ENTER) {
                    break;
                } else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                    *s = original_shape;
                    break;
                }
            }
        }
    }
    
    selected_shape_idx = -1;
    render_all_shapes();
    printf("\033[32mShape modified successfully!\033[0m\n");
    printf("Press Enter to continue...");
    fflush(stdout);
    getchar();
}
int main() {
    enable_ansi_escapes();
    
    // Seed with only the circle shape as requested
    shapes[shape_count].type = SHAPE_CIRCLE;
    shapes[shape_count].draw_char = '*';
    shapes[shape_count].data.circle.cx = 25;
    shapes[shape_count].data.circle.cy = 10;
    shapes[shape_count].data.circle.r = 6;
    shape_count++;
    render_all_shapes();
    while (1) {
        clear_screen();
        printf("\033[1;35m==================================================================================\033[0m\n");
        printf("\033[1;36m                     2D CHARACTER GRAPHICS VECTOR EDITOR                          \033[0m\n");
        printf("\033[1;35m==================================================================================\033[0m\n");
        
        display_canvas(canvas);
        print_shape_list();
        
        printf("\n\033[1;32m[A] Add Shape    [D] Delete Shape    [M] Modify Shape    [C] Clear Canvas    [Q] Quit    [Arrows] Move Cursor (%02d, %02d)\033[0m\n", cursor_x, cursor_y);
        printf("Select command: ");
        fflush(stdout);
        
        int ch = get_key();
        
        if (ch == KEY_UP) {
            if (cursor_y > 0) cursor_y--;
        } else if (ch == KEY_DOWN) {
            if (cursor_y < HEIGHT - 1) cursor_y++;
        } else if (ch == KEY_LEFT) {
            if (cursor_x > 0) cursor_x--;
        } else if (ch == KEY_RIGHT) {
            if (cursor_x < WIDTH - 1) cursor_x++;
        } else if (ch == 'a' || ch == 'A') {
            add_shape_menu();
        } else if (ch == 'd' || ch == 'D') {
            delete_shape_menu();
        } else if (ch == 'm' || ch == 'M') {
            modify_shape_menu();
        } else if (ch == 'c' || ch == 'C') {
            printf("\nAre you sure you want to clear all shapes? (y/n): ");
            fflush(stdout);
            int confirm = getch();
            if (confirm == 'y' || confirm == 'Y') {
                shape_count = 0;
                render_all_shapes();
                printf("\n\033[32mCanvas cleared!\033[0m\n");
                printf("Press Enter to continue...");
                getchar();
            }
        } else if (ch == 'q' || ch == 'Q') {
            printf("\nExiting editor. Goodbye!\n");
            break;
        }
    }
    
    return 0;
}
