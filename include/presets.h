#ifndef PRESETS_H
#define PRESETS_H

#include "simulation.h"

/*
 * Load a named preset configuration into the simulation.
 * n should be between 1 and 9.
 */
void load_preset(Simulation *sim, int n);

#endif /* PRESETS_H */
