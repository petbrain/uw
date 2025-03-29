#include <arpa/inet.h>

#include <include/uw_netutils.h>
#include <src/uw_struct_internal.h>

uint16_t UW_ERROR_BAD_ADDRESS_FAMILY = 0;
uint16_t UW_ERROR_BAD_IP_ADDRESS = 0;
uint16_t UW_ERROR_MISSING_NETMASK = 0;
uint16_t UW_ERROR_BAD_NETMASK = 0;

UwResult uw_parse_ipv4_address(UwValuePtr addr)
{
    UW_CSTRING_LOCAL(c_addr, addr);
    struct in_addr ipaddr;

    int rc = inet_pton(AF_INET, c_addr, &ipaddr);
    if (rc == -1) {
        return UwError(UW_ERROR_BAD_ADDRESS_FAMILY);
    }
    if (rc != 1) {
        UwValue error = UwError(UW_ERROR_BAD_IP_ADDRESS);
        _uw_set_status_desc(&error, "Bad IPv4 address %s", c_addr);
        return uw_move(&error);
    }
    return UwUnsigned(ntohl(ipaddr.s_addr));
}

UwResult uw_parse_ipv4_subnet(UwValuePtr subnet, UwValuePtr netmask)
{
    IPv4subnet ipv4_subnet;

    if (!uw_is_string(subnet)) {
        return UwError(UW_ERROR_BAD_IP_ADDRESS);
    }

    // check CIDR notation
    UwValue parts = uw_string_split_chr(subnet, '/');
    if (uw_error(&parts)) {
        return uw_move(&parts);
    }
    unsigned num_parts = uw_list_length(&parts);
    if (num_parts > 1) {
        // try CIDR netmask
        UwValue cidr_netmask = uw_list_item(&parts, 1);
        if (uw_error(&cidr_netmask)) {
            return uw_move(&cidr_netmask);
        }
        UW_CSTRING_LOCAL(c_netmask, &cidr_netmask);

        long n = strtol(c_netmask, nullptr, 10);
        if (n == 0 || n >= 32 || num_parts > 2) {
            UwValue error = UwError(UW_ERROR_BAD_NETMASK);
            UW_CSTRING_LOCAL(c_subnet, subnet);
            _uw_set_status_desc(&error, "Bad netmask %s", c_subnet);
            return uw_move(&error);
        }
        ipv4_subnet.netmask = 0xffffffff << (32 - n);

    } else {
        // not CIDR notation, parse netmask parameter
        if (!uw_is_string(netmask)) {
            return UwError(UW_ERROR_MISSING_NETMASK);
        }
        UwResult parsed_netmask = uw_parse_ipv4_address(netmask);
        if (uw_error(&parsed_netmask)) {
            return uw_move(&parsed_netmask);
        }
        ipv4_subnet.netmask = parsed_netmask.unsigned_value;
    }

    // parse subnet address
    UwValue addr = uw_list_item(&parts, 0);
    if (uw_error(&addr)) {
        return uw_move(&addr);
    }
    UwResult parsed_subnet = uw_parse_ipv4_address(&addr);
    if (uw_error(&parsed_subnet)) {
        return uw_move(&parsed_subnet);
    }
    ipv4_subnet.subnet = parsed_subnet.unsigned_value;

    return UwUnsigned(ipv4_subnet.value);
}

/****************************************************************
 * Connection type
 */

UwTypeId UwTypeId_Connection = 0;

static UwResult connection_init(UwValuePtr self, void* ctor_args)
{
    // call super method, we know the ancestor is Compound
    UwValue status = _uw_types[UwTypeId_Compound]->init(self, ctor_args);

    // if we did not knew, then:
    // UwValue status = uw_ancestor_of(UwTypeId_Connection)->init(self, ctor_args);

    if (uw_error(&status)) {
        return uw_move(&status);
    }

    // init connection

    _UwConnectionData* data = _uw_connection_data_ptr(self);
    data->sock = -1;

    // no need to call super method

    return UwOK();
}

static void connection_fini(UwValuePtr self)
{
    // do not call Struct.fini because it's a no op
}

static void connection_hash(UwValuePtr self, UwHashContext* ctx)
{
    _UwConnectionData* data = _uw_connection_data_ptr(self);

    _uw_hash_uint64(ctx, self->type_id);
    _uw_hash_uint64(ctx, data->sock);
    _uw_hash_uint64(ctx, (uint64_t) data->handler);
    _uw_hash_uint64(ctx, (uint64_t) data->data);
}

static void connection_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _UwConnectionData* data = _uw_connection_data_ptr(self);

    _uw_dump_start(fp, self, first_indent);
    _uw_dump_struct_data(fp, self);

    fprintf(fp, " socket %d, handler %p, data %p\n",
            data->sock, data->handler, data->data);
}

static bool connection_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return false;
}

static bool connection_equal(UwValuePtr self, UwValuePtr other)
{
    return false;
}

static UwType connection_type = {
    .id             = 0,
    .ancestor_id    = UwTypeId_Struct,
    .name           = "Connection",
    .allocator      = &default_allocator,

    .create         = _uw_struct_create,
    .destroy        = _uw_struct_destroy,
    .clone          = _uw_struct_clone,
    .hash           = connection_hash,
    .deepcopy       = _uw_struct_deepcopy,
    .dump           = connection_dump,
    .to_string      = _uw_struct_to_string,
    .is_true        = _uw_struct_is_true,
    .equal_sametype = connection_equal_sametype,
    .equal          = connection_equal,

    .data_offset    = sizeof(_UwStructData),
    .data_size      = sizeof(_UwConnectionData),

    .init           = connection_init,
    .fini           = connection_fini
};

// make sure _UwStructData has correct padding
static_assert((sizeof(_UwStructData) & (alignof(_UwConnectionData) - 1)) == 0);

/****************************************************************
 * Initialization
 */

[[ gnu::constructor ]]
static void init_netutils()
{
    // init statuses
    UW_ERROR_BAD_ADDRESS_FAMILY = uw_define_status("BAD_ADDRESS_FAMILY");
    UW_ERROR_BAD_IP_ADDRESS     = uw_define_status("BAD_IP_ADDRESS");
    UW_ERROR_MISSING_NETMASK    = uw_define_status("MISSING_NETMASK");
    UW_ERROR_BAD_NETMASK        = uw_define_status("BAD_NETMASK");

    // init connection type
    UwTypeId_Connection = uw_add_type(&connection_type);
}
