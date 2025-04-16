#include "include/uw.h"

UwResult uw_parse_kvargs(int argc, char* argv[])
{
    UwValue kwargs = uw_create(UwTypeId_Map);
    if (argc == 0) {
        return uw_move(&kwargs);
    }

    // add argv[0] to kwargs
    UwValue zero = UwUnsigned(0);
    UwValue argv0 = UwCharPtr((char8_t*) argv[0]);
    uw_expect_ok( uw_map_update(&kwargs, &zero, &argv0) );

    for(int i = 1; i < argc; i++) {{

        // convert arg to UW string
        UwValue arg = _uw_create_string_u8((char8_t*) argv[i]);
        uw_return_if_error(&arg);

        // split by =
        UwValue kv = uw_string_split_chr(&arg, '=', 1);
        uw_return_if_error(&kv);

        UwValue key = uw_array_item(&kv, 0);
        if (uw_array_length(&kv) == 1) {
            // `=` is missing, value is null
            UwValue null = UwNull();
            uw_expect_ok( uw_map_update(&kwargs, &key, &null) );
        } else {
            // add key-value, overwriting previous one
            UwValue value = uw_array_item(&kv, 1);
            uw_expect_ok( uw_map_update(&kwargs, &key, &value) );
        }
    }}
    return uw_move(&kwargs);
}
