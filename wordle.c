#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 20

extern int total_guesses; 
extern int total_wins; 
extern int total_losses; 
extern char** words; 

volatile sig_atomic_t stop_server; 

typedef struct {
    char* guess;
    short guesses_remaining;
    int client_sock;
} client;

client** clients; 
char** valid_dictionary_words;
int num_valid_dictionary_words;
int server_sock;  

pthread_mutex_t total_guesses_lock;
pthread_mutex_t total_wins_lock;
pthread_mutex_t total_losses_lock;
pthread_mutex_t stop_server_lock;
pthread_mutex_t words_lock;
pthread_mutex_t clients_lock;
pthread_mutex_t server_sock_lock;

void initalize_mutexes(){
    pthread_mutex_init(&total_guesses_lock, NULL);
    pthread_mutex_init(&total_wins_lock, NULL);
    pthread_mutex_init(&total_losses_lock, NULL);
    pthread_mutex_init(&stop_server_lock, NULL);
    pthread_mutex_init(&words_lock, NULL);
    pthread_mutex_init(&clients_lock, NULL);
    pthread_mutex_init(&server_sock_lock, NULL);
}

void destroy_mutexes(){
    pthread_mutex_destroy(&total_guesses_lock);
    pthread_mutex_destroy(&total_wins_lock);
    pthread_mutex_destroy(&total_losses_lock);
    pthread_mutex_destroy(&stop_server_lock);
    pthread_mutex_destroy(&words_lock);
    pthread_mutex_destroy(&clients_lock);
    pthread_mutex_destroy(&server_sock_lock);
}

void load_inital_data(char* word_file, int port) {
    total_guesses = total_wins = total_losses = stop_server = 0;
    clients = (client**)calloc(MAX_CLIENTS, sizeof(client*));
    free(words);
    words = calloc(1000, sizeof(char*));
    valid_dictionary_words = (char**)calloc(num_valid_dictionary_words, sizeof(char*));
    initalize_mutexes();

    int fd = open(word_file, O_RDONLY);
    for (int i = 0; i < num_valid_dictionary_words; i++) {
        *(valid_dictionary_words + i) = calloc(6, sizeof(char));
        read(fd, *(valid_dictionary_words + i), 5);
        lseek(fd, 1, SEEK_CUR);
    }
    close(fd);

    printf("MAIN: opened %s (%d words)\n", word_file, num_valid_dictionary_words);
    printf("MAIN: Wordle server listening on port {%d}\n", port);
}

void increment(pthread_mutex_t *mutex){
    pthread_mutex_lock(mutex);
    if (mutex == &total_guesses_lock) total_guesses++;
    else if (mutex == &total_wins_lock) total_wins++;
    else if (mutex == &total_losses_lock) total_losses++;
    pthread_mutex_unlock(mutex);
}

void close_client(client* client_data){
    if (!client_data) return; 

    for (int i = 0; i < MAX_CLIENTS; i++){
        if (*(clients+i) == client_data){
            pthread_mutex_lock(&clients_lock);
            close(client_data->client_sock);
            free(client_data->guess);
            free(client_data);
            *(clients+i) = NULL;
            pthread_mutex_unlock(&clients_lock);
            break;
        }
    }
}

void close_server() {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (*(clients+i)){
            close_client(*(clients+i));
        }
    }
    free(clients);
    pthread_mutex_unlock(&clients_lock);
}

void send_feedback(client* client_data, char* guess_result, int is_valid_guess) {
    char* feedback = calloc(9, sizeof(char));
    *feedback = (is_valid_guess) ? 'Y' : 'N';
    *(short*)(feedback + 1) = htons(client_data->guesses_remaining);
    strcpy(feedback + 3, guess_result);
    send(client_data->client_sock, feedback, 9, 0);
    if (is_valid_guess) printf("THREAD %lu: sending reply: %s (%d guesses left)\n", (unsigned long) pthread_self(), guess_result, client_data->guesses_remaining);
    else printf("THREAD %lu: invalid guess; sending reply: ????? (%d guesses left)\n", (unsigned long) pthread_self(), client_data->guesses_remaining);
    free(feedback);
}

int process_guess(client* client_data, char* correct_word) {
    printf("THREAD %lu: rcvd guess: %s\n", (unsigned long) pthread_self(), client_data->guess);
    char* guess_result = calloc(6, sizeof(char));
    int is_valid_guess = 0;

    int *matched = calloc(5, sizeof(int)); 
    for (int i = 0; i < num_valid_dictionary_words; i++) {
        if (strcmp(*(valid_dictionary_words + i), client_data->guess) == 0) {
            is_valid_guess = 1;
            break;
        }
    }

    if (is_valid_guess){
        increment(&total_guesses_lock);
        client_data->guesses_remaining--;
        for (int i = 0; i < 5; i++) *(guess_result + i) = '-';
        if (strcmp(client_data->guess, correct_word) == 0) {
            for(int i = 0; i < 5; i++) *(guess_result + i) = toupper(*(correct_word + i));
            increment(&total_wins_lock);
            send_feedback(client_data, guess_result, is_valid_guess);
            free(guess_result);
            free(matched);
            return 1;
        }
        else{
            for (int i = 0; i < 5; i++) {
                if (*(client_data->guess + i) == *(correct_word + i)) {
                    *(guess_result + i) = toupper(*(client_data->guess + i));
                    *(matched + i) = 1;
                }
            }

            for (int i = 0; i < 5; i++) {
                if (*(guess_result + i) == '-' && strchr(correct_word, *(client_data->guess + i))) {
                    for (int j = 0; j < 5; j++) {
                        if (*(correct_word + j) == *(client_data->guess + i) && !(*(matched + j))) {
                            *(guess_result + i) = tolower(*(client_data->guess + i));
                            *(matched + j) = 1;
                            break;
                        }
                    }
                }
            }
        }
    } 
    else for (int i = 0; i < 5; i++) *(guess_result + i) = '?';
    if(client_data -> guesses_remaining == 0) increment(&total_losses_lock);
    send_feedback(client_data, guess_result, is_valid_guess);
    free(guess_result);
    free(matched);
    return 0;
}

void cleanup_resources(char* correct_word, char* read_buffer, char* accum_buffer) {
    if (correct_word) free(correct_word);
    if (read_buffer) free(read_buffer);
    if (accum_buffer) free(accum_buffer);
}

void read_and_buffer(client* client_data, char *read_buffer, int len, char* accum_buffer, int* accum_len, char* correct_word) {
    int bytes_read = recv(client_data->client_sock, read_buffer, len, MSG_WAITALL);
    if (bytes_read <= 0) {
        printf("THREAD %lu: client gave up; closing TCP connection...\n", (unsigned long) pthread_self());
        for(int i = 0; i < 5; i++) *(correct_word + i) = toupper(*(correct_word + i));
        increment(&total_losses_lock);
        printf(" game over; word was %s!\n", correct_word);
        cleanup_resources(correct_word, read_buffer, accum_buffer);
        close_client(client_data);
        pthread_exit(NULL);
    }
    
    strncpy(accum_buffer + *accum_len, read_buffer, bytes_read);
    *accum_len += bytes_read;
}

void* play_game(void* client_ptr) {
    client* client_data = (client*)client_ptr;
    char* correct_word = calloc(6, sizeof(char));
    int x = rand();
    strncpy(correct_word, *(valid_dictionary_words + (x % num_valid_dictionary_words)), 5);
    pthread_mutex_lock(&words_lock);
    int idx;
    for (idx = 0; idx < 1000; idx++) if (*(words + idx) == NULL) break;
    *(words + idx) = calloc(6, sizeof(char));
    strncpy(*(words + idx), correct_word, 5);
    for (char* ch = *(words + idx); *ch; ch++) *ch = toupper(*ch);
    pthread_mutex_unlock(&words_lock);

    char* read_buffer = calloc(5, sizeof(char));
    char* accum_buffer = calloc(100000, sizeof(char));
    int read_in = 0; 

    while(client_data->guesses_remaining != 0 && !stop_server) {
        printf("THREAD %lu: waiting for guess\n", (unsigned long) pthread_self());

        while (read_in < 5 && !stop_server) {
            read_and_buffer(client_data, read_buffer, 5 - read_in, accum_buffer, &read_in, correct_word);
        }

        while (read_in >= 5 && !stop_server){
            for (int i = 0; i < 6; i++) *(client_data->guess + i) = *(accum_buffer + i);
            if (process_guess(client_data, correct_word)) {
                for(int i = 0; i < 5; i++) *(correct_word + i) = toupper(*(correct_word + i));
                printf("THREAD %lu: game over; word was %s!\n", (unsigned long) pthread_self(), correct_word);
                cleanup_resources(correct_word, read_buffer, accum_buffer);
                close_client(client_data);
                return NULL;
            }
            strncpy(accum_buffer, accum_buffer + 5, read_in); 
            read_in -= 5;
        }
    }

    if (stop_server) {
        printf("THREAD %lu: server shutting down...\n", (unsigned long) pthread_self());
        cleanup_resources(correct_word, read_buffer, accum_buffer);
        close_server();
        return NULL;
    } 

    for(int i = 0; i < 5; i++) *(correct_word + i) = toupper(*(correct_word + i));
    printf("THREAD %lu: game over; word was %s!\n", (unsigned long) pthread_self(), correct_word);
    cleanup_resources(correct_word, read_buffer, accum_buffer);
    close_client(client_data);
    return NULL;
}

int setup_server(int port) {
    struct sockaddr_in server_addr;
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    for (size_t i = 0; i < sizeof(server_addr); i++) *(((char*)&server_addr) + i) = 0;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_sock, MAX_CLIENTS);  
    return server_sock;
}

void sigusr1_handler(int signo) {
    (void) signo;
    printf("MAIN: SIGUSR1 rcvd; Wordle server shutting down...\n");
    pthread_mutex_lock(&stop_server_lock);
    stop_server = 1;
    pthread_mutex_unlock(&stop_server_lock);
    pthread_mutex_lock(&server_sock_lock);
    close(server_sock);
    pthread_mutex_unlock(&server_sock_lock);
}

void handle_signals(){
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGUSR2, SIG_IGN);
}

int wordle_server(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "Invalid argument(s)\n"); return EXIT_FAILURE; }

    int port = atoi(*(argv + 1));
    char* word_file = *(argv + 2);
    num_valid_dictionary_words = atoi(*(argv + 3));
    
    handle_signals();
    server_sock = setup_server(port);

    load_inital_data(word_file, port);

    while (!stop_server) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if(stop_server) {
            close(server_sock);
            break;
        }
        if (client_sock < 0){
            if (errno == EBADF) {
                printf("MAIN: listener socket closed. Shutting down...\n");
                break; 
            }
            continue;
        }
        printf("MAIN: rcvd incoming connection request\n");
        client* client_data = calloc(1, sizeof(client));
        client_data->guess = calloc(6, sizeof(char));
        client_data->guesses_remaining = 6;
        client_data->client_sock = client_sock;
        
        pthread_mutex_lock(&clients_lock);
        for (int i = 0; i < MAX_CLIENTS; i++){
            if (*(clients + i) == NULL){
                *(clients + i) = client_data;
                pthread_t client_thread;
                pthread_create(&client_thread, NULL, play_game, *(clients + i));
                pthread_detach(client_thread);
                break;
            }
        }
        pthread_mutex_unlock(&clients_lock);
    }
    for (int i = 0; i < num_valid_dictionary_words; i++) free(*(valid_dictionary_words + i));
    free(valid_dictionary_words);
    destroy_mutexes();
    close_server(); 
    return EXIT_SUCCESS;
}