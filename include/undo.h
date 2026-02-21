#ifndef UNDO_H
#define UNDO_H

#include "transfer_function.h"

#define UNDO_MAX 128

typedef struct {
    singularity_array_t zeros;
    singularity_array_t poles;
} UndoFrame;

typedef struct {
    UndoFrame frames[UNDO_MAX];
    int pos;    /* index of the current snapshot (-1 = none) */
    int total;  /* number of valid snapshots                 */
} UndoStack;

void undo_init(UndoStack *u);
void undo_push(UndoStack *u,
               const singularity_array_t *zeros,
               const singularity_array_t *poles);
int  undo_undo(UndoStack *u,
               singularity_array_t *zeros,
               singularity_array_t *poles);
int  undo_redo(UndoStack *u,
               singularity_array_t *zeros,
               singularity_array_t *poles);
void undo_destroy(UndoStack *u);

#endif /* UNDO_H */
