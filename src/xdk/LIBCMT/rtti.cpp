#include "typeinfo"

std::bad_cast::bad_cast(const char *msg) : exception(msg) {}

std::bad_typeid::bad_typeid(const char *msg) : exception(msg) {}

std::__non_rtti_object::__non_rtti_object(const char *msg) : bad_typeid(msg) {}

// sw2 scatter-include (default/rtti <- rndobj/Fur_NG.cpp)
#include "rndobj/Fur_NG.cpp"
