/*
* Chat-app: applicazione network di messaggistica istantanea. 
*
*server.c:   implementazione del server dell'applicazione. 
*
* Samayandys Sisa Ruiz Muenala, 25 gennaio 2023
*
*/
#include "utility_s.c" 

fd_set master;                              // set master
fd_set read_fds;                            // set di lettura per la select
int fdmax;                                  // numero max di descrittori
struct session_log* connections = NULL;     // lista delle sessioni attualmente attive


/*
 * Function: show_home
 * -----------------------
 * stampa il menù iniziale del server.
 */
void show_home(){

    system("clear");

    printf("***********************************");
    printf(" SERVER STARTED ");
    printf("***********************************\n");
    printf("Digita un comando:\n\n");
    printf("1)  help --> mostra i dettagli dei comandi\n");
    printf("2)  list --> mostra un elenco degli utenti connessi\n");
    printf("3)  esc  --> chiude il server\n");
}


/*
* Function: help_display
* ------------------------
* mostra in dettaglio le funzionalità dei comandi.
*/
void help_display(){

    printf("\n***************** COMMANDS *****************\n");
	printf("\n-> help:  mostra questa breve descrizione dei comandi;\n");
	printf("-> list:  mostra l’elenco degli utenti connessi alla rete indicando username, timestamp di connessione e numero di porta;\n");
	printf("-> esc:   termina il server. Le chat in corso possono proseguire.\n");
}


/*
* Function: setup_list
* ------------------------
* inizializza la lista connections all'avvio del server.
*/
void setup_list(){

    FILE *fptr;
    char buffer[BUFF_SIZE];

    memset(buffer, 0, sizeof(buffer));

    // inizializzo la lista delle sessioni ancora aperte (per cui non ho ricevuto il logout)
    if ((fptr = fopen("./active_logs.txt","r")) == NULL){
        printf("[-]No active logs stored.\n");
        return;
    }
    printf("[+]Log file correctly opened.\n");

    // scompongo ogni riga del file nei singoli campi dati 
    while(fgets(buffer, BUFF_SIZE, fptr) != NULL) {

        struct session_log* temp = (struct session_log*)malloc(sizeof(struct session_log));
        if (temp == NULL){
            perror("[-]Memory not allocated\n");
            exit(-1);
        }

        sscanf (buffer, "%s %d %[^\n]",temp->username,&temp->port,temp->timestamp_login);

        strcpy(temp->timestamp_logout, NA_LOGOUT);  
        temp->socket_fd = -1;
        temp->next = NULL;

        // aggiungo un nuovo elemento in coda alla lista delle connessioni
        if(connections == NULL){
            connections = temp;
        }
        else
        {
            struct session_log* lastNode = connections;
            while(lastNode->next != NULL)
                lastNode = lastNode->next;
            lastNode->next = temp;        
        }
    }          
    
    fclose(fptr);  

    printf("[+]Connections list correctly initialized.\n");
    return;
}                                                             


/*
* Function: get_socket
* -----------------------
* Restituisce il numero del socket associato alla connessione con 'user' se online,
* altrimenti -1.
*/
int get_socket(char* user){

    int ret = -1;
    struct session_log* temp = connections;

    printf("[+]Getting socket of %s..\n", user);

    while (temp){

        if (strcmp(temp->username, user)==0 && strcmp(temp->timestamp_logout, NA_LOGOUT)==0 && temp->socket_fd!=-1){
            ret = temp->socket_fd;
            printf("[+]Socket obtained.\n");
            break;
        }
        temp = temp->next;
    }

    return ret;
}


/*
* Function: logout
* -------------------
* aggiorna una determinata sessione nella lista delle connessioni modificando timestamp di logout ed eventualmente n° di socket.
* sono distinti i casi in cui il logout è regolare (richiesta inviata dal client), irregolare, oppure si tratta di un logout relativo ad una sessione precedente.
*/
void logout(int socket, bool regular){  

    char user[USER_LEN+1];
    char buff[TIME_LEN+1];
    struct session_log* temp = connections;
    struct session_log* temp1 = connections;

    memset(user, 0, sizeof(user));
    memset(buff, 0, sizeof(buff));

    if (!regular){      // nel caso di disconnessione irregolare
        time_t now = time(NULL);    // imposto il timestamp all'ora attuale
        strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
    }
    else{
        // altrimenti ricevo il timestamp dilogout
        recv(socket, (void*)buff, TIME_LEN+1, 0);
    }
    printf("[+]Logout timestamp obtained.\n");

    // ricavo il suo username tramite il socket
    printf("[+]Looking for username..\n");
    while(temp){
        if(temp->socket_fd==socket){
            strcpy(user, temp->username);
            printf("[+]Username found.\n");
            break;
        }
        temp = temp->next;
    }

    // cerco un'altra connessione nella lista che abbia questo username e timestamp di logout NULLo
    // aggiorno quindi il timestamp di logout

    printf("[+]Looking for session log to update..\n");

    while (temp1)
    {
        if( strcmp(temp1->username, user)==0 && strcmp(temp1->timestamp_logout, NA_LOGOUT)==0 )
        {
            strcpy(temp1->timestamp_logout, buff);
            printf("[+]Session updated.\n");
            break;
        }
        temp1 = temp1->next;
    }

    // se è stato aggiornata una sessione corrente chiudo il socket
    if (temp1->socket_fd!=-1){
        temp1->socket_fd = -1;
        close(socket);
        FD_CLR(socket, &master);
        printf("[+]Socket closed.\n");
    }  

    printf("[+]Logout successfully completed.\n");

}


/*
* Function: show_list
* ---------------------
* mostra l’elenco degli utenti connessi alla rete, indicando username, timestamp di connessione e numero di
* porta nel formato “username*timestamp*porta”
*/
void show_list(){

    struct session_log* temp = connections;

    // scorro la lista delle sessioni e stampo dettagli di quelle attive
    printf("\n********************* USERS ONLINE *********************\n");
    printf("\n\tUSER*TIME_LOGIN*PORT\n\n");

    while(temp){

        if ( strcmp(temp->timestamp_logout, NA_LOGOUT)==0 && temp->socket_fd!=-1 ){
            printf("\t");
            printf("%s*%s*%d\n", temp->username, temp->timestamp_login, temp->port);
        }

        temp = temp->next;
    }
}


/*
* Function: terminate_server
* ----------------------------
* Termina il server. Salva tutte le sessioni nella lista connections in file e si occupa di chiudere tutti i socket attivi.
*/
void terminate_server(){

    FILE *fptr;
    FILE *fptr1;
    int counter = 0;
    struct session_log* temp = connections;

    printf("[+]Terminating server..\n");

    // apro i file relativi ai log delle sessioni
    if ( (fptr = fopen("./active_logs.txt","w"))!=NULL && (fptr1 = fopen("./logs_ar.txt", "a"))!=NULL ){
        printf("[+]Log files correctly opened.\n");
    }
    else{
        perror("[-]Error opening files");
        return;
    }                                                                    
   
    // scorro le sessioni
    while(connections){

        connections = connections->next;
        if ( strcmp(temp->timestamp_logout, NA_LOGOUT)==0 ){    // se la sessione è ancora attiva

            // salvo nel file active_logs.txt
            fprintf(fptr, "%s %d %s\n", 
                temp->username, temp->port, temp->timestamp_login);
            if (temp->socket_fd!=-1){
                close(temp->socket_fd);
                FD_CLR(temp->socket_fd, &master);
            }
            counter++;
        }
        else{   // se la sessione è terminata
            // salvo in logs_ar.txt
            fprintf(fptr1, "%s %d %s %s\n", 
                temp->username, temp->port, temp->timestamp_login, temp->timestamp_logout);
        }

        temp = connections;
    }

    // elimino il file active_logs.txt se non ci sono sessioni aperte da salvare
    if (counter==0){
        remove("./active_logs.txt");
    }
    printf("[+]Log files correctly updated.\n");

    fclose(fptr);  
    fclose(fptr1);

    printf("[+]Server terminated.\n");
    exit(1);
}


/*
 * Function:  login
 * ----------------
 * registra il login di un dispositivo client.
 */
void login(int dvcSocket)     
{
    FILE *fptr;
    char buff[BUFF_SIZE];
    char cur_name[USER_LEN+1];
    char cur_pw[USER_LEN+1];
    char psw[USER_LEN+1];
    char user[USER_LEN+1];
    char t_buff[TIME_LEN+1];
    uint16_t port;
    time_t now = time(NULL);
    bool found = false;

    memset(buff, 0, sizeof(buff));
    memset(cur_name, 0, sizeof(cur_name));
    memset(cur_pw, 0, sizeof(cur_pw));
    memset(psw, 0, sizeof(psw));
    memset(user, 0, sizeof(user));
    memset(t_buff, 0, sizeof(t_buff));


    struct session_log* new_node = (struct session_log*)malloc(sizeof( struct session_log));
    if (new_node == NULL){
        perror("[-]Memory not allocated\n");
        exit(-1);
    }

    // ricevo username
    basic_receive(dvcSocket, user);

    // ricevo password
    basic_receive(dvcSocket, psw);

    // ricevo il numero della porta
    recv(dvcSocket, (void*)&port, sizeof(uint16_t), 0);
    port = ntohs(port);

    printf("[+]Received credentials for login: %s, %s.\n", user, psw);

    // confronto username e psw con i dati di users.txt
    fptr = fopen("./users.txt","r");
    printf("[+]Users file correctly opened for reading.\n");
    while ( fgets( buff, BUFF_SIZE, fptr ) != NULL )
    {
        sscanf (buff, "%s %s %*d", cur_name, cur_pw);            

        if ( strcmp(cur_name, user) == 0 && strcmp(cur_pw, psw) == 0 )
        {   printf("[+]Found a match for received credentials.\n");
            found = true;
            break;
        }
    }

    if (!found){
        printf("[-]Could not find matching credentials in database.\n");
        send_server_message(dvcSocket, "Credenziali non trovate nel database!\n", true);
        return;
    }

    new_node->port = port;
    new_node->socket_fd = dvcSocket;
    strftime(t_buff, TIME_LEN, "%Y-%m-%d %H:%M:%S", localtime(&now));
    strcpy(new_node->username, user);
    strcpy(new_node->timestamp_login, t_buff);
    strcpy(new_node->timestamp_logout, NA_LOGOUT);

    printf("[+]Login:\n");
    printf("\t%s", new_node->username);
    printf(" %s\n", new_node->timestamp_login);

    // aggiungo in coda alla lista delle connessioni gestite
    if(connections == NULL){
        connections = new_node;
        connections->next = NULL;
    }
    else
    {   
        struct session_log* lastNode = connections;
    
        while(lastNode->next != NULL)
        {
            lastNode = lastNode->next;
        }
        lastNode->next = new_node;

    }

    printf("[+]Login saved.\n");

    //invio un messaggio di conferma all'utente
    send_server_message(dvcSocket, NULL, false);

    // aggiorno fdmax
    if(dvcSocket>fdmax){
        fdmax = dvcSocket;
    } 
}


/*
 * Function:  signup
 * -----------------
 * registra un nuovo dispositivo client
 */
void signup(int dvcSocket){

    FILE *fpta;
    char new_psw[USER_LEN+1];
    char new_user[USER_LEN+1];
    uint16_t new_port;
    int n_port;

    memset(new_psw, 0, sizeof(new_psw));
    memset(new_user, 0, sizeof(new_user));

    printf("[+]Signup handler in action.\n");

    // ricevo lo user
    basic_receive(dvcSocket, new_user);         

    // ricevo la password
    basic_receive(dvcSocket, new_psw);      

    // ricevo la porta
    recv(dvcSocket, (void*)&new_port, sizeof(uint16_t), 0);
    n_port = ntohs(new_port);

    printf("[+]Received credentials: user: %s, crypted psw: %s\n", new_user, new_psw);

    // controllo l'eventuale presenza di un omonimo, se presente scarto il dato in entrata            
    if ( check_username(new_user) == 1 )
    {
        //invio segnalazione al client
        send_server_message(dvcSocket, "Username già presente nel database. Scegliere un altro username!", true);
        return;
    }

    // apro il file users.txt per aggiungere il nuovo utente
    fpta = fopen("./users.txt","a");
    printf("[+]Users file correctly opened for appending.\n");
    fprintf(fpta, "%s %s %d\n", new_user, new_psw, n_port);
    fflush(fpta);
    fclose(fpta);

    printf("[+]New user registered.\n");

    send_server_message(dvcSocket, NULL, false);

    close(dvcSocket);
    FD_CLR(dvcSocket, &master);
} 


/*
* Function: offline_message_handler
* ------------------------------------
* salva i messaggi pendenti ( destinati ad utenti attualmente offline )
*/
void offline_message_handler(int s_client){

    char t_buff[TIME_LEN+1];
    char sender[USER_LEN+1];
    char rec[USER_LEN+1];
    char m_buff[MSG_LEN];

    memset(t_buff, 0, sizeof(t_buff));
    memset(sender, 0, sizeof(sender));
    memset(rec, 0, sizeof(rec));
    memset(m_buff, 0, sizeof(m_buff));

    printf("[+]Handling offline message..\n");

    struct message* new_node = (struct message*)malloc(sizeof(struct message));
    if (new_node == NULL){
        perror("[-]Memory not allocated");
        exit(-1);
    }

    // ricevo il mittente
    basic_receive(s_client, sender);

    // ricevo il destinatario
    basic_receive(s_client, rec);

    // ricevo il timestamp
    recv(s_client, (void*)t_buff, TIME_LEN+1, 0);

    // ricevo il testo del messaggio
    basic_receive(s_client, m_buff);
    printf("[+]Messagge received for %s.\n", rec);

    strcpy(new_node->recipient, rec);
    strcpy(new_node->sender, sender);
    strcpy(new_node->time_stamp, t_buff);
    strcpy(new_node->text, m_buff);

    new_node->next = NULL;

    // salvo localmente il messaggio
    add_to_stored_messages(new_node);

    free(new_node);

}


/*
* Function: send_message_to_device
* -------------------------------
* invia un messaggio ad un device.
* Restituisce 1 se l'invio va a buon fine, altrimenti -1.
*/ 
int send_message_to_device(int sck, struct message* m){

    uint16_t port;
    char cmd[CMD_SIZE+1] = "RMP";

    // invio il comando di messaggio
    if ( send(sck, (void*)cmd, CMD_SIZE+1, 0)<=0 )
    {   printf("[-]Can't connect with %s is offline.\n", m->recipient);
        return -1;
    }

    // invio un gruppo per conformarmi con il handler della parte client
    basic_send(sck, "-");

    // invio il mittente
    basic_send(sck, m->sender);

    // invio il timestamp
    send(sck, (void*)m->time_stamp, TIME_LEN+1, 0);

    // invio il messaggio
    basic_send(sck, m->text);

    // invio la porta
    port = get_port(m->sender);
    port = ntohs(port);
    send(sck, (void*)&port, sizeof(uint16_t), 0);
    

    printf("[+]Message succesfully sent to new contact %s.\n", m->recipient);

    return 1;
}


/*
 * Function:  new_contact_handler
 * --------------------------------
 * gestisce il primo messaggio ad un nuovo contatto. Se questo è online invia il messaggio al destinatario 
 * e il numero di porta del nuovo contatto al mittente altrimenti si lmita a salvare il messaggio.
 */
void new_contact_handler(int dvcSocket){       

    FILE *fptr;
    int port;
    char new_user[USER_LEN+1];
    char cur_name[USER_LEN+1];
    char buff[BUFF_SIZE];
    uint8_t ack = 1;
    bool exists = false;
    struct session_log* temp = connections;
    struct message* new_msg = (struct message*)malloc(sizeof(struct message));

    if (new_msg==NULL){
        perror("[-]Memory not allocated");
        exit(-1);
    }
    
    printf("[+]new_contact_handler in action..\n");

    memset(new_user, 0, sizeof(new_user));

    //ricevo lo username
    basic_receive(dvcSocket, new_user);

    // controllo se lo user esiste
    fptr = fopen("./users.txt","r");
    printf("[+]Users file correctly opened for reading.\n");

    while ( fgets( buff, BUFF_SIZE, fptr ) != NULL )
    {
        memset(cur_name, 0, sizeof(cur_name));

        sscanf (buff, "%s %*s %d", cur_name, &port);            

        if ( strcmp(cur_name, new_user) == 0 )
        {  
            exists = true;
            break;
        }
    }

    fclose(fptr);

    if (exists){    // se l'utente esiste
        printf("[+]User found.\n");
        send_server_message(dvcSocket, NULL, false);    // invio un messaggio di validità al client
    }
    else{
        printf("[-]User search failed.\n");
        send_server_message(dvcSocket, "User non esistente", true);
        return;
    }

    // ricevo il messaggio
    basic_receive(dvcSocket, new_msg->time_stamp);
    basic_receive(dvcSocket, new_msg->recipient);
    basic_receive(dvcSocket, new_msg->sender);
    basic_receive(dvcSocket, new_msg->text);

    // controllo se l'utente è online
    while(temp!=NULL){
        if (strcmp(temp->username, new_user)==0 && strcmp(temp->timestamp_logout, NA_LOGOUT)==0 && temp->socket_fd!=-1){
            printf("[+]Recipient is online.\n");
            break;
        }
        temp = temp->next; 
    }

    if (temp!=NULL){    // se l'utente risulta online
        // provo a inviare il messaggio
        int attempts = 1;
        while (attempts<4){

            printf("[+]Attempt to send message to new contact: n° %d\n", attempts);
            if (send_message_to_device(temp->socket_fd, new_msg)==1){
                break;
            }
            attempts++;
        }

        if (attempts<4){    // invio andato a buon fine
            // ricavo la porta
            uint16_t port = temp->port;

            // invio l'ack
            ack = 2;    // ack di avvenuta ricezione
            send(dvcSocket, (void*)&ack, sizeof(uint8_t), 0);
            // invio la porta del destinatario
            port = htons(port);
            send(dvcSocket, (void*)&port, sizeof(uint16_t), 0);
            
            return;
        }
        else{
            // assumo che l'utente sia offline
            // aggiorno la sua sessione
            logout(temp->socket_fd, false);
        }
    }
        
    printf("[-]Recipient is offline.\n");
    // salvo il messaggio
    add_to_stored_messages(new_msg);

    // invio ack di avvenuta memorizzazione
    send(dvcSocket, (void*)&ack, sizeof(uint8_t), 0);
    printf("[+]Ack sent.\n");

}


/*
* Function: input_handler
* --------------------------------
* gestisce le richeste fatte tramite stdin del server. Legge l'input e avvia il corrispondente handler.
*/
void input_handler(){

    char input[BUFF_SIZE];

    memset(input, 0, sizeof(input));
	
	fgets(input,sizeof(input),stdin);
    input[strlen(input)-1] = '\0';      // rimuovo l'ultimo char ('\n')
	
    printf("\n");

	if(strcmp(input,"help")==0){
        help_display();
	}
	else if(strcmp(input,"list")==0){
		show_list();
    }
	else if(strcmp(input,"esc")==0){
		terminate_server();
	}
	else{
        show_home();
		printf("\n[-]Invalid command. Try Again.\n");
	}

}


/*
* Function: hanging_handler
* ----------------------------
* fornisce una anteprima dei messaggi pendenti destinati al cliente che ha effettuato la richiesta.
* per ogni utente viene mostrato username, il numero di messaggi pendenti, e il timestamp del più recente.
*/
void hanging_handler(int fd){

    FILE *fp, *fp1;
    char user[USER_LEN+1];
    char buff_info[BUFF_SIZE];
    char buff_mess[BUFF_SIZE];
    char buffer[BUFF_SIZE];
    char counter[BUFF_SIZE];
    struct preview_user* list = NULL;
    struct preview_user* next;

    memset(user, 0, sizeof(user));
    memset(buff_info, 0, sizeof(buff_info));
    memset(buff_mess, 0, sizeof(buff_mess));
    memset(buffer, 0, sizeof(buffer));
    memset(counter, 0, sizeof(counter));

    // ricevo il nome dello user (destinatario dei messaggi)
    basic_receive(fd, user);

    // devo aprire i file dei messaggi
    fp = fopen("./chat_info.txt", "r");
    fp1 = fopen("./chats.txt", "r");

    if (fp == NULL || fp1 == NULL){
        perror("[-]Error opening file");
        send_server_message(fd, "Nessun messaggio pendente\n", true);
        return;
    }

    // leggo il contenuto dei file
    printf("[+]Fetching buffered messages..\n");
    while( fgets(buff_info, BUFF_SIZE, fp)!=NULL && fgets(buff_mess, BUFF_SIZE, fp1)!=NULL ) {

        char day[TIME_LEN];
        char hour[TIME_LEN];
        char sen[USER_LEN+1];
        char rec[USER_LEN+1];

        memset(day, 0, sizeof(day));
        memset(hour, 0, sizeof(hour));
        memset(sen, 0, sizeof(sen));
        memset(rec, 0, sizeof(rec));

        struct preview_user* temp;
        sscanf(buff_info, "%s %s %s %s", sen, rec, day, hour);

        // controllo che ci siano messaggi destinati a 'user'
        if (strcmp(rec, user)==0){

            printf("[+]Mesage found.\n");

            // controllo che il mittente abbia già una preview dedicata
            temp = name_checked(list, sen);
            if ( temp == NULL ){
                
                temp = (struct preview_user*)malloc(sizeof(struct preview_user));
                if (temp == NULL){
                    perror("[-]Memory not allocated\n");
                    exit(-1);
                }
                strcpy(temp->user, sen);
                temp->messages_counter = 1;
                sprintf(temp->timestamp, "%s %s", day, hour);
                
                temp->next = list;
                list = temp;
            }
            else{
                printf("[+]Creating new preview.\n");
                temp->messages_counter++;
                sprintf(temp->timestamp, "%s %s", day, hour);
            }
        }
    }

    fclose(fp);
    fclose(fp1);
    printf("[+]Files closed.\n");

    if (list == NULL){
        printf("[-]No messages for %s.\n", user);
        send_server_message(fd, "Nessun messaggio pendente\n", true);
        return;
    }

    // invio un messaggio al device per avvisarlo che ci sono messaggi pendenti
    send_server_message(fd, NULL, false);

    // preparo la stringa da inviare
    printf("[+]Setting up string to send to device..\n");

    while (list){
        
        strcat(buffer, list->user);
        strcat(buffer, "\t\t");

        sprintf(counter, "%d", list->messages_counter);
        strcat(buffer, counter);

        strcat(buffer, "\t\t");
        strcat(buffer, list->timestamp);
        strcat(buffer, "\n");

        next = list->next;
        free(list);
        list = next;
    }

    // invio la stringa
    basic_send(fd, buffer);

    printf("[+]Buffer successfully sent to device.\n");
        
}


/*
* Function: pending_messages
* ----------------------------
* invia al client tutti i messaggi pendenti da un determinato user, invia poi al mittente un ack di ricezione.
*/
void pending_messages(int fd){

    FILE* fp, *fp1;
    int socket_ack;
    struct ack *ackp;
    char buff_info[BUFF_SIZE];
    char buff_chat[MSG_LEN];
    char sender[USER_LEN+1];
    char recipient[USER_LEN+1];
    struct message *to_store = NULL; 
    struct message* to_send = NULL;
    struct message* cur;
    uint8_t texts_counter = 0;

    memset(buff_info, 0, sizeof(buff_info));
    memset(buff_chat, 0, sizeof(buff_chat));
    memset(sender, 0, sizeof(sender));
    memset(recipient, 0, sizeof(recipient));


    basic_receive(fd, recipient);   // ricevi il destinatario
    basic_receive(fd, sender);      // ricevi il mittente

    // apro i file dei messaggi salvati
    fp = fopen("./chat_info.txt", "r+");
    fp1 = fopen("./chats.txt", "r+");

    if (fp == NULL || fp1 == NULL){    // i file non esistono
        printf("[-]No stored messages to send.\n");
        send(fd, (void*)&texts_counter, sizeof(uint8_t), 0);
        return;
    }

    // si gestiscono due liste
    while( fgets(buff_info, BUFF_SIZE, fp)!=NULL && fgets(buff_chat, MSG_LEN, fp1)!=NULL ) {
        char day[TIME_LEN];
        char hour[TIME_LEN];

        memset(day, 0, sizeof(day));
        memset(hour, 0, sizeof(hour));

        struct message* temp = (struct message*)malloc(sizeof(struct message));
        if (temp == NULL){
            perror("[-]Memory not allocated\n");
            exit(-1);
        }
        sscanf(buff_info, "%s %s %s %s", temp->sender, temp->recipient, day, hour);
        sprintf(temp->time_stamp, "%s %s", day, hour);
        buff_chat[strcspn(buff_chat, "\n")] = 0;
        strcpy(temp->text, buff_chat);
        temp->next = NULL;

        // se mittente e destinatario sono quelli cercati
        if (strcmp(temp->sender, sender)==0 && strcmp(temp->recipient, recipient)==0){
           
            // aggiungo il messaggio in coda alla lista degli elementi da inviare
            if(to_send == NULL){
                to_send = temp;
            }
            else{
                struct message* lastNode = to_send;
                while(lastNode->next != NULL)
                    lastNode = lastNode->next;
                lastNode->next = temp;
            }
            texts_counter++;
            printf("[+]Message from %s to %s found: %d.\n", sender, recipient, texts_counter);
        }
        else{
            // altrimenti viene aggiunto alla lista generale
            if(to_store == NULL)
                to_store = temp;
            else
            {
                struct message* lastNode = to_store;
                while(lastNode->next != NULL)
                    lastNode = lastNode->next;
                lastNode->next = temp;        
            }
        }
    }

    // invia il numero di messaggi della lista
    send(fd, (void*)&texts_counter, sizeof(uint8_t), 0);

    // se zero ci si ferma
    if (texts_counter==0) {
        printf("[+]No stored messages from %s to %s.\n", sender, recipient);
        return;
    }

    // altrimenti invio i messaggi pendenti
    cur = to_send;
    while(to_send){

        printf("to_send non è nullo\n");
        char buffer[BUFF_SIZE];
        to_send = to_send->next;

        memset(buffer, 0, sizeof(buffer));

        // bufferizzo i dati del messaggio
        sprintf(buffer, "%s %s", cur->time_stamp, cur->sender);

        // invio il buffer contenente info sul messaggio al client
        basic_send(fd, buffer);
 
        // invio il buffer contenente il messaggio al client
        strcpy(buffer, cur->text);
        basic_send(fd, buffer);

        free(cur);
        cur = to_send;
        printf("[+]Hanging message sent.\n");
    }

    // invia un singolo ack al mittente 
    ackp = (struct ack*)malloc(sizeof(struct ack));
    if (ackp == NULL){
            perror("[-]Memory not allocated\n");
            exit(-1);
    }
    strcpy(ackp->sender, sender);
    strcpy(ackp->recipient, recipient);

    socket_ack = get_socket(sender);

    if (socket_ack == -1){  // se il mittente è offline

        // salvo gli ack in buffered_acks.txt
        FILE *fp2 = fopen("./buffered_acks.txt", "a");
        fprintf(fp2, "%s %s\n", ackp->sender, ackp->recipient);
        fclose(fp2);
        printf("[+]Ack saved.\n");
    }
    else{// il mittente è online
         // invio il comando di ack di ricezione e le info
        char ack[CMD_SIZE+1] = "AK1";

        send(socket_ack, ack, CMD_SIZE+1, 0);

        // seguo inviando destinatario
        basic_send(socket_ack, ackp->recipient);

        printf("[+]Ack sent.\n");
    }

    // aggiorno i file dei messaggi salvati
    if (to_store==NULL){
        fclose(fp);
        fclose(fp1);
        remove("./chat_info.txt");
        remove("./chats.txt");
    }
    else{
        FILE *fpw = freopen(NULL, "w+", fp);
        FILE *fpw1 = freopen(NULL, "w+", fp1);
        cur = to_store;
        while(to_store){
            to_store = to_store->next;       
            fprintf(fpw, "%s %s %s\n", cur->sender, cur->recipient, cur->time_stamp);
            fprintf(fpw1, "%s\n", cur->text);
            fflush(fp1);
            free(cur);
            cur = to_store;
        }
        fclose(fpw);
        fclose(fpw1);
    }

    printf("[+]Files correctly updated.\n");
}


/*
* Function: send_buffered_acks
* --------------------------------
* invia all'utente tutti gli ack di ricezione salvati mentre questo era offline.
*/
void send_buffered_acks( int sck ){

    FILE* fp;
    uint8_t counter;
    char buffer[BUFF_SIZE];
    struct ack* list = NULL;
    struct ack* to_send = NULL;

    fp = fopen("./buffered_acks.txt", "r+");

    if (fp == NULL){
        counter = 0;
    }
    else {
        // apro il file per cercare acks destinati allo user

        memset(buffer, 0, sizeof(buffer));
        while( fgets(buffer, BUFF_SIZE, fp)!=NULL){

            struct ack* cur_ack =  (struct ack*)malloc(sizeof(struct ack));
            if (cur_ack==NULL){
                perror("[-]Error allocating memory");
                exit(-1);
            }
            sscanf(buffer, "%s %s", cur_ack->sender, cur_ack->recipient);

            // se trovo un ack destinato a quell'utente
            if(strcmp(cur_ack->sender, get_name_from_sck(connections, sck))==0){
                
                // aggiungo alla lista degli ack da inviare
                cur_ack->next = to_send;
                to_send = cur_ack;
                counter++;

            }
            else{
                // aggiungo alla lista degli ack da salvare in file
                cur_ack->next = list;
                list = cur_ack;
            }
        }    
    }

    // invio il numero di ack
    send(sck, (void*)&counter, sizeof(uint8_t), 0);

    if (counter==0){
        printf("[-]No acks for %s to send.\n", get_name_from_sck(connections, sck));
        return;
    }
    else{
        // invio tutti gl ack
        struct ack* next;
        while(to_send!=NULL){
            
            basic_send(sck, to_send->recipient);

            next = to_send->next;
            free(to_send);
            to_send = next;

            printf("[+]Ack sent.\n");
        }
    }

    // salvo i restanti ack
    if (list==NULL){
        fclose(fp);
        remove("./buffered_acks.txt");
    }
    else{
        rewind(fp);
        struct ack* next;
        while(list!=NULL){
            fprintf(fp, "%s %s\n", list->sender, list->recipient);
            next = list->next;
            free(list);
            list = next;
        }
        fclose(fp);
    }

    return;
}


/*
* Function: update_con
* ----------------------
* funzione che aggiorna il campo socket di una connessone attiva. Utile nel caso in cui il server sia stato terminato
* mentre un device era ancora connesso ed è rimasto connesso al seguente riavvio del server. Così non è necessario che 
* riesegua il login per contattare il server.
*/
void update_con(int sck){

    uint16_t port;
    struct session_log* cur = connections;

    // ricevo la porta
    recv(sck, (void*)&port, sizeof(uint16_t), 0);
    port = ntohs(port);

    // cerco tra le connessioni attive se ce n'è una col nome corrispondente a quella porta
    while(cur){

        if (cur->port == port && strcmp(cur->timestamp_logout, NA_LOGOUT)==0 ){
            cur->socket_fd = sck;
            printf("[+]Session updated.\n");
            break;
        }
        cur = cur->next;
    }
}

/*
* Function: client_handler
* -----------------------
* gestisce le interazioni client server. per ogni richiesta ricevuta avvia l'apposito gestore
*/
void client_handler(char* cmd, int s_fd){       

    if (strcmp(cmd, "SGU")==0){
        signup(s_fd);
    }
    else if(strcmp(cmd, "LGI")==0){
        login(s_fd);
    }
    else if(strcmp(cmd, "SLL")==0){
        logout(s_fd, true);
    }
    else if(strcmp(cmd, "RCA")==0){
        send_buffered_acks(s_fd);
    }
    else{
        // aggiorno la sua connessione se necessario
        update_con(s_fd);

        if(strcmp(cmd,"LGO")==0){
            logout(s_fd, true);
        }
        else if(strcmp(cmd, "NCH")==0){
            new_contact_handler(s_fd);
        }
        else if(strcmp(cmd, "HNG")==0){
            hanging_handler(s_fd);
        }
        else if(strcmp(cmd, "SHW")==0){
            pending_messages(s_fd);
        }
        else if(strcmp(cmd, "SOM")==0){    
            offline_message_handler(s_fd);
        }
        else{
            printf("[-]Invalid command received from client: %s\n", cmd);
        }
    } 
}


int main(int argc, char* argcv[])
{
    struct sockaddr_in dv_addr;     // indirizzo del device
    struct sockaddr_in sv_addr;     // indirizzo del server
    int listener;                   // descrittore del listening socket
    int newfd;                      // descrittore di un nuovo socket
    socklen_t addrlen;

    char buff[BUFF_SIZE];           // buffer per i dati in ricezione

    int i;
    int sv_port;
    int ret_b, ret_l, ret_r;        // valori di ritorno delle funzioni


    FD_ZERO(&master);               // azzero i set
    FD_ZERO(&read_fds);

    if (argc > 1) {
        sv_port = atoi(argcv[1]);
    }
    else {
        sv_port = DEFAULT_SV_PORT;
    }

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("[-]Error in socket");
        exit(1);
    }
    printf("\n[+]Server socket created successfully.\n");

    sv_addr.sin_family = AF_INET;
    sv_addr.sin_addr.s_addr = INADDR_ANY;
    sv_addr.sin_port = htons(sv_port);

    ret_l = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if (ret_l == -1){
        perror("[-]Error in setsockopt");
        exit(-1);
    }

    ret_b = bind(listener, (struct sockaddr*)& sv_addr, sizeof(sv_addr));
    if(ret_b < 0){
        perror("[-]Error in bind");
        exit(1);
    }
    printf("[+]Binding successfull.\n");

    ret_l = listen(listener, 10);
    if(ret_l < 0){
        perror("[-]Error in listening");
        exit(1);
    }
    printf("[+]Listening..\n");

    // aggiungo stdin e listener al master set
    FD_SET(0,&master);
    FD_SET(listener, &master);

    fdmax = listener; 

    setup_list();
    sleep(1);
    show_home();
    prompt_user();

    for(;;) {
        read_fds = master; 
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("[-]Error in select");
            exit(4);
        }
        printf("\n[+]Select worked.\n");

        for(i = 0; i <= fdmax; i++) {

            if (FD_ISSET(i, &read_fds)){
            
                // se stdin diviene attivo
                if(i==0){
                    input_handler();
                }
                // se il listener diviene attivo
                else if(i==listener){

                    // chiamo accept per ottenere una nuova connessione
                    addrlen = sizeof(dv_addr);
                    if((newfd = accept(listener,(struct sockaddr*)&dv_addr,&addrlen))<0){
                        perror("[-]Error in accept");
                        sleep(10);
                    }
                    else{
                        FD_SET(newfd, &master); // Aggiungo il nuovo socket
                        if( newfd >fdmax){
                            fdmax = newfd;
                        } 
                        printf("[+]New connection accepted.\n");
                    }
                }
                else{
                    // nel caso di errore nella ricezione dei dati
                    memset(buff, 0, sizeof(buff));
                    if ((ret_r = recv(i, (void*)buff, CMD_SIZE+1, 0)) <= 0) {

                        if (ret_r == 0) {
                                // chiusura del device, chiusura non regolare si traduce in un logout
                                logout(i, false);
                                printf("[-]Selectserver: socket %d hung up\n", i);

                        } else {
                                // errore nel recv
                                perror("[-]Error in recv");
                        } 
                    } 
                    else {
                        // ricevo il tipo di comando e gestisco tramite handler
                        printf("[+]Client request received: %s\n", buff);
                        
                        client_handler(buff, i);
                    }
                }
                prompt_user(); 
            }             
        }
    } 
    
    return 0;
}
