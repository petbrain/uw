#include <stdio.h>
#include <string.h>

#include "include/uw.h"
#include "include/uw_args.h"
#include "include/uw_datetime.h"
#include "include/uw_netutils.h"
#include "include/uw_to_json.h"
#include "src/uw_string_internal.h"

int num_tests = 0;
int num_ok = 0;
int num_fail = 0;
//bool print_ok = true;
bool print_ok = false;

#define TEST(condition) \
    do {  \
        if (condition) {  \
            num_ok++;  \
            if (print_ok) fprintf(stderr, "OK: " #condition "\n");  \
        } else {  \
            num_fail++;  \
            fprintf(stderr, "FAILED at line %d: " #condition "\n", __LINE__);  \
        }  \
        num_tests++;  \
    } while (false)

void test_icu()
{
#   ifdef UW_WITH_ICU
        puts("With ICU");
        TEST(uw_isspace(' '));
        TEST(uw_isspace(0x2003));
#   else
        puts("Without ICU");
        TEST(uw_isspace(' '));
        TEST(!uw_isspace(0x2003));
#   endif
}

void test_integral_types()
{
    // generics test
    TEST(strcmp(uw_get_type_name((char) UwTypeId_Bool), "Bool") == 0);
    TEST(strcmp(uw_get_type_name((int) UwTypeId_Signed), "Signed") == 0);
    TEST(strcmp(uw_get_type_name((unsigned long long) UwTypeId_Float), "Float") == 0);

    // Null values
    UwValue null_1 = UwNull();
    UWDECL_Null(null_2);
    TEST(uw_is_null(&null_1));
    TEST(uw_is_null(&null_2));

    TEST(strcmp(uw_get_type_name(&null_1), "Null") == 0);  // generics test

    // Bool values
    UwValue bool_true  = UwBool(true);
    UwValue bool_false = UwBool(false);
    TEST(uw_is_bool(&bool_true));
    TEST(uw_is_bool(&bool_false));

    // Int values
    UwValue int_0 = UwSigned(0);
    UwValue int_1 = UwSigned(1);
    UwValue int_neg1 = UwSigned(-1);
    TEST(uw_is_int(&int_0));
    TEST(uw_is_int(&int_1));
    TEST(uw_is_signed(&int_1));
    TEST(uw_is_signed(&int_neg1));
    TEST(uw_equal(&int_0, 0));
    TEST(!uw_equal(&int_0, 1));
    TEST(uw_equal(&int_1, 1));
    TEST(uw_equal(&int_neg1, -1));

    UwValue int_2 = UwSigned(2);
    TEST(uw_is_signed(&int_2));
    TEST(uw_equal(&int_2, 2));
    UwValue int_3 = UwUnsigned(3);
    TEST(uw_is_unsigned(&int_3));
    TEST(uw_equal(&int_3, 3));

    // Float values
    UwValue f_0 = UwFloat(0.0);
    UwValue f_1 = UwFloat(1.0);
    UwValue f_neg1 = UwFloat(-1.0);
    TEST(uw_is_float(&f_0));
    TEST(uw_is_float(&f_1));
    TEST(uw_is_float(&f_neg1));
    TEST(uw_equal(&f_0, &f_0));
    TEST(uw_equal(&f_1, &f_1));
    TEST(uw_equal(&f_0, 0.0));
    TEST(!uw_equal(&f_0, 1.0));
    TEST(uw_equal(&f_1, 1.0));
    TEST(uw_equal(&f_neg1, -1.0));
    TEST(!uw_equal(&f_neg1, 1.0));

    UwValue f_2 = UwFloat(2.0);
    TEST(uw_is_float(&f_2));
    TEST(uw_equal(&f_2, 2.0));
    UwValue f_3 = UwFloat(3.0f);
    TEST(uw_is_float(&f_3));
    TEST(uw_equal(&f_3, 3.0));
    TEST(uw_equal(&f_3, 3.0f));

    // null vs null
    TEST(uw_equal(&null_1, &null_2));
    TEST(uw_equal(&null_1, nullptr));

    // null vs bool
    TEST(!uw_equal(&null_1, &bool_true));
    TEST(!uw_equal(&null_1, &bool_false));
    TEST(!uw_equal(&null_1, true));
    TEST(!uw_equal(&null_1, false));

    // null vs int
    TEST(!uw_equal(&null_1, &int_0));
    TEST(!uw_equal(&null_1, &int_1));
    TEST(!uw_equal(&null_1, &int_neg1));
    TEST(!uw_equal(&null_1, (char) 2));
    TEST(!uw_equal(&null_1, (unsigned char) 2));
    TEST(!uw_equal(&null_1, (short) 2));
    TEST(!uw_equal(&null_1, (unsigned short) 2));
    TEST(!uw_equal(&null_1, 2));
    TEST(!uw_equal(&null_1, 2U));
    TEST(!uw_equal(&null_1, (unsigned long) 2));
    TEST(!uw_equal(&null_1, (unsigned long long) 2));

    // null vs float
    TEST(!uw_equal(&null_1, &f_0));
    TEST(!uw_equal(&null_1, &f_1));
    TEST(!uw_equal(&null_1, &f_neg1));
    TEST(!uw_equal(&null_1, 2.0f));
    TEST(!uw_equal(&null_1, 2.0));

    // bool vs null
    TEST(!uw_equal(&bool_true, &null_1));
    TEST(!uw_equal(&bool_false, &null_1));
    TEST(!uw_equal(&bool_true, nullptr));
    TEST(!uw_equal(&bool_false, nullptr));

    // bool vs bool
    TEST(uw_equal(&bool_true, true));
    TEST(!uw_equal(&bool_true, false));
    TEST(uw_equal(&bool_false, false));
    TEST(!uw_equal(&bool_false, true));

    TEST(uw_equal(&bool_true, &bool_true));
    TEST(uw_equal(&bool_false, &bool_false));
    TEST(!uw_equal(&bool_true, &bool_false));
    TEST(!uw_equal(&bool_false, &bool_true));

    // bool vs int
    TEST(!uw_equal(&bool_true, &int_0));
    TEST(!uw_equal(&bool_true, &int_1));
    TEST(!uw_equal(&bool_true, &int_neg1));
    TEST(!uw_equal(&bool_false, &int_0));
    TEST(!uw_equal(&bool_false, &int_1));
    TEST(!uw_equal(&bool_false, &int_neg1));
    TEST(!uw_equal(&bool_true, (char) 0));
    TEST(!uw_equal(&bool_true, (char) 2));
    TEST(!uw_equal(&bool_false, (char) 0));
    TEST(!uw_equal(&bool_false, (char) 2));
    TEST(!uw_equal(&bool_true, (unsigned char) 0));
    TEST(!uw_equal(&bool_true, (unsigned char) 2));
    TEST(!uw_equal(&bool_false, (unsigned char) 0));
    TEST(!uw_equal(&bool_false, (unsigned char) 2));
    TEST(!uw_equal(&bool_true, (short) 0));
    TEST(!uw_equal(&bool_true, (short) 2));
    TEST(!uw_equal(&bool_false, (short) 0));
    TEST(!uw_equal(&bool_false, (short) 2));
    TEST(!uw_equal(&bool_true, (unsigned short) 0));
    TEST(!uw_equal(&bool_true, (unsigned short) 2));
    TEST(!uw_equal(&bool_false, (unsigned short) 0));
    TEST(!uw_equal(&bool_false, (unsigned short) 2));
    TEST(!uw_equal(&bool_true, 0));
    TEST(!uw_equal(&bool_true, 2));
    TEST(!uw_equal(&bool_false, 0));
    TEST(!uw_equal(&bool_false, 2));
    TEST(!uw_equal(&bool_true, 0U));
    TEST(!uw_equal(&bool_true, 2U));
    TEST(!uw_equal(&bool_false, 0U));
    TEST(!uw_equal(&bool_false, 2U));
    TEST(!uw_equal(&bool_true, 0L));
    TEST(!uw_equal(&bool_true, 2L));
    TEST(!uw_equal(&bool_false, 0L));
    TEST(!uw_equal(&bool_false, 2L));
    TEST(!uw_equal(&bool_true, 0UL));
    TEST(!uw_equal(&bool_true, 2UL));
    TEST(!uw_equal(&bool_false, 0UL));
    TEST(!uw_equal(&bool_false, 2UL));
    TEST(!uw_equal(&bool_true, 0LL));
    TEST(!uw_equal(&bool_true, 2LL));
    TEST(!uw_equal(&bool_false, 0LL));
    TEST(!uw_equal(&bool_false, 2LL));
    TEST(!uw_equal(&bool_true, 0ULL));
    TEST(!uw_equal(&bool_true, 2ULL));
    TEST(!uw_equal(&bool_false, 0ULL));
    TEST(!uw_equal(&bool_false, 2ULL));

    // bool vs float
    TEST(!uw_equal(&bool_true, &f_0));
    TEST(!uw_equal(&bool_true, &f_1));
    TEST(!uw_equal(&bool_true, &f_neg1));
    TEST(!uw_equal(&bool_false, &f_0));
    TEST(!uw_equal(&bool_false, &f_1));
    TEST(!uw_equal(&bool_false, &f_neg1));
    TEST(!uw_equal(&bool_true, 0.0f));
    TEST(!uw_equal(&bool_true, 2.0f));
    TEST(!uw_equal(&bool_false, 0.0f));
    TEST(!uw_equal(&bool_false, 2.0f));
    TEST(!uw_equal(&bool_true, 0.0f));
    TEST(!uw_equal(&bool_true, 2.0f));
    TEST(!uw_equal(&bool_false, 0.0f));
    TEST(!uw_equal(&bool_false, 2.0f));
    TEST(!uw_equal(&bool_true, 0.0));
    TEST(!uw_equal(&bool_true, 2.0));
    TEST(!uw_equal(&bool_false, 0.0));
    TEST(!uw_equal(&bool_false, 2.0));
    TEST(!uw_equal(&bool_true, 0.0));
    TEST(!uw_equal(&bool_true, 2.0));
    TEST(!uw_equal(&bool_false, 0.0));
    TEST(!uw_equal(&bool_false, 2.0));

    // int vs null
    TEST(!uw_equal(&int_0, &null_1));
    TEST(!uw_equal(&int_0, nullptr));
    TEST(!uw_equal(&int_1, &null_1));
    TEST(!uw_equal(&int_neg1, &null_1));

    // int vs bool
    TEST(!uw_equal(&int_0, &bool_true));
    TEST(!uw_equal(&int_0, &bool_false));
    TEST(!uw_equal(&int_0, true));
    TEST(!uw_equal(&int_0, false));
    TEST(!uw_equal(&int_1, &bool_true));
    TEST(!uw_equal(&int_1, &bool_false));
    TEST(!uw_equal(&int_1, true));
    TEST(!uw_equal(&int_1, false));
    TEST(!uw_equal(&int_neg1, &bool_true));
    TEST(!uw_equal(&int_neg1, &bool_false));
    TEST(!uw_equal(&int_neg1, true));
    TEST(!uw_equal(&int_neg1, false));

    // int vs int
    TEST(uw_equal(&int_0, &int_0));
    TEST(uw_equal(&int_1, &int_1));
    TEST(uw_equal(&int_neg1, &int_neg1));
    TEST(!uw_equal(&int_0, &int_1));
    TEST(!uw_equal(&int_1, &int_0));
    TEST(!uw_equal(&int_neg1, &int_0));
    TEST(!uw_equal(&int_0, &int_neg1));
    TEST(uw_equal(&int_1, (char) 1));
    TEST(!uw_equal(&int_1, (char) 2));
    TEST(!uw_equal(&int_1, (char) -1));
    TEST(uw_equal(&int_1, (unsigned char) 1));
    TEST(!uw_equal(&int_1, (unsigned char) 2));
    TEST(!uw_equal(&int_1, (unsigned char) 0));
    TEST(uw_equal(&int_1, (short) 1));
    TEST(!uw_equal(&int_1, (short) 2));
    TEST(!uw_equal(&int_1, (short) -1));
    TEST(uw_equal(&int_1, (unsigned short) 1));
    TEST(!uw_equal(&int_1, (unsigned short) 2));
    TEST(!uw_equal(&int_1, (unsigned short) 0));
    TEST(uw_equal(&int_1, 1));
    TEST(!uw_equal(&int_1, 2));
    TEST(!uw_equal(&int_1, -1));
    TEST(uw_equal(&int_1, 1U));
    TEST(!uw_equal(&int_1, 2U));
    TEST(!uw_equal(&int_1, 0U));
    TEST(uw_equal(&int_1, 1L));
    TEST(!uw_equal(&int_1, 2L));
    TEST(!uw_equal(&int_1, -1L));
    TEST(uw_equal(&int_1, 1UL));
    TEST(!uw_equal(&int_1, 2UL));
    TEST(!uw_equal(&int_1, 0UL));
    TEST(uw_equal(&int_1, 1LL));
    TEST(!uw_equal(&int_1, 2LL));
    TEST(!uw_equal(&int_1, -1LL));
    TEST(uw_equal(&int_1, 1ULL));
    TEST(!uw_equal(&int_1, 2ULL));
    TEST(!uw_equal(&int_1, 0ULL));

    // int vs float
    TEST(uw_equal(&int_0, &f_0));
    TEST(uw_equal(&int_1, &f_1));
    TEST(uw_equal(&int_neg1, &f_neg1));
    TEST(!uw_equal(&int_0, &f_1));
    TEST(!uw_equal(&int_1, &f_0));
    TEST(!uw_equal(&int_neg1, &f_0));
    TEST(!uw_equal(&int_0, &f_neg1));
    TEST(uw_equal(&int_1, 1.0));
    TEST(!uw_equal(&int_1, 2.0));
    TEST(!uw_equal(&int_1, -1.0));
    TEST(uw_equal(&int_1, 1.0f));
    TEST(!uw_equal(&int_1, 2.0f));
    TEST(!uw_equal(&int_1, -1.0f));

    // float vs null
    TEST(!uw_equal(&f_0, &null_1));
    TEST(!uw_equal(&f_0, nullptr));
    TEST(!uw_equal(&f_1, &null_1));
    TEST(!uw_equal(&f_neg1, &null_1));

    // float vs bool
    TEST(!uw_equal(&f_0, &bool_true));
    TEST(!uw_equal(&f_0, &bool_false));
    TEST(!uw_equal(&f_0, true));
    TEST(!uw_equal(&f_0, false));
    TEST(!uw_equal(&f_1, &bool_true));
    TEST(!uw_equal(&f_1, &bool_false));
    TEST(!uw_equal(&f_1, true));
    TEST(!uw_equal(&f_1, false));
    TEST(!uw_equal(&f_neg1, &bool_true));
    TEST(!uw_equal(&f_neg1, &bool_false));
    TEST(!uw_equal(&f_neg1, true));
    TEST(!uw_equal(&f_neg1, false));

    // float vs int
    TEST(uw_equal(&f_0, &int_0));
    TEST(uw_equal(&f_1, &int_1));
    TEST(uw_equal(&f_neg1, &int_neg1));
    TEST(!uw_equal(&f_0, &int_1));
    TEST(!uw_equal(&f_1, &int_0));
    TEST(!uw_equal(&f_neg1, &int_0));
    TEST(!uw_equal(&f_0, &int_neg1));
    TEST(uw_equal(&f_1, (char) 1));
    TEST(!uw_equal(&f_1, (char) 2));
    TEST(!uw_equal(&f_1, (char) -1));
    TEST(uw_equal(&f_1, (unsigned char) 1));
    TEST(!uw_equal(&f_1, (unsigned char) 2));
    TEST(!uw_equal(&f_1, (unsigned char) 0));
    TEST(uw_equal(&f_1, (short) 1));
    TEST(!uw_equal(&f_1, (short) 2));
    TEST(!uw_equal(&f_1, (short) -1));
    TEST(uw_equal(&f_1, (unsigned short) 1));
    TEST(!uw_equal(&f_1, (unsigned short) 2));
    TEST(!uw_equal(&f_1, (unsigned short) 0));
    TEST(uw_equal(&f_1, 1));
    TEST(!uw_equal(&f_1, 2));
    TEST(!uw_equal(&f_1, -1));
    TEST(uw_equal(&f_1, 1U));
    TEST(!uw_equal(&f_1, 2U));
    TEST(!uw_equal(&f_1, 0U));
    TEST(uw_equal(&f_1, 1L));
    TEST(!uw_equal(&f_1, 2L));
    TEST(!uw_equal(&f_1, -1L));
    TEST(uw_equal(&f_1, 1UL));
    TEST(!uw_equal(&f_1, 2UL));
    TEST(!uw_equal(&f_1, 0UL));
    TEST(uw_equal(&f_1, 1LL));
    TEST(!uw_equal(&f_1, 2LL));
    TEST(!uw_equal(&f_1, -1LL));
    TEST(uw_equal(&f_1, 1ULL));
    TEST(!uw_equal(&f_1, 2ULL));
    TEST(!uw_equal(&f_1, 0ULL));

    // float vs float
    TEST(!uw_equal(&f_0, &f_1));
    TEST(!uw_equal(&f_1, &f_0));
    TEST(!uw_equal(&f_neg1, &f_0));
    TEST(!uw_equal(&f_0, &f_neg1));
    TEST(uw_equal(&f_1, 1.0));
    TEST(!uw_equal(&f_1, 2.0));
    TEST(!uw_equal(&f_1, -1.0));
    TEST(uw_equal(&f_1, 1.0f));
    TEST(!uw_equal(&f_1, 2.0f));
    TEST(!uw_equal(&f_1, -1.0f));
}

void test_string()
{
    TEST(uw_isspace(0) == false);

    { // testing char_size=1
        UwValue v = uw_create_empty_string(0, 1);
        TEST(_uw_string_length(&v) == 0);
        TEST(_uw_string_capacity(&v) == 12);
        TEST(_uw_string_char_size(&v) == 1);
        //uw_dump(stderr, &v);

        uw_string_append(&v, "hello");

        TEST(_uw_string_length(&v) == 5);
        TEST(_uw_string_capacity(&v) == 12);
        //uw_dump(stderr, &v);

        uw_string_append(&v, '!');

        TEST(_uw_string_length(&v) == 6);
        TEST(_uw_string_capacity(&v) == 12);
        //uw_dump(stderr, &v);

        // XXX TODO increase capacity to more than 64K
        for (int i = 0; i < 250; i++) {
            uw_string_append(&v, ' ');
        }
        TEST(_uw_string_length(&v) == 256);
        TEST(_uw_string_char_size(&v) == 1);
        //uw_dump(stderr, &v);

        uw_string_append(&v, "pet");
        uw_string_erase(&v, 5, 255);
        TEST(uw_equal(&v, "hello pet"));
        //uw_dump(stderr, &v);
        TEST(!uw_equal(&v, ""));

        // test comparison
        UwValue v2 = uw_create_string("hello pet");
        TEST(uw_equal(&v, &v2));
        TEST(uw_equal(&v2, "hello pet"));
        TEST(!uw_equal(&v, "hello Pet"));
        TEST(!uw_equal(&v2, "hello Pet"));
        TEST(uw_equal(&v, u8"hello pet"));
        TEST(uw_equal(&v2, u8"hello pet"));
        TEST(!uw_equal(&v, u8"hello Pet"));
        TEST(!uw_equal(&v2, u8"hello Pet"));
        TEST(uw_equal(&v, U"hello pet"));
        TEST(uw_equal(&v2, U"hello pet"));
        TEST(!uw_equal(&v, U"hello Pet"));
        TEST(!uw_equal(&v2, U"hello Pet"));

        UwValue v3 = uw_create_string("hello pet");
        // test C string
        UW_CSTRING_LOCAL(cv3, &v3);
        TEST(strcmp(cv3, "hello pet") == 0);

        // test substring
        TEST(uw_substring_eq(&v, 4, 7, "o p"));
        TEST(!uw_substring_eq(&v, 4, 7, ""));
        TEST(uw_substring_eq(&v, 0, 4, "hell"));
        TEST(uw_substring_eq(&v, 7, 100, "et"));
        TEST(uw_substring_eq(&v, 4, 7, u8"o p"));
        TEST(!uw_substring_eq(&v, 4, 7, u8""));
        TEST(uw_substring_eq(&v, 0, 4, u8"hell"));
        TEST(uw_substring_eq(&v, 7, 100, u8"et"));
        TEST(uw_substring_eq(&v, 4, 7, U"o p"));
        TEST(!uw_substring_eq(&v, 4, 7, U""));
        TEST(uw_substring_eq(&v, 0, 4, U"hell"));
        TEST(uw_substring_eq(&v, 7, 100, U"et"));

        // test erase and truncate
        uw_string_erase(&v, 4, 255);
        TEST(uw_equal(&v, "hell"));

        uw_string_erase(&v, 0, 2);
        TEST(uw_equal(&v, "ll"));

        uw_string_truncate(&v, 0);
        TEST(uw_equal(&v, ""));

        TEST(_uw_string_length(&v) == 0);
        TEST(_uw_string_capacity(&v) == 264);
        //uw_dump(stderr, &v);

        // test append substring
        uw_string_append_substring(&v, "0123456789", 3, 7);
        TEST(uw_equal(&v, "3456"));
        uw_string_append_substring(&v, u8"0123456789", 3, 7);
        TEST(uw_equal(&v, "34563456"));
        uw_string_append_substring(&v, U"0123456789", 3, 7);
        TEST(uw_equal(&v, "345634563456"));
        uw_string_truncate(&v, 0);

        // change char size to 2-byte by appending wider chars -- the string will be copied
        uw_string_append(&v, u8"สวัสดี");

        TEST(_uw_string_length(&v) == 6);
        TEST(_uw_string_capacity(&v) == 268);  // capacity is slightly changed because of alignment and char_size increase
        TEST(_uw_string_char_size(&v) == 2);
        TEST(uw_equal(&v, u8"สวัสดี"));
        //uw_dump(stderr, &v);
    }

    { // testing char_size=2
        UwValue v = uw_create_empty_string(1, 2);
        TEST(_uw_string_length(&v) == 0);
        TEST(_uw_string_capacity(&v) == 6);
        TEST(_uw_string_char_size(&v) == 2);
        //uw_dump(stderr, &v);

        uw_string_append(&v, u8"สบาย");

        TEST(_uw_string_length(&v) == 4);
        TEST(_uw_string_capacity(&v) == 6);
        //uw_dump(stderr, &v);

        uw_string_append(&v, 0x0e14);
        uw_string_append(&v, 0x0e35);

        TEST(_uw_string_length(&v) == 6);
        TEST(_uw_string_capacity(&v) == 6);
        TEST(uw_equal(&v, u8"สบายดี"));
        //uw_dump(stderr, &v);

        // test truncate
        uw_string_truncate(&v, 4);
        TEST(uw_equal(&v, u8"สบาย"));
        TEST(!uw_equal(&v, ""));
        //uw_dump(stderr, &v);

        // increase capacity to 2 bytes
        for (int i = 0; i < 251; i++) {
            uw_string_append(&v, ' ');
        }
        TEST(_uw_string_length(&v) == 255);
        TEST(_uw_string_capacity(&v) == 260);
        TEST(_uw_string_char_size(&v) == 2);
        //uw_dump(stderr, &v);

        uw_string_append(&v, U"สบาย");
        uw_string_erase(&v, 4, 255);
        TEST(uw_equal(&v, u8"สบายสบาย"));
        TEST(!uw_equal(&v, ""));

        // test comparison
        UwValue v2 = uw_create_string(u8"สบายสบาย");
        TEST(uw_equal(&v, &v2));
        TEST(uw_equal(&v, u8"สบายสบาย"));
        TEST(uw_equal(&v2, u8"สบายสบาย"));
        TEST(!uw_equal(&v, u8"ความสบาย"));
        TEST(!uw_equal(&v2, u8"ความสบาย"));
        TEST(uw_equal(&v, U"สบายสบาย"));
        TEST(uw_equal(&v2, U"สบายสบาย"));
        TEST(!uw_equal(&v, U"ความสบาย"));
        TEST(!uw_equal(&v2, U"ความสบาย"));

        // test substring
        TEST(uw_substring_eq(&v, 3, 5, u8"ยส"));
        TEST(!uw_substring_eq(&v, 3, 5, u8""));
        TEST(uw_substring_eq(&v, 0, 3, u8"สบา"));
        TEST(uw_substring_eq(&v, 6, 100, u8"าย"));
        TEST(uw_substring_eq(&v, 3, 5, U"ยส"));
        TEST(!uw_substring_eq(&v, 3, 5, U""));
        TEST(uw_substring_eq(&v, 0, 3, U"สบา"));
        TEST(uw_substring_eq(&v, 6, 100, U"าย"));

        // test erase and truncate
        uw_string_erase(&v, 4, 255);
        TEST(uw_equal(&v, u8"สบาย"));

        uw_string_erase(&v2, 0, 4);
        TEST(uw_equal(&v, u8"สบาย"));

        uw_string_truncate(&v, 0);
        TEST(uw_equal(&v, ""));

        // test append substring
        uw_string_append_substring(&v, u8"สบายสบาย", 1, 4);
        TEST(uw_equal(&v, u8"บาย"));
        uw_string_append_substring(&v, U"สบายสบาย", 1, 4);
        TEST(uw_equal(&v, U"บายบาย"));
        uw_string_truncate(&v, 0);

        TEST(_uw_string_length(&v) == 0);
        TEST(_uw_string_capacity(&v) == 260);
        //uw_dump(stderr, &v);
    }

    { // testing char_size=3
        UwValue v = uw_create_empty_string(1, 3);
        TEST(_uw_string_length(&v) == 0);
        TEST(_uw_string_capacity(&v) == 4);
        TEST(_uw_string_char_size(&v) == 3);
        //uw_dump(stderr, &v);
    }

    { // testing char_size=4
        UwValue v = uw_create_empty_string(1, 4);
        TEST(_uw_string_length(&v) == 0);
        TEST(_uw_string_capacity(&v) == 3);
        TEST(_uw_string_char_size(&v) == 4);
        //uw_dump(stderr, &v);
    }

    { // test trimming
        UwValue v = uw_create_string(u8"  สวัสดี   ");
        TEST(uw_strlen(&v) == 11);
        uw_string_ltrim(&v);
        TEST(uw_equal(&v, u8"สวัสดี   "));
        uw_string_rtrim(&v);
        TEST(uw_equal(&v, u8"สวัสดี"));
        TEST(uw_strlen(&v) == 6);
    }

    { // test uw_strcat (by value)
        UwValue v = uw_strcat(
            uw_create_string("Hello! "), UwCharPtr("Thanks"), UwChar32Ptr(U"🙏"), UwCharPtr(u8"สวัสดี")
        );
        TEST(uw_equal(&v, U"Hello! Thanks🙏สวัสดี"));
        //uw_dump(stderr, &v);
    }

    { // test uw_strcat (by reference)
        UwValue s1 = uw_create_string("Hello! ");
        UwValue s2 = UwCharPtr("Thanks");
        UwValue s3 = UwChar32Ptr(U"🙏");
        UwValue s4 = UwCharPtr(u8"สวัสดี");
        UwValue v = uw_strcat(&s1, &s2, &s3, &s4);
        TEST(uw_equal(&v, U"Hello! Thanks🙏สวัสดี"));
        //uw_dump(stderr, &v);
    }

    { // test split/join
        UwValue str = uw_create_string(U"สบาย/สบาย/yo/yo");
        UwValue array = uw_string_split_chr(&str, '/', 0);
        //uw_dump(stderr, &array);
        UwValue array2 = uw_string_rsplit_chr(&str, '/', 1);
        //uw_dump(stderr, &array2);
        UwValue first = uw_array_item(&array2, 0);
        UwValue last = uw_array_item(&array2, 1);
        TEST(uw_equal(&first, U"สบาย/สบาย/yo"));
        TEST(uw_equal(&last, "yo"));
        UwValue array3 = uw_string_split_chr(&str, '/', 1);
        //uw_dump(stderr, &array3);
        UwValue first3 = uw_array_item(&array3, 0);
        UwValue last3 = uw_array_item(&array3, 1);
        TEST(uw_equal(&first3, U"สบาย"));
        TEST(uw_equal(&last3, U"สบาย/yo/yo"));
        UwValue v = uw_array_join('/', &array);
        TEST(uw_equal(&v, U"สบาย/สบาย/yo/yo"));
    }

    // test append_buffer
    {
        char8_t data[2500];
        memset(data, '1', sizeof(data));

        UWDECL_String(str);

        uw_string_append_buffer(&str, data, sizeof(data));
        TEST(_uw_string_capacity(&str) >= _uw_string_length(&str));
        TEST(_uw_string_length(&str) == 2500);
        //uw_dump(stderr, &str);
    }

    { // test startswith/endswith
        UwValue str = uw_create_string("hello world");
        TEST(uw_startswith(&str, 'h'));
        TEST(!uw_startswith(&str, U'ค'));
        TEST(uw_startswith(&str, "hello"));
        TEST(!uw_startswith(&str, "world"));
        TEST(uw_endswith(&str, 'd'));
        TEST(!uw_endswith(&str, U'า'));
        TEST(uw_endswith(&str, "world"));
        TEST(!uw_endswith(&str, "hello"));
    }
    {
        UwValue str = uw_create_string(u8"ความคืบหน้า");
        TEST(!uw_startswith(&str, 'h'));
        TEST(uw_startswith(&str, U'ค'));
        TEST(uw_startswith(&str, u8"ความ"));
        TEST(uw_startswith(&str, U"ความ"));
        TEST(!uw_startswith(&str, "wow"));
        TEST(!uw_endswith(&str, 'd'));
        TEST(uw_endswith(&str, U'า'));
        TEST(uw_endswith(&str, u8"คืบหน้า"));
        TEST(uw_endswith(&str, U"คืบหน้า"));
        TEST(!uw_endswith(&str, "wow"));
    }

    { // test isdigit
        UwValue empty = UwString();
        UwValue nondigit = uw_create_string(u8"123รูปโป๊");
        UwValue digit = uw_create_string("456");
        TEST(!uw_string_isdigit(&empty));
        TEST(!uw_string_isdigit(&nondigit));
        TEST(uw_string_isdigit(&digit));
    }
}

void test_array()
{
    UwValue array = UwArray();

    TEST(uw_array_length(&array) == 0);

    for(unsigned i = 0; i < 1000; i++) {
        {
            UwValue item = UwUnsigned(i);
            uw_array_append(&array, &item);
        }

        TEST(uw_array_length(&array) == i + 1);

        {
            UwValue v = uw_array_item(&array, i);
            UwValue item = UwUnsigned(i);
            TEST(uw_equal(&v, &item));
        }
    }

    {
        UwValue item = uw_array_item(&array, -2);
        UwValue v = UwUnsigned(998);
        TEST(uw_equal(&v, &item));
    }

    uw_array_del(&array, 100, 200);
    TEST(uw_array_length(&array) == 900);

    {
        UwValue item = uw_array_item(&array, 99);
        UwValue v = UwUnsigned(99);
        TEST(uw_equal(&v, &item));
    }
    {
        UwValue item = uw_array_item(&array, 100);
        UwValue v = UwUnsigned(200);
        TEST(uw_equal(&v, &item));
    }

    {
        UwValue slice = uw_array_slice(&array, 750, 850);
        TEST(uw_array_length(&slice) == 100);
        {
            UwValue item = uw_array_item(&slice, 1);
            TEST(uw_equal(&item, 851));
        }
        {
            UwValue item = uw_array_item(&slice, 98);
            TEST(uw_equal(&item, 948));
        }
    }

    UwValue pulled = uw_array_pull(&array);
    TEST(uw_equal(&pulled, 0));
    TEST(uw_array_length(&array) == 899);
    pulled = uw_array_pull(&array);
    TEST(uw_equal(&pulled, 1));
    TEST(uw_array_length(&array) == 898);

    { // test join
        UwValue array = UwArray();
        uw_array_append(&array, "Hello");
        uw_array_append(&array, u8"สวัสดี");
        uw_array_append(&array, "Thanks");
        uw_array_append(&array, U"mulțumesc");
        UwValue v = uw_array_join('/', &array);
        TEST(uw_equal(&v, U"Hello/สวัสดี/Thanks/mulțumesc"));
        //uw_dump(stderr, &v);
    }

    { // test join with CharPtr
        UwValue array       = UwArray();
        UwValue sawatdee   = UwCharPtr(u8"สวัสดี");
        UwValue thanks     = UwCharPtr("Thanks");
        UwValue multsumesc = UwChar32Ptr(U"mulțumesc");
        UwValue wat        = UwChar32Ptr(U"🙏");
        uw_array_append(&array, "Hello");
        uw_array_append(&array, &sawatdee);
        uw_array_append(&array, &thanks);
        uw_array_append(&array, &multsumesc);
        UwValue v = uw_array_join(&wat, &array);
        TEST(uw_equal(&v, U"Hello🙏สวัสดี🙏Thanks🙏mulțumesc"));
        //uw_dump(stderr, &v);
    }

    { // test dedent
        UwValue array = UwArray(
            UwCharPtr("   first line"),
            UwCharPtr("  second line"),
            UwCharPtr("    third line")
        );
        uw_array_dedent(&array);
        UwValue v = uw_array_join(',', &array);
        TEST(uw_equal(&v, " first line,second line,  third line"));
        //uw_dump(stderr, &v);
    }
}

void test_map()
{
    {
        UwValue map = UwMap();
        UwValue key = UwUnsigned(0);
        UwValue value = UwBool(false);

        uw_map_update(&map, &key, &value);
        TEST(uw_map_length(&map) == 1);
        uw_destroy(&key);
        uw_destroy(&value);

        key = UwUnsigned(0);
        TEST(uw_map_has_key(&map, &key));
        uw_destroy(&key);

        key = UwNull();
        TEST(!uw_map_has_key(&map, &key));
        uw_destroy(&key);

        for (int i = 1; i < 50; i++) {
            key = UwUnsigned(i);
            value = UwUnsigned(i);
            uw_map_update(&map, &key, &value);
            uw_destroy(&key);
            uw_destroy(&value);
        }
        uw_map_del(&map, 25);

        TEST(uw_map_length(&map) == 49);
        //uw_dump(stderr, &map);
    }

    {
        // XXX CType leftovers
        UwValue map = UwMap(
            UwCharPtr("let's"),       UwCharPtr("go!"),
            UwNull(),                 UwBool(true),
            UwBool(true),             UwCharPtr("true"),
            UwSigned(-10),            UwBool(false),
            UwSigned('b'),            UwSigned(-42),
            UwUnsigned(100),          UwSigned(-1000000L),
            UwUnsigned(300000000ULL), UwFloat(1.23),
            UwCharPtr(u8"สวัสดี"),      UwChar32Ptr(U"สบาย"),
            UwCharPtr("finally"),     UwMap(UwCharPtr("ok"), UwCharPtr("done"))
        );
        TEST(uw_map_length(&map) == 9);
        //uw_dump(stderr, &map);
    }
}

void test_file()
{
    // UTF-8 crossing read boundary
    {
        char8_t a[] = u8"###################################################################################################\n";
        char8_t b[] = u8"##############################################################################################\n";
        char8_t c[] = u8"สบาย\n";

        char8_t data_filename[] = u8"./test/data/utf8-crossing-buffer-boundary";

        UwValue file = uw_file_open(data_filename, O_RDONLY, 0);
        TEST(uw_ok(&file));
        UwValue status = uw_start_read_lines(&file);
        TEST(uw_ok(&status));
        UwValue line = uw_create_string("");
        for (;;) {{
            UwValue status = uw_read_line_inplace(&file, &line);
            TEST(uw_ok(&status));
            if (uw_error(&status)) {
                return;
            }
            if (!uw_equal(&line, a)) {
                break;
            }
        }}
        TEST(uw_equal(&line, b));
        {
            UwValue status = uw_read_line_inplace(&file, &line);
            TEST(uw_ok(&status));
        }
        TEST(uw_equal(&line, c));

        { // test path functions
            UwValue s = uw_create_string("/bin/bash");
            UwValue basename = uw_basename(&s);
            //uw_dump(stderr, &basename);
            TEST(uw_equal(&basename, "bash"));
            UwValue dirname = uw_dirname(&s);
            //uw_dump(stderr, &dirname);
            TEST(uw_equal(&dirname, "/bin"));
            UwValue path = uw_path(UwCharPtr(""), UwCharPtr("bin"), UwCharPtr("bash"));
            TEST(uw_equal(&path, "/bin/bash"));
            //uw_dump(stderr, &path);
            UwValue part1 = uw_create_string("");
            UwValue part2 = uw_create_string("bin");
            UwValue part3 = uw_create_string("bash");
            UwValue path2 = uw_path(&part1, &part2, &part3);
            TEST(uw_equal(&path2, "/bin/bash"));

            UwValue s2 = uw_create_string("blahblahblah");
            //uw_dump(stderr, &s2);
            UwValue basename2 = uw_basename(&s2);
            //uw_dump(stderr, &basename2);
            TEST(uw_equal(&basename2, "blahblahblah"));
        }
    }

    // compare line readers
    {
        char* sample_files[] = {
            "./test/data/sample.json",
            "./test/data/sample-no-trailing-lf.json"
        };
        for (unsigned i = 0; i < UW_LENGTH(sample_files); i++) {{

            UwValue file_name = UwCharPtr(sample_files[i]);

            UwValue file_size = uw_file_size(&file_name);
            TEST(uw_is_unsigned(&file_size));
            if (!uw_is_unsigned(&file_size)) {
                uw_dump(stderr, &file_size);
            }

            // read content from file

            char data[file_size.unsigned_value + 1];
            bzero(data, sizeof(data));

            UwValue file = uw_file_open(&file_name, O_RDONLY, 0);
            TEST(uw_ok(&file));

            unsigned bytes_read;
            UwValue status = uw_file_read(&file, data, sizeof(data), &bytes_read);
            TEST(uw_ok(&status));
            TEST(bytes_read == file_size.unsigned_value);
            data[file_size.unsigned_value] = 0;

            // reopen file
            uw_destroy(&file);
            file = uw_file_open(&file_name, O_RDONLY, 0);
            TEST(uw_ok(&file));

            // create string IO to compare with

            UwValue str_io = uw_create_string_io(data);

            status = uw_start_read_lines(&file);
            TEST(uw_ok(&status));
            status = uw_start_read_lines(&str_io);
            TEST(uw_ok(&status));

            UwValue line_f = uw_create_string("");
            UwValue line_s = uw_create_string("");
            for (;;) {{
                UwValue status_f = uw_read_line_inplace(&file, &line_f);
                UwValue status_s = uw_read_line_inplace(&str_io, &line_s);
                TEST(uw_equal(&status_f, &status_s));
                if (!uw_equal(&status_f, &status_s)) {
                    fprintf(stderr, "%s -- Line number: %u (file), %u (string I/O)\n",
                            sample_files[i],
                            uw_get_line_number(&file),
                            uw_get_line_number(&str_io)
                    );
                    uw_dump(stderr, &status_f);
                    uw_dump(stderr, &line_f);
                    uw_dump(stderr, &status_s);
                    uw_dump(stderr, &line_s);
                    break;
                }
                if (uw_eof(&status_f)) {
                    break;
                }
                TEST(uw_ok(&status_f));
                if (uw_error(&status_f)) {
                    break;
                }
                TEST(uw_equal(&line_f, &line_s));
                if (!uw_equal(&line_f, &line_s)) {
                    fprintf(stderr, "%s -- Line number: %u (file), %u (string I/O)\n",
                            sample_files[i],
                            uw_get_line_number(&file),
                            uw_get_line_number(&str_io)
                    );
                    uw_dump(stderr, &line_f);
                    uw_dump(stderr, &line_s);
                    break;
                }
                //fprintf(stderr, "Line %u\n", uw_get_line_number(&file));
                //uw_dump(stderr, &line_f);
            }}
        }}
    }
}

void test_string_io()
{
    UwValue sio = uw_create_string_io("one\ntwo\nthree");
    {
        UwValue line = uw_read_line(&sio);
        TEST(uw_equal(&line, "one\n"));
    }
    {
        UwValue line = UwString();
        UwValue status = uw_read_line_inplace(&sio, &line);
        TEST(uw_ok(&status));
        TEST(uw_equal(&line, "two\n"));

        // push back
        {
            bool status = uw_unread_line(&sio, &line);
            TEST(status);
        }
    }
    {
        // read pushed back
        UwValue line = uw_create_string("");
        UwValue status = uw_read_line_inplace(&sio, &line);
        TEST(uw_ok(&status));
        TEST(uw_equal(&line, "two\n"));
    }
    {
        UWDECL_String(line);
        UwValue status = uw_read_line_inplace(&sio, &line);
        TEST(uw_ok(&status));
        TEST(uw_equal(&line, "three"));
    }
    {
        // EOF
        UWDECL_String(line);
        UwValue status = uw_read_line_inplace(&sio, &line);
        TEST(uw_error(&status));
    }
    // start over again
    {
        UwValue status = uw_start_read_lines(&sio);
        TEST(uw_ok(&status));
        UwValue line = uw_read_line(&sio);
        TEST(uw_equal(&line, "one\n"));
    }
}

void test_netutils()
{
    {
        UwValue subnet = uw_create_string("192.168.0.0/24");
        UwValue netmask = UwNull();
        UwValue parsed_subnet = uw_parse_ipv4_subnet(&subnet, &netmask);
        IPv4subnet n_subnet = {
            .subnet  = (192 << 24) + (168 << 16),
            .netmask = 0xFFFFFF00
        };
        TEST(parsed_subnet.unsigned_value == n_subnet.value);
    }
    {
        UwValue subnet = uw_create_string("192.168.0.0");
        UwValue netmask = uw_create_string("255.255.255.0");
        UwValue parsed_subnet = uw_parse_ipv4_subnet(&subnet, &netmask);
        IPv4subnet n_subnet = {
            .subnet  = (192 << 24) + (168 << 16),
            .netmask = 0xFFFFFF00
        };
        TEST(parsed_subnet.unsigned_value == n_subnet.value);
    }
    {
        // prefer CIDR netmask
        UwValue subnet = uw_create_string("192.168.0.0/8");
        UwValue netmask = uw_create_string("255.255.255.0");
        UwValue parsed_subnet = uw_parse_ipv4_subnet(&subnet, &netmask);
        IPv4subnet n_subnet = {
            .subnet  = (192 << 24) + (168 << 16),
            .netmask = 0xFF000000
        };
        TEST(parsed_subnet.unsigned_value == n_subnet.value);
    }
    {
        // bad IP address
        UwValue subnet = uw_create_string("392.168.0.0/24");
        UwValue netmask = UwNull();
        UwValue parsed_subnet = uw_parse_ipv4_subnet(&subnet, &netmask);
        TEST(uw_error(&parsed_subnet));
        TEST(parsed_subnet.status_code == UW_ERROR_BAD_IP_ADDRESS);
        //uw_dump(stderr, &parsed_subnet);
    }
    {
        // bad CIDR netmask
        UwValue subnet = uw_create_string("192.168.0.0/124");
        UwValue netmask = UwNull();
        UwValue parsed_subnet = uw_parse_ipv4_subnet(&subnet, &netmask);
        TEST(parsed_subnet.status_code == UW_ERROR_BAD_NETMASK);
        //uw_dump(stderr, &parsed_subnet);
    }
    {
        // bad CIDR netmask
        UwValue subnet = uw_create_string("192.168.0.0/24/12");
        UwValue netmask = UwNull();
        UwValue parsed_subnet = uw_parse_ipv4_subnet(&subnet, &netmask);
        TEST(parsed_subnet.status_code == UW_ERROR_BAD_NETMASK);
        //uw_dump(stderr, &parsed_subnet);
    }

    // uw_split_addr_port
    {
        UwValue addr_port = uw_create_string("example.com:80");
        UwValue parts = uw_split_addr_port(&addr_port);
        UwValue addr = uw_array_item(&parts, 0);
        UwValue port = uw_array_item(&parts, 1);
        TEST(uw_equal(&addr, "example.com"));
        TEST(uw_equal(&port, "80"));
    }
    {
        UwValue addr_port = uw_create_string("80");
        UwValue parts = uw_split_addr_port(&addr_port);
        UwValue addr = uw_array_item(&parts, 0);
        UwValue port = uw_array_item(&parts, 1);
        TEST(uw_equal(&addr, ""));
        TEST(uw_equal(&port, "80"));
    }
    {
        UwValue addr_port = uw_create_string("::1");
        UwValue parts = uw_split_addr_port(&addr_port);
        UwValue addr = uw_array_item(&parts, 0);
        UwValue port = uw_array_item(&parts, 1);
        TEST(uw_equal(&addr, "::1"));
        TEST(uw_equal(&port, ""));
    }
    {
        UwValue addr_port = uw_create_string("[2001:db8:85a3:8d3:1319:8a2e:370:7348]:443");
        UwValue parts = uw_split_addr_port(&addr_port);
        UwValue addr = uw_array_item(&parts, 0);
        UwValue port = uw_array_item(&parts, 1);
        TEST(uw_equal(&addr, "[2001:db8:85a3:8d3:1319:8a2e:370:7348]"));
        TEST(uw_equal(&port, "443"));
    }
}

void test_args()
{
    {
        char* argv[] = {
            "/bin/sh",
            "foo=bar",
            "one=1",
            "two",
            "three",
            "four=4"
        };
        UwValue args = uw_parse_kvargs(
            UW_LENGTH(argv),
            argv
        );
        TEST(uw_is_map(&args));
        for(unsigned i = 0; i < UW_LENGTH(argv); i++) {{
            UwValue k = UwNull();
            UwValue v = UwNull();
            TEST(uw_map_item(&args, i, &k, &v));
            if (i == 0) {
                TEST(uw_equal(&k, 0));
                TEST(uw_equal(&v, argv[i]));
            } else {
                char* separator = strchr(argv[i], '=');
                if (separator) {
                    unsigned n = separator - argv[i];
                    char key[n + 1];
                    strncpy(key, argv[i], n);
                    key[n] = 0;
                    TEST(uw_equal(&k, key));
                    TEST(uw_equal(&v, separator + 1));
                } else {
                    TEST(uw_equal(&k, argv[i]));
                    TEST(uw_is_null(&v));
                }
            }
        }}
        //uw_dump(stderr, &args);
    }
}

void test_json()
{
    UwValue value = UwArray(
        UwString_1_12(4, 't', 'h', 'i', 's', 0,0,0,0,0,0,0,0),
        UwString_1_12(2, 'i', 's', 0,0,0,0,0,0,0,0,0,0),
        UwString_1_12(1, 'a', 0,0,0,0,0,0,0,0,0,0,0),
        UwMap(
            UwString_1_12(6, 'n', 'u', 'm', 'b', 'e', 'r',0,0,0,0,0,0),
            UwSigned(1),
            UwString_1_12(4, 'l', 'i', 's', 't', 0,0,0,0,0,0,0,0),
            UwArray(
                UwString_1_12(3, 'o', 'n', 'e', 0,0,0,0,0,0,0,0,0),
                UwString_1_12(3, 't', 'w', 'o', 0,0,0,0,0,0,0,0,0),
                UwMap(
                    UwString_1_12(5, 't', 'h', 'r', 'e', 'e', 0,0,0,0,0,0,0),
                        UwArray(
                            UwSigned(1),
                            UwSigned(2),
                            UwMap( uw_create_string("four"), uw_create_string("five\nsix\n"))
                        )
                )
            )
        ),
        UwString_1_12(8, 'd', 'a', 'z', ' ', 'g', 'o', 'o', 'd', 0,0,0,0)
    );
    {
        UwValue result = uw_to_json(&value, 0);
        UwValue reference = uw_create_string(
            "[\"this\",\"is\",\"a\",{\"number\":1,\"list\":[\"one\",\"two\",{\"three\":[1,2,{\"four\":\"five\\nsix\\n\"}]}]},\"daz good\"]"
        );
        //uw_dump(stderr, &result);
        //uw_dump(stderr, &reference);
        //UW_CSTRING_LOCAL(json, &result);
        //fprintf(stderr, "%s\n", json);
        TEST(uw_equal(&result, &reference));
    }
    {
        UwValue result = uw_to_json(&value, 4);
        UwValue reference = uw_create_string(
            "[\n"
            "    \"this\",\n"
            "    \"is\",\n"
            "    \"a\",\n"
            "    {\n"
            "        \"number\": 1,\n"
            "        \"list\": [\n"
            "            \"one\",\n"
            "            \"two\",\n"
            "            {\"three\": [\n"
            "                1,\n"
            "                2,\n"
            "                {\"four\": \"five\\nsix\\n\"}\n"
            "            ]}\n"
            "        ]\n"
            "    },\n"
            "    \"daz good\"\n"
            "]"
        );
        //uw_dump(stderr, &result);
        //UW_CSTRING_LOCAL(json, &result);
        //fprintf(stderr, "%s\n", json);
        TEST(uw_equal(&result, &reference));
    }
}

int main(int argc, char* argv[])
{
    //debug_allocator.verbose = true;
    //init_allocator(&debug_allocator);

    //init_allocator(&stdlib_allocator);

    //pet_allocator.trace = true;
    //pet_allocator.verbose = true;
    init_allocator(&pet_allocator);

    UwValue start_time = uw_monotonic();

    test_icu();
    test_integral_types();
    test_string();
    test_array();
    test_map();
    test_file();
    test_string_io();
    test_netutils();
    test_args();
    test_json();

    UwValue end_time = uw_monotonic();
    UwValue timediff = uw_timestamp_diff(&end_time, &start_time);
    UwValue timediff_str = uw_to_string(&timediff);
    UW_CSTRING_LOCAL(timediff_cstr, &timediff_str);
    fprintf(stderr, "time elapsed: %s\n", timediff_cstr);

    if (num_fail == 0) {
        fprintf(stderr, "%d test%s OK\n", num_tests, (num_tests == 1)? "" : "s");
    }

    fprintf(stderr, "leaked blocks: %zu\n", default_allocator.stats->blocks_allocated);
}
