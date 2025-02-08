#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100
#define MAX_CLIENTS 10

typedef struct {
    char file_name[MAX_FILENAME];
    int num_chunks;
    char chunks[MAX_CHUNKS][HASH_SIZE + 1];
} Client_file_info;

typedef struct {
    int num_files;
    Client_file_info files[MAX_FILES];
    int peers[MAX_FILES][MAX_CLIENTS + 1];
    int seeders[MAX_FILES][MAX_CLIENTS + 1];
} Tracker_file_info;

typedef struct {
    int rank;
    int num_files;
    int num_req_files;
    char requested_files[MAX_FILES][MAX_FILENAME + 1];
    Client_file_info files[MAX_FILES];
} Owned_files;


// functie pentru scrierea unui fisier descarcat
void print_downloaded_file(Client_file_info *recv_file, char *file_name, int rank) {
    FILE *file;
    char file_name1[MAX_FILENAME];
    sprintf(file_name1, "client%d_%s", rank, file_name);
    file = fopen(file_name1, "w");
    for (int i = 0; i < recv_file->num_chunks; i++) {
        fprintf(file, "%s", recv_file->chunks[i]);
        if (i < recv_file->num_chunks - 1) {
            fprintf(file, "\n");
        }
    }
    fclose(file);
}
void *download_thread_func(void *arg)
{
    // datele despre fisierele detinute de client
    Owned_files *owned_files = (Owned_files*) arg;
    // buffer pentru cereri
    char request[1024];
    // date despre fisierele cerute de client
    int num_chunks = 0;
    int seeders[MAX_CLIENTS + 1];
    int peers[MAX_CLIENTS + 1];
    char chunks[MAX_CHUNKS][HASH_SIZE + 1];
    
    // descarcarea fisierelor cerute
    for (int i = 0; i < owned_files->num_req_files; i++) {
        memset(chunks, 0, sizeof(chunks));
        // cerere de informatii despre fisier
        sprintf(request, "GET %s\n", owned_files->requested_files[i]);
        // trimitere cerere catre tracker
        MPI_Send(request, 1024, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
        // primire informatii despre fisier
        MPI_Recv(&num_chunks, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(seeders, MAX_CLIENTS + 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(peers, MAX_CLIENTS + 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(chunks, num_chunks * (HASH_SIZE + 1), MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // adaugare date despre fisier in structura clientului
        owned_files->files[owned_files->num_files].num_chunks = num_chunks;
        strcpy(owned_files->files[owned_files->num_files].file_name, owned_files->requested_files[i]);
        int recv_chunks = 0; // numarul de chunk-uri descarcate
        int pidx = 0; // index pentru peer
        int sidx = 0; // index pentru seeder
        // descarcarea chunk-urilor
        while (recv_chunks < num_chunks) {
            // cautare peer
            for (int j = pidx + 1; j <= MAX_CLIENTS; j++) {
                if (peers[j] == 1) { // daca am gasit un peer
                    pidx = j;
                    // cerere de chunk
                    sprintf(request, "GET_CHUNK %s %d\n", owned_files->requested_files[i], recv_chunks);
                    MPI_Send(request, 1024, MPI_CHAR, pidx, 1, MPI_COMM_WORLD);
                    char chunk[HASH_SIZE + 1];
                    MPI_Recv(chunk, HASH_SIZE + 1, MPI_CHAR, pidx, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    // verificam daca chunk-ul descarcat este cel corect
                    if (strcmp(chunk, chunks[recv_chunks]) == 0) {
                        strcpy(owned_files->files[owned_files->num_files].chunks[recv_chunks], chunk);
                        recv_chunks++;
                        break;
                    }
                }
                // resetare index peer
                if (j >= MAX_CLIENTS) {
                    pidx = 0;
                }
            }
            // cautare seeder
            for (int j = sidx + 1; j <= MAX_CLIENTS; j++) {
                if (seeders[j] == 1) { // daca am gasit un seeder
                    sidx = j;
                    // cerere de chunk
                    sprintf(request, "GET_CHUNK %s %d\n", owned_files->requested_files[i], recv_chunks);
                    MPI_Send(request, 1024, MPI_CHAR, sidx, 1, MPI_COMM_WORLD);
                    char chunk[HASH_SIZE + 1];
                    MPI_Recv(chunk, HASH_SIZE + 1, MPI_CHAR, sidx, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    // verificam daca chunk-ul descarcat este cel corect
                    if (strcmp(chunk, chunks[recv_chunks]) == 0) {
                        strcpy(owned_files->files[owned_files->num_files].chunks[recv_chunks], chunk);
                        recv_chunks++;
                        break;
                    }
                }
                // resetare index seeder
                if (j >= MAX_CLIENTS) {
                    sidx = 0;
                }
            }
            // la fiecare 10 chunk-uri descarcate
            // cer un update de la tracker despre fisier
            if ((recv_chunks + 1) % 10 == 0) {
                sprintf(request, "UPDATE %s\n", owned_files->requested_files[i]);
                MPI_Send(request, 1024, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
                MPI_Recv(&num_chunks, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(seeders, MAX_CLIENTS + 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(peers, MAX_CLIENTS + 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                // resetare cautarea peer si seeder
                pidx = 0;
                sidx = 0;
            }
        }
        // scriere fisier descarcat
        print_downloaded_file(&owned_files->files[owned_files->num_files], owned_files->requested_files[i], owned_files->rank);
        owned_files->num_files++;
        // trimit o cerere ca am terminat de descarcat fisierul
        // trackerul va marca clinetul ca seeder pentru fisier
        // si va demarca clientul ca peer
        sprintf(request, "ADD %s %d/n", owned_files->requested_files[i], owned_files->rank);
        MPI_Send(request, 1024, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
    }
    // trimit o cerere ca am terminat de descarcat toate fisierele
    sprintf(request, "FINISHED %d\n", owned_files->rank);
    MPI_Send(request, 1024, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
    return NULL;
}

void *upload_thread_func(void *arg)
{
    // date despre fisierele detinute de client
    Owned_files *owned_files = (Owned_files*) arg;
    char request[1024];
    while (1) {
        MPI_Status status;
        // astept cerere
        MPI_Recv(request, 1024, MPI_CHAR, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
        if (strncmp(request, "GET_CHUNK", 9) == 0) { // cerere de chunk
            char file_name[MAX_FILENAME];
            int chunk;
            sscanf(request, "GET_CHUNK %s %d\n", file_name, &chunk);
            int idx = 0;
            // cautare fisier
            for (idx = 0; idx < owned_files->num_files; idx++) {
                if (strcmp(owned_files->files[idx].file_name, file_name) == 0) {
                    break;
                }
            }
            // trimitere chunk
            MPI_Send(owned_files->files[idx].chunks[chunk], HASH_SIZE + 1, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
        } else { // cerere de oprire de la tracker
            if (strncmp(request, "STOP", 4) == 0) {
                break;
            }
        }
    }
    return NULL;
}


void tracker(int numtasks, int rank) {
    // date despre fisierele detinute de clienti
    Tracker_file_info tracker_info;
    tracker_info.num_files = 0;
    // initializare date
    for (int j = 0; j < MAX_FILES; j++) {
        for (int i = 0; i <= MAX_CLIENTS; i++) {
            tracker_info.peers[j][i] = 0;
            tracker_info.seeders[j][i] = 0;
        }
    }
    int i = 0;
    // primire date despre fisierele detinute de clienti
    while (i < numtasks - 1) {
        Owned_files client_info;
        MPI_Status status;
        MPI_Recv(&client_info, sizeof(client_info), MPI_BYTE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        for (int j = 0; j < client_info.num_files; j++) {
            int found = 0;
            int idx = 0;
            // cautare fisier
            for (idx = 0; idx < tracker_info.num_files; idx++) {
                if (strcmp(tracker_info.files[idx].file_name, client_info.files[j].file_name) == 0) {
                    found = 1;
                    break;
                }
            }
            // adaugare fisier daca nu exista
            if (!found) {
                strcpy(tracker_info.files[tracker_info.num_files].file_name, client_info.files[j].file_name);
                tracker_info.files[tracker_info.num_files].num_chunks = client_info.files[j].num_chunks;
                for (int k = 0; k < client_info.files[j].num_chunks; k++) {
                    strcpy(tracker_info.files[tracker_info.num_files].chunks[k], client_info.files[j].chunks[k]);
                }
                tracker_info.num_files++;
            }
            tracker_info.seeders[idx][client_info.rank] = 1;
        }
        i++;
    }
    // trimitere mesaj catre clienti ca trackerul a terminat de primt date
    int finished = 1;
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(&finished, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }
    int num_finished = 0; // numarul de clienti care au terminat
    // primire cereri de la clienti
    while (1) {
        char request[1024];
        MPI_Status status;
        MPI_Recv(request, 1024, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        if (strncmp(request, "GET", 3) == 0) { // cerere de informatii despre fisier
            char file_name[MAX_FILENAME];
            sscanf(request, "GET %s\n", file_name);
            int idx = 0;
            // cautare fisier
            for (idx = 0; idx < tracker_info.num_files; idx++) {
                if (strcmp(tracker_info.files[idx].file_name, file_name) == 0) {
                    break;
                }
            }
            // trimitere informatii despre fisier
            MPI_Send(&tracker_info.files[idx].num_chunks, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
            MPI_Send(tracker_info.seeders[idx], MAX_CLIENTS + 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
            MPI_Send(tracker_info.peers[idx], MAX_CLIENTS + 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
            MPI_Send(tracker_info.files[idx].chunks, tracker_info.files[idx].num_chunks * (HASH_SIZE + 1), MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
            // marcare client ca peer
            tracker_info.peers[idx][status.MPI_SOURCE] = 1;
        } else { // un client a terminat de descarcat un fisier
            if (strncmp(request, "ADD", 3) == 0) {
                char file_name[MAX_FILENAME];
                int rank;
                sscanf(request, "ADD %s %d\n", file_name, &rank);
                int idx = 0;
                // cautare fisier
                for (idx = 0; idx < tracker_info.num_files; idx++) {
                    if (strcmp(tracker_info.files[idx].file_name, file_name) == 0) {
                        break;
                    }
                }
                // marcare client ca seeder
                tracker_info.seeders[idx][rank] = 1;
                // demarcare client ca peer
                tracker_info.peers[idx][rank] = 0;
            } else { // un client a terminat de descarcat toate fisierele
                if (strncmp(request, "FINISHED", 8) == 0) {
                    num_finished++;
                    if (num_finished == numtasks - 1) {
                        sprintf(request, "STOP\n");
                        // trimite mesaj de oprire catre toti clientii
                        for (int i = 1; i < numtasks; i++) {
                            MPI_Send(request, 1024, MPI_CHAR, i, 1, MPI_COMM_WORLD);
                        }
                        // oprim trackerul
                        break;
                    }
                } else {
                    if (strncmp(request, "UPDATE", 6) == 0) { // cerere de update
                        char file_name[MAX_FILENAME];
                        sscanf(request, "UPDATE %s\n", file_name);
                        int idx = 0;
                        // cautare fisier
                        for (idx = 0; idx < tracker_info.num_files; idx++) {
                            if (strcmp(tracker_info.files[idx].file_name, file_name) == 0) {
                                break;
                            }
                        }
                        // trimitere informatii despre fisier
                        MPI_Send(&tracker_info.files[idx].num_chunks, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
                        MPI_Send(tracker_info.seeders[idx], MAX_CLIENTS + 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
                        MPI_Send(tracker_info.peers[idx], MAX_CLIENTS + 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
                        
                    }
                }
            }
        }
    }
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;
    Owned_files owned_files;
    owned_files.rank = rank;
    char file_name[MAX_FILENAME];
    sprintf(file_name, "in%d.txt", rank);
    FILE *file = fopen(file_name, "r");
    fscanf(file, "%d", &owned_files.num_files);
    if (file == NULL) {
        printf("Eroare la deschiderea fisierului %s\n", file_name);
        exit(-1);
    }
    // citire date despre fisierele detinute de client
    for (int i = 0; i < owned_files.num_files; i++) {
        fscanf(file, "%s", owned_files.files[i].file_name);
        fscanf(file, "%d", &owned_files.files[i].num_chunks);
        memset(owned_files.files[i].chunks, 0, sizeof(owned_files.files[i].chunks));
        for (int j = 0; j < owned_files.files[i].num_chunks; j++) {
            fscanf(file, "%s", owned_files.files[i].chunks[j]);
        }
    }
    fscanf(file, "%d", &owned_files.num_req_files);
    for (int i = 0; i < owned_files.num_req_files; i++) {
        fscanf(file, "%s", owned_files.requested_files[i]);
    }
    // trimitere date despre fisierele detinute de client catre tracker
    MPI_Send(&owned_files, sizeof(owned_files), MPI_BYTE, TRACKER_RANK, 0, MPI_COMM_WORLD);
    int finished = 0;
    // asteptare mesaj de la tracker ca a terminat de primit date
    MPI_Recv(&finished, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &owned_files);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &owned_files);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
    
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
