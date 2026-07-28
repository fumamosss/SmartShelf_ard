#ifndef LOAD_CELLS_H
#define LOAD_CELLS_H

#include <Arduino.h>

// initialize load cell modules - stub for now
void load_cells_begin();

// check if any weight change was detected - returns false in stub
bool load_cells_has_weight_change();

// get index of the cell where weight changed - returns -1 in stub
int load_cells_get_changed_cell();

// get current weight reading for a specific cell - returns 0.0 in stub
float load_cells_get_weight(int cell);

#endif
