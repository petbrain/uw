#pragma once

#include <sys/socket.h>

#include <uw.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*PollEventHandlerFunc)(int sock, uint32_t epoll_events, void* data);

typedef struct {
    int sock;
    struct sockaddr_storage local_addr;
    struct sockaddr_storage remote_addr;
    PollEventHandlerFunc handler;
    void* data;
} _UwConnectionData;

extern UwTypeId UwTypeId_Connecttion;

#define _uw_connection_data_ptr(value)  ((_UwConnectionData*) _uw_get_data_ptr((value), UwTypeId_Connection))


extern uint16_t UW_ERROR_BAD_ADDRESS_FAMILY;
extern uint16_t UW_ERROR_BAD_IP_ADDRESS;
extern uint16_t UW_ERROR_MISSING_NETMASK;
extern uint16_t UW_ERROR_BAD_NETMASK;

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

#ifdef __cplusplus
}
#endif
