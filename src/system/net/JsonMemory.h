
#ifndef _jsonmemory_h_
#define _jsonmemory_h_

#ifdef __cplusplus
extern "C" {
#endif
extern void *JsonMalloc(int);
extern void *JsonCalloc(int, int);
extern void *JsonRealloc(void *, int);
#ifdef __cplusplus
}
#endif

#endif
