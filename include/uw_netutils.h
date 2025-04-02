#pragma once

#include <uw.h>

#ifdef __cplusplus
extern "C" {
#endif


extern uint16_t UW_ERROR_BAD_ADDRESS_FAMILY;
extern uint16_t UW_ERROR_BAD_IP_ADDRESS;
extern uint16_t UW_ERROR_MISSING_NETMASK;
extern uint16_t UW_ERROR_BAD_NETMASK;
extern uint16_t UW_ERROR_PORT_UNSPECIFIED;


UwResult uw_parse_ipv4_address(UwValuePtr addr);
/*
 * Parse IPv4 address.
 *
 * On success return UwType_Unsigned which contains IP address
 * in low 32 bits in host byte order.
 * The helper functions uw_ipv4_address() is to convert it to uint32_t.
 *
 * On error return status value.
 */

static inline uint32_t uw_ipv4_address(UwValuePtr addr)
{
    return (uint32_t) addr->unsigned_value;
}

UwResult uw_parse_ipv4_subnet(UwValuePtr subnet, UwValuePtr netmask);
/*
 * Parse IPv4 subnet.
 * If subnet is in CIDR notation, netmask argument is not used.
 *
 * On success return UwType_Unsigned which contains IPv4subnet.
 * The helper functions uw_ipv4_subnet() and uw_ipv4_netmask()
 * are to extract subnet and netmask fields.
 *
 * On error return status value.
 */

typedef union {
    uint64_t  value;
    struct {
        // values are in host byte order
        uint32_t  subnet;
        uint32_t  netmask;
    };
} IPv4subnet;

static inline uint32_t uw_ipv4_subnet(UwValuePtr subnet)
{
    return ((IPv4subnet*) &subnet->unsigned_value)->subnet;
}

static inline uint32_t uw_ipv4_netmask(UwValuePtr subnet)
{
    return ((IPv4subnet*) &subnet->unsigned_value)->netmask;
}

UwResult uw_split_addr_port(UwValuePtr addr_port);
/*
 * Split address and port separated by colon.
 * Return array containing exactly two items.
 * Both parts can be empty string.
 * Port can be a service name, understood by getaddrinfo.
 */

#ifdef __cplusplus
}
#endif
