/*
 * ICMP Ping Application (cping)
 * 
 * Creates and sends ICMP echo requests directly for debugging purposes.
 * Requires root privileges to create raw sockets.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

#define PACKET_SIZE 64
#define TIMEOUT_SEC 5

/* Global variables for statistics */
static int packets_sent = 0;
static int packets_received = 0;
static volatile int keep_running = 1;

/* Signal handler for SIGINT */
void signal_handler(int sig)
{
    keep_running = 0;
}

/* Calculate checksum */
unsigned short checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    /* Make 16 bit words out of every two adjacent 8 bit words and 
       calculate the sum of all 16 bit words */
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    /* Add left-over byte, if any */
    if (len == 1) {
        sum += *(unsigned char*)buf << 8;
    }

    /* Fold 32-bit sum to 16 bits: add carrier to result */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    result = ~sum;
    return result;
}

/* Get current time in microseconds */
long long get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* Create ICMP packet */
int create_icmp_packet(char *packet, int seq, int id)
{
    struct icmphdr *icmp = (struct icmphdr *)packet;
    char *data = packet + sizeof(struct icmphdr);
    int data_len = PACKET_SIZE - sizeof(struct icmphdr);
    
    /* Fill ICMP header */
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->un.echo.id = htons(id);
    icmp->un.echo.sequence = htons(seq);
    
    /* Fill data with timestamp and pattern */
    long long timestamp = get_time_us();
    memcpy(data, &timestamp, sizeof(timestamp));
    
    /* Fill rest with pattern */
    for (int i = sizeof(timestamp); i < data_len; i++) {
        data[i] = 0x40 + (i % 26); /* A-Z pattern */
    }
    
    /* Calculate checksum */
    icmp->checksum = checksum(packet, PACKET_SIZE);
    
    return PACKET_SIZE;
}

/* Parse ICMP reply */
int parse_icmp_reply(char *packet, int len, int expected_id)
{
    struct iphdr *ip = (struct iphdr *)packet;
    struct icmphdr *icmp;
    long long *timestamp;
    long long now, rtt;
    
    /* Skip IP header */
    icmp = (struct icmphdr *)(packet + (ip->ihl * 4));
    
    /* Check if it's our packet */
    if (icmp->type == ICMP_ECHOREPLY && 
        ntohs(icmp->un.echo.id) == expected_id) {
        
        /* Calculate RTT */
        timestamp = (long long *)(packet + (ip->ihl * 4) + sizeof(struct icmphdr));
        now = get_time_us();
        rtt = now - *timestamp;
        
        printf("64 bytes from %s: icmp_seq=%d time=%.2f ms\n",
               inet_ntoa(*(struct in_addr *)&ip->saddr),
               ntohs(icmp->un.echo.sequence),
               rtt / 1000.0);
        
        packets_received++;
        return 1;
    }
    
    return 0;
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in dest_addr;
    char packet[PACKET_SIZE];
    char reply_buffer[1024];
    int packet_len;
    struct timeval timeout;
    int seq = 1;
    int pid = getpid();
    ssize_t bytes_sent, bytes_received;
    
    /* Check command line arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <destination_ip>\n", argv[0]);
        fprintf(stderr, "Example: %s 8.8.8.8\n", argv[0]);
        return 1;
    }
    
    /* Check for root privileges */
    if (getuid() != 0) {
        fprintf(stderr, "Error: This program requires root privileges\n");
        fprintf(stderr, "Please run with sudo: sudo %s %s\n", argv[0], argv[1]);
        return 1;
    }
    
    /* Create raw socket */
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket creation failed");
        return 1;
    }
    
    /* Set socket timeout */
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return 1;
    }
    
    /* Set up destination address */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, argv[1], &dest_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", argv[1]);
        close(sockfd);
        return 1;
    }
    
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    
    printf("PING %s (%s): %d data bytes\n", argv[1], argv[1], 
           PACKET_SIZE - (int)sizeof(struct icmphdr));
    
    /* Main ping loop */
    while (keep_running) {
        /* Create ICMP packet */
        packet_len = create_icmp_packet(packet, seq, pid);
        
        /* Send packet */
        bytes_sent = sendto(sockfd, packet, packet_len, 0,
                           (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        
        if (bytes_sent < 0) {
            perror("sendto failed");
            break;
        }
        
        packets_sent++;
        
        /* Wait for reply */
        bytes_received = recv(sockfd, reply_buffer, sizeof(reply_buffer), 0);
        
        if (bytes_received > 0) {
            parse_icmp_reply(reply_buffer, bytes_received, pid);
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Request timeout for icmp_seq %d\n", seq);
        } else {
            perror("recv failed");
        }
        
        seq++;
        sleep(1);
    }
    
    /* Print statistics */
    printf("\n--- %s ping statistics ---\n", argv[1]);
    printf("%d packets transmitted, %d received, %.0f%% packet loss\n",
           packets_sent, packets_received,
           packets_sent > 0 ? (100.0 * (packets_sent - packets_received) / packets_sent) : 0);
    
    close(sockfd);
    return 0;
}