#ifndef OBJ_DATAFILE_FLEX_H
#define OBJ_DATAFILE_FLEX_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
extern DataType gDataLine;
#else
extern int gDataLine;
#endif

extern void DataFail(const char *);
extern int DataInput(void *, int);

#ifdef __cplusplus
}
#endif

#endif
