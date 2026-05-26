#include "hamobj/SongUtl.h"
#include "obj/Object.h"

BEGIN_CUSTOM_PROPSYNC(Range)
    SYNC_PROP(start, o.start)
    SYNC_PROP(end, o.end)
END_CUSTOM_PROPSYNC
