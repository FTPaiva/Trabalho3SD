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
#define COORDINATOR_IP "127.0.0.1"
#define COORDINATOR_PORT 8888

struct RequestMessage {
    char identifier;
    int pid;
    char zeros[BUFFER_SIZE - 3];
};

void sendRequest(int socket, int r, int k, const struct sockaddr_in& coordinatorAddress);
void sendRelease(int socket, int r, int k, const struct sockaddr_in& coordinatorAddress);

void writeLog(const string& logMessage) {
    ofstream logfile("resultado.txt", ios::app);
    if (logfile.is_open()) {
        auto now = chrono::system_clock::now();
        auto now_c = chrono::system_clock::to_time_t(now);
        struct tm* timeinfo = localtime(&now_c);
        logfile << "[" << put_time(timeinfo, "%Y-%m-%d %H:%M:%S") << "] " << logMessage << endl;
        logfile.close();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Argumentos insuficientes. Uso: ./processo <r> <k>" << std::endl;
        return 1;
    }

    int r = std::atoi(argv[1]); // Número de iterações
    int k = std::atoi(argv[2]); // Tempo de espera após receber o grant (em segundos)

    // Criação do socket UDP
    int processSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (processSocket < 0) {
        std::cerr << "Erro ao criar o socket." << std::endl;
        return 1;
    }

    // Configuração do endereço do processo
    struct sockaddr_in processAddress;
    std::memset(&processAddress, 0, sizeof(processAddress));
    processAddress.sin_family = AF_INET;
    processAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Qualquer interface de rede
    processAddress.sin_port = 0; // Porta aleatória

    // Vincula o endereço ao socket
    if (bind(processSocket, (struct sockaddr*)&processAddress, sizeof(processAddress)) < 0) {
        std::cerr << "Erro ao vincular o endereço ao socket." << std::endl;
        return 1;
    }

    // Obtém o endereço e porta atribuídos ao processo
    socklen_t processAddressLength = sizeof(processAddress);
    if (getsockname(processSocket, (struct sockaddr*)&processAddress, &processAddressLength) < 0) {
        std::cerr << "Erro ao obter o endereço atribuído ao processo." << std::endl;
        return 1;
    }

    // Configuração do endereço do coordenador
    struct sockaddr_in coordinatorAddress;
    std::memset(&coordinatorAddress, 0, sizeof(coordinatorAddress));
    coordinatorAddress.sin_family = AF_INET;
    coordinatorAddress.sin_addr.s_addr = inet_addr(COORDINATOR_IP);
    coordinatorAddress.sin_port = htons(COORDINATOR_PORT);
    socklen_t coordinatorAddressLenght = sizeof(coordinatorAddress);

    // Loop para enviar as mensagens de request e receber os grants
    for (int i = 0; i < r; ++i) {

        sendRequest(processSocket, r, k, coordinatorAddress);

        char buffer[BUFFER_SIZE];
        struct sockaddr_in senderAddress;
        socklen_t senderAddressLength = sizeof(senderAddress);

        // Recebe mensagem
        ssize_t bytesRead = recvfrom(processSocket, buffer, BUFFER_SIZE, 0,
                                     (struct sockaddr*)&senderAddress, &senderAddressLength);


        if (bytesRead < 0) {
            std::cerr << "Erro ao receber mensagem." << std::endl;
            continue;
        }

        // Processa a mensagem recebida
        //if (bytesRead == sizeof(RequestMessage)) {
        if (bytesRead > 0) {
            RequestMessage received_message;
            std::memcpy(&received_message, buffer, sizeof(RequestMessage));

            if (received_message.identifier == '2') {
                std::cout << "Recebida uma mensagem de grant do coordenador. PID: " << received_message.pid << std::endl;
                writeLog(to_string(getpid()));
                sleep(k); // Espera por k segundos após receber o grant

                sendRelease(processSocket, r, k, coordinatorAddress);
            } else {
                std::cerr << "Mensagem recebida não é um grant." << std::endl;
            }
        }
    }

    // Fechamento do socket
    close(processSocket);

    return 0;
}

void sendRequest(int socket, int r, int k, const struct sockaddr_in& coordinatorAddress) {
    RequestMessage request_message;
    request_message.identifier = '1';
    request_message.pid = getpid();
    std::memset(request_message.zeros, '0', BUFFER_SIZE - 3);

    ssize_t bytesSent = sendto(socket, &request_message, sizeof(RequestMessage), 0,
                               (struct sockaddr*)&coordinatorAddress, sizeof(coordinatorAddress));

    if (bytesSent < 0) {
        std::cerr << "Erro ao enviar mensagem de request." << std::endl;
    } else {
        std::cout << "Enviada uma mensagem de request ao coordenador. PID: " << request_message.pid << std::endl;
    }
}

void sendRelease(int socket, int r, int k, const struct sockaddr_in& coordinatorAddress) {
    // Envia a mensagem de release ao coordenador
    RequestMessage release_message;
    release_message.identifier = '3';
    release_message.pid = getpid();
    std::memset(release_message.zeros, '0', BUFFER_SIZE - 3);

    ssize_t bytesSent = sendto(socket, &release_message, sizeof(RequestMessage), 0,
                                (struct sockaddr*)&coordinatorAddress, sizeof(coordinatorAddress));

    if (bytesSent < 0) {
        std::cerr << "Erro ao enviar mensagem de release." << std::endl;
    } else {
        std::cout << "Enviada uma mensagem de release ao coordenador." << std::endl;
    }
}