/*
 * E1000 Network Driver with UDP to ICMP Conversion
 * 
 * This driver intercepts UDP packets destined to 100.100.100.100 and converts 
 * them to ICMP echo requests. It also converts ICMP echo replies with magic 
 * number back to UDP packets.
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <linux/in.h>
#include <linux/checksum.h>
#include <net/ip.h>
#include <net/checksum.h>

#define MAGIC_NUMBER 0xDECAF
#define TARGET_IP 0x646464C8 // 100.100.100.100 in big-endian
#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY 0

/* Structure to store UDP port information in ICMP payload */
struct udp_info {
    __be16 src_port;
    __be16 dst_port;
    __be32 magic;
};

/* Calculate checksum for IP header */
static __sum16 ip_checksum(struct iphdr *iph)
{
    int length = iph->ihl * 4;
    __sum16 old_check = iph->check;
    __wsum sum;
    
    iph->check = 0;
    sum = csum_partial(iph, length, 0);
    iph->check = old_check;
    
    return csum_fold(sum);
}

/* Calculate checksum for ICMP packet */
static __sum16 icmp_checksum(struct icmphdr *icmph, int data_len)
{
    __sum16 old_check = icmph->checksum;
    __wsum sum;
    
    icmph->checksum = 0;
    sum = csum_partial(icmph, sizeof(struct icmphdr) + data_len, 0);
    icmph->checksum = old_check;
    
    return csum_fold(sum);
}

/* Calculate checksum for UDP packet */
static __sum16 udp_checksum(struct sk_buff *skb, struct iphdr *iph, struct udphdr *udph)
{
    struct pseudo_header {
        __be32 saddr;
        __be32 daddr;
        __u8 zero;
        __u8 protocol;
        __be16 total_len;
    } pseudo_hdr;
    
    __sum16 old_check = udph->check;
    __wsum sum;
    int udp_len = ntohs(udph->len);
    
    /* Create pseudo header */
    pseudo_hdr.saddr = iph->saddr;
    pseudo_hdr.daddr = iph->daddr;
    pseudo_hdr.zero = 0;
    pseudo_hdr.protocol = IPPROTO_UDP;
    pseudo_hdr.total_len = udph->len;
    
    udph->check = 0;
    
    /* Calculate checksum over pseudo header + UDP header + data */
    sum = csum_partial(&pseudo_hdr, sizeof(pseudo_hdr), 0);
    sum = csum_partial(udph, udp_len, sum);
    
    udph->check = old_check;
    
    return csum_fold(sum);
}

/* Convert UDP packet to ICMP echo request */
static int udp_to_icmp(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct udphdr *udph;
    struct icmphdr *icmph;
    struct udp_info *udp_info_ptr;
    __be32 target_ip;
    __be16 src_port, dst_port;
    int payload_len;
    
    /* Get headers */
    iph = ip_hdr(skb);
    udph = udp_hdr(skb);
    
    /* Verify this is UDP packet to 100.100.100.100 */
    if (iph->protocol != IPPROTO_UDP || iph->daddr != htonl(TARGET_IP)) {
        return 0; /* Not our target packet */
    }
    
    /* Extract payload information */
    udp_info_ptr = (struct udp_info *)((char *)udph + sizeof(struct udphdr));
    
    /* Verify magic number is present */
    if (ntohl(udp_info_ptr->magic) != MAGIC_NUMBER) {
        return 0; /* Not our packet */
    }
    
    /* Extract target IP from first 4 bytes of payload */
    target_ip = *(__be32 *)udp_info_ptr;
    src_port = udph->source;
    dst_port = udph->dest;
    
    /* Convert UDP header to ICMP header */
    icmph = (struct icmphdr *)udph;
    icmph->type = ICMP_ECHO_REQUEST;
    icmph->code = 0;
    icmph->checksum = 0;
    icmph->un.echo.id = src_port; /* Store source port in ID field */
    icmph->un.echo.sequence = htons(1);
    
    /* Update payload to preserve magic number and other info */
    udp_info_ptr = (struct udp_info *)((char *)icmph + sizeof(struct icmphdr));
    udp_info_ptr->src_port = src_port;
    udp_info_ptr->dst_port = dst_port;
    udp_info_ptr->magic = htonl(MAGIC_NUMBER);
    
    /* Update IP header */
    iph->daddr = target_ip;
    iph->protocol = IPPROTO_ICMP;
    
    /* Recalculate payload length for ICMP */
    payload_len = ntohs(iph->tot_len) - (iph->ihl * 4) - sizeof(struct udphdr);
    iph->tot_len = htons((iph->ihl * 4) + sizeof(struct icmphdr) + payload_len);
    
    /* Recalculate checksums */
    icmph->checksum = icmp_checksum(icmph, payload_len);
    iph->check = ip_checksum(iph);
    
    /* Disable hardware checksum offloading */
    skb->ip_summed = CHECKSUM_COMPLETE;
    
    printk(KERN_INFO "E1000: Converted UDP to ICMP echo request for %pI4\n", &target_ip);
    
    return 1; /* Packet was converted */
}

/* Convert ICMP echo reply back to UDP packet */
static int icmp_to_udp(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct icmphdr *icmph;
    struct udphdr *udph;
    struct udp_info *udp_info_ptr;
    __be16 src_port, dst_port;
    int payload_len;
    
    /* Get headers */
    iph = ip_hdr(skb);
    icmph = (struct icmphdr *)((char *)iph + (iph->ihl * 4));
    
    /* Verify this is ICMP echo reply */
    if (iph->protocol != IPPROTO_ICMP || icmph->type != ICMP_ECHO_REPLY) {
        return 0; /* Not ICMP echo reply */
    }
    
    /* Check for magic number in payload */
    udp_info_ptr = (struct udp_info *)((char *)icmph + sizeof(struct icmphdr));
    if (ntohl(udp_info_ptr->magic) != MAGIC_NUMBER) {
        return 0; /* Not our packet */
    }
    
    /* Extract port information */
    src_port = icmph->un.echo.id; /* Source port stored in ID field */
    dst_port = udp_info_ptr->dst_port;
    
    /* Calculate payload length */
    payload_len = ntohs(iph->tot_len) - (iph->ihl * 4) - sizeof(struct icmphdr);
    
    /* Convert ICMP header to UDP header */
    udph = (struct udphdr *)icmph;
    udph->source = htons(53); /* DNS port as source (arbitrary) */
    udph->dest = src_port; /* Original source becomes destination */
    udph->len = htons(sizeof(struct udphdr) + payload_len);
    udph->check = 0;
    
    /* Update IP header */
    iph->daddr = htonl(TARGET_IP); /* Change destination back to 100.100.100.100 */
    iph->protocol = IPPROTO_UDP;
    iph->tot_len = htons((iph->ihl * 4) + sizeof(struct udphdr) + payload_len);
    
    /* Recalculate checksums */
    udph->check = udp_checksum(skb, iph, udph);
    iph->check = ip_checksum(iph);
    
    /* Disable hardware checksum offloading */
    skb->ip_summed = CHECKSUM_COMPLETE;
    
    printk(KERN_INFO "E1000: Converted ICMP echo reply back to UDP\n");
    
    return 1; /* Packet was converted */
}

/* Hook for outgoing packets (send path) */
static netdev_tx_t e1000_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
    /* Try to convert UDP to ICMP */
    udp_to_icmp(skb);
    
    /* Continue with normal transmission */
    /* This would normally call the original e1000_xmit_frame */
    /* For this assignment, we assume the packet is transmitted */
    
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}

/* Hook for incoming packets (receive path) */
static int e1000_receive_skb(struct sk_buff *skb)
{
    /* Try to convert ICMP to UDP */
    icmp_to_udp(skb);
    
    /* Continue with normal reception */
    /* This would normally call netif_rx or similar */
    netif_rx(skb);
    
    return 0;
}

/* Module initialization */
static int __init e1000_init_module(void)
{
    printk(KERN_INFO "E1000: UDP to ICMP conversion driver loaded\n");
    return 0;
}

/* Module cleanup */
static void __exit e1000_cleanup_module(void)
{
    printk(KERN_INFO "E1000: UDP to ICMP conversion driver unloaded\n");
}

module_init(e1000_init_module);
module_exit(e1000_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("E1000 driver with UDP to ICMP conversion");
MODULE_VERSION("1.0");
