#include "undo.h"
#include <stdlib.h>
#include <string.h>

static void clone_array(singularity_array_t *dst, const singularity_array_t *src) {
    dst->size     = src->size;
    dst->capacity = src->capacity ? src->capacity : 1;
    dst->data     = malloc(dst->capacity * sizeof(singularity_t));
    if (dst->data && src->data)
        memcpy(dst->data, src->data, src->size * sizeof(singularity_t));
}

static void release_frame(UndoFrame *f) {
    free(f->zeros.data); f->zeros.data = NULL;
    free(f->poles.data); f->poles.data = NULL;
}

static void restore_frame(const UndoFrame *f,
                           singularity_array_t *zeros,
                           singularity_array_t *poles) {
    free(zeros->data);
    free(poles->data);
    clone_array(zeros, &f->zeros);
    clone_array(poles, &f->poles);
}

void undo_init(UndoStack *u) {
    memset(u, 0, sizeof(*u));
    u->pos   = -1;
    u->total =  0;
}

void undo_push(UndoStack *u,
               const singularity_array_t *zeros,
               const singularity_array_t *poles) {
    /* Discard redo history above current position */
    for (int i = u->pos + 1; i < u->total; i++)
        release_frame(&u->frames[i]);
    u->total = u->pos + 1;

    if (u->total >= UNDO_MAX) {
        /* Circular shift: drop oldest, slide everything left */
        release_frame(&u->frames[0]);
        memmove(&u->frames[0], &u->frames[1],
                (UNDO_MAX - 1) * sizeof(UndoFrame));
        u->total--;
        u->pos--;
    }

    u->pos++;
    clone_array(&u->frames[u->pos].zeros, zeros);
    clone_array(&u->frames[u->pos].poles, poles);
    u->total = u->pos + 1;
}

int undo_undo(UndoStack *u,
              singularity_array_t *zeros,
              singularity_array_t *poles) {
    if (u->pos <= 0) return 0;
    u->pos--;
    restore_frame(&u->frames[u->pos], zeros, poles);
    return 1;
}

int undo_redo(UndoStack *u,
              singularity_array_t *zeros,
              singularity_array_t *poles) {
    if (u->pos >= u->total - 1) return 0;
    u->pos++;
    restore_frame(&u->frames[u->pos], zeros, poles);
    return 1;
}

void undo_destroy(UndoStack *u) {
    for (int i = 0; i < u->total; i++)
        release_frame(&u->frames[i]);
    u->pos   = -1;
    u->total =  0;
}
