#include <arpa/inet.h>

#include <include/uw_netutils.h>
#include <src/uw_struct_internal.h>

uint16_t UW_ERROR_BAD_ADDRESS_FAMILY = 0;
uint16_t UW_ERROR_BAD_IP_ADDRESS = 0;
uint16_t UW_ERROR_MISSING_NETMASK = 0;
uint16_t UW_ERROR_BAD_NETMASK = 0;
uint16_t UW_ERROR_PORT_UNSPECIFIED = 0;

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
    UwValue parts = uw_string_split_chr(subnet, '/', 0);
    uw_return_if_error(&parts);

    unsigned num_parts = uw_array_length(&parts);
    if (num_parts > 1) {
        // try CIDR netmask
        UwValue cidr_netmask = uw_array_item(&parts, 1);
        uw_return_if_error(&cidr_netmask);

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
        UwValue parsed_netmask = uw_parse_ipv4_address(netmask);
        uw_return_if_error(&parsed_netmask);

        ipv4_subnet.netmask = parsed_netmask.unsigned_value;
    }

    // parse subnet address
    UwValue addr = uw_array_item(&parts, 0);
    uw_return_if_error(&addr);

    UwValue parsed_subnet = uw_parse_ipv4_address(&addr);
    uw_return_if_error(&parsed_subnet);

    ipv4_subnet.subnet = parsed_subnet.unsigned_value;

    return UwUnsigned(ipv4_subnet.value);
}

UwResult uw_split_addr_port(UwValuePtr addr_port)
{
    UwValue emptystr = UwString();
    UwValue parts = uw_string_rsplit_chr(addr_port, ':', 1);
    uw_return_if_error(&parts);

    if (uw_array_length(&parts) == 1) {
        // Assume addr_port contains port (or service name) only.
        // Insert empty string for address part.
        if (!uw_array_insert(&parts, 0, &emptystr)) {
            return UwOOM();
        }
    } else {
        UwValue addr = uw_array_item(&parts, 0);
        UwValue port = uw_array_item(&parts, 1);

        uw_return_if_error(&addr);
        uw_return_if_error(&port);

        if (uw_strchr(&addr, ':', 0, nullptr)) {
            // IPv6 address?
            if (uw_startswith(&addr, '[') && uw_endswith(&addr, ']')) {
                // all right
            } else {
                // port is missing
                uw_array_clean(&parts);
                if (!uw_array_append(&parts, addr_port)) {
                    return UwOOM();
                }
                if (!uw_array_append(&parts, &emptystr)) {
                    return UwOOM();
                }
            }
        }
    }
    return uw_move(&parts);
}
