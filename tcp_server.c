#ifdef _WIN32
// Voor Windows: definieer minimaal Windows 7 functionaliteit
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <winsock2.h>     // Voor socket programming onder Windows
#include <ws2tcpip.h>     // Voor functies zoals getaddrinfo, inet_pton, inet_ntop
#include <stdio.h>        // Voor fprintf, stderr
#include <unistd.h>       // Voor close
#include <stdlib.h>       // Voor exit
#include <time.h>         // Voor time(), rand()
#include <string.h>       // Voor memset

// Initialiseer Winsock voor gebruik op Windows
void OSInit(void) {
    WSADATA wsaData;
    int WSAError = WSAStartup(MAKEWORD(2, 0), &wsaData);
    if (WSAError != 0) {
        fprintf(stderr, "WSAStartup errno = %d\n", WSAError);
        exit(-1);
    }
}

// Opruimen van Winsock bij beëindiging
void OSCleanup(void) {
    WSACleanup();
}

// Eigen perror-definitie voor Windows die WSAGetLastError() toont
#define perror(string) fprintf(stderr, string ": WSA errno = %d\n", WSAGetLastError())

#else
// UNIX / Linux headers
#include <sys/socket.h>   // Voor socketstructuur en functies
#include <sys/types.h>    // Voor datatypes zoals size_t
#include <netdb.h>        // Voor getaddrinfo
#include <netinet/in.h>   // Voor sockaddr_in
#include <arpa/inet.h>    // Voor htons, inet_pton, etc.
#include <errno.h>        // Voor errno
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// Dummy platform-afhankelijke functies op Linux
void OSInit(void) {}
void OSCleanup(void) {}
#endif

// Constante waarden (makkelijk aanpasbaar)
#define PORT "24042"
#define MAX_NUMBER 1000000
#define BUFFER_SIZE 100

// Voorwaartse declaraties
int initialization();
int connection(int internet_socket);
void execution(int client_internet_socket);
void cleanup(int internet_socket, int client_internet_socket);

// =====================
// MAIN FUNCTIE
// =====================
int main(int argc, char *argv[]) {
    srand(time(NULL)); // Initialiseer de pseudo-random generator éénmalig

    OSInit(); // Start OS-specifieke socketomgeving

    int internet_socket = initialization();               // Zet server socket op
    printf("Server draait! Wacht op verbinding...\nDruk op Enter om af te sluiten als je wilt debuggen.\n");
    getchar(); // voorkomt dat het venster meteen sluit bij fout

    int client_internet_socket = connection(internet_socket); // Accepteer client
    execution(client_internet_socket);                    // Speel het raadspel
    cleanup(internet_socket, client_internet_socket);     // Sluit verbindingen

    OSCleanup(); // Sluit OS-specifieke socketomgeving af
    return 0;
}

// =====================
// INITIALISATIE SERVER SOCKET
// =====================
int initialization() {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // Ondersteun IPv4 of IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // Voor een bind()

    // Haal mogelijke lokale adressen op (NULL betekent bind op alle interfaces)
    int status = getaddrinfo(NULL, PORT, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    int internet_socket = -1;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        // Maak socket aan
        internet_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (internet_socket == -1) {
            perror("socket");
            continue;
        }

        // Koppel socket aan adres
        if (bind(internet_socket, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind");
            close(internet_socket);
            internet_socket = -1;
            continue;
        }

        // Zet socket in luistermodus
        if (listen(internet_socket, 1) == -1) {
            perror("listen");
            close(internet_socket);
            internet_socket = -1;
            continue;
        }

        // Gelukt
        break;
    }

    freeaddrinfo(res); // Vrijgeven van adresinformatie

    if (internet_socket == -1) {
        fprintf(stderr, "socket: geen geldig socketadres gevonden\n");
        exit(2);
    }

    return internet_socket;
}

// =====================
// CLIENT VERBINDING ACCEPTEREN
// =====================
int connection(int internet_socket) {
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof client_addr;

    // Wacht op inkomende verbinding (blokkerend)
    int client_socket = accept(internet_socket, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket == -1) {
        perror("accept");
        close(internet_socket);
        exit(3);
    }

    return client_socket;
}

// =====================
// SPEL UITVOEREN MET CLIENT
// =====================
void execution(int client_internet_socket) {
    int guess = rand() % MAX_NUMBER + 1; // Te raden getal

    while (1) {
        // Vraag client om een gok
        const char *prompt = "Enter a number between 1 and 1000000: ";
        send(client_internet_socket, prompt, strlen(prompt), 0);

        char buffer[BUFFER_SIZE];
        int received = recv(client_internet_socket, buffer, sizeof(buffer) - 1, 0);
        if (received == -1) {
            perror("recv");
            break;
        } else if (received == 0) {
            printf("Client sloot de verbinding\n");
            break;
        }

        buffer[received] = '\0'; // Zorg voor string terminatie

        // Probeer string om te zetten naar een geldig geheel getal
        char *endptr;
        long guess_received = strtol(buffer, &endptr, 10);
        if (endptr == buffer || guess_received < 1 || guess_received > MAX_NUMBER) {
            const char *invalid = "Invalid input. Please enter a number between 1 and 1000000.\n";
            send(client_internet_socket, invalid, strlen(invalid), 0);
            continue;
        }

        printf("Ontvangen gok van client: %ld\n", guess_received);

        if (guess_received == guess) {
            const char *correct = "Correct guess! New number generated.\n";
            send(client_internet_socket, correct, strlen(correct), 0);
            printf("Correct! Nieuw getal wordt gegenereerd.\n");
            guess = rand() % MAX_NUMBER + 1;
        } else if (guess_received < guess) {
            const char *higher = "Guess higher!\n";
            send(client_internet_socket, higher, strlen(higher), 0);
        } else {
            const char *lower = "Guess lower!\n";
            send(client_internet_socket, lower, strlen(lower), 0);
        }
    }
}

// =====================
// SOCKETS NETJES AFSLUITEN
// =====================
void cleanup(int internet_socket, int client_internet_socket) {
#ifdef _WIN32
    shutdown(client_internet_socket, SD_RECEIVE); // Alleen lezen afsluiten (Windows)
#else
    shutdown(client_internet_socket, SHUT_RD);    // Alleen lezen afsluiten (Linux)
#endif

    close(client_internet_socket); // Sluit client socket
    close(internet_socket);       // Sluit server socket
}
