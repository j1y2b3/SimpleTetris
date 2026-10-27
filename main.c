#include "PDCurses/curses.h"

#include <stdio.h>
#include <time.h>

#define BOARD_WIDTH 20
#define BOARD_HEIGHT 20

#define PIECES_NUM 1
#define PIECE_MAX_SIZE 4

#define DROP_INTERVAL 1

#define min(a, b) ((a) < (b) ? (a) : (b))

int board[BOARD_HEIGHT][BOARD_WIDTH] = { 0 };
int window_width, window_height;

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
    for (int i = 0; i < piece->height; i++) {
        if (y + i < 0 || BOARD_HEIGHT <= y + i) continue;

        for (int j = 0; j < piece->width; j++) {
            if (x + j < 0 || BOARD_WIDTH <= x + j) continue;
            if (!(piece->shape[i][j])) continue;

            board[y + i][x + j] = color;
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
}

int display() {
    for (int i = 0; i < BOARD_HEIGHT; i++) {
        for (int j = 0; j < BOARD_WIDTH; j++) {
            printf("%d ", board[i][j]); // DEBUG
            if (!board[i][j]) continue;
            if (mvaddch(i, j * 2 - 1, ' ' | A_REVERSE) == ERR) return ERR;
            if (mvaddch(i, j * 2, ' ' | A_REVERSE) == ERR) return ERR;
        }
        printf("\n"); // DEBUG
    }
    refresh();
    return OK;
}

void loop() {
    time_t cur_time = time(NULL);
    time_t last_time = time(NULL);
    while (1) {
        // 两侧竖栏
        for (int i = 0; i < window_height; i++) {
            mvaddch(i, 0, ACS_VLINE);
            mvaddch(i, BOARD_WIDTH * 2 - 1, ACS_VLINE);
        }

        // 输入
        int input = getch();
        if (input != ERR) {
            ;
        }

        // 下落


        // 显示
        display();
        // 光标移至右下角
        mvaddch(window_height - 1, window_width - 1, ' ');
        // 刷新
        refresh();
    }
}

int main(void) {
    // 初始化
    read_pieces("pieces.txt");
    initscr();
    getmaxyx(stdscr, window_height, window_width);

    // 窗口大小不足退出
    if (window_width < (BOARD_WIDTH + 2) * 2 || window_height < BOARD_HEIGHT + 2) {
        endwin();
        printf("Window is too small!\n");
        return 1;
    }
    // 两侧竖栏
    for (int i = 0; i < window_height; i++) {
        mvaddch(i, 0, ACS_VLINE);
        mvaddch(i, BOARD_WIDTH * 2 - 1, ACS_VLINE);
    }
    // 放块
    put_piece(&pieces[0], 2, 2, 1);
    // 删一行
    del_row(3);
    // 显示
    display();
    // 光标移至右下角
    mvaddch(window_height - 1, window_width - 1, ' ');
    // 刷新
    refresh();
    // 等待
    getch();

    endwin();
    return 0;
}