#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <stdint.h>
#define GAME_FLAP_VELOCITY_FP  (-640)
#define GAME_GRAVITY_FP          50
#define GAME_MAX_FALL_FP        460
#define GAME_PIPE_SPEED_PX        1
#define GAME_PIPE_HALF_GAP       15
#define GAME_PIPE_WIDTH          12

/* Bird position and velocity */

typedef struct {
    int16_t y;
    int16_t vy;
    int32_t y_fp;
} Bird;

/* Pipe position */

typedef struct {
    int16_t x;
    int16_t gap_y;
} Pipe;

void Logic_Init(Bird *b, Pipe *p);

void Logic_Flap(Bird *b);

void Logic_Update(
    Bird *b,
    Pipe *p,
    uint8_t jump
);

uint8_t Logic_CheckCollision(
    const Bird *b,
    const Pipe *p
);

#endif /* GAME_LOGIC_H */
