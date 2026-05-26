#include "setup.h"
typedef struct sockaddr_storage Curl_sockaddr_storage_t;
volatile int size_css = sizeof(Curl_sockaddr_storage_t);
volatile int align_css = __alignof(Curl_sockaddr_storage_t);
