/*
 * UDP Ping Application
 * 
 * Sends UDP packets to 100.100.100.100 with target IP address and magic number
 * in the payload. The E1000 driver will convert these to ICMP echo requests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define MAGIC_NUMBER 0xDECAF
#define TARGET_SERVER "100.100.100.100"
#define SERVER_PORT 12345
#define CLIENT_PORT 54321
#define TIMEOUT_SEC 5

struct udp_payload {
    uint32_t target_ip;    /* Target IP address (first 4 bytes) */
    uint32_t magic;        /* Magic number 0xDECAF */
};

/* Convert IP string to network byte order */
uint32_t ip_string_to_addr(const char *ip_str)
{
    struct in_addr addr;
    if (inet_aton(ip_str, &addr) == 0) {
        fprintf(stderr, "Invalid IP address: %s\n", ip_str);
        return 0;
    }
    return addr.s_addr;
}

/* Get current time in microseconds */
long long get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    struct udp_payload payload;
    char buffer[1024];
    socklen_t addr_len = sizeof(server_addr);
    ssize_t bytes_sent, bytes_received;
    long long start_time, end_time;
    struct timeval timeout;
    
    /* Check command line arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <target_ip>\n", argv[0]);
        fprintf(stderr, "Example: %s 142.251.43.100\n", argv[0]);
        return 1;
    }
    
    /* Create UDP socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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
    
    /* Bind to client port */
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(CLIENT_PORT);
    
    if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return 1;
    }
    
    /* Set up server address (100.100.100.100) */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, TARGET_SERVER, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address: %s\n", TARGET_SERVER);
        close(sockfd);
        return 1;
    }
    
    /* Prepare payload */
    payload.target_ip = ip_string_to_addr(argv[1]);
    if (payload.target_ip == 0) {
        close(sockfd);
        return 1;
    }
    payload.magic = htonl(MAGIC_NUMBER);
    
    printf("UDP echo: %s\n", argv[1]);
    
    /* Record start time */
    start_time = get_time_us();
    
    /* Send UDP packet */
    bytes_sent = sendto(sockfd, &payload, sizeof(payload), 0,
                       (struct sockaddr *)&server_addr, sizeof(server_addr));
    
    if (bytes_sent < 0) {
        perror("sendto failed");
        close(sockfd);
        return 1;
    }
    
    if (bytes_sent != sizeof(payload)) {
        fprintf(stderr, "Partial send: %zd bytes sent, expected %zu\n", 
                bytes_sent, sizeof(payload));
        close(sockfd);
        return 1;
    }
    
    /* Wait for response */
    bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                             (struct sockaddr *)&server_addr, &addr_len);
    
    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Timeout: No response received within %d seconds\n", TIMEOUT_SEC);
        } else {
            perror("recvfrom failed");
        }
        close(sockfd);
        return 1;
    }
    
    /* Record end time */
    end_time = get_time_us();
    
    /* Verify response */
    if (bytes_received >= sizeof(payload)) {
        struct udp_payload *resp = (struct udp_payload *)buffer;
        if (ntohl(resp->magic) == MAGIC_NUMBER) {
            printf("Congrats: test passed\n");
            printf("Response received in %.2f ms\n", (end_time - start_time) / 1000.0);
        } else {
            printf("Warning: Response received but magic number mismatch\n");
            printf("Expected: 0x%X, Got: 0x%X\n", MAGIC_NUMBER, ntohl(resp->magic));
        }
    } else {
        printf("Warning: Response received but size mismatch\n");
        printf("Expected: %zu bytes, Got: %zd bytes\n", sizeof(payload), bytes_received);
    }
    
    close(sockfd);
    return 0;
}
