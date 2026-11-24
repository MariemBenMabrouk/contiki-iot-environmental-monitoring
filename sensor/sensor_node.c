#include "contiki.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "net/uip-debug.h"
#include "net/rpl/rpl.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define SEND_INTERVAL (10 * CLOCK_SECOND)

PROCESS(environmental_monitoring_process,
        "6LoWPAN Environmental Sensor");

AUTOSTART_PROCESSES(&environmental_monitoring_process);

static struct uip_udp_conn *udp_conn;
static struct etimer timer;

static uint16_t sample_number = 0;

/* Derive the sensor ID from the complete link-layer address, not just its
 * final byte (which may be identical on multiple motes). */
static uint32_t get_node_id(void)
{
    uint32_t hash = 2166136261UL;
    int i;

    for(i = 0; i < sizeof(uip_lladdr.addr); i++) {
        hash = (hash ^ uip_lladdr.addr[i]) * 16777619UL;
    }

    return hash;
}

/* Configure IPv6 address */
static void set_global_address(void)
{
    uip_ipaddr_t ipaddr;

    uip_ip6addr(&ipaddr,
                0xaaaa, 0, 0, 0,
                0, 0, 0, 0);

    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);

    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
}

/* Generate and send environmental measurements */
static void send_measurements(void)
{
    uint32_t node_id = get_node_id();

    /* Different baseline for each sensor */
    uint16_t temperature_base =
        190 + (node_id * 23U) % 80;

    uint16_t luminosity_base =
        150 + (node_id * 137U) % 500;

    /* Time-dependent variations */
    uint16_t phase =
        (sample_number + node_id * 3U) % 12;

    uint16_t temperature =
        temperature_base + (phase % 7) * 5;

    uint16_t luminosity =
        luminosity_base +
        ((sample_number * 83U + node_id * 47U) % 200);

    char message[128];

    snprintf(message, sizeof(message),
             "{\"node\":%lu,\"temperature\":%u.%u,\"luminosity\":%u,\"sample\":%u}",
             (unsigned long)node_id,
             temperature / 10,
             temperature % 10,
             luminosity,
             sample_number);

    /* Send data to gateway */
    uip_udp_packet_send(udp_conn,
                        message,
                        strlen(message));

    printf("ENVIRONMENTAL DATA: %s\n", message);

    sample_number++;
}

/* Main Contiki process */
PROCESS_THREAD(environmental_monitoring_process, ev, data)
{
    uip_ipaddr_t destination;
    int i;

    PROCESS_BEGIN();

    set_global_address();

    printf("\n=== ENVIRONMENTAL SENSOR STARTED ===\n");
    printf("Node ID: %lu\n", (unsigned long)get_node_id());

    printf("Link-layer address: ");
    for(i = 0; i < sizeof(uip_lladdr.addr); i++) {
        printf("%02x", uip_lladdr.addr[i]);
    }
    printf("\n");

    /* Gateway IPv6 address */
    uip_ip6addr(&destination,
                0xaaaa, 0, 0, 0,
                0, 0x00ff, 0xfe00, 1);

    udp_conn = udp_new(&destination,
                       UIP_HTONS(UDP_SERVER_PORT), NULL);

    if(udp_conn == NULL) {
        printf("ERROR: UDP connection creation failed\n");
        PROCESS_EXIT();
    }

    udp_bind(udp_conn, UIP_HTONS(UDP_CLIENT_PORT));

    printf("UDP connection configured\n");

    etimer_set(&timer, SEND_INTERVAL);

    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

        send_measurements();

        etimer_reset(&timer);
    }

    PROCESS_END();
}
