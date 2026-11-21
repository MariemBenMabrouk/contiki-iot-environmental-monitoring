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

static void set_global_address(void)
{
    uip_ipaddr_t ipaddr;

    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
}

static void send_measurements(void)
{
    uint16_t phase = sample_number % 12;
    uint16_t temperature = 210 + (phase % 7) * 7;
    uint16_t luminosity = 180 + ((sample_number * 83) % 720);

    char message[100];

    snprintf(message, sizeof(message),
             "{\"node\":%u,\"temperature\":%u.%u,\"luminosity\":%u,\"sample\":%u}",
             uip_lladdr.addr[7],
             temperature / 10,
             temperature % 10,
             luminosity,
             sample_number);

    uip_udp_packet_send(udp_conn, message, strlen(message));

    printf("ENVIRONMENTAL DATA: %s\n", message);

    sample_number++;
}

PROCESS_THREAD(environmental_monitoring_process, ev, data)
{
    uip_ipaddr_t destination;

    PROCESS_BEGIN();

    set_global_address();

    uip_ip6addr(&destination,
                0xaaaa, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);

    udp_conn = udp_new(&destination,
                       UIP_HTONS(UDP_SERVER_PORT), NULL);

    udp_bind(udp_conn, UIP_HTONS(UDP_CLIENT_PORT));

    printf("Environmental sensor started\n");

    etimer_set(&timer, SEND_INTERVAL);

    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

        send_measurements();

        etimer_reset(&timer);
    }

    PROCESS_END();
}
