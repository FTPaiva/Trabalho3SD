#include <iostream>
#include <fstream>
#include <queue>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <map>
#include <iomanip>
#include <chrono>
#include <ctime>

using namespace std;


#define BUFFER_SIZE 10
#define COORDINATOR_PORT 8888
#define COORDINATOR_IP "127.0.0.1"

std::map<std::string, struct sockaddr_in> clientAddresses;

struct RequestMessage {
    char identifier;
    int pid;
    char zeros[BUFFER_SIZE - 3]; // Zeros para preencher a mensagem
};

void processMessage(const RequestMessage& requestMessage, int socket, struct sockaddr_in senderAddress, std::queue<int>& requestQueue);
void sendGrant(int socket, int processPid);

// Função para escrever no arquivo de log
void writeLog(const string& logMessage) {
    ofstream logfile("log.txt", ios::app);
    if (logfile.is_open()) {
        auto now = chrono::system_clock::now();
        auto now_c = chrono::system_clock::to_time_t(now);
        struct tm* timeinfo = localtime(&now_c);
        logfile << "[" << put_time(timeinfo, "%Y-%m-%d %H:%M:%S") << "] " << logMessage << endl;
        logfile.close();
    }
}

void printQueue(queue<int> q)
{
	//printing content of queue 
    string result = "queue ="; 
	while (!q.empty()){
		result.append(" " + to_string(q.front()));
		q.pop();
	}
	writeLog(result);
}

int main() {
    // Criação do socket UDP
    int coordinatorSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (coordinatorSocket < 0) {
        std::cerr << "Erro ao criar o socket." << std::endl;
        return 1;
    }

    // Configuração do endereço do coordenador
    struct sockaddr_in coordinatorAddress;
    std::memset(&coordinatorAddress, 0, sizeof(coordinatorAddress));
    coordinatorAddress.sin_family = AF_INET;
    coordinatorAddress.sin_addr.s_addr = inet_addr(COORDINATOR_IP);
    coordinatorAddress.sin_port = htons(COORDINATOR_PORT);

    // Vincula o endereço ao socket
    if (bind(coordinatorSocket, (struct sockaddr*)&coordinatorAddress, sizeof(coordinatorAddress)) < 0) {
        std::cerr << "Erro ao vincular o endereço ao socket." << std::endl;
        return 1;
    }

    std::queue<int> requestQueue;

    while (true) {

        char buffer[BUFFER_SIZE];
        struct sockaddr_in senderAddress;
        socklen_t senderAddressLength = sizeof(senderAddress);

        // Recebe mensagem
        ssize_t bytesRead = recvfrom(coordinatorSocket, buffer, BUFFER_SIZE, 0,
                                     (struct sockaddr*)&senderAddress, &senderAddressLength);

        if (bytesRead < 0) {
            std::cerr << "Erro ao receber mensagem." << std::endl;
            continue;
        }

        if (bytesRead > 0) {
            std::cerr << "Mensagem recebida com sucesso." << std::endl;
        }

        // Processa a mensagem
        if (bytesRead > 0) {
            RequestMessage request_message;
            std::memcpy(&request_message, buffer, sizeof(RequestMessage));
            processMessage(request_message, coordinatorSocket, senderAddress, requestQueue);
        }
    }

    // Fechamento do socket
    close(coordinatorSocket);

    return 0;
}

void processMessage(const RequestMessage& requestMessage, int socket, struct sockaddr_in senderAddress, std::queue<int>& requestQueue) {
    std::cout << "Recebida uma mensagem: identifier=" << requestMessage.identifier
              << ", pid=" << requestMessage.pid << std::endl;

    if (requestMessage.identifier == '1') {
        writeLog("[R] Request - " + to_string(requestMessage.pid));
        if (requestQueue.empty()) {
            requestQueue.push(requestMessage.pid);
            clientAddresses[std::to_string(requestMessage.pid)] = senderAddress;
            sendGrant(socket, requestMessage.pid);
        } else { 
            requestQueue.push(requestMessage.pid);
            clientAddresses[std::to_string(requestMessage.pid)] = senderAddress;
        }
    } else if (requestMessage.identifier == '3') {
        writeLog("[R] Release - " + to_string(requestMessage.pid));
        if (!requestQueue.empty()) {
            requestQueue.pop(); // Retira PID que enviou release
            if (!requestQueue.empty()) { // Envia Grant para proximo da fila
                int nextProcessPid = requestQueue.front();
                sendGrant(socket, nextProcessPid);
            }
        }
        
    } else {
        std::cerr << "Mensagem inválida." << std::endl;
    }
}


void sendGrant(int socket, int processPid) {
    RequestMessage grantMessage;
    grantMessage.identifier = '2';
    grantMessage.pid = getpid();
    std::memset(grantMessage.zeros, '0', BUFFER_SIZE - 3);

    struct sockaddr_in client_address = clientAddresses[std::to_string(processPid)];
    socklen_t address_length = sizeof(client_address);

    size_t bytesSent = sendto(socket, &grantMessage, sizeof(grantMessage), 0,
                                (struct sockaddr*)&client_address, sizeof(client_address));

    if (bytesSent < 0) {
        std::cerr << "Erro ao enviar mensagem de grant." << std::endl;
        perror("sendto() failed"); // Print the error message using perror()
        std::cout << "Error: " << strerror(errno) << std::endl;

        switch (errno) {
            case EAFNOSUPPORT:
                perror("sendto() failed: Addresses in the specified address family cannot be used with this socket");
                break;
            case EWOULDBLOCK:
                perror("sendto() failed: The socket's file descriptor is marked O_NONBLOCK and the requested operation would block");
                break;
            case EBADF:
                perror("sendto() failed: The socket argument is not a valid file descriptor");
                break;
            case ECONNRESET:
                perror("sendto() failed: A connection was forcibly closed by a peer");
                break;
            case EINTR:
                perror("sendto() failed: A signal interrupted sendto() before any data was transmitted");
                break;
            case EMSGSIZE:
                perror("sendto() failed: The message is too large to be sent all at once, as the socket requires");
                break;
            case ENOTCONN:
                perror("sendto() failed: The socket is connection-mode but is not connected");
                break;
            case ENOTSOCK:
                perror("sendto() failed: The socket argument does not refer to a socket");
                break;
            case EOPNOTSUPP:
                perror("sendto() failed: The socket argument is associated with a socket that does not support one or more of the values set in flags");
                break;
            case EPIPE:
                perror("sendto() failed: The socket is shut down for writing, or the socket is connection-mode and is no longer connected. In the latter case, and if the socket is of type SOCK_STREAM, the SIGPIPE signal is generated to the calling thread");
                break;
            case EIO:
                perror("sendto() failed: An I/O error occurred while reading from or writing to the file system");
                break;
            case ELOOP:
                perror("sendto() failed: A loop exists in symbolic links encountered during resolution of the pathname in the socket address");
                break;
            case ENAMETOOLONG:
                perror("sendto() failed: A component of a pathname exceeded {NAME_MAX} characters, or an entire pathname exceeded {PATH_MAX} characters");
                break;
            case ENOENT:
                perror("sendto() failed: A component of the pathname does not name an existing file or the pathname is an empty string");
                break;
            case ENOTDIR:
                perror("sendto() failed: A component of the path prefix of the pathname in the socket address is not a directory");
                break;
            case EACCES:
                perror("sendto() failed: Search permission is denied for a component of the path prefix; or write access to the named socket is denied");
                break;
            case EDESTADDRREQ:
                perror("sendto() failed: The socket is not connection-mode and does not have its peer address set, and no destination address was specified");
                break;
            case EHOSTUNREACH:
                perror("sendto() failed: The destination host cannot be reached (probably because the host is down or a remote router cannot reach it)");
                break;
            case EINVAL:
                perror("sendto() failed: The dest_len argument is not a valid length for the address family");
                break;
            case EISCONN:
                perror("sendto() failed: A destination address was specified and the socket is already connected. This error may or may not be returned for connection mode sockets");
                break;
            case ENETDOWN:
                perror("sendto() failed: The local network interface used to reach the destination is down");
                break;
            case ENETUNREACH:
                perror("sendto() failed: No route to the network is present");
                break;
            case ENOBUFS:
                perror("sendto() failed: Insufficient resources were available in the system to perform the operation");
                break;
            case ENOMEM:
                perror("sendto() failed: Insufficient memory was available to fulfill the request");
                break;
            default:
                perror("sendto() failed with unknown error");
                break;
        }

    } else {
        std::cout << "Enviada uma mensagem de grant para o processo com PID " << processPid << "." << std::endl;
        writeLog("[S] Grant - " + to_string(processPid));


    }
}