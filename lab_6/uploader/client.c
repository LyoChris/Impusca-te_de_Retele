#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <libgen.h>
#include <openssl/evp.h>
#include "share.h"

uint16_t port;

uint32_t calculate_filesize(char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        return 0;
    }
    fseek(file, 0, SEEK_END);
    uint32_t filesize = ftell(file);
    fclose(file);
    return filesize;
}

int sha256_file_path(const char *path, uint8_t out[32]) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(f); return -1; }

    uint8_t buf[8192];
    int ok = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == 1;
    while (ok) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n) ok = EVP_DigestUpdate(ctx, buf, n) == 1;
        if (n < sizeof(buf)) { if (ferror(f)) ok = 0; break; }
    }
    unsigned int dummy = 0;
    ok = ok && (EVP_DigestFinal_ex(ctx, out, &dummy) == 1);

    EVP_MD_CTX_free(ctx);
    fclose(f);
    return ok ? 0 : -1;
}

uint32_t crc32_simple(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (int k = 0; k < 8; k++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320u;
            else         crc = (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

void upload_manager_client(int sock, char line[]) {
    char filepath[512];
    char* p = strstr(line, " ");
    if (!p) {
        printf("[CLIENT]: Folosire: upload <cale_fisier>\n");
        return;
    }
    strcpy(filepath, p + 1);
    filepath[strlen(filepath) - 1] = '\0';

    char *filename = basename(filepath);
    uint32_t filesize = calculate_filesize(filepath);
    if (filesize == 0) {
        printf("[CLIENT]: Eroare la deschiderea fisierului %s\n", filepath);
        return;
    }

    printf("[CLIENT]: Pregatesc upload pentru '%s' (size=%u bytes)\n", filename, filesize);
    fflush(stdout);

    upload_request_t upl_req;
    memset(&upl_req, 0, sizeof(upl_req));
    strncpy(upl_req.filename, filename, 256);
    upl_req.filesize = htonl(filesize);

    printf("[CLIENT]: Calculez SHA256 local...\n");
    fflush(stdout);
    if (sha256_file_path(filepath, upl_req.file_sha256) != 0) {
        printf("[CLIENT]: Eroare la calcularea SHA256 pentru fisierul %s\n", filepath);
        return;
    }
    printf("[CLIENT]: SHA256 calculat.\n");

    upl_req.mode = 0;

    packet_header_t header;
    memset(&header, 0, sizeof(header));
    header.type = UPL_REQ;
    header.length = sizeof(upload_request_t);
    printf("[CLIENT]: Trimit UPL_REQ...\n");
    fflush(stdout);
    if (write(sock, &header, sizeof(packet_header_t)) <= 0) {
        perror("[CLIENT]: Eroare la write() catre server");
        return;
    }
    if (write(sock, &upl_req, sizeof(upload_request_t)) <= 0) {
        perror("[CLIENT]: Eroare la write() catre server");
        return;
    }

    packet_header_t resp_header;
    memset(&resp_header, 0, sizeof(resp_header));
    if (read(sock, &resp_header, sizeof(packet_header_t)) <= 0) {
        perror("[CLIENT]: Eroare la read() de la server");
        return;
    }
    if (resp_header.type != UPL_RES) {
        printf("[CLIENT]: Raspuns invalid de la server (type=%u)\n", (unsigned)resp_header.type);
        return;
    }
    upload_response_t upl_resp;
    memset(&upl_resp, 0, sizeof(upload_response_t));
    if (read(sock, &upl_resp, sizeof(upload_response_t)) <= 0) {
        perror("[CLIENT]: Eroare la read() de la server");
        return;
    }

    if (upl_resp.status != 0) {
        printf("[CLIENT]: Eroare la upload: %s\n", upl_resp.message);
        return;
    }

    uint32_t start_offset = ntohl(upl_resp.offset);
    printf("[CLIENT]: %s (offset=%u)\n", upl_resp.message, start_offset);
    fflush(stdout);

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        perror("[CLIENT]: Eroare la fopen local");
        return;
    }
    if (start_offset > 0) {
        if (fseek(f, (long)start_offset, SEEK_SET) != 0) {
            perror("[CLIENT]: Eroare la fseek local");
            fclose(f);
            return;
        }
        printf("[CLIENT]: Reiau trimiterea de la offset=%u...\n", start_offset);
        fflush(stdout);
    }

    uint32_t cur_offset = start_offset;
    uint32_t chunk_no = 0;

    /* progres: afisam procentul dupa fiecare ACK; o singura linie dinamica */
    printf("[CLIENT]: Upload in curs: 0%%");
    fflush(stdout);

    while (true) {
        upload_chunk_t chunk;
        memset(&chunk, 0, sizeof(chunk));

        size_t n = fread(chunk.data, 1, BUFFER_SIZE, f);
        if (n == 0) {
            if (feof(f)) break;
            perror("\n[CLIENT]: Eroare la fread");
            fclose(f);
            return;
        }

        uint32_t size_host = (uint32_t)n;
        uint32_t crc_host  = crc32_simple(chunk.data, n);

        chunk.chunk_no        = htonl(chunk_no);
        chunk.offset          = htonl(cur_offset);
        chunk.size            = htonl(size_host);
        chunk.crc32_checksum  = htonl(crc_host);

        packet_header_t chh;
        memset(&chh, 0, sizeof(chh));
        chh.type   = UPL_CHUNK;
        chh.length = sizeof(upload_chunk_t);

        if (write(sock, &chh, sizeof(chh)) <= 0) {
            perror("\n[CLIENT]: Eroare la write() header UPL_CHUNK");
            fclose(f);
            return;
        }
        if (write(sock, &chunk, sizeof(chunk)) <= 0) {
            perror("\n[CLIENT]: Eroare la write() body UPL_CHUNK");
            fclose(f);
            return;
        }

        packet_header_t ackh;
        memset(&ackh, 0, sizeof(ackh));
        if (read(sock, &ackh, sizeof(ackh)) <= 0) {
            perror("\n[CLIENT]: Eroare la read() header UPL_ACK");
            fclose(f);
            return;
        }
        if (ackh.type != UPL_ACK || ackh.length != sizeof(upload_ack_t)) {
            printf("\n[CLIENT]: Raspuns ACK invalid (type=%u, len=%u)\n",
                   (unsigned)ackh.type, (unsigned)ackh.length);
            fclose(f);
            return;
        }

        upload_ack_t ackb;
        memset(&ackb, 0, sizeof(ackb));
        if (read(sock, &ackb, sizeof(ackb)) <= 0) {
            perror("\n[CLIENT]: Eroare la read() body UPL_ACK");
            fclose(f);
            return;
        }

        if (ackb.status != 0) {
            uint32_t srv_off = ntohl(ackb.offset);
            /* NACK – retransmitem de la cur_offset */
            if (fseek(f, (long)cur_offset, SEEK_SET) != 0) {
                perror("\n[CLIENT]: Eroare la fseek pentru retransmisie");
                fclose(f);
                return;
            }
            /* nu avansam chunk_no / offset */
            continue;
        }

        /* ACK OK */
        uint32_t next_off = ntohl(ackb.offset);
        cur_offset = next_off;
        chunk_no++;

        /* progress update (o singura linie) */
        unsigned pct = (filesize == 0) ? 100u : (unsigned)((cur_offset * 100ull) / filesize);
        printf("\r[CLIENT]: Upload in curs: %u%%", pct > 100 ? 100 : pct);
        fflush(stdout);
    }

    /* terminare */
    packet_header_t done;
    memset(&done, 0, sizeof(done));
    done.type = UPL_COMPLETE;
    done.length = 0;

    if (write(sock, &done, sizeof(done)) <= 0) {
        perror("\n[CLIENT]: Eroare la write() UPL_COMPLETE");
        fclose(f);
        return;
    }

    packet_header_t fin_h;
    memset(&fin_h, 0, sizeof(fin_h));
    if (read(sock, &fin_h, sizeof(fin_h)) <= 0) {
        perror("\n[CLIENT]: Eroare la read() header final");
        fclose(f);
        return;
    }
    if (fin_h.type != UPL_RES || fin_h.length != sizeof(upload_response_t)) {
        printf("\n[CLIENT]: Header final invalid (type=%u, len=%u)\n",
               (unsigned)fin_h.type, (unsigned)fin_h.length);
        fclose(f);
        return;
    }
    upload_response_t fin_b;
    memset(&fin_b, 0, sizeof(fin_b));
    if (read(sock, &fin_b, sizeof(fin_b)) <= 0) {
        perror("\n[CLIENT]: Eroare la read() body final");
        fclose(f);
        return;
    }
    fclose(f);

    /* curatare linie progres + rezultat final */
    printf("\r[CLIENT]: Upload in curs: 100%%\n");
    if (fin_b.status != 0) {
        printf("[CLIENT]: Upload finalizat cu eroare: %s\n", fin_b.message);
        return;
    }
    printf("[CLIENT]: Upload finalizat cu succes: %s\n", fin_b.message);
}

void download_manager_client(int sock, char line[]) {
    char reqname[256];
    char *p = strstr(line, " ");
    if (!p) {
        printf("[CLIENT]: Folosire: download <nume_fisier>\n");
        return;
    }

    strncpy(reqname, p + 1, sizeof(reqname)-1);
    reqname[sizeof(reqname)-1] = '\0';
    reqname[strcspn(reqname, "\r\n")] = '\0';

    if (reqname[0] == '\0') {
        printf("[CLIENT]: Nume fisier lipsa.\n");
        return;
    }


    download_request_t dreq;
    memset(&dreq, 0, sizeof(dreq));
    strncpy(dreq.filename, reqname, sizeof(dreq.filename)-1);

    packet_header_t h;
    memset(&h, 0, sizeof(h));
    h.type   = DWL_REQ;
    h.length = sizeof(dreq);

    printf("[CLIENT]: Cer descarcarea '%s'...\n", dreq.filename);
    fflush(stdout);

    if (write(sock, &h, sizeof(h)) <= 0) {
        perror("[CLIENT]: Eroare la write() header DWL_REQ");
        return;
    }
    if (write(sock, &dreq, sizeof(dreq)) <= 0) {
        perror("[CLIENT]: Eroare la write() body DWL_REQ");
        return;
    }


    packet_header_t rh;
    memset(&rh, 0, sizeof(rh));
    if (read(sock, &rh, sizeof(rh)) <= 0) {
        perror("[CLIENT]: Eroare la read() header DWL_RES init");
        return;
    }
    if (rh.type != DWL_RES || rh.length != sizeof(download_response_t)) {
        printf("[CLIENT]: Raspuns initial invalid (type=%u, len=%u)\n",
               (unsigned)rh.type, (unsigned)rh.length);
        return;
    }

    download_response_t rbody;
    memset(&rbody, 0, sizeof(rbody));
    if (read(sock, &rbody, sizeof(rbody)) <= 0) {
        perror("[CLIENT]: Eroare la read() body DWL_RES init");
        return;
    }
    if (rbody.status != 0) {
        printf("[CLIENT]: Server a refuzat download-ul: %s\n", rbody.message);
        return;
    }

    uint32_t filesize = ntohl(rbody.filesize);
    printf("[CLIENT]: Server OK. Fisierul are %u bytes. Incep download-ul...\n", filesize);
    fflush(stdout);


    char partpath[512];
    snprintf(partpath, sizeof(partpath), "%s.part", reqname);
    FILE *f = fopen(partpath, "wb");
    if (!f) {
        perror("[CLIENT]: Eroare la fopen .part local");
        return;
    }

    
    printf("[CLIENT]: Download in curs: 0%%");
    fflush(stdout);

    uint32_t cur_offset = 0;

    
    while (true) {
        packet_header_t ch;
        memset(&ch, 0, sizeof(ch));
        if (read(sock, &ch, sizeof(ch)) <= 0) {
            perror("\n[CLIENT]: Eroare la read() header chunk/final");
            fclose(f);
            return;
        }

        if (ch.type == DWL_CHUNK) {
            if (ch.length != sizeof(download_chunk_t)) {
                printf("\n[CLIENT]: Dimensiune DWL_CHUNK neasteptata: %u\n", (unsigned)ch.length);
                fclose(f);
                return;
            }

            download_chunk_t cb;
            memset(&cb, 0, sizeof(cb));
            if (read(sock, &cb, sizeof(cb)) <= 0) {
                perror("\n[CLIENT]: Eroare la read() body DWL_CHUNK");
                fclose(f);
                return;
            }

            uint32_t chunk_no_host = ntohl(cb.chunk_no);
            uint32_t offset_host   = ntohl(cb.offset);
            uint32_t size_host     = ntohl(cb.size);
            uint32_t crc_host      = ntohl(cb.crc32_checksum);

            
            uint32_t crc_calc = crc32_simple(cb.data, (size_t)size_host);
            download_ack_t ack;
            memset(&ack, 0, sizeof(ack));

            if (crc_calc != crc_host) {
                ack.status   = 1;
                ack.chunk_no = htonl(chunk_no_host);
                ack.offset   = htonl(cur_offset);

                packet_header_t ah; memset(&ah, 0, sizeof(ah));
                ah.type   = DWL_ACK;
                ah.length = sizeof(ack);
                if(write(sock, &ah, sizeof(ah)) <= 0) {
                    perror("\n[CLIENT]: Eroare la write() DWL_ACK NACK");
                    fclose(f);
                    return;
                }
                if(write(sock, &ack, sizeof(ack)) <= 0) {
                    perror("\n[CLIENT]: Eroare la write() DWL_ACK NACK");
                    fclose(f);
                    return;
                }
                continue;
            }

            
            if (fseek(f, (long)offset_host, SEEK_SET) != 0) {
                perror("\n[CLIENT]: Eroare la fseek() local .part");
                fclose(f);
                return;
            }
            size_t wn = fwrite(cb.data, 1, (size_t)size_host, f);
            if (wn != (size_t)size_host) {
                perror("\n[CLIENT]: Eroare la fwrite() local .part");
                fclose(f);
                return;
            }

            cur_offset = offset_host + size_host;

            
            ack.status   = 0;
            ack.chunk_no = htonl(chunk_no_host);
            ack.offset   = htonl(cur_offset);

            packet_header_t ah; memset(&ah, 0, sizeof(ah));
            ah.type   = DWL_ACK;
            ah.length = sizeof(ack);
            if(write(sock, &ah, sizeof(ah)) <= 0) {
                perror("\n[CLIENT]: Eroare la write() DWL_ACK");
                fclose(f);
                return;
            }
            if(write(sock, &ack, sizeof(ack)) <= 0) {
                perror("\n[CLIENT]: Eroare la write() DWL_ACK");
                fclose(f);
                return;
            }

            
            unsigned pct = (filesize == 0) ? 100u : (unsigned)((cur_offset * 100ull) / filesize);
            printf("\r[CLIENT]: Download in curs: %u%%", pct > 100 ? 100 : pct);
            fflush(stdout);
            continue;
        }

        
        if (ch.type == DWL_RES && ch.length == sizeof(download_response_t)) {
            download_response_t fin;
            memset(&fin, 0, sizeof(fin));
            if (read(sock, &fin, sizeof(fin)) <= 0) {
                perror("\n[CLIENT]: Eroare la read() body DWL_RES final");
                fclose(f);
                return;
            }
            fclose(f);

            
            printf("\r[CLIENT]: Download in curs: 100%%\n");

            if (fin.status != 0) {
                printf("[CLIENT]: Download finalizat cu eroare: %s\n", fin.message);
                return;
            }

            
            uint32_t srv_filesize = ntohl(fin.filesize);
            if (cur_offset != srv_filesize) {
                printf("[CLIENT]: Size mismatch: got %u, expected %u\n",
                       cur_offset, srv_filesize);
                return;
            }

            
            uint8_t local_sha[32];
            if (sha256_file_path(partpath, local_sha) != 0) {
                printf("[CLIENT]: Atentie: nu am putut calcula SHA256 local.\n");
            } else {
                if (memcmp(local_sha, fin.file_sha256, 32) != 0) {
                    printf("[CLIENT]: SHA256 mismatch intre server si fisierul descarcat.\n");
                    return;
                }
            }

            
            if (rename(partpath, reqname) != 0) {
                perror("[CLIENT]: Eroare la rename() .part -> final");
                return;
            }

            printf("[CLIENT]: Download finalizat cu succes: %s\n", fin.message);
            return;
        }

        printf("\n[CLIENT]: Pachet neasteptat in timpul download-ului (type=%u)\n", (unsigned)ch.type);
        fclose(f);
        return;
    }
}

void list_manager_client(int sock) {
    packet_header_t h;
    memset(&h, 0, sizeof(h));
    h.type = LIST_REQ;
    h.length = 0;

    if (write(sock, &h, sizeof(h)) <= 0) {
        perror("[CLIENT]: Eroare la write() LIST");
        return;
    }

    packet_header_t rh;
    memset(&rh, 0, sizeof(rh));
    if (read(sock, &rh, sizeof(rh)) <= 0) {
        perror("[CLIENT]: Eroare la read() header LIST_RES");
        return;
    }
    if (rh.type != LIST_RES) {
        printf("[CLIENT]: Raspuns LIST invalid (type=%u, len=%u)\n",
               (unsigned)rh.type, (unsigned)rh.length);
        return;
    }

    list_response_t body;
    memset(&body, 0, sizeof(body));
    if (read(sock, &body, sizeof(body)) <= 0) {
        perror("[CLIENT]: Eroare la read() body LIST_RES");
        return;
    }

    if (body.status != 0) {
        printf("[CLIENT]: LIST esuat: %s\n", body.message);
        return;
    }

    unsigned count = ntohl(body.count);
    printf("[CLIENT]: Fisiere pe server (%u):\n%s", count, body.message);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    
    int sock;

    printf("[CLIENT]: Pornim clientul...\n");

    if(argc != 3) {
        printf("USAGE: %s <server_adress <port>\n", argv[0]);
        return -1;
    }

    port = atoi(argv[2]);

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[CLIENT]: Eroare la socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(port);

    printf("[CLIENT]: Incercam conectarea la server...\n");

    if(connect(sock, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
        perror("[CLIENT]: Eroare la connect");
        return 2;
    }

    printf("[CLIENT]: Conectat cu succes la server...\n");
    printf("[CLIENT]: Plese login or create an account.\n");


    current_user user;
    user.is_logged_in = false;
    user.username[0] = '\0';
    char username[64];
    char *line = NULL;
    size_t max_size = 0;
    ssize_t n;

    while(true) {
        if(user.is_logged_in){
            printf("[%s]: ", username);
            fflush(stdout);
        }
        else{
            printf("[CLIENT]: ");
            fflush(stdout);
        }
        if((n = getline(&line, &max_size, stdin)) <= 0)
        {
            free(line);
            continue;
        }

        if(strncmp(line, "login", 5) == 0) {
            char password[64];

            printf("Username: ");
            fflush(stdout);
            if((n = getline(&line, &max_size, stdin)) <= 0) {
                free(line);
                continue;
            }
            line[strcspn(line, "\n")] = 0; 
            strncpy(username, line, 64);

            printf("Password: ");
            fflush(stdout);
            if((n = getline(&line, &max_size, stdin)) <= 0) {
                free(line);
                continue;
            }
            line[strcspn(line, "\n")] = 0; 
            strncpy(password, line, 64);

            packet_header_t header;
            memset(&header, 0, sizeof(header));
            header.type = LOGIN_REQ;
            header.length = sizeof(login_request_t);
            if (write(sock, &header, sizeof(packet_header_t)) <= 0) {
                perror("[CLIENT]: Eroare la write() catre server");
                continue;
            }

            login_request_t login_req;
            memset(&login_req, 0, sizeof(login_req));
            strncpy(login_req.username, username, 64);
            strncpy(login_req.password, password, 64);
            login_req.create_account = 0;

            if (write(sock, &login_req, sizeof(login_request_t)) <= 0) {
                perror("[CLIENT]: Eroare la write() catre server");
                continue;
            }
        }
        else if(strncmp(line, "create", 6) == 0) {
            char password[64];

            printf("Choose a username: ");
            fflush(stdout);
            if((n = getline(&line, &max_size, stdin)) <= 0) {
                free(line);
                continue;
            }
            line[strcspn(line, "\n")] = 0; 
            strncpy(username, line, 64);

            printf("Choose a password: ");
            fflush(stdout);
            if((n = getline(&line, &max_size, stdin)) <= 0) {
                free(line);
                continue;
            }
            line[strcspn(line, "\n")] = 0; 
            strncpy(password, line, 64);

            packet_header_t header;
            memset(&header, 0, sizeof(header));
            header.type = LOGIN_REQ;
            header.length = sizeof(login_request_t);
            if (write(sock, &header, sizeof(packet_header_t)) <= 0) {
                perror("[CLIENT]: Eroare la write() catre server");
                continue;
            }

            login_request_t login_req;
            memset(&login_req, 0, sizeof(login_req));
            strncpy(login_req.username, username, 64);
            strncpy(login_req.password, password, 64);
            login_req.create_account = 1;

            if (write(sock, &login_req, sizeof(login_request_t)) <= 0) {
                perror("[CLIENT]: Eroare la write() catre server");
                continue;
            }
        }
        else if(strncmp(line, "upload", 6) == 0) {
            upload_manager_client(sock, line);
            continue;
        }
        else if(strncmp(line, "download", 8) == 0) {
            download_manager_client(sock, line);
            continue;
        }
        else if (strncmp(line, "quit", 4) == 0 || strncmp(line, "exit", 4) == 0) {
            packet_header_t h;
            memset(&h, 0, sizeof(h));
            h.type   = EXIT_REQ;
            h.length = 0;
            if( write(sock, &h, sizeof(h)) <= 0) {
                perror("[CLIENT]: Eroare la write() EXIT_REQ");
            }
            close(sock);
            return 0;
        }
        else if (strncmp(line, "logout", 6) == 0) {
            packet_header_t h;
            h.type = LOGOUT_REQ;
            h.length = 0;
            if (write(sock, &h, sizeof(h)) <= 0) {
                perror("[CLIENT]: Eroare la write() LOGOUT_REQ");
                continue;
            }

            packet_header_t rh = {0};
            if (read(sock, &rh, sizeof(rh)) <= 0) {
                perror("[CLIENT]: Eroare la read() header LOGOUT reply");
                continue;
            }
            if (rh.type != LOGIN_RES) {
                printf("[CLIENT]: Raspuns LOGOUT invalid (type=%u, len=%u)\n",
                (unsigned)rh.type, (unsigned)rh.length);
                continue;
            }

            login_response_t body = {0};
            if (read(sock, &body, sizeof(body)) <= 0) {
                perror("[CLIENT]: Eroare la read() body LOGOUT reply");
                continue;
            }

            printf("[SERVER]: %s\n", body.message);
            if (body.status == 0) {
                user.is_logged_in = false;
                user.username[0] = '\0';
            }
            continue;
        }
        else if (strncmp(line, "list", 4) == 0) {
            list_manager_client(sock);
            continue;
        }
        else {
            printf("[CLIENT]: Comanda necunoscuta. Va rugam folositi 'login' sau 'create'.\n");

        }

        packet_header_t header;
        memset(&header, 0, sizeof(header));
        if (read(sock, &header, sizeof(packet_header_t)) <= 0) {
            perror("[CLIENT]: Eroare la read() de la server");
            continue;
        }

        if(header.type == LOGIN_RES) {
            login_response_t login_rep;
            memset(&login_rep, 0, sizeof(login_rep));
            if (read(sock, &login_rep, sizeof(login_response_t)) <= 0) {
                perror("[CLIENT]: Eroare la read() de la server");
                continue;
            }
            if(login_rep.status == 0)
                user.is_logged_in = true;

            printf("[SERVER]: %s\n", login_rep.message);
        }
    }
    return 0;
}