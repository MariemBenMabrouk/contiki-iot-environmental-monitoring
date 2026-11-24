
#include "contiki.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "net/uip-debug.h"
#include "net/rpl/rpl.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* =================================
   CONFIGURATION
   ================================= */

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define SEND_INTERVAL (10 * CLOCK_SECOND)

/* =================================
   PROCESS DECLARATION
   ================================= */

PROCESS(environmental_monitoring_process,
        "6LoWPAN Environmental Sensor");

AUTOSTART_PROCESSES(&environmental_monitoring_process);

/* =================================
   GLOBAL VARIABLES
   ================================= */

static struct uip_udp_conn *udp_conn;
static struct etimer timer;

static uint16_t sample_number = 0;

/* =================================
   SENSOR IDENTIFICATION
   ================================= */

/* Extract sensor ID from link-layer address */
static uint8_t get_node_id(void)
{
    return uip_lladdr.addr[7];
}

/* =================================
   IPV6 CONFIGURATION
   ================================= */

static void set_global_address(void)
{
    uip_ipaddr_t ipaddr;

    uip_ip6addr(&ipaddr,
                0xaaaa, 0, 0, 0,
                0, 0, 0, 0);

    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);

    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
}

/* =================================
   ENVIRONMENTAL MEASUREMENTS
   ================================= */

static void send_measurements(void)
{
    uint8_t node_id = get_node_id();

    uint16_t temperature_base;
    uint16_t luminosity_base;

    /* Assign a different environment to each sensor */

    switch(node_id) {

        case 1:
            temperature_base = 210;
            luminosity_base = 180;
            break;

        case 2:
            temperature_base = 240;
            luminosity_base = 350;
            break;

        case 3:
            temperature_base = 190;
            luminosity_base = 500;
            break;

        case 4:
            temperature_base = 270;
            luminosity_base = 250;
            break;

        case 5:
            temperature_base = 220;
            luminosity_base = 650;
            break;

        default:
            temperature_base = 200;
            luminosity_base = 300;

            printf("WARNING: Unknown sensor ID: %u\n",
                   node_id);
            break;
    }

    /* Generate variations over time */

    uint16_t temperature =
        temperature_base + (sample_number % 5) * 2;

    uint16_t luminosity =
        luminosity_base + (sample_number * 17) % 100;

    char message[128];

    snprintf(message, sizeof(message),
             "{\"node\":%u,\"temperature\":%u.%u,\"luminosity\":%u,\"sample\":%u}",
             node_id,
             temperature / 10,
             temperature % 10,
             luminosity,
             sample_number);

    /* Send measurements to gateway */

    uip_udp_packet_send(udp_conn,
                        message,
                        strlen(message));

    printf("ENVIRONMENTAL DATA: %s\n", message);

    sample_number++;
}

/* =================================
   MAIN CONTIKI PROCESS
   ================================= */

PROCESS_THREAD(environmental_monitoring_process, ev, data)
{
    uip_ipaddr_t destination;
    int i;

    PROCESS_BEGIN();

    /* Configure IPv6 */

    set_global_address();

    /* Display sensor identity */

    printf("\n================================\n");
    printf("ENVIRONMENTAL SENSOR STARTED\n");
    printf("Node ID: %u\n", get_node_id());

    printf("Link-layer address: ");

    for(i = 0; i < sizeof(uip_lladdr.addr); i++) {
        printf("%02x", uip_lladdr.addr[i]);
    }

    printf("\n================================\n");

    /* Gateway IPv6 address */

    uip_ip6addr(&destination,
                0xaaaa, 0, 0, 0,
                0, 0x00ff, 0xfe00, 1);

    /* Create UDP connection */

    udp_conn = udp_new(&destination,
                       UIP_HTONS(UDP_SERVER_PORT),
                       NULL);

    if(udp_conn == NULL) {
        printf("ERROR: UDP connection creation failed\n");
        PROCESS_EXIT();
    }

    udp_bind(udp_conn, UIP_HTONS(UDP_CLIENT_PORT));

    printf("UDP connection configured\n");

    /* Start periodic measurements */

    etimer_set(&timer, SEND_INTERVAL);

    while(1) {

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

        send_measurements();

        etimer_reset(&timer);
    }

    PROCESS_END();
}
