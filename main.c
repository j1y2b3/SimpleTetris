#include "PDCurses/curses.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define BOARD_WIDTH 20
#define BOARD_HEIGHT 20
#define BOARD_LEFT 1
#define SIDEBAR_WIDTH 25

#define PIECES_NUM 8
#define PIECE_MAX_SIZE 4

#define KEY_TIMEOUT 16
#define DROP_INTERVAL 1

#define MAX_SCORE 99

int board[BOARD_HEIGHT][BOARD_WIDTH] = { 0 };
int window_width, window_height;
int score = 0;

struct Piece {
    int shape[PIECE_MAX_SIZE][PIECE_MAX_SIZE];
    int width, height, left, top;
} pieces[PIECES_NUM];

void get_piece_meta(struct Piece* piece) {
    piece->width = 0;
    piece->left = -1;
    for (int i = 0; i < PIECE_MAX_SIZE; i++) {
        for (int j = 0; j < PIECE_MAX_SIZE; j++) {
            if (piece->shape[j][i]) {
                ++(piece->width);
                if (piece->left == -1) {
                    piece->left = i;
                }
                break;
            }
        }
    }

    piece->height = 0;
    piece->top = -1;
    for (int i = 0; i < PIECE_MAX_SIZE; i++) {
        for (int j = 0; j < PIECE_MAX_SIZE; j++) {
            if (piece->shape[i][j]) {
                ++(piece->height);
                if (piece->top == -1) {
                    piece->top = i;
                }
                break;
            }
        }
    }
}

int read_pieces(const char* file) {
    FILE* fp = fopen(file, "r");
    if (fp == NULL) {
        perror("fopen");
        return ERR;
    }

    for (int piece_idx = 0; piece_idx < PIECES_NUM; piece_idx++) {
        for (int i = 0; i < PIECE_MAX_SIZE; i++) {
            for (int j = 0; j < PIECE_MAX_SIZE; j++) {
                if (fscanf(fp, "%d", &pieces[piece_idx].shape[i][j]) != 1) {
                    return ERR;
                }
            }
        }
        get_piece_meta(&pieces[piece_idx]);
    }

    fclose(fp);
    return OK;
}

void put_piece(struct Piece* piece, int x, int y, int color) {
    for (int i = piece->top; i < piece->top + piece->height; i++) {
        int by = y + i - piece->top;
        if (by < 0 || BOARD_HEIGHT <= by) continue;

        for (int j = piece->left; j < piece->left + piece->width; j++) {
            if (!piece->shape[i][j]) continue;

            int bx = x + j - piece->left;
            if (bx < 0 || BOARD_WIDTH <= bx) continue;

            board[by][bx] = color;
        }
    }
}

void move_piece(struct Piece* piece, int xfrom, int yfrom, int xto, int yto) {
    put_piece(piece, xfrom, yfrom, 0);
    put_piece(piece, xto, yto, 1);
}

void rotate_piece(struct Piece* piece) {
    // 行列转置
    for (int i = 0; i < PIECE_MAX_SIZE; i++) {
        for (int j = i + 1; j < PIECE_MAX_SIZE; j++) {
            int temp = piece->shape[i][j];
            piece->shape[i][j] = piece->shape[j][i];
            piece->shape[j][i] = temp;
        }
    }
    // 左右翻转
    for (int i = 0; i < PIECE_MAX_SIZE; i++) {
        for (int j = 0; j < PIECE_MAX_SIZE / 2; j++) {
            int temp = piece->shape[i][j];
            piece->shape[i][j] = piece->shape[i][PIECE_MAX_SIZE - j - 1];
            piece->shape[i][PIECE_MAX_SIZE - j - 1] = temp;
        }
    }
    // 状态刷新
    get_piece_meta(piece);
}

void del_row(int row) {
    if (row < 0 || BOARD_HEIGHT <= row) return;

    for (int i = row; i > 0; i--) {
        for (int j = 0; j < BOARD_WIDTH; j++) {
            board[i][j] = board[i - 1][j];
        }
    }
    for (int j = 0; j < BOARD_WIDTH; j++) {
        board[0][j] = 0;
    }
    // 得分
    ++score;
    PlaySound(TEXT("audio/getscore.wav"), NULL, SND_FILENAME | SND_ASYNC);
}

// 判断方块能否放在 (x, y)：不越界且不与已有方块重叠
int can_place(struct Piece* piece, int x, int y) {
    for (int i = piece->top; i < piece->top + piece->height; i++) {
        int ny = y + i - piece->top;

        for (int j = piece->left; j < piece->left + piece->width; j++) {
            if (!piece->shape[i][j]) continue;

            int nx = x + j - piece->left;
            if (nx < 0 || BOARD_WIDTH <= nx) return 0;
            if (ny >= BOARD_HEIGHT) return 0;
            if (ny >= 0 && board[ny][nx]) return 0;
        }
    }
    return 1;
}

// 尝试将方块移动 (dx, dy)，成功返回 1
int try_move(int piece_idx, int* x, int* y, int dx, int dy) {
    put_piece(&pieces[piece_idx], *x, *y, 0);

    if (can_place(&pieces[piece_idx], *x + dx, *y + dy)) {
        *x += dx;
        *y += dy;
        put_piece(&pieces[piece_idx], *x, *y, 1);
        return 1;
    }

    put_piece(&pieces[piece_idx], *x, *y, 1);
    return 0;
}

// 尝试旋转方块，必要时左右平移一格让出空间，成功返回 1
int try_rotate(int piece_idx, int* x, int* y) {
    static const int kicks[] = { 0, -1, 1, -2, 2 };

    struct Piece rotated = pieces[piece_idx];
    rotate_piece(&rotated);

    put_piece(&pieces[piece_idx], *x, *y, 0);

    for (int i = 0; i < 5; i++) {
        int nx = *x + kicks[i];
        if (can_place(&rotated, nx, *y)) {
            pieces[piece_idx] = rotated;
            *x = nx;
            put_piece(&pieces[piece_idx], *x, *y, 1);
            return 1;
        }
    }

    put_piece(&pieces[piece_idx], *x, *y, 1);
    return 0;
}

// 消除所有满行
void clear_full_rows() {
    for (int i = BOARD_HEIGHT - 1; i >= 0; i--) {
        int full = 1;
        for (int j = 0; j < BOARD_WIDTH; j++) {
            if (!board[i][j]) {
                full = 0;
                break;
            }
        }

        if (full) {
            del_row(i);
            ++i; // 上面的行下移后，需要重新检查当前行
        }
    }
}

// 界面框架
void draw_interface() {
    for (int i = 0; i < window_height; i++) {
        // 两侧竖栏
        mvaddch(i, BOARD_LEFT - 1, ACS_VLINE);
        mvaddch(i, BOARD_LEFT + BOARD_WIDTH * 2, ACS_VLINE);
        // 侧边栏
        mvaddch(i, BOARD_LEFT + BOARD_WIDTH * 2 + SIDEBAR_WIDTH, ACS_VLINE);
    }
    // 侧边栏内容
    mvaddstr(0, BOARD_LEFT + BOARD_WIDTH * 2 + 1, "rotate: w/W/up ksy");
    mvaddstr(1, BOARD_LEFT + BOARD_WIDTH * 2 + 1, "fast fall: s/S/down key");
    mvaddstr(2, BOARD_LEFT + BOARD_WIDTH * 2 + 1, "right: d/D/right key");
    mvaddstr(3, BOARD_LEFT + BOARD_WIDTH * 2 + 1, "left: a/A/left key");
    mvaddstr(4, BOARD_LEFT + BOARD_WIDTH * 2 + 1, "quit: q/Q/esc");
    char score_str[SIDEBAR_WIDTH + 1];
    sprintf(score_str, "score: %d/%d", score, MAX_SCORE);
    mvaddstr(5, BOARD_LEFT + BOARD_WIDTH * 2 + 1, score_str);
}

int display() {
    for (int i = 0; i < BOARD_HEIGHT; i++) {
        for (int j = 0; j < BOARD_WIDTH; j++) {
            chtype cell = board[i][j] ? (' ' | A_REVERSE) : ' ';

            if (mvaddch(i + window_height - BOARD_HEIGHT, BOARD_LEFT + j * 2, cell) == ERR) return ERR;
            if (mvaddch(i + window_height - BOARD_HEIGHT, BOARD_LEFT + j * 2 + 1, cell) == ERR) return ERR;
        }
    }
    refresh();
    return OK;
}

void loop() {
    int piece_idx = rand() % PIECES_NUM;
    int x = (BOARD_WIDTH - pieces[piece_idx].width) / 2;
    int y = 0;

    put_piece(&pieces[piece_idx], x, y, 1);

    time_t last_time = time(NULL);

    while (1) {
        draw_interface();

        // 输入
        timeout(KEY_TIMEOUT);
        int input = getch();
        switch (input) {
        case 'q':
        case 'Q':
        case 27: // ESC
            put_piece(&pieces[piece_idx], x, y, 0);
            return;
        case KEY_LEFT:
        case 'a':
        case 'A':
            try_move(piece_idx, &x, &y, -1, 0);
            break;
        case KEY_RIGHT:
        case 'd':
        case 'D':
            try_move(piece_idx, &x, &y, 1, 0);
            break;
        case KEY_DOWN:
        case 's':
        case 'S':
            if (try_move(piece_idx, &x, &y, 0, 1)) {
                last_time = time(NULL); // 手动下落重置计时
            }
            break;
        case KEY_UP:
        case 'w':
        case 'W':
            try_rotate(piece_idx, &x, &y);
            break;
        default:
            break;
        }

        // 下落
        time_t cur_time = time(NULL);
        if (cur_time - last_time >= DROP_INTERVAL) {
            last_time = cur_time;

            if (!try_move(piece_idx, &x, &y, 0, 1)) {
                // 无法下落：方块落地，消行后生成新方块
                clear_full_rows();

                piece_idx = rand() % PIECES_NUM;
                x = (BOARD_WIDTH - pieces[piece_idx].width) / 2;
                y = 0;

                // 新方块放不下则游戏结束
                if (!can_place(&pieces[piece_idx], x, y)) {
                    PlaySound(TEXT("audio/fail.wav"), NULL, SND_FILENAME | SND_ASYNC);
                    mvprintw(BOARD_HEIGHT / 2, BOARD_LEFT, "GAME OVER");
                    mvprintw(BOARD_HEIGHT / 2 + 1, BOARD_LEFT, "Press any key...");
                    refresh();
                    timeout(-1);
                    getch();
                    return;
                }

                put_piece(&pieces[piece_idx], x, y, 1);
                last_time = time(NULL);
            }
        }

        if (score > MAX_SCORE) {
            PlaySound(TEXT("audio/win.wav"), NULL, SND_FILENAME | SND_ASYNC);
            mvprintw(BOARD_HEIGHT / 2, BOARD_LEFT, "YOU WIN");
            mvprintw(BOARD_HEIGHT / 2 + 1, BOARD_LEFT, "Press any key...");
            refresh();
            timeout(-1);
            getch();
            return;
        }

        // 显示
        display();
        // 刷新
        refresh();
    }
}

/* ---------------- 启动界面（复古街机 / CRT 质感） ---------------- */

#define LOGO_W 5      // 字标每格 5 像素宽
#define LOGO_H 5      // 字标每格 5 像素高
#define BLOCK_W 2     // 每个方块在终端里占 2 列，视觉上接近正方形
#define LOGO_COUNT 6
#define LOGO_GAP 1

#define GHOST_COUNT 8
#define GHOST_SHAPE_NUM 7
#define GHOST_SIZE 4

// 七种经典方块色 + 文字色、暗色
enum {
    PAIR_CYAN = 1,
    PAIR_YELLOW,
    PAIR_PURPLE,
    PAIR_GREEN,
    PAIR_RED,
    PAIR_BLUE,
    PAIR_ORANGE,
    PAIR_TEXT,
    PAIR_DIM
};

// T E T R I S 的 5x5 点阵
static const char* const logo_letters[LOGO_COUNT][LOGO_H] = {
    { "#####", "  #  ", "  #  ", "  #  ", "  #  " }, // T
    { "#####", "#    ", "#### ", "#    ", "#####" }, // E
    { "#####", "  #  ", "  #  ", "  #  ", "  #  " }, // T
    { "#### ", "#   #", "#### ", "#  # ", "#   #" }, // R
    { "#####", "  #  ", "  #  ", "  #  ", "#####" }, // I
    { " ####", "#    ", " ### ", "    #", "#### " }  // S
};

// 背景残影用的四种块
static const char* const ghost_shapes[GHOST_SHAPE_NUM][GHOST_SIZE] = {
    { "####", "    ", "    ", "    " }, // I
    { "##  ", "##  ", "    ", "    " }, // O
    { "### ", " #  ", "    ", "    " }, // T
    { " ## ", "##  ", "    ", "    " }, // S
    { "##  ", " ## ", "    ", "    " }, // Z
    { "#   ", "### ", "    ", "    " }, // J
    { "  # ", "### ", "    ", "    " }  // L
};

static struct {
    int shape, x, y;
} ghosts[GHOST_COUNT];

static void init_colors(void) {
    if (!has_colors()) return;

    start_color();
    init_pair(PAIR_CYAN, COLOR_CYAN, COLOR_BLACK);
    init_pair(PAIR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_PURPLE, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(PAIR_GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(PAIR_RED, COLOR_RED, COLOR_BLACK);
    init_pair(PAIR_BLUE, COLOR_BLUE, COLOR_BLACK);
    init_pair(PAIR_ORANGE, COLOR_YELLOW, COLOR_BLACK); // 终端无橙色，用亮黄近似
    init_pair(PAIR_TEXT, COLOR_WHITE, COLOR_BLACK);
    init_pair(PAIR_DIM, COLOR_BLUE, COLOR_BLACK);
}

// 扫描线 + 暗角
static void draw_crt_bg(void) {
    for (int y = 1; y < window_height; y += 2) {
        for (int x = 0; x < window_width; x++) {
            mvaddch(y, x, ACS_HLINE | A_DIM | COLOR_PAIR(PAIR_DIM));
        }
    }

    for (int y = 0; y < window_height; y++) {
        mvaddch(y, 0, ACS_VLINE | A_DIM | COLOR_PAIR(PAIR_DIM));
        mvaddch(y, window_width - 1, ACS_VLINE | A_DIM | COLOR_PAIR(PAIR_DIM));
    }
    for (int x = 0; x < window_width; x++) {
        mvaddch(0, x, ACS_HLINE | A_DIM | COLOR_PAIR(PAIR_DIM));
        mvaddch(window_height - 1, x, ACS_HLINE | A_DIM | COLOR_PAIR(PAIR_DIM));
    }
}

static void init_ghosts(void) {
    int max_x = window_width / BLOCK_W - GHOST_SIZE;
    if (max_x < 1) max_x = 1;

    for (int i = 0; i < GHOST_COUNT; i++) {
        ghosts[i].shape = rand() % GHOST_SHAPE_NUM;
        ghosts[i].x = rand() % max_x;
        ghosts[i].y = rand() % (window_height + GHOST_SIZE) - GHOST_SIZE;
    }
}

static void draw_ghosts(void) {
    for (int i = 0; i < GHOST_COUNT; i++) {
        for (int r = 0; r < GHOST_SIZE; r++) {
            int sy = ghosts[i].y + r;
            if (sy < 0 || sy >= window_height) continue;

            for (int c = 0; c < GHOST_SIZE; c++) {
                if (ghost_shapes[ghosts[i].shape][r][c] != '#') continue;

                int sx = (ghosts[i].x + c) * BLOCK_W;
                if (sx < 0 || sx + 1 >= window_width) continue;

                chtype attr = ' ' | A_REVERSE | A_DIM | COLOR_PAIR(PAIR_DIM);
                mvaddch(sy, sx, attr);
                mvaddch(sy, sx + 1, attr);
            }
        }
    }
}

// 字标：mode 为 0 时用七色，否则整体用 mode 指定的颜色画淡影（色散）
static void draw_logo(int y0, int x0, int mode) {
    int x = x0;

    for (int l = 0; l < LOGO_COUNT; l++) {
        for (int r = 0; r < LOGO_H; r++) {
            int sy = y0 + r;
            if (sy < 0 || sy >= window_height) continue;

            for (int c = 0; c < LOGO_W; c++) {
                if (logo_letters[l][r][c] != '#') continue;

                int sx = x + c * BLOCK_W;
                if (sx < 0 || sx + 1 >= window_width) continue;

                int color = mode ? mode : PAIR_CYAN + (l + r) % 7;
                chtype attr = ' ' | A_REVERSE | COLOR_PAIR(color);
                if (mode) attr |= A_DIM;
                if (!mode && color == PAIR_ORANGE) attr |= A_BOLD; // 亮黄近似橙

                mvaddch(sy, sx, attr);
                mvaddch(sy, sx + 1, attr);
            }
        }

        x += LOGO_W * BLOCK_W + LOGO_GAP;
    }
}

// 启动界面：显示字标与提示，按任意键返回
void start_screen(void) {
    static const char* const prompt = "PRESS ANY KEY TO START";
    int logo_width = LOGO_COUNT * LOGO_W * BLOCK_W + (LOGO_COUNT - 1) * LOGO_GAP;

    int x0 = (window_width - logo_width) / 2;
    int y0 = (window_height - LOGO_H) / 2 - 1;
    if (x0 < 1) x0 = 1;
    if (y0 < 1) y0 = 1;

    init_ghosts();

    int blink = 0;
    while (1) {
        erase();
        draw_crt_bg();
        draw_ghosts();

        // 边缘轻微色散：先画偏移的淡影，再画主字标
        draw_logo(y0, x0 - 1, PAIR_RED);
        draw_logo(y0, x0 + 1, PAIR_BLUE);
        draw_logo(y0, x0, 0);

        int px = (window_width - (int)strlen(prompt)) / 2;
        if (px < 0) px = 0;

        chtype attr = COLOR_PAIR(PAIR_TEXT) | A_BOLD;
        if (blink) attr |= A_REVERSE;
        attron(attr);
        mvprintw(y0 + LOGO_H + 2, px, "%s", prompt);
        attroff(attr);

        move(window_height - 1, window_width - 1);
        refresh();

        // 残影缓慢下落
        for (int i = 0; i < GHOST_COUNT; i++) {
            if (++ghosts[i].y > window_height) {
                int max_x = window_width / BLOCK_W - GHOST_SIZE;
                if (max_x < 1) max_x = 1;
                ghosts[i].y = -GHOST_SIZE;
                ghosts[i].x = rand() % max_x;
                ghosts[i].shape = rand() % GHOST_SHAPE_NUM;
            }
        }

        timeout(400);
        if (getch() != ERR) break;
        blink = !blink;
    }

    timeout(KEY_TIMEOUT);
}

int main(void) {
    // 初始化
    if (read_pieces("pieces.txt") != OK) {
        printf("Failed to read pieces.txt\n");
        return 1;
    }
    srand((unsigned)time(NULL));
    initscr();
    noecho();
    curs_set(0);
    cbreak();
    keypad(stdscr, TRUE);
    getmaxyx(stdscr, window_height, window_width);

    // 窗口大小不足退出
    if (window_width < (BOARD_WIDTH + 2) * 2 + SIDEBAR_WIDTH
        || window_height < BOARD_HEIGHT + 2) {
        endwin();
        printf("Window is too small!\n");
        return 1;
    }

    init_colors();
    start_screen();
    erase();

    loop();

    endwin();
    return 0;
}