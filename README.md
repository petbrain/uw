# UW library

## Warning

Everything is work in progress and subject to change.

## Building

The following environment variables are honoured by cmake:

* `DEBUG`: debug build (XXX not fully implementeded in cmake yet)
* `UW_WITHOUT_ICU`: if defined (the value does not matter), build without ICU dependency

## UW values

UW is primarily targeted to 64-bit systems. All values are 128 bit wide.

The basic UW values do not require memory allocation.
Functions never return pointers to UW value, they always return the whole 128 bit structure.
It's the caller's responsibility to destroy values returned by functions.
The simplest and natural way to do that is assigning them to automatically cleaned
local variables declared with `UwValue`.

Generally speaking, variables should always be declared using `UwValue`
type specifier and initialization clause.
Such variables are automatically destroyed on scope exit thanks to `gnu::cleanup` attribute.
Yes, it's a `GNU` extension, but this is supported by `Clang` too.
Sorry, msvc users.
```c
{
    UwValue myvar1 = UwUnsigned(123);
    UwValue myvar2 = UwNull();

    // do the job
}
```

Note, if `UwValue` is used in a loop body, always use nested scope (double curly brackets)
to run destructors on each iteration:
```c
for (unsigned i = 0, n = uw_array_length(my_array); i < n; i++) {{
    UwValue value = uw_array_item(my_array, i);

    // process item

    // without nested scope we'd have to explicitly
    // call uw_destroy(&item)
}}
```

Functions should use `UwResult` for the type of return value.
Upon exit the value should be returned with `uw_move` that prevents automatic destruction:
```c
UwResult foo()
{
    UwValue result = UwBool(false);

    // do the job

    return uw_move(&result);
}
```

Values are passed to functions by reference to make things a bit more efficient.
Functions do not and should not modify arguments, they treat them as immutable.

There are a few exceptions that accept UW values by value: `UwArray()`, `UwMap()`,
`uw_strcat()`, and `uw_path()`. All they are variadic functions, and they destroy
their arguments upon return. This is convenient to construct objects
using function calls only, i.e.
```
UwValue my_map = UwMap(
    UwCharPtr("foo"), get_answer(),
    UwCharPtr("bar"), UwSigned(42)
)
```

but extra care should be taken when passing local variables:
always use `uw_clone()` for this purpose.


## Type system

The first member of UW value structure is type identifier.
It takes 16 bits. Other bits in the first 64-bit half can be used for various purposes.
The second 64-bit half contains either a value of specific type or a pointer.

The type of value can be obtained with `uw_typeof` macro that return a pointer to `UwType` structure.
That structure contains `id`, `name`, `ancestor_id`, basic interface, and other fields.

As you might already guess, `ancestor_id` field implies type hierarchy.

### UW type hierarchy

```
                                UwType
                                  |
   +------+-----+-------+---------+---------+----------+--------+
   |      |     |       |         |         |          |        |
UwNull UwBool UwInt  UwFloat UwDateTime,   UwPtr,   UwString UwStruct
                |            UwTimestamp  UwCharPtr             |
                |                                               |
           +----+----+                 +----------+---------+---+-----+
           |         |                 |          |         |         |
       UwSigned UwUnsigned          UwStatus  UwStringIO  UwFile  UwCompound
                                                                      |
                                                               +------+
                                                               |      |
                                                            UwArray  UwMap
```

Integral types `UwNull`, `UwBool`, `UwInt`, `UwFloat`, `UwPtr`, and `UwCharPtr`
are self-contained and do not contain pointers to allocated memory blocks.
Actually, `UwPtr` and `UwCharPtr` may do, but it's entirely up to the user
to manage that.

`UwString` may allocate memory if string does not fit into 128-bit structure.
For single-byte character this limit is 12 which is more than double of average
length of English word.

`UwCharPtr` facilitates work with null-terminated C strings.
Its `clone` method converts C strings to `UwString` and other types, such as
array and map, take advantage of that:
```c
UwValue my_map = UwMap(
    UwCharPtr(u8"สวัสดี"), UwChar32Ptr(U"สบาย"),
    UwCharPtr("let's"),  UwCharPtr("go!")
)
// now my_map contains UW strings only
```

`char*` and `char8_t*` are treated equally, as UTF-8 strings.

`Struct` type is the basic type for structured and `Compound` types.
It handles data allocation and reference counting, although allocated data
is optional for `Status`.

The data attached to `Struct` may contain UW values, but it does not handle
circular references which may cause memory leak.

`Compound` type handles circular references. It's the base type for `UwArray`
and `UwMap` which may contain any items, including self.
No garbage collection is necessary.

`Status` type is used to check returned UW values for errors.
Because of this special purpose, `Status` values can't be added to arrays
and used in maps.

## Interfaces

`UwType` structure embeds two interfaces: `basic` and `struct`.
They are simply pointers to functions.

Additional interfaces are stored in a dynamically allocated array
and require linear lookup to get interface pointer by id.
This is still fast as long as the number of interfaces is usually
countable by fingers.

From the user's perspective interfaces are structures containing function pointers
but UW library knows nothing about user-defined types and treats
interfaces as arrays of function pointers.
The number of functions in each interface is stored in a global list
when interface is registered.

Methods of interfaces are invoked like this:
```c
    uw_interface(UwTypeId_MyType, MyInterface)->my_method(self);
```

Note that type id must be provided explicitly and cannot be taken from `self`.
That's because methods are linked to interfaces, not data they process.
If we took type id from `self`, this would lead to infinite loop if the method
called super.

If a method of interface wants to call super method, it should do this way:
```c
    uw_interface(uw_ancestor_of(UwTypeId_MyType), MyInterface)->my_method(self);
```

## Strings

UW strings support any character width, from 1 to 4 bytes.
The initial width can be specified when string is created.
It is automagically expanded when necessary.

Short strings are stored in 128-bit wide UW value structure:
* 1 byte wide: up to 12 characters
* 2 byte wide: up to 6 characters
* 3 byte wide: up to 4 characters
* 4 byte wide: up to 3 characters

Longer strings are stored in dynamically allocated memory blocks.
Blocks contain reference count, so making a copy is fast operation (`clone`, in terms of UW).

Strings are mutable, but they are copied on write.
In-place modifications are allowed only for strings with reference count equal to one.

The length of string is always 32 bit wide, even on ILP64.

### COW rules for strings

* if a string is copied, only refcount is incremented
* if a string is about to be modified and refcount is 1, it is modified in place.
* if a string is about to be modified and refcount is more than 1, a copy is created
  with refcount 1 and then modified in place.
* string capacity is preserved on copy

## Iterators

Iterators are at the very early development stage.
The only iterator at the moment is `LineReader` interface
moved from `StringIO`.

Basically, most iterables may have multiple iterators.
Mind algorithms with nested loops iterating over same array or list.

However, some iterables may support single iterator only.
These are files and most kinds of streams that can be read
only sequentially, without prior knowledge of size.

Such iterators can be considered as singletons.
Moreover, iterables that allow singleton iterators only,
can provide iterator interfaces for self.

There can be multiple versions of iterators for the same iterable.
Back to files, `LineReader` is not the only possible iterator.
There also can be `ByteReader` and `CharReader` and file iterable
may provide all of them.

Iterators that are separate from their iterables are represented
by `UwType_Iterator`.
This type contains iterator state and a clone of iterable value.

Iterables must include `itercount` field that shows how many iterations
are currently in progress.
Methods that modify iterable must fail when this field is nonzero.
