// sw3 cross-dialect scatter shim — restore side (see dialect_object_push.h).
// Pops the ObjMacros.h-dialect macro definitions saved by the matching push,
// restoring the consumer's own dialect after an Object.h-dialect owner #include.
#pragma pop_macro("END_HANDLERS")
#pragma pop_macro("HANDLE_ACTION_STATIC")
#pragma pop_macro("HANDLE_EXPR_STATIC")
#pragma pop_macro("HANDLE_STATIC")
#pragma pop_macro("HANDLE_ACTION_IF_ELSE")
#pragma pop_macro("HANDLE_ACTION_IF")
#pragma pop_macro("HANDLE_ACTION")
#pragma pop_macro("HANDLE_EXPR")
#pragma pop_macro("HANDLE")
#pragma pop_macro("SYNC_PROP_BITFIELD")
#pragma pop_macro("SYNC_PROP_MODIFY")
#pragma pop_macro("SYNC_PROP_SET")
#pragma pop_macro("SYNC_PROP")
#pragma pop_macro("ASSERT_REVS")
#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")
