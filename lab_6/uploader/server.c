#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h> 
#include <sys/resource.h>
#include <ctype.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <limits.h>
#include <fnmatch.h>
#include <dirent.h>
#include <time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <netinet/in.h>
#include <signal.h>
#include <openssl/evp.h>
#include "share.h"

#define PORT 4088

#define STORAGE "./storage/"

void handle_sig_child(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

char* is_logged(char *username, char *password){
    FILE* file = fopen("users.txt", "r");
    if (file == NULL) {
        return "Error opening users file.";
    }
    char line[128];
    while (fgets(line, sizeof(line), file)) {
        char file_username[64], file_password[64];
        if (sscanf(line, "%63s %63s", file_username, file_password) == 2) {
            if (strcmp(username, file_username) == 0 && strcmp(password, file_password) == 0) {
                fclose(file);
                return "Login successful.";
            }
        }
    }

    return "Invalid username or password.";
}

char* create_account(char *username, char *password){
    FILE* file = fopen("users.txt", "a+");
    if (file == NULL) {
        return "Error opening users file.";
    }
    fprintf(file, "%s %s\n", username, password);
    fclose(file);
    return "Account created successfully.";
}

void login_manager(int fd, current_user* user) {
        login_request_t login_req;
        memset(&login_req, 0, sizeof(login_req));
        if (read(fd, &login_req, sizeof(login_request_t)) <= 0) {
            perror("Eroare la read() de la client 0");
            return;
        }

        login_response_t login_rep;
        memset(&login_rep, 0, sizeof(login_rep));
        if(user->is_logged_in == true) {
            login_rep.status = 1;
            snprintf(login_rep.message, BUFFER_SIZE, "User %s is already logged in.", user->username);

            packet_header_t header;
            memset(&header, 0, sizeof(header));
            header.type = LOGIN_RES;
            header.length = sizeof(login_response_t);
            if (write(fd, &header, sizeof(packet_header_t)) <= 0) {
                perror("Eroare la write() catre client 1");
                return;
            }

            if (write(fd, &login_rep, sizeof(login_response_t)) <= 0) {
                perror("Eroare la write() catre client 2");
                return;
            }
            return;
        }
        if (login_req.create_account) {
            char *msg = create_account(login_req.username, login_req.password);

            if (strcmp(msg, "Account created successfully.") == 0) {
                login_rep.status = 0;
                snprintf(login_rep.message, BUFFER_SIZE, "Welcome %s!", login_req.username);

                user->is_logged_in = true;
                strncpy(user->username, login_req.username, sizeof(user->username));
                user->username[sizeof(user->username)-1] = '\0';
                printf("[SERVER] User %s s-a logat cu succes.\n", user->username);
            } else {
                login_rep.status = 1;
                snprintf(login_rep.message, BUFFER_SIZE, "%s", msg);
                user->is_logged_in = false;  // stay in loop
            }
            if (write(fd, &login_rep, sizeof(login_response_t)) <= 0) {
                perror("Eroare la write() catre client 1");
                return;
            }
            return;
        } else {
            char *msg = is_logged(login_req.username, login_req.password);

            if (strcmp(msg, "Login successful.") == 0) {
                login_rep.status = 0;
                snprintf(login_rep.message, BUFFER_SIZE, "Welcome %s!", login_req.username);

                user->is_logged_in = true;
                strncpy(user->username, login_req.username, sizeof(user->username));
                user->username[sizeof(user->username)-1] = '\0';
                printf("[SERVER] User %s s-a logat cu succes.\n", user->username);
            } else {
                login_rep.status = 1;
                snprintf(login_rep.message, BUFFER_SIZE, "%s", msg);
                user->is_logged_in = false;  // stay in loop
            }

            packet_header_t header;
            memset(&header, 0, sizeof(header));
            header.type = LOGIN_RES;
            header.length = sizeof(login_response_t);
            if (write(fd, &header, sizeof(packet_header_t)) <= 0) {
                perror("Eroare la write() catre client 2");
                return;
            }

            if (write(fd, &login_rep, sizeof(login_response_t)) <= 0) {
                perror("Eroare la write() catre client 0");
                return;
            }
        }
}

bool file_exists_partial(const char *filename, current_user* user, uint32_t *existing_size) {
    char filepath[512] = STORAGE;
    strcat(filepath, user->username);
    strcat(filepath, "/");
    strcat(filepath, filename);
    strcat(filepath, ".part");
    FILE *file = fopen(filepath, "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        *existing_size = ftell(file);
        fclose(file);
        return true;
    }
    return false;
}

bool file_exists(const char *filename, current_user* user) {
    char filepath[512] = STORAGE;
    strcat(filepath, user->username);
    strcat(filepath, "/");
    strcat(filepath, filename);
    FILE *file = fopen(filepath, "rb");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
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

void upload_manager_server(int fd, current_user* user) {
    upload_request_t upload_req;
    memset(&upload_req, 0, sizeof(upload_req));
    if (read(fd, &upload_req, sizeof(upload_request_t)) <= 0) {
        perror("[SERVER]: Eroare la read() UPL_REQ");
        return;
    }

    if (user->is_logged_in == false) {
        printf("[SERVER]: Upload respins – client neautentificat.\n");
        upload_response_t upload_rep;
        memset(&upload_rep, 0, sizeof(upload_rep));
        upload_rep.status = 1;
        upload_rep.code = 1;
        upload_rep.offset = 0;
        snprintf(upload_rep.message, BUFFER_SIZE, "Please log in first.");

        packet_header_t header;
        memset(&header, 0, sizeof(header));
        header.type = UPL_RES;
        header.length = sizeof(upload_response_t);

        if(write(fd, &header, sizeof(packet_header_t)) <= 0) {
            perror("[SERVER]: Eroare la write() catre client");
            return;
        }
        if(write(fd, &upload_rep, sizeof(upload_response_t)) <= 0) {
            perror("[SERVER]: Eroare la write() catre client");
        }
        return;
    }

    /* host order pentru filesize */
    upload_req.filesize = ntohl(upload_req.filesize);
    printf("[SERVER]: UPL_REQ de la '%s' pentru fisier '%s' (size=%u)\n", user->username, upload_req.filename, upload_req.filesize);

    uint32_t existing_size = 0;
    bool part_exists = file_exists_partial(upload_req.filename, user, &existing_size);

    char userdir[640];
    mkdir(STORAGE, 0775);
    snprintf(userdir, sizeof(userdir), "%s%s", STORAGE, user->username);
    mkdir(userdir, 0775);

    char filepath_part[768];
    snprintf(filepath_part, sizeof(filepath_part), "%s/%s.part", userdir, upload_req.filename);

    FILE *part = NULL;
    uint32_t start_offset = 0;
    int open_ok = 0;

    if (part_exists) {
        if (existing_size > upload_req.filesize) {
            printf("[SERVER]: .part mai mare decat fisierul final estimat (%u > %u) -> reset.\n",
                   existing_size, upload_req.filesize);
            part = fopen(filepath_part, "wb");
            if (part) { start_offset = 0; open_ok = 1; }
        } else {
            part = fopen(filepath_part, "r+b");
            if (part) {
                if (fseek(part, (long)existing_size, SEEK_SET) == 0) {
                    start_offset = existing_size;
                    open_ok = 1;
                    printf("[SERVER]: Reiau upload din .part: offset=%u\n", start_offset);
                } else {
                    fclose(part);
                    part = fopen(filepath_part, "wb");
                    if (part) { start_offset = 0; open_ok = 1; }
                }
            }
        }
    } else {
        part = fopen(filepath_part, "wb");
        if (part) { start_offset = 0; open_ok = 1; printf("[SERVER]: Incep upload nou.\n"); }
    }

    if (!open_ok) {
        perror("[SERVER]: Eroare la deschiderea .part");
        upload_response_t upload_rep;
        memset(&upload_rep, 0, sizeof(upload_rep));
        upload_rep.status = 1;
        upload_rep.code = 5; /* io_error */
        upload_rep.offset = htonl(0);
        snprintf(upload_rep.message, BUFFER_SIZE, "Server I/O error opening staging file.");

        packet_header_t header;
        memset(&header, 0, sizeof(header));
        header.type = UPL_RES;
        header.length = sizeof(upload_response_t);

        if(write(fd, &header, sizeof(packet_header_t)) <= 0) {
            perror("[SERVER]: Eroare la write() catre client");
            return;
        }
        if(write(fd, &upload_rep, sizeof(upload_response_t)) <= 0) {
            perror("[SERVER]: Eroare la write() catre client");
        }
        return;
    }

    upload_response_t upload_rep;
    memset(&upload_rep, 0, sizeof(upload_rep));
    upload_rep.status = 0;
    upload_rep.code = 0;
    upload_rep.offset = htonl(start_offset);
    snprintf(upload_rep.message, BUFFER_SIZE, "Upload can proceed from offset %u.", start_offset);

    packet_header_t header;
    memset(&header, 0, sizeof(header));
    header.type = UPL_RES;
    header.length = sizeof(upload_response_t);

    if(write(fd, &header, sizeof(packet_header_t)) <= 0) {
        perror("[SERVER]: Eroare la write() catre client");
        return;
    }
    if(write(fd, &upload_rep, sizeof(upload_response_t)) <= 0) {
        perror("[SERVER]: Eroare la write() catre client");
    }

    char finalpath[1024];
    snprintf(finalpath, sizeof(finalpath), "%s/%s", userdir, upload_req.filename);

    uint32_t cur_offset = start_offset;

    while (true) {
        packet_header_t chunk_header;
        memset(&chunk_header, 0, sizeof(chunk_header));
        if (read(fd, &chunk_header, sizeof(packet_header_t)) <= 0) {
            perror("[SERVER]: Eroare la read() header chunk/complete");
            break;
        }

        if (chunk_header.type == UPL_COMPLETE) {
            fflush(part);

            if (cur_offset != upload_req.filesize) {
                upload_response_t fin_err;
                memset(&fin_err, 0, sizeof(fin_err));
                fin_err.status = 1;
                fin_err.code = 6; /* size_mismatch */
                fin_err.offset = htonl(cur_offset);
                snprintf(fin_err.message, BUFFER_SIZE,
                         "Size mismatch: got %u, expected %u.", cur_offset, upload_req.filesize);

                packet_header_t fh;
                memset(&fh, 0, sizeof(fh));
                fh.type = UPL_RES;
                fh.length = sizeof(upload_response_t);

                if(write(fd, &fh, sizeof(packet_header_t)) <= 0) {
                    perror("[SERVER]: Eroare la write() catre client");
                    return;
                }
                if(write(fd, &fin_err, sizeof(upload_response_t)) <= 0) {
                    perror("[SERVER]: Eroare la write() catre client");
                }
                fclose(part);
                printf("[SERVER]: Upload esuat: size mismatch.\n");
                return;
            }

            fclose(part);
            printf("[SERVER]: Upload complet (%u bytes). Publicare...\n", cur_offset);

            if (rename(filepath_part, finalpath) != 0) {
                perror("[SERVER]: Eroare la rename() final (publish)");
                upload_response_t fin_err;
                memset(&fin_err, 0, sizeof(fin_err));
                fin_err.status = 1;
                fin_err.code = 5; /* io_error */
                fin_err.offset = htonl(cur_offset);
                snprintf(fin_err.message, BUFFER_SIZE, "Server I/O error publishing file.");

                packet_header_t fh;
                memset(&fh, 0, sizeof(fh));
                fh.type = UPL_RES;
                fh.length = sizeof(upload_response_t);

                if(write(fd, &fh, sizeof(packet_header_t)) <= 0) {
                    perror("[SERVER]: Eroare la write() catre client");
                    return;
                }
                if(write(fd, &fin_err, sizeof(upload_response_t)) <= 0) {
                    perror("[SERVER]: Eroare la write() catre client");
                }
                return;
            }

            upload_response_t fin_ok;
            memset(&fin_ok, 0, sizeof(fin_ok));
            fin_ok.status = 0;
            fin_ok.code = 0;
            fin_ok.offset = htonl(cur_offset);
            snprintf(fin_ok.message, BUFFER_SIZE, "Upload complete.");

            packet_header_t fh;
            memset(&fh, 0, sizeof(fh));
            fh.type = UPL_RES;
            fh.length = sizeof(upload_response_t);

            if (write(fd, &fh, sizeof(fh)) <= 0) {
                perror("[SERVER]: Eroare la write() header final");
                return;
            }
            if (write(fd, &fin_ok, sizeof(fin_ok)) <= 0) {
                perror("[SERVER]: Eroare la write() body final");
                return;
            }
            printf("[SERVER]: Publicare reusita: %s\n", finalpath);
            return;
        }

        if (chunk_header.type != UPL_CHUNK) {
            fprintf(stderr, "[SERVER]: Expected UPL_CHUNK, got %d\n", chunk_header.type);
            break;
        }

        upload_chunk_t upload_chunk;
        memset(&upload_chunk, 0, sizeof(upload_chunk));
        if (read(fd, &upload_chunk, sizeof(upload_chunk_t)) <= 0) {
            perror("[SERVER]: Eroare la read() UPL_CHUNK body");
            break;
        }

        uint32_t chunk_no_host = ntohl(upload_chunk.chunk_no);
        uint32_t offset_host   = ntohl(upload_chunk.offset);
        uint32_t size_host     = ntohl(upload_chunk.size);
        uint32_t crc_host      = ntohl(upload_chunk.crc32_checksum);

        /* CRC check */
        uint32_t crc_calc = crc32_simple(upload_chunk.data, (size_t)size_host);
        if (crc_calc != crc_host) {
            printf("[SERVER]: CRC NACK pe chunk=%u (recv_crc=%08X calc_crc=%08X) offset_curent=%u\n",
                   chunk_no_host, crc_host, crc_calc, cur_offset);

            upload_ack_t nack;
            memset(&nack, 0, sizeof(nack));
            nack.status   = 1;
            nack.chunk_no = htonl(chunk_no_host);
            nack.offset   = htonl(cur_offset);

            packet_header_t ack_header;
            memset(&ack_header, 0, sizeof(ack_header));
            ack_header.type   = UPL_ACK;
            ack_header.length = sizeof(upload_ack_t);

            if(write(fd, &ack_header, sizeof(packet_header_t)) <= 0) {
                perror("[SERVER]: Eroare la write() catre client");
                return;
            }
            if(write(fd, &nack, sizeof(upload_ack_t)) <= 0) {
                perror("[SERVER]: Eroare la write() catre client");
            }
            continue;
        }

        if (fseek(part, (long)offset_host, SEEK_SET) != 0) {
            perror("[SERVER]: Eroare la fseek() .part");
            break;
        }

        size_t written = fwrite(upload_chunk.data, 1, (size_t)size_host, part);
        if (written != (size_t)size_host) {
            perror("[SERVER]: Eroare la fwrite() .part");
            break;
        }

        cur_offset = offset_host + size_host;

        /* log “succinct” per chunk */
        printf("[SERVER]: Chunk OK: #%u offset=%u size=%u\n",
               chunk_no_host, offset_host, size_host);

        upload_ack_t upload_ack;
        memset(&upload_ack, 0, sizeof(upload_ack_t));
        upload_ack.status   = 0;
        upload_ack.chunk_no = htonl(chunk_no_host);
        upload_ack.offset   = htonl(cur_offset);

        packet_header_t ack_header;
        memset(&ack_header, 0, sizeof(ack_header));
        ack_header.type   = UPL_ACK;
        ack_header.length = sizeof(upload_ack_t);

        if(write(fd, &ack_header, sizeof(packet_header_t)) <= 0) {
            perror("[SERVER]: Eroare la write() catre client");
            return;
        }
        if(write(fd, &upload_ack, sizeof(upload_ack_t)) <= 0) {
            perror("[SERVER]: Eroare la write() catre client");
        }
    }

    if (part) fclose(part);
    printf("[SERVER]: Upload intrerupt/invalidat. .part pastrat pentru resume.\n");
}

void download_manager_server(int fd, current_user* user) {
    download_request_t req;
    memset(&req, 0, sizeof(req));
    if (read(fd, &req, sizeof(req)) <= 0) {
        perror("[SERVER]: Eroare la read() DWL_REQ");
        return;
    }

    if (user->is_logged_in == false) {
        printf("[SERVER]: Download respins: client neautentificat.\n");
        download_response_t rep;
        memset(&rep, 0, sizeof(rep));
        rep.status = 1;
        rep.code   = 1;
        rep.filesize = htonl(0);
        rep.offset   = htonl(0);
        snprintf(rep.message, BUFFER_SIZE, "Please log in first.");

        packet_header_t h; memset(&h, 0, sizeof(h));
        h.type   = DWL_RES;
        h.length = sizeof(rep);
        if(write(fd, &h, sizeof(h)) <= 0) {
            perror("[SERVER]: Eroare la write() header DWL_RES");
        }
        if(write(fd, &rep, sizeof(rep)) <= 0) {
            perror("[SERVER]: Eroare la write() body DWL_RES");
        }
        return;
    }

    char userdir[640];
    snprintf(userdir, sizeof(userdir), "%s%s", STORAGE, user->username);

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", userdir, req.filename);

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        perror("[SERVER]: fopen download fisier");
        download_response_t rep;
        memset(&rep, 0, sizeof(rep));
        rep.status = 1;
        rep.code   = 2;
        rep.filesize = htonl(0);
        rep.offset   = htonl(0);
        snprintf(rep.message, BUFFER_SIZE, "File not found: %s", req.filename);

        packet_header_t h; 
        memset(&h, 0, sizeof(h));
        h.type   = DWL_RES;
        h.length = sizeof(rep);
        if(write(fd, &h, sizeof(h)) <= 0) {
            perror("[SERVER]: Eroare la write() header DWL_RES");
        }
        if(write(fd, &rep, sizeof(rep)) <= 0) {
            perror("[SERVER]: Eroare la write() body DWL_RES");
        }
        return;
    }

    fseek(f, 0, SEEK_END);
    long fszl = ftell(f);
    if (fszl < 0) fszl = 0;
    fseek(f, 0, SEEK_SET);
    uint32_t filesize = (uint32_t)fszl;


    uint8_t file_sha256[32] = {0};
    if (sha256_file_path(filepath, file_sha256) != 0) {
        memset(file_sha256, 0, 32);
        printf("[SERVER]: Atentie: SHA256 nu a putut fi calculat pentru %s (continui oricum)\n", req.filename);
    }


    download_response_t rep;
    memset(&rep, 0, sizeof(rep));
    rep.status   = 0;
    rep.code     = 0;
    rep.filesize = htonl(filesize);
    rep.offset   = htonl(0);
    memcpy(rep.file_sha256, file_sha256, 32);
    snprintf(rep.message, BUFFER_SIZE, "Download will start from offset 0.");

    packet_header_t h; memset(&h, 0, sizeof(h));
    h.type   = DWL_RES;
    h.length = sizeof(rep);
    if(write(fd, &h, sizeof(packet_header_t)) <= 0) {
        perror("[SERVER]: Eroare la write() catre client");
            return;
    }
    if(write(fd, &rep, sizeof(download_response_t)) <= 0) {
        perror("[SERVER]: Eroare la write() catre client");
    }

    printf("[SERVER]: DWL_REQ pentru '%s' (size=%u) -> incep trimiterea.\n", req.filename, filesize);

    uint32_t offset = 0;
    uint32_t chunk_no = 0;

    while (true) {
        download_chunk_t chunk;
        memset(&chunk, 0, sizeof(chunk));

        size_t n = fread(chunk.data, 1, BUFFER_SIZE, f);
        if (n == 0) {
            if (!feof(f)) {
                perror("[SERVER]: Eroare la fread() in download");
                download_response_t fin_err;
                memset(&fin_err, 0, sizeof(fin_err));
                fin_err.status = 1;
                fin_err.code   = 5; /* io_error */
                fin_err.filesize = htonl(filesize);
                fin_err.offset   = htonl(offset);
                memcpy(fin_err.file_sha256, file_sha256, 32);
                snprintf(fin_err.message, BUFFER_SIZE, "Server I/O error during download.");

                packet_header_t fh; memset(&fh, 0, sizeof(fh));
                fh.type   = DWL_RES;
                fh.length = sizeof(fin_err);
                if(write(fd, &fh, sizeof(packet_header_t)) <= 0) {
                    perror("[SERVER]: Eroare la write() catre client");
                    return;
                }
                if(write(fd, &fin_err, sizeof(download_response_t)) <= 0) {
                    perror("[SERVER]: Eroare la write() catre client");
                }
            }
            break;
        }

        uint32_t size_host = (uint32_t)n;
        uint32_t crc_host  = crc32_simple(chunk.data, n);

        chunk.chunk_no        = htonl(chunk_no);
        chunk.offset          = htonl(offset);
        chunk.size            = htonl(size_host);
        chunk.crc32_checksum  = htonl(crc_host);

        packet_header_t ch; memset(&ch, 0, sizeof(ch));
        ch.type   = DWL_CHUNK;
        ch.length = sizeof(download_chunk_t);

        if (write(fd, &ch, sizeof(ch)) <= 0) {
            perror("[SERVER]: Eroare la write() header DWL_CHUNK");
            break;
        }
        if (write(fd, &chunk, sizeof(chunk)) <= 0) {
            perror("[SERVER]: Eroare la write() body DWL_CHUNK");
            break;
        }

        packet_header_t ackh; memset(&ackh, 0, sizeof(ackh));
        if (read(fd, &ackh, sizeof(ackh)) <= 0) {
            perror("[SERVER]: Eroare la read() header DWL_ACK");
            break;
        }
        if (ackh.type != DWL_ACK || ackh.length != sizeof(download_ack_t)) {
            fprintf(stderr, "[SERVER]: ACK invalid (type=%u, len=%u)\n",
                    (unsigned)ackh.type, (unsigned)ackh.length);
            break;
        }

        download_ack_t ackb; memset(&ackb, 0, sizeof(ackb));
        if (read(fd, &ackb, sizeof(ackb)) <= 0) {
            perror("[SERVER]: Eroare la read() body DWL_ACK");
            break;
        }

        if (ackb.status != 0) {
            uint32_t srv_off = ntohl(ackb.offset);
            printf("[SERVER]: NACK pe chunk #%u -> reiau de la offset=%u\n",
                   ntohl(ackb.chunk_no), srv_off);

            if (srv_off > filesize) srv_off = filesize;
            if (fseek(f, (long)srv_off, SEEK_SET) != 0) {
                perror("[SERVER]: fseek pentru reluare NACK");
                break;
            }
            offset   = srv_off;
            chunk_no = ntohl(ackb.chunk_no); 
            continue;
        }


        offset   += size_host;
        chunk_no += 1;

        printf("[SERVER]: DWL chunk OK: #%u offset=%u size=%u\n",
               chunk_no - 1, offset - size_host, size_host);
        if (offset >= filesize) break;
    }

    fclose(f);

    download_response_t fin_ok;
    memset(&fin_ok, 0, sizeof(fin_ok));
    fin_ok.status   = 0;
    fin_ok.code     = 0;
    fin_ok.filesize = htonl(filesize);
    fin_ok.offset   = htonl(offset);
    memcpy(fin_ok.file_sha256, file_sha256, 32);
    snprintf(fin_ok.message, BUFFER_SIZE, "Download complete.");

    packet_header_t fh; memset(&fh, 0, sizeof(fh));
    fh.type   = DWL_RES;
    fh.length = sizeof(fin_ok);
    if (write(fd, &fh, sizeof(fh)) <= 0) {
        perror("[SERVER]: Eroare la write() header DWL_RES final");
        return;
    }
    if (write(fd, &fin_ok, sizeof(fin_ok)) <= 0) {
        perror("[SERVER]: Eroare la write() body DWL_RES final");
        return;
    }

    printf("[SERVER]: Download finalizat pentru '%s' (trimis %u/%u bytes).\n", req.filename, offset, filesize);
}

void list_manager_server(int fd, current_user* user) {
    list_response_t resp;
    memset(&resp, 0, sizeof(resp));

    if (!user->is_logged_in) {
        resp.status = 1;
        resp.code   = 1;
        resp.count  = htonl(0);
        snprintf(resp.message, BUFFER_SIZE, "Please log in first.");
        packet_header_t h = { .type = LIST_RES, .length = sizeof(resp) };
        if( write(fd, &h, sizeof(h)) <= 0) {
            perror("[SERVER]: write header LIST_RES");
            return;
        }
        if( write(fd, &resp, sizeof(resp)) <= 0) {
            perror("[SERVER]: write body LIST_RES");
            return;
        }
        return;
    }

    char userdir[1024];
    snprintf(userdir, sizeof(userdir), "%s%s", STORAGE, user->username);

    DIR *dir = opendir(userdir);
    if (!dir) {
        perror("[SERVER]: opendir LIST");
        resp.status = 1;
        resp.code   = 2;
        resp.count  = htonl(0);
        snprintf(resp.message, BUFFER_SIZE, "Could not open user directory.");
        packet_header_t h = { .type = LIST_RES, .length = sizeof(resp) };
        if( write(fd, &h, sizeof(h)) <= 0) {
            perror("[SERVER]: write header LIST_RES");
            return;
        }
        if( write(fd, &resp, sizeof(resp)) <= 0) {
            perror("[SERVER]: write body LIST_RES");
            return;
        }
        return;
    }

    struct dirent *de;
    size_t used = 0;
    unsigned count = 0;
    while ((de = readdir(dir)) != NULL) {
        const char *name = de->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        size_t nlen = strlen(name);
        if (nlen >= 5 && strcmp(name + nlen - 5, ".part") == 0) continue;

        char path[1400];
        snprintf(path, sizeof(path), "%s/%s", userdir, name);

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        char line[256];
        int ln = snprintf(line, sizeof(line), "%lld\t%s\n", (long long)st.st_size, name);
        if (ln < 0) continue;

        if (used + (size_t)ln >= BUFFER_SIZE - 1) break;
        memcpy(resp.message + used, line, (size_t)ln);
        used += (size_t)ln;
        count++;
    }
    closedir(dir);
    resp.message[used] = '\0';

    resp.status = 0;
    resp.code   = 0;
    resp.count  = htonl(count);

    if (count == 0 && used == 0) {
        snprintf(resp.message, BUFFER_SIZE, "(empty)");
    }

    packet_header_t h = { .type = LIST_RES, .length = sizeof(resp) };
    if (write(fd, &h, sizeof(h)) <= 0) {
        perror("[SERVER]: write header LIST_RES");
        return;
    }
    if (write(fd, &resp, sizeof(resp)) <= 0) {
        perror("[SERVER]: write body LIST_RES");
        return;
    }

    printf("[SERVER]: LIST for '%s' -> %u entries\n", user->username, count);
}

void handle_client(int fd){
    current_user user;
    user.is_logged_in = false;

    while(true){
        packet_header_t header;
        memset(&header, 0, sizeof(header));
        if (read(fd, &header, sizeof(packet_header_t)) <= 0) {
            perror("Eroare la read() de la client 1");
            return;
        }

        if(header.type == LOGIN_REQ){
            login_manager(fd, &user);
        }
        else if (header.type == UPL_REQ) {
            upload_manager_server(fd, &user);
        }
        else if (header.type == DWL_REQ) {
            download_manager_server(fd, &user);
        }
        else if(header.type == EXIT_REQ){
            printf("[SERVER]: Clientul '%s' s-a deconectat.\n", user.username);
            close(fd);
            return;
        }
        else if(header.type == LIST_REQ){
            list_manager_server(fd, &user);
        }
        else if(header.type == LOGOUT_REQ){
            login_response_t out;
            memset(&out, 0, sizeof(out));
            if (!user.is_logged_in) {
                out.status = 1;
                snprintf(out.message, BUFFER_SIZE, "Not logged in.");
            } else {
                printf("[SERVER]: User '%s' logged out.\n", user.username);
                user.is_logged_in = false;
                user.username[0] = '\0';
                out.status = 0;
                snprintf(out.message, BUFFER_SIZE, "Logged out.");
            }
            packet_header_t hh = { .type = LOGIN_RES, .length = sizeof(out) };
            if(write(fd, &hh, sizeof(hh)) <= 0) {
                perror("Eroare la write() catre client");
                return;
            }
            if(write(fd, &out, sizeof(out)) <= 0) {
                perror("Eroare la write() catre client");
                return;
            }
        }
        else {
            if(user.is_logged_in == false){
                login_response_t login_rep;
                memset(&login_rep, 0, sizeof(login_rep));
                login_rep.status = 1;
                snprintf(login_rep.message, BUFFER_SIZE, "Please log in first.");

                packet_header_t header;
                memset(&header, 0, sizeof(header));
                header.type = LOGIN_RES;
                header.length = sizeof(login_response_t);
                if (write(fd, &header, sizeof(packet_header_t)) <= 0) {
                    perror("Eroare la write() catre client 1");
                    return;
                }

                if (write(fd, &login_rep, sizeof(login_response_t)) <= 0) {
                    perror("Eroare la write() catre client 3");
                    return;
                }
            }
        }
    }
    
    
}

int main(int argc, char* argv[]) {
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sock;
    char* request;
    uint32_t request_size;

    //signal(SIGPIPE, SIG_IGN);

    printf("[SERVER] Pornim serverul pe portul %d...\n", PORT);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sig_child;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la socket");
        return 1;
    }

    int on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    memset(&from, 0, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if(bind(sock, (struct sockaddr*) &server, sizeof(server)) == -1) {
        perror("Eroare la bind");
        return 2;
    }

     if(listen(sock, 10) == -1) {
        perror("Eroare la listen");
        return 3;
    }

    while(true) {
        int client;
        if((client = accept(sock, NULL, NULL)) < 0) {
            perror("Eroare la accept");
            continue;
        }

        pid_t pid;
        if((pid = fork()) < 0) {
            perror("Eroare la fork");
            close(client);
            continue;
        }

        if(pid == 0) {
            handle_client(client);
            //close(client);
            exit(0);
        }
        else {
            close(client);
        }
    }
    
    return 0;
}