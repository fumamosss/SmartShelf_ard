#include "load_cells.h"

// stub - will be replaced with HX711 implementation later
// each storage cell will have 4 load cells connected via DT/SCK pins
void load_cells_begin()
{
    Serial.println("[LOAD CELLS] stub - no real HX711 modules initialized");
}

bool load_cells_has_weight_change()
{
    // stub - no weight changes detected
    return false;
}

int load_cells_get_changed_cell()
{
    // stub - no cell changed
    return -1;
}

float load_cells_get_weight(int cell)
{
    // stub - return zero weight
    return 0.0;
}
