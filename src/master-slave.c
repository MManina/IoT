#include "contiki.h"
#include <stdlib.h>
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/ctimer.h"
#include "dev/leds.h"

/*-------------------------------------------*/

#define SEND_INTERVAL (10 * CLOCK_SECOND)
#define OFF_LEDS_TIMER_INTERVAL (CLOCK_SECOND/10)
#define BLUE_LEDS_TIMER_INTERVAL (CLOCK_SECOND*0.2)
#define RED_LEDS_TIMER_INTERVAL (CLOCK_SECOND/2)
#define UDP_PORT 5555
#define MY_ID 10
#define TEAM_MATE_ID 9

enum{ search, syn, cnt }stage = search;

enum{ unknowm, master, slave }mote = unknowm;

/*-------------------------------------------*/

PROCESS(search_process, "Search process");
PROCESS(master_syn_process, "Master process");
PROCESS(slave_syn_process, "Slave process");
PROCESS(master_cnt_process, "Connection master process");
PROCESS(slave_cnt_process, "Connection slave process");
PROCESS(led_process, "Led process");

AUTOSTART_PROCESSES(&search_process, &led_process);

/*-------------------------------------------*/

static uint16_t my_id = MY_ID;
static struct simple_udp_connection udp_socket;
static uip_ipaddr_t broadcast_ipaddr;
static uip_ipaddr_t unicast_ipaddr;
static uip_ipaddr_t my_unicast_addr;
static uint16_t ping = 0;
static uint16_t pong = 0;

/*-------------------------------------------*/

static void off_led_callback()
{
	leds_off(LEDS_RED);
}

/* Fonction callback lors du passage en stage connection */
static void connection_callback()
{
	stage = cnt;
	printf("stage = cnt");
	process_start(&slave_cnt_process, NULL);
}

static void pong_callback()
{
	char pong_str[50];
	
	snprintf(pong_str, sizeof(pong_str), "%d- pong -%d", pong, ping);
	printf("==> send %d- pong -%d", pong, ping);
	leds_on(LEDS_GREEN);
	leds_off(LEDS_RED);
	
	simple_udp_sendto(&udp_socket, pong_str, strlen(pong_str), &unicast_ipaddr);
}

/* Fonction callback lorsqu'on reçois un message */
static void message_received_callback(struct
simple_udp_connection *c,
const uip_ipaddr_t *sender_addr,
uint16_t sender_port,
const uip_ipaddr_t *receiver_addr,
uint16_t receiver_port,
const uint8_t *data,
uint16_t datalen)
{
	struct ctimer ct;
	char received_message[50];
	
	if(uip_ipaddr_cmp(receiver_addr, &broadcast_ipaddr) && stage == search && mote == unknowm){
		/* Reception d'un ID*/
		uint16_t received_id;
		memcpy(&received_id, data, sizeof(received_id));
		printf("<== received ID : %d\n", received_id);
		
		/* Passage en stage syn et mode slave */
		if(received_id == TEAM_MATE_ID){
			stage = syn;
			printf("stage = syn\n");
			mote = slave;
			printf("mote = slave\n");
			unicast_ipaddr = *sender_addr;
			process_start(&slave_syn_process, NULL);
		}
	}else if(uip_ipaddr_cmp(receiver_addr, &my_unicast_addr)){
		
		/* sauvegarde l'adresse du sender */
		unicast_ipaddr = *sender_addr;	
		
		/* Passage en stage syn et mode master */
		if(stage == search && mote == unknowm)
		{
			/* Reception du message */
			memcpy(&received_message, data, sizeof(received_message));
			printf("<== received message: %s\n", received_message);
			
			stage = syn;
			printf("stage = syn\n");
			mote = master;
			printf("mote = master\n");
			process_start(&master_syn_process, NULL);
		}
		
		/* Passage en mode connection */
		else if(stage == syn && mote == slave){
			/* Reception du message */
			memcpy(&received_message, data, sizeof(received_message));
			printf("<== received message: %s\n", received_message);
			
			ctimer_set(&ct, CLOCK_SECOND * 5, connection_callback, NULL);
		}
		
		else if(stage == cnt && mote == master){
			/* Reception du message */
			memcpy(&received_message, data, sizeof(received_message));
			printf("<== received message: %s\n", received_message);
			leds_on(LEDS_RED);
			ctimer_set(&ct, OFF_LEDS_TIMER_INTERVAL, off_led_callback, NULL);
		}
		
		else if(stage == cnt && mote == slave){
			/* Reception du message */
			memcpy(&ping, data, sizeof(ping));
			printf("<== received ping: %d\n", ping);
			leds_on(LEDS_RED);
			leds_off(LEDS_GREEN);
			
			pong++;
			
			ctimer_set(&ct, CLOCK_SECOND, pong_callback, NULL);
		}
	}
}

/*-------------------------------------------*/

/* Process qui gère les leds */
PROCESS_THREAD(led_process, ev, data)
{
	static struct etimer on_timer;
	static struct etimer off_timer;
	static int timer;
	static int led; 
	
	PROCESS_BEGIN();
	
	while(stage != cnt){
		if(stage == search){
			timer = RED_LEDS_TIMER_INTERVAL;
			led = LEDS_RED;
		}
		else if(stage == syn){
			timer = BLUE_LEDS_TIMER_INTERVAL;
			led = LEDS_BLUE;
		}
		
		/* Temps d'attente avant le prochain blink */
		etimer_set(&on_timer, timer);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&on_timer));
		
		/* led blink */
		leds_on(led);
		etimer_set(&off_timer, OFF_LEDS_TIMER_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&off_timer));
		leds_off(led);
	}
	
	PROCESS_END();
}

/* Process qui gère le stage search */
PROCESS_THREAD(search_process, ev, data)
{
	static struct etimer search_timer;
	
	/* définit l'adresse du broadcast */
	uip_ip6addr(&broadcast_ipaddr, 0xff02,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0001);
	/* définit mon adresse */
	uip_ip6addr(&my_unicast_addr, 0xfe80,0x0000,0x0000,0x0000,0x0212,0x4b00,0x11f4,0xeb4a);
	
	PROCESS_BEGIN();
	
	/* Initialisation du socket UDP */
	simple_udp_register(&udp_socket, UDP_PORT, NULL, UDP_PORT, NULL);
	
	/* Enregistrement d'un socket UDP et une callback excuter lors de la reception d'un id */
	simple_udp_register(&udp_socket, UDP_PORT, NULL, UDP_PORT, message_received_callback);
	
	while(stage == search)
	{
		/* Envoyer le message l'IP du mote root */
		printf("==> send my id %d\n", my_id);
		simple_udp_sendto(&udp_socket, &my_id, sizeof(my_id), &broadcast_ipaddr);
		
		/* Attente avant de renvoyer l'ID */
		etimer_set(&search_timer, SEND_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&search_timer));
	}
	
	PROCESS_END();
}

/* Process qui gère le stage syn en mode master */
PROCESS_THREAD(master_syn_process, ev, data)
{
	static struct etimer master_syn__timer;
	static char str[50];
	
	PROCESS_BEGIN();
	 
	if(stage == syn)
	{
		
		/* Attente de 5 secondes avant d'envoyer une réponse */
		etimer_set(&master_syn__timer, CLOCK_SECOND * 5);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&master_syn__timer));
		 
		/* Envois de la réponse */
		snprintf(str, sizeof(str), "Hello from %d", my_id);
		printf("==> send message : ");
		printf(str);
		printf("\n");
		simple_udp_sendto(&udp_socket, str, strlen(str), &unicast_ipaddr);
		
		/* Attente de 3 secondes avant de passer en stage cnt */
		etimer_set(&master_syn__timer, CLOCK_SECOND * 3);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&master_syn__timer));
		 
		stage = cnt;
		printf("stage = cnt\n");
		process_start(&master_cnt_process, NULL);
	}
	
	PROCESS_END();
}

/* Process qui gère le stage syn en mode slave */
PROCESS_THREAD(slave_syn_process, ev, data)
{
	static struct etimer slave_timer;
	static char str[50];
	
	PROCESS_BEGIN();
	
	while(stage == syn){
		printf("==> send message : Connection request from %d\n", my_id);
		snprintf(str, sizeof(str), "Connection request from %d", my_id);
		
		/* Envoie du message */
		simple_udp_sendto(&udp_socket, str, strlen(str), &unicast_ipaddr);
		
		/* Attente de 2 secondes avant de renvoyer un message */
		etimer_set(&slave_timer, CLOCK_SECOND * 2);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&slave_timer));
	}
	
	PROCESS_END();
}

/* Process qui gère le stage connection en mode master */
PROCESS_THREAD(master_cnt_process, ev, data)
{
	static struct etimer cnt_master_timer;
	static struct etimer off_timer;
	
	PROCESS_BEGIN();
	
	/* Changement de channel */
	NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 11+my_id);
	
	while(stage == cnt)
	{
		
		printf("==> send message : %d ping\n", ping);
		
		/* Envoie d'un ping */
		simple_udp_sendto(&udp_socket, &ping, sizeof(ping), &unicast_ipaddr);
		
		leds_on(LEDS_GREEN);
		etimer_set(&off_timer, OFF_LEDS_TIMER_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&off_timer));
		leds_off(LEDS_GREEN);
		
		/* Incrémentation de la séquence */
		ping++;
		
		/* Attente de renvoyer un ping */
		etimer_set(&cnt_master_timer, CLOCK_SECOND * 2);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&cnt_master_timer));
	}
	
	PROCESS_END();
}

PROCESS_THREAD(slave_cnt_process, ev, data)
{
	PROCESS_BEGIN();
	
	/* Changement de channel */
	NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 11+my_id);
	
	PROCESS_END();
}
