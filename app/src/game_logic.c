#include "game_logic.h"

#define GAME_BIRD_X      27
#define GAME_BIRD_RIGHT  37
#define GAME_BIRD_RADIUS  4

void Logic_Init(Bird *b, Pipe *p)
{
  b->y = 32;
  b->vy = 0;
  b->y_fp = 32 * 256;
  p->x = 110;
  p->gap_y = 32;
}

void Logic_Flap(Bird *b)
{
  /* Reset vertical velocity on each press, as in Flappy Bird. */
  b->vy = GAME_FLAP_VELOCITY_FP;
}

void Logic_Update(Bird *b, Pipe *p, uint8_t jump)
{
  if (jump) Logic_Flap(b);

  /* Fractional-pixel physics: slow, smooth motion at a fixed 30 FPS. */
  b->vy += GAME_GRAVITY_FP;
  if (b->vy > GAME_MAX_FALL_FP) b->vy = GAME_MAX_FALL_FP;
  b->y_fp += b->vy;
  b->y = (int16_t)(b->y_fp / 256);

  p->x -= GAME_PIPE_SPEED_PX;
}

uint8_t Logic_CheckCollision(const Bird *b, const Pipe *p)
{
  /* Screen top and floor (floor occupies y=63). */
  if (b->y < 4 || b->y >= 59) return 1;

  /* 10-pixel-wide bird against 2-pixel overhanging pipe rims. */
  if (p->x - 2 < GAME_BIRD_RIGHT &&
      p->x + GAME_PIPE_WIDTH + 2 > GAME_BIRD_X &&
      (b->y - GAME_BIRD_RADIUS < p->gap_y - GAME_PIPE_HALF_GAP ||
       b->y + GAME_BIRD_RADIUS >= p->gap_y + GAME_PIPE_HALF_GAP)) {
    return 1;
  }
  return 0;
}
