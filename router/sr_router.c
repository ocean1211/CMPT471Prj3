/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/* TODO: Add constant definitions here... */

/* TODO: Add helper functions here... */

/* Sherlock */
/* Send arp sends arps out needs type request/reply given and destination IP and address */
void send_arp(
  struct sr_instance * sr,
  enum sr_arp_opcode arp_opcode,
  char * interface,
  unsigned char * target_hardware_addr,
  uint32_t target_ip_addr
){
  sr_arp_hdr_t * arp_packet = malloc(sizeof(sr_arp_hdr_t));
  struct sr_if * receive_interface = sr_get_interface(sr, interface);
  unsigned int len_frame = sizeof(sr_arp_hdr_t) + sizeof(sr_ethernet_hdr_t);
  uint8_t * frame = malloc(len_frame);
  unsigned char * address = NULL;
  struct sr_if * interface_list = sr->if_list;

  arp_packet->ar_hrd = htons(arp_hrd_ethernet);
  arp_packet->ar_pro = htons(ethertype_ip);
  arp_packet->ar_hln = ETHER_ADDR_LEN;
  arp_packet->ar_pln = 4;
  arp_packet->ar_op = htons(arp_opcode);
  memcpy(arp_packet->ar_sha, receive_interface->addr, ETHER_ADDR_LEN);
  arp_packet->ar_sip = receive_interface->ip;
  memcpy(arp_packet->ar_tha, target_hardware_addr, ETHER_ADDR_LEN);
  arp_packet->ar_tip = target_ip_addr;

  memcpy(frame + sizeof(sr_ethernet_hdr_t), arp_packet, sizeof(sr_arp_hdr_t));
  while(interface_list != NULL){
    if(strcmp(interface_list->name, interface) == 0){
      address = interface_list->addr;
    }
    interface_list = interface_list->next;
  }

  memcpy(((sr_ethernet_hdr_t *)frame)->ether_dhost, target_hardware_addr, ETHER_ADDR_LEN);
  memcpy(((sr_ethernet_hdr_t *)frame)->ether_shost, address, ETHER_ADDR_LEN);
  ((sr_ethernet_hdr_t *)frame)->ether_type = htons(ethertype_arp);
  sr_send_packet(sr, frame, len_frame, interface);
  free(frame);
  free(arp_packet);
}

/* Try and send packet otherwise queue it */
void try_sending(
  struct sr_instance * sr,
  uint32_t d_ip, /* destination ip */
  uint8_t * frame, /* Ethernet frame */
  unsigned int len_frame,
  char * interface
){

  struct sr_arpentry * arp_lookup_result = sr_arpcache_lookup(&(sr->cache), d_ip);

  if(arp_lookup_result != NULL){
    printf("%s\n", "ARP: Found entry");
    unsigned char * dest_addr;
    dest_addr = arp_lookup_result->mac;
    memcpy(((sr_ethernet_hdr_t *)(frame))->ether_dhost, dest_addr, ETHER_ADDR_LEN);
    /*printf("%s\n", "------------------------------------Sent Packet------------------------------------");
    print_hdrs(frame, len_frame);
    printf("%s\n", "----------------------------------Sent Packet End----------------------------------");*/
    sr_send_packet(sr, frame, len_frame, interface);

    free(arp_lookup_result);
  }
  /* couldn't find entry, so we're going to send an arp_request for it */
  else{
    printf("%s\n", "ARP: Cannot find entry");
    struct sr_arpreq * request = sr_arpcache_queuereq(&(sr->cache), d_ip, frame, len_frame, interface);
    handle_arpreq(sr, request);
  }
}

/* arp_handler by Sherlock */

void arp_handler(
  struct sr_instance * sr,
  struct sr_arp_hdr * packet/* lent */,
  unsigned int len,
  char* interface/* lent */
){

  struct sr_if * receive_interface = sr_get_interface(sr, interface);
  struct sr_arpreq * arp_request;

  if (receive_interface->ip == packet->ar_tip){
    printf("ARP: This is my packet\n");

    /* First check if arp entry already in my table, if it isn't add it into the cache */

    if (sr_arpcache_lookup(&(sr->cache), packet->ar_sip) == NULL){
      arp_request = sr_arpcache_insert(&(sr->cache), packet->ar_sha, packet->ar_sip);

      if (ntohs(packet->ar_op) == arp_op_request){
        printf("ARP: Sending ARP\n");
        send_arp(sr, arp_op_reply, interface, packet->ar_sha, packet->ar_sip);
      }else{
        if(arp_request !=  NULL){
          struct sr_packet *request_packet = arp_request->packets;
          while(request_packet){
            try_sending(sr, arp_request->ip, request_packet->buf, request_packet->len, request_packet->iface);
            request_packet = request_packet->next;
          }
          printf("ARP: Destroying ARP request\n");
          sr_arpreq_destroy(&(sr->cache), arp_request);
        }
      }
    }
    /* Now check if arp is a request and if it is, send a reply */

    /* Else -- this is a reply and I don't need to do anything */
  }
}

/* end arp_handler */









/* See pseudo-code in sr_arpcache.h */
void handle_arpreq(struct sr_instance* sr, struct sr_arpreq *req){
  /* Sherlock */
  /* pseudo-code from sr_arpcache.h is as follows:

  if difftime (now, req->sent) > 1.0
    if req->times_sent >= 5:
      send icmp host unreachable to source addr of all pkts waiting on this request
      arpreq_destroy
    else:
      send arp request
      req->sent = now
      req->times_sent++
  */

  time_t now;
  time(&now);

  /*or time_t now = time(NULL);*/

  struct sr_arpcache * sr_cache = &(sr->cache);

  if(difftime(now, req->sent) > 1.0){

      if(req->times_sent >= 5){
        /*send icmp host unreachable to source addr of all pkts waiting on this request*/
        /* Cheng to add icmp function here */

        printf("!!!!!!!!!!!Core dump\n");

        struct sr_packet *req_packets = req->packets;
        while(req_packets != NULL){
          /*Sending icmp type 3 code 1 Time exceeded*/
          /*send_icmp(sr,req_packets->buf,sizeof(req_packets->buf),3,1);*/
          req_packets = req_packets->next;
        }

        printf("!!!!!!!!!!!Core dump end\n");

        sr_arpreq_destroy(sr_cache, req);

      }else{
        /*send arp request*/
        char * interface = NULL;
        struct sr_rt * routing_table = sr->routing_table;
        while(routing_table != NULL){
          if(routing_table->gw.s_addr == req->ip){
            interface = routing_table->interface;
          }
          routing_table = routing_table->next;
        }
        if(interface != NULL){
          uint8_t broadcast_to_everyone[ETHER_ADDR_LEN];
          int i;
          for(i = 0; i < ETHER_ADDR_LEN; i++){
            broadcast_to_everyone[i] = 255;
          }
          printf("*****here is sending arp \n");

          send_arp(sr, arp_op_request, interface, (unsigned char *)broadcast_to_everyone, req->ip);
        }
        req->sent = now;
        req->times_sent++;
      }
  }

}

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

    /* TODO: (opt) Add initialization code here */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT free either (signified by "lent" comment).
 * Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */){

  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d\n",len);

  /* TODO: Add forwarding logic here */
  /*Cheng*/

  /*Printing header*/
  /*print_hdrs(packet, len);*/

  /*Reading packet type*/

  struct sr_ip_hdr *ip_hdr = (struct sr_ip_hdr*)(packet + sizeof(struct sr_ethernet_hdr));;


  if(ethertype(packet) == ethertype_ip){

    /*Case 1 if packet is ip packet / icmp packet*/
    printf("%s\n", "Case 1 IP Packet");

    bool for_me;
    for_me = false;

    /*Trying to match the interface ip with packest's dst ip*/
    struct sr_if * current_if;

    /*Start with the first interface*/
    current_if = sr->if_list;
    while(current_if != NULL){

      /*Check if the interface ip matches with packet*/
      if(current_if->ip == ip_hdr->ip_dst){
        for_me = true;
      }
      current_if = current_if->next;
    }

    /*Decrease ttl by 1*/
    ip_hdr->ip_ttl--;
    printf("TTL:=%i\n",ip_hdr->ip_ttl);

    /*Compute new checksum*/
    ip_hdr->ip_sum = 0;
    /*ip_hdr->ip_sum = cksum(ip_hdr, ip_hdr->ip_hl * 4);*/
    ip_hdr->ip_sum = cksum(ip_hdr, sizeof(sr_ip_hdr_t));

    /*Check if ttl is equal to 0*/
    if(ip_hdr->ip_ttl != 0){

      /*Check if the packet is destined for me*/
      if((for_me)&&(ip_hdr->ip_p == ip_protocol_icmp)){

        printf("%s\n", "This ICMPT packet is destined for this router");

        /*Sending icmp type 0 code 0 Echo reply*/
        send_icmp(sr,packet,len,0,0);

      }else{
        /*Packet is not for me*/
        printf("%s\n", "This packet will be forwarded");

        /*Search the matching routing table with packet's source ip*/
        struct sr_rt * matching_rt = search_rt(sr, ip_hdr->ip_dst);

        /*Packet is ip packet and has matching address from routing table*/
        if(matching_rt != NULL){
          printf("%s\n", "Found address");

          /*Search the matching interface with the routing table*/
          struct sr_if * matching_if = sr_get_interface(sr, matching_rt->interface);

          /*Allocate memory for forwarding ethernet packet*/
          struct sr_ethernet_hdr * forward_eth = malloc(len);

          /*Copy all the content from packet to forward packet*/
          memcpy(forward_eth, packet, len);

          /*Copy the new source into forward packet*/
          memcpy(forward_eth->ether_shost, matching_if->addr, ETHER_ADDR_LEN);

          /*Trying sending ip packet*/
          try_sending(sr, matching_rt->gw.s_addr, (uint8_t *)forward_eth, len, matching_if->name);

          /*Free memory*/
          free(forward_eth);

        }else{
          printf("%s\n", "No such address");
          /*Sending icmp type 3 code 0 Destination net unreachable*/
          send_icmp(sr,packet,len,3,0);
        }
      }



    }else{
      /*Should be sending icmp type 11*/
      printf("%s\n", "*****************************************************TTL expired");
      /*Sending icmp type 11 code 0 Time exceeded*/
      send_icmp(sr,packet,len,11,0);
    }


  }else{
    /*Case 2 if packet is arp packet*/
    printf("%s\n", "Case 2 ARP Packet");
    arp_handler(sr, (sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)), (len - sizeof(sr_ethernet_hdr_t)), interface);
  }

}/* -- sr_handlepacket -- */

/*search_rt by Cheng */
struct sr_rt * search_rt(struct sr_instance *sr, uint32_t ip_dest){
  /*Try to match ip dest with the routing table in sr*/
  struct sr_rt* addr = sr->routing_table;
  struct sr_rt * target = NULL;

  while(addr != NULL){
    if(addr->dest.s_addr == (addr->mask.s_addr & ip_dest)){
      target = addr;
    }
    addr = addr->next;
  }

  return target;
}
/*search_rt by Cheng */

/*send_icmp by Cheng */
void send_icmp(struct sr_instance* sr,
        uint8_t * packet,
        unsigned int len,
        int type,
        int code){

  /*Make the return packet*/
  struct sr_ip_hdr *ip_hdr = (struct sr_ip_hdr*)(packet + sizeof(struct sr_ethernet_hdr));;
  uint8_t * reply_ip_payload = ((uint8_t *)ip_hdr) + (ip_hdr->ip_hl * 4);
  struct sr_icmp_hdr * reply_icmp_hdr = (sr_icmp_hdr_t *)(reply_ip_payload);
  unsigned int reply_icmp_payload = ntohs(ip_hdr->ip_len) - (ip_hdr->ip_hl * 4) - sizeof(sr_icmp_hdr_t);

  /*Search the matching routing table with packet's source ip*/
  struct sr_rt * matching_rt = search_rt(sr, ip_hdr->ip_src);

  /*Search the matching interface with the routing table*/
  struct sr_if * matching_if = sr_get_interface(sr, matching_rt->interface);

  /*Allocate memory for new icmp header*/
  int reply_icmp_len = sizeof(sr_icmp_hdr_t) + reply_icmp_payload;
  struct sr_icmp_hdr *reply_icmp = calloc(reply_icmp_len, 1);

  /*Copy the icmp payload to icmp header*/
  memcpy(reply_icmp + 1, (uint8_t *)(reply_icmp_hdr + 1), reply_icmp_payload);

  /* The new ICMP header */
  reply_icmp->icmp_type = type;
  reply_icmp->icmp_code = code;
  reply_icmp->icmp_sum = 0;
  reply_icmp->icmp_sum = cksum(reply_icmp, reply_icmp_len);

  /*Allocate memory for new ethernet packet*/
  struct sr_ethernet_hdr *reply_eth = calloc(sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + reply_icmp_len, sizeof(uint8_t));

  /* The new ethernet header */
  reply_eth->ether_type = htons(ethertype_ip);

  /*Copy the ip header to ethernet packet*/
  memcpy(reply_eth->ether_shost, matching_if->addr, ETHER_ADDR_LEN);

  /*Allocate memory for new ip header*/
  struct sr_ip_hdr *reply_ip = (sr_ip_hdr_t *)(reply_eth + 1);

  /*Copy the icmp header to ip*/
  memcpy(reply_ip + 1, (uint8_t *)reply_icmp, reply_icmp_len);

  /* The new IP header */
  reply_ip->ip_v = 4;
  reply_ip->ip_off = htons(IP_DF);
  reply_ip->ip_hl = 5;
  reply_ip->ip_p = ip_protocol_icmp;
  reply_ip->ip_src = ip_hdr->ip_dst;
  reply_ip->ip_dst = ip_hdr->ip_src;
  reply_ip->ip_len = ip_hdr->ip_len;
  reply_ip->ip_ttl = 64;
  reply_ip->ip_sum = 0;
  reply_ip->ip_sum = cksum(reply_ip, sizeof(sr_ip_hdr_t));

  /*Try sending the new packet*/
  try_sending(sr, matching_rt->gw.s_addr, (uint8_t *)reply_eth, sizeof(sr_ethernet_hdr_t)+sizeof(sr_ip_hdr_t)+len, matching_if->name);

  /*Freeing memory*/
  free(reply_eth);
  free(reply_icmp);

}
/*send_icmp by Cheng */
