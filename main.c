#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>

#define MAP_FACTOR 2
#define AMOUNT_OF_ROOMS 50
#define ROOM_MAX 15

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef enum {
// EMPTY,
  FLOOR, // floor is now going to be the default
  WALL,
// COIN,
// DOOR,
// ENEMY
  UNEXISTENT // things that are outside the map
} surrounding;

surrounding unexistent_surrounding = UNEXISTENT;

typedef enum {
  NONE,
  PLAYER,
  // MONSTER
} being;

// A cell can either have no living being, and so the surrounding is printed,
// otherwise we show the livin being
typedef struct {
  surrounding surrounding;
  being *being;
} cell;

// typedef struct {
//   int x;
//   int y;
// } point;

typedef struct {
  cell **cells;
  int width;
  int height;
} map;


// A map can be bigger than the screen, so we need to keep track of the
// offset of the map on the screen.
typedef struct {
  int x;
  int y;
} offset;

typedef struct {
  int width;
  int height;
  offset offset;
  offset last_offset;
  map *map;
} screen;


typedef enum {
  UP,
  DOWN,
  LEFT,
  RIGHT,
  NO_DOOR
} door_pos;

char *message = "\033[34m<j> to move down, <k> to move up, <h> to move left, <l> to move right, <Q> to quit\033[0m";

////////////////////////////////////////////////////////////////////////////////

void set_raw_mode() {
  struct termios t;
  tcgetattr(0, &t);
  t.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(0, TCSANOW, &t);
}

void show_text (char *text) {
  struct winsize size;
  ioctl(0, TIOCGWINSZ, (char *) &size);

  printf("\033[%d;%dH", size.ws_row, 0);
  printf("  %s", text);
  printf("\033[%d;%dH", size.ws_row, 0);
}

////////////////////////////////////////////////////////////////////////////////

void print_being(being *b) {
  switch (*b) {
    case NONE: break;
    case PLAYER:
      printf("\033[34m🯅\033[0m");
      break;
  }
}

void print_surrounding(surrounding *c) {
  switch (*c) {
    case FLOOR:
      printf(" ");
      break;
    case WALL:
      printf("█");
      break;
    case UNEXISTENT:
      printf("░");
      break;
    default:
      printf("?");
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////

cell map_get(map *m, int x, int y) {
  return m->cells[y][x];
}

void map_set_surrounding(map *m, int x, int y, surrounding s) {
  m->cells[y][x].surrounding = s;
}

void map_set_being(map *m, int x, int y, being *b) {
  m->cells[y][x].being = b;
}

void map_fill(map *m, surrounding s, int x, int y, int width, int height) {
  for (int i = y; i < y + height; i++)
    for (int j = x; j < x + width; j++)
      map_set_surrounding(m, j, i, s);
}

void map_draw_room(map *m, surrounding s, int x, int y, int width, int height, door_pos *d) {
  map_fill(m, s, x, y, width, 1);
  map_fill(m, s, x, y, 1, height);
  map_fill(m, s, x + width - 1, y, 1, height);
  map_fill(m, s, x, y + height - 1, width, 1);
  switch (*d) {
    case UP:
      map_set_surrounding(m, x + width / 2, y, FLOOR);
      break;
    case DOWN:
      map_set_surrounding(m, x + width / 2, y + height - 1, FLOOR);
      break;
    case LEFT:
      map_set_surrounding(m, x, y + height / 2, FLOOR);
      break;
    case RIGHT:
      map_set_surrounding(m, x + width - 1, y + height / 2, FLOOR);
      break;
    case NO_DOOR:
      break;
  }
}

void generate_rooms(map *m, int n) {
  // Procedurally generates a bunch of rooms like a maze on the map
  // it uses map_draw_room

  // set seed
  srand(time(NULL));

  // Steps:
  // 1. Bruteforce an amount n of rooms that don't overlap
  // 2. For each room, pick a random door position
  // 3. Draw each room
  // 4. Draw a corridor between the rooms
  // 5. Repeat until n rooms are drawn

  int rooms_drawn = 0;
  while (rooms_drawn < n) {
    door_pos d = rand() % 3 + 1;
    int x = rand() % (m->width - 2) + 1;
    int y = rand() % (m->height - 2) + 1;
    int width = rand() % MIN(ROOM_MAX, m->width - x - 2) + 3;
    int height = rand() % MIN(ROOM_MAX, m->height - y - 2) + 3;
    map_draw_room(m, WALL, x, y, width, height, &d);
    rooms_drawn++;
  }
}

map *map_create(int width, int height, surrounding default_surrounding) {
  map *m = malloc(sizeof(map));

  m->cells = malloc(sizeof(cell *) * height);
  for (int i = 0; i < height; i++)
    m->cells[i] = malloc(sizeof(cell) * width);

  m->width = width;
  m->height = height;

  map_fill(m, default_surrounding, 0, 0, width, height);

  // add the player at the center cell of the map
  map_set_being(m, width / 2, height / 2, (being *) PLAYER);

  // procedurally generate rooms
  generate_rooms(m, AMOUNT_OF_ROOMS);

  return m;
}

////////////////////////////////////////////////////////////////////////////////

int check_map_border_collision(screen *s, int x, int y) {
  return x < 0 || x >= s->map->width || y < 0 || y >= s->map->height;
}


void update_player_position(screen *s, int x, int y) {

  if (check_map_border_collision(s, x, y) ||
      map_get(s->map, x, y).surrounding == WALL) {
    message = "Hit!";
    s->offset = s->last_offset;
    return;
  }

  for (int i = 0; i < s->map->height; i++)
    for (int j = 0; j < s->map->width; j++)
      if (map_get(s->map, j, i).being == PLAYER)
        map_set_being(s->map, j, i, NONE);

  // add the player to the new position if it's not outside the map
  if (x >= 0 && x < s->map->width && y >= 0 && y < s->map->height)
    map_set_being(s->map, x, y, (being *) PLAYER);

}

////////////////////////////////////////////////////////////////////////////////

screen *screen_create(map *m, int width, int height) {
  screen *s = malloc(sizeof(screen));

  s->width = width;
  s->height = height;
  s->offset.x = 0;
  s->offset.y = 0;
  s->map = m;

  return s;
}

void screen_update(screen *s) {
  // clear the screen
  printf("\033[2J");
  // move the cursor to the top left
  printf("\033[0;0H");

  // update player position at the center of the screen
  update_player_position(s, s->offset.x + s->width / 2, s->offset.y + s->height / 2);
}

void screen_draw(screen *s) {
  for (int i = 0; i < s->height; i++) {
    for (int j = 0; j < s->width; j++) {
      // do not print the map outside the screen
      if (i + s->offset.y < 0 || j + s->offset.x < 0 ||
          i + s->offset.y >= s->map->height ||
          j + s->offset.x >= s->map->width) {
        print_surrounding(&unexistent_surrounding);
        continue;
      }

      cell c = map_get(s->map, s->offset.x + j, s->offset.y + i);
      if (c.being == NONE)
        print_surrounding(&c.surrounding);
      else
        print_being(&c.being);

    }
    printf("\n");
  }
}

void screen_move(screen *s, int dx, int dy) {
  s->last_offset = s->offset;
  s->offset.x += dx;
  s->offset.y += dy;
}

void screen_handle_input(screen *s) {
  char c = getchar();
  switch (c) {
    case 'h':
      screen_move(s, -1, 0);
      break;
    case 'j':
      screen_move(s, 0, 1);
      break;
    case 'k':
      screen_move(s, 0, -1);
      break;
    case 'l':
      screen_move(s, 1, 0);
      break;
    case 'Q':
      // Clear screen & put the cursor at the top left
      printf("\033[2J\033[0;0H");
      exit(0);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////

void play(screen *s) {
  set_raw_mode();
  while (1) {
    screen_update(s);
    screen_draw(s);
    show_text(message);
    screen_handle_input(s);
  }
}

////////////////////////////////////////////////////////////////////////////////


int main() {
  screen *screen;
  map *map;

  struct winsize size;
  ioctl(0, TIOCGWINSZ, (char *) &size);

  map = map_create(size.ws_col * MAP_FACTOR, size.ws_row * MAP_FACTOR, FLOOR);
  screen = screen_create(map, size.ws_col, size.ws_row - 1);

  // create a wall
  // map_fill(map, WALL, 5, 5, 10, 1);

  play(screen);

  return 0;
}