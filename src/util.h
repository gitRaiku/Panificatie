#ifndef VEC_H
#define VEC_H

#include <stdlib.h>
#include <stdint.h>

#define VEC_LEN(_v) (sizeof(_v) / sizeof(*_v))
#define DEF_VEC(type, name) \
  struct name {             \
    type *__restrict v;     \
    uint32_t l, s;          \
  };

#define EZERO(cmd, res, args...) if ((cmd) == 0) { fprintf(stderr, res ": %m!\n", ##args); exit(1); }
#define ENULL(cmd, res, args...) if ((cmd) == NULL) { fprintf(stderr, res ": %m!\n", ##args); exit(1); }
#define ENEZ(cmd, res, args...) if ((cmd) != 0) { fprintf(stderr, res ": %m!\n", ##args); exit(1); }
#define ENEG(cmd, res, args...) if ((cmd) < 0) { fprintf(stderr, res ": %m!\n", ##args); exit(1); }
#define EEZERO(cmd, res, args...) if ((cmd) == 0) { fprintf(stderr, res "!\n", ##args); exit(1); }
#define EENULL(cmd, res, args...) if ((cmd) == NULL) { fprintf(stderr, res "!\n", ##args); exit(1); }
#define EENEZ(cmd, res, args...) if ((cmd) != 0) { fprintf(stderr, res "!\n", ##args); exit(1); }
#define EENEG(cmd, res, args...) if ((cmd) < 0) { fprintf(stderr, res "!\n", ##args); exit(1); }
#define TODO(str) {fprintf(stderr, "TODO: "str" at "__FILE__": %u!\n", __LINE__); exit(1); }
#define veci(type, ret) { struct type *_cv = malloc(sizeof(struct type)); _cv->s = 4; _cv->l = 0; _cv->v = malloc(sizeof(*_cv->v) * _cv->s); ENULL(_cv->v, "MEMORY WHAT?"); (ret) = _cv; }
#define vecsi(type, ret) { struct type *_cv = malloc(sizeof(struct type)); _cv->s = 1; _cv->l = 0; _cv->v = malloc(sizeof(*_cv->v) * _cv->s); ENULL(_cv->v, "MEMORY WHAT?"); (ret) = _cv; }
#define vecp(_v, _val) { if ((_v)->l == (_v)->s) { (_v)->s *= 2; (_v)->v = realloc((_v)->v, sizeof(*(_v)->v) * (_v)->s); ENULL((_v)->v, "MEMORY WHAT?"); } (_v)->v[(_v)->l] = (_val); ++(_v)->l; } 
#define vect(_v) { (_v)->s = (_v)->l; (_v)->v = realloc((_v)->v, sizeof(*(_v)->v) * (_v)->s); } 
#define vecfree(_v) { free((_v)->v); free(_v); } 
#define vecforeach(_v, type, ret) for (type *ret = (_v)->v; ret != (_v)->v + (_v)->l; ++ret)
#define svecforeach(_v, type, ret) for (type *ret = (_v); ret != (type*)(_v) + VEC_LEN(_v); ++ret)
#define alpmforeach(_list, res) for (alpm_list_t *res = (_list); res; res = res->next) 

DEF_VEC(char *, strv);

#endif
