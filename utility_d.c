 /*
 * utility_d.c:     file contenente definizioni macro, strutture dati e funzioni di routine utilizzate dai device.
 *
 * Samayandys Sisa Ruiz Muenala, 10 novembre 2022
 * 
 */


/*---------------------------------------
*           LIBRERIE DI SISTEMA
*----------------------------------------*/

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>


/*---------------------------------------
*               MACRO
*----------------------------------------*/

#define BUFF_SIZE 1024          // dimensione massima di un buffer di ausilio
#define CMD_SIZE 3              // dimensione di una stringa comando
#define RES_SIZE 1              // dimensione di una stringa responso
#define USER_LEN 50             // massima lunghezza di un username
#define MSG_LEN 1024            // massima lunghezza di un messaggio
#define TIME_LEN 20             // dimensione di una stringa timestamp
#define CRYPT_SALT 0xFACA       // salt per la funzione di criptazione


/*---------------------------------------
*           STRUTTURE DATI
*----------------------------------------*/
/* 
* message: struttura che descrive un messaggio. 
*/
struct message {
    char sender[USER_LEN+1];
    char recipient[USER_LEN+1];   
    char time_stamp[TIME_LEN+1];
    char group[USER_LEN+6];      // '-' se non fa parte di una conversazione di gruppo
    char text[MSG_LEN];
    char status[3];              // '*': se non ancora letto dal destinatario, '**': altrimenti
    struct message* next;
};

/*
* con_peer: struttura che contiene informazioni sulle connessioni attive peer to peer
*/
struct con_peer {
    char username[USER_LEN+1];
    int socket_fd;          
    struct con_peer* next;
};

/*
* chat: struttura che descrive una conversazione di gruppo o a due
*/
struct chat {
    char recipient[USER_LEN+1];
    char group[USER_LEN+6];
    int sck;
    bool on;                    // se true la conversazione è correntemente visualizzata a video
    int users_counter;
    struct con_peer* members;
    struct chat* next;
};

/*
* ack: contiene informazioni sulla ricezione o sul salvataggio di determinati messaggi
*/
struct ack{
    char sender[USER_LEN+1];       
    char recipient[USER_LEN+1];
    char start_time[TIME_LEN+1];
    char end_time[TIME_LEN+1];
    int port_recipient;         // porta del destinatario del messaggio
    int status;                 // 1: memorizzato dal server, 2: inviato al destinatario 
};


/*-------------------------------
*         UTILITY FUNCTIONS
*--------------------------------*/

/*
* Function: prompt_user
* ----------------------
* richiede all'utente un input
*/
void prompt_user(){
    
    printf("\n>> ");
    fflush(stdout);
}


/*
 * Function:  add_to_con
 * -----------------------
 * aggiunge una nuova connessione peer alla lista 'head'
 */
void add_to_con(struct con_peer **head, int sck, char* u)
{
    struct con_peer *newNode = malloc(sizeof(struct con_peer));
    strcpy(newNode->username, u);
    newNode->socket_fd = sck;
    newNode->next = NULL;

    if(*head == NULL)
         *head = newNode;
    else
    {
        struct con_peer *lastNode = *head;
        
        while(lastNode->next != NULL)
        {
            lastNode = lastNode->next;
        }

        lastNode->next = newNode;
    }
}


/*
* Function: basic_send
* -----------------------
* gestisce l'invio di una stringa tramite il socket 'sck'
*/
void basic_send(int sck, char* mes){

    uint16_t lmsg;
    char buff[BUFF_SIZE];

    strcpy(buff, mes);
    lmsg = strlen(mes)+1;
    lmsg = htons(lmsg);
    send(sck, (void*)&lmsg, sizeof(uint16_t), 0);
    send(sck, (void*)buff, strlen(buff)+1, 0);
}

/*
* Function: basic_receive
* -----------------------
* gestisce la ricezione di una stringa tramite il socket 'sck'
*/
void basic_receive(int sck, char* buff){

    uint16_t lmsg;
    
    recv(sck, (void*)&lmsg, sizeof(uint16_t), 0);
    lmsg = ntohs(lmsg);
    recv(sck, (void*)buff, lmsg, 0);
}

/*
* Function: get_conn_peer
* ---------------------------
* se con lo user 'p' è già stata instaurata una connessione TCP restituisce il socket relativo
* altrimenti restituisce -1
*/
int get_conn_peer(struct con_peer* list, char* p){

    int sck = -1;
    struct con_peer* temp = list;
    while (temp){
        if (strcmp(temp->username, p)==0){
            sck = temp->socket_fd;
            break;
        }
        temp = temp->next;   
    }
    return sck;
}

/*
* Function: first_ack_peer
* ---------------------------
* funzione invocata ogni qualvolta si instaura una connessione con un nuovo peer;
* provvede ad inviare al nuovo peer, user e porta del client che la invoca.
*/
void first_ack_peer(const char* hn, int hp, int peer_sck, bool send_cmd){

    uint16_t lmsg, host_port;
    char cmd [CMD_SIZE+1] = "FAK";

    // invio info di chi sono ossia nome e porta
    // invio il comando
    if (send_cmd==true){
        send(peer_sck, (void*)cmd, CMD_SIZE+1, 0);
    }

    // invio il nome
    lmsg = strlen(hn)+1;
    lmsg = htons(lmsg);
    send(peer_sck, (void*)&lmsg, sizeof(uint16_t), 0);
    send(peer_sck, (void*)hn, strlen(hn)+1, 0);

    // invio porta
    host_port = htons(hp);
    send(peer_sck, (void*)&host_port, sizeof(uint16_t), 0);
}


/*
* Function: check_contact_list
* ---------------------------
* data la stringa 'name' controlla che esista un contatto con tale nome;
* se esiste viene restituito il numero di porta del contatto altrimenti -1.
*/
int check_contact_list(char* name){

    FILE* fptr;
    char buff[USER_LEN+1];
    char cur_name[USER_LEN+1];
    int cur_port;

    fptr = fopen("./contact_list.txt", "r");
    if (fptr==NULL){
        printf("[-]No contacts yet.\n");
        return -1;
    }

    while ( fgets( buff, sizeof buff, fptr ) != NULL )
    {
        sscanf (buff, "%s %d", cur_name, &cur_port);            

        if ( strcmp(cur_name, name) == 0 )
        {   printf("[+]Username found in contact list.\n");
            return cur_port;
        }
    }
    return -1;
}

/*
* Function: add_contact_list
* ----------------------------
* dati user e porta provvede ad aggiungere il nuovo contatto nella rubrica
*/
void add_contact_list(char* name, int po){

    FILE* fptr;
    fptr = fopen("./contact_list.txt", "a");
    
    fprintf(fptr, "%s %d\n", name, po);
    fclose(fptr);
}

/*
* Function: compare_timestamp
* -------------------------------
* confronta il timestamp di due oggetti messagge
*/
int compare_timestamp(const struct message *a, const struct message *b) {

     return strcmp(a->time_stamp, b->time_stamp);
}

/*
* Function: insert_sorted
* --------------------------
* inserisce un nuovo elemento nella lista 'headptr'. L'inserimento è ordinato in base al timestamp.
*/
void insert_sorted(struct message* headptr, struct message *new_node) {

    struct message *ptr = new_node;

    if (headptr == NULL || compare_timestamp(ptr, headptr) < 0) {
        ptr->next = headptr;
        return;
    } else {
        struct message *cur = headptr;
        while (cur->next != NULL && compare_timestamp(ptr, cur->next) >= 0) {
            cur = cur->next;
        }
        ptr->next = cur->next;
        cur->next = ptr;
        return;
    }
}

/*
* Function: sort_messages
* --------------------------
* ordina il contenuto dei file chat in base al timestamp
*/
void sort_messages(char* id){

    FILE *fp, *fp1;
    char file_name[USER_LEN+20];
    char file_name1[USER_LEN+20];
    char buff_info[BUFF_SIZE];
    char buff_chat[MSG_LEN];
    struct message* new_list = NULL;
    struct message* temp;

    // ottengo i nomi dei file
    strcpy(file_name, "./cache/");
    strcat(file_name, id);
    strcat(file_name, "_info.txt");

    strcpy(file_name1, "./cache/");
    strcat(file_name1, id);
    strcat(file_name1, "_texts.txt");

    if ( (fp = fopen(file_name,"r+"))==NULL || (fp1 = fopen(file_name1, "r+"))==NULL){
        perror("[-]Error opening users files");
        return;
    }
    printf("[+]Chat files correctly opened.\n ");

    // scorro le righe dei file
    while( fgets(buff_info, BUFF_SIZE, fp)!=NULL && fgets(buff_chat, MSG_LEN, fp1)!=NULL ) {

        struct message* new_msg = malloc(sizeof(struct message));
        if (new_msg == NULL){
            perror("[-]Memory not allocated");
            exit(-1);
        }

        // memorizzo i singoli dati riportati in ogni riga in una struct message
        sscanf (buff_info, "%s %s %s", new_msg->time_stamp, new_msg->group, new_msg->sender);
        sscanf (buff_chat, "%s %s", new_msg->status, new_msg->text);
        // aggiungo il nuovo oggetto message ad una lista 
        insert_sorted(new_list, new_msg);   // inserimento ordinato
    }

    temp = new_list;
    while (new_list){

        new_list = new_list->next;

        fprintf(fp, "%s %s %s\n", temp->time_stamp, temp->group, temp->sender);
        fflush(fp);
        fprintf(fp1, "%s %s\n", temp->status, temp->text);
        fflush(fp1);

        free(temp);
        temp = new_list;
    }

    fclose(fp);
    fclose(fp1);

    printf("[+]Cache sorted.\n");
}

/*
* Function: get_name_from_sck
* -------------------------------
* cerca nella lista 'p' il peer avente socket pari a 's' e restitusce lo username del peer.
*/
char* get_name_from_sck(struct con_peer* p, int s){

    struct con_peer* temp = p;
    while (temp){
        if (temp->socket_fd==s){
            return temp->username;
        }
        temp = temp->next;
    }
    return NULL;
}

/*
* Function: encrypt
* ----------------------
* data la stringa 'password' provvede a criptarla.
*/
void encrypt(char password[],int key)
{
    unsigned int i;
    for(i=0;i<strlen(password);++i)
    {
        password[i] = password[i] - key;
    }
}

