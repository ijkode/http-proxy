#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "threadpool.h"

#define MAX_SIZE 1000

typedef struct properties {
    int pool_size;
    int browser;
    int flag;
    int port;
    int number_of_requests;
    char* filter;
    char method[MAX_SIZE];
    char path[MAX_SIZE];
    char full_path[MAX_SIZE];
    char protocol[MAX_SIZE];
    char host[MAX_SIZE];
} properties;

properties *props;

//rewrites the content type
char *get_mime_type(char *name) {

    char *ext = strrchr(name, '.');

    if (!ext) return NULL;

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";

    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";

    if (strcmp(ext, ".gif") == 0) return "image/gif";

    if (strcmp(ext, ".png") == 0) return "image/png";

    if (strcmp(ext, ".css") == 0) return "text/css";

    if (strcmp(ext, ".au") == 0) return "audio/basic";

    if (strcmp(ext, ".wav") == 0) return "audio/wav";

    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";

    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";

    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";

    return NULL;

}

//handle the subnets
int subnet(char* ip, char* ip_ws) {
    char* subs=strstr(ip_ws, "/");
    int sub_num=atoi(subs + 1);
    unsigned int mask=0xFFFFFFFF<<(32 - sub_num);
    char ip_ns[100];
    memset(ip_ns, 0, 100);
    for (size_t i =0; i < strlen(ip_ws); i++) {
        if (ip_ws[i] == '/') {
            ip_ns[i] = '\0';
            break;
        }
        ip_ns[i] = ip_ws[i];
    }

    struct sockaddr_in first;
    struct sockaddr_in second;
    inet_aton(ip, &first.sin_addr);
    inet_aton(ip_ns, &second.sin_addr);
    return (ntohl(first.sin_addr.s_addr)&mask)==(ntohl(second.sin_addr.s_addr)&mask);
}

//check spaces in the request
int check_spaces(char* request) {
    int spaces = 0;
    for (size_t i = 0; i < strlen(request); ++i) {
        if(request[i] == ' ') {
            spaces++;
        }
    }
    return spaces;
}

//validates and parse the request
int check_request(char *request, properties *props, int connect_Fd) {

    int method_flag = 0;
    int path_flag = 0;
    int protocol_flag = 0;
    int host_flag = 0;
    int spaces = check_spaces(request);
    printf("HTTP request =\n%s\nLEN = %d\n", request, (int) strlen(request));
    char *found = strtok(request, " ");
    strcpy(props->method, found);
    int counter = 0;
    while (found != NULL) {
        found = strtok(NULL, " ");
        if(counter == 0) {
            if(spaces > 3) {
                int slash = 0;
                for (size_t i = 0; i < strlen(found); ++i) {
                    if(found[i] == '/') {
                        slash++;
                    }
                    if(slash == 3) {
                        for (size_t j = 0; j < strlen(found); ++j) {
                            if(found[i] == '\0') {
                                break;
                            }
                            props->path[j] = found[i];
                            i++;
                        }
                        break;
                    }
                }
            } else{
                strcpy(props->path, found);
            }
        }
        if(counter == 1) {
            strcpy(props->protocol, found);
            props->protocol[8] = '\0';

        }
        if(counter == 2) {
            strcpy(props->host, found);
            for (int i = (int)strlen(props->host); i > 0 ; --i) {
                if(props->host[i] == ' ' || props->host[i] == '\n' || props->host[i] == '\r') {
                    props->host[i] = '\0';
                }
            }
        }
        counter++;
    }
    strcpy(props->full_path, props->host);
    strcat(props->full_path, props->path);
    if(props->full_path[strlen(props->full_path) - 1] == '/') {
        strcat(props->full_path, "index.html");
    }

    FILE * fp = fopen (props->filter, "r");
    if(fp == NULL) {
        perror("error: fopen\n");
        exit(1);
    }
    else {
        char buffer[128];
        while(fgets(buffer, sizeof(buffer), fp) != NULL) {
            buffer[strcspn(buffer, " \r\n")] = '\0';
            props->host[strcspn(props->host, " \r\n")] = '\0';

            if(strcmp(buffer, props->host) == 0) {
                char forbidden[MAX_SIZE];
                memset(forbidden, 0, MAX_SIZE);
                strcat(forbidden, "HTTP/1.0 403 Forbidden\nContent-Type: text/html\nContent-Length: 111\nConnection: close\r\n\r\n");
                strcat(forbidden, "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H4>403 Forbidden</H4>\nAccess denied.\n</BODY></HTML>\n");
                if((write(connect_Fd, forbidden, sizeof(forbidden))) < 0) {
                    perror("error: write\n");
                    exit(1);
                }
                close(connect_Fd);
                return 1;
            }
        }

    }

    fclose(fp);

    if(strcmp(props->method, "GET") == 0) {
        method_flag = 1;
    }
    if(props->path[0] == '/') {
        path_flag = 1;
    }
    if(strcmp(props->protocol, "HTTP/1.0") == 0 || strcmp(props->protocol, "HTTP/1.1") == 0) {
        protocol_flag = 1;
    }
    if(props->host[0] != '/') {
        host_flag = 1;
    }

    if(method_flag == 0) {
        char not_supported[512];
        memset(not_supported, 0, 512);
        strcat(not_supported, "HTTP/1.0 501 Not supported\nContent-Type: text/html\nContent-Length: 113\nConnection: close\r\n\r\n");
        strcat(not_supported, "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\n<BODY><H4>501 Not supported</H4>\nMethod is not supported.\n</BODY></HTML>\n");
        if((write(connect_Fd, not_supported, sizeof(not_supported))) < 0) {
            perror("error: write\n");
            exit(1);
        }
        close(connect_Fd);
        return 1;
    }
    if(path_flag == 1 && protocol_flag == 1 && host_flag == 1){
        struct hostent *hp;
        hp = gethostbyname(props->host);
        if (hp == NULL) {
            char not_found[512];
            memset(not_found, 0, 512);
            strcat(not_found, "HTTP/1.0 404 Not Found\nContent-Type: text/html\nContent-Length: 112\nConnection: close\r\n\r\n");
            strcat(not_found, "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H4>404 Not Found</H4>\nFile not found.\n</BODY></HTML>\n");
            if((write(connect_Fd, not_found, sizeof(not_found))) < 0) {
                perror("error: write\n");
                exit(1);
            }
            close(connect_Fd);
            return 1;
        }
        return 0;
    } else {
        char bad_request[512];
        memset(bad_request, 0, 512);
        strcat(bad_request, "HTTP/1.0 400 Bad Request\nContent-Type: text/html\nContent-Length: 113\nConnection: close\r\n\r\n");
        strcat(bad_request, "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H4>400 Bad request</H4>\nBad Request.\n</BODY></HTML>\n");
        if((write(connect_Fd, bad_request, sizeof(bad_request))) < 0) {
            perror("error: write\n");
            exit(1);
        }
        close(connect_Fd);
        return 1;
    }
}

//create directories
int create_dir(char *name) {
    int dir;
    dir = mkdir(name, S_IRWXU);
    if (dir != 0 && errno != EEXIST) {
        perror("error: mkdir\n");
        exit(1);
    }

    return 0;
}

//helper for directories
void direct(properties *props) {

    char* end_slash=strrchr(props->full_path, '/');
    long index=end_slash-props->full_path;
    char new_full_path[MAX_SIZE];
    memset(new_full_path,0,MAX_SIZE);
    strcpy(new_full_path, props->full_path);
    new_full_path[index]='\0';
    char last[MAX_SIZE];
    memset(last,0,MAX_SIZE);
    char* token = strtok(new_full_path, "/");
    while (token != NULL) {
        strcat(last,token);
        strcat(last,"/");
        create_dir(last);
        token = strtok(NULL, "/");
    }
}

//handle the client and server side
int client_handler(void* arg) {
    int connect_Fd = *((int*) arg);
    char readbuf[1024];
    memset(readbuf, 0, 1024);
    while ((read(connect_Fd, readbuf, sizeof(readbuf))) > 0) { //extract the request
        break;
    }

    if (check_request(readbuf, props, connect_Fd) == 0) {
        FILE *fp = fopen(props->full_path, "r");
        if (fp != NULL) {
            printf("File is given from local filesystem\nHTTP/1.0 200 OK\r\n");

            // calculating the size of the file
            fseek(fp, 0L, SEEK_END);
            // calculating the size of the file
            long int res = ftell(fp);
            char *print = (char *) malloc(res);
            if (print == NULL) {
                char internal_error[512];
                memset(internal_error, 0, 512);
                strcat(internal_error, "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n");
                strcat(internal_error, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n");
                if((write(connect_Fd, internal_error, sizeof(internal_error))) < 0) {
                    perror("error: write\n");
                    exit(1);
                }
                close(connect_Fd);
                return 1;
            }
            fseek(fp, 0, SEEK_SET);
            memset(print, 0, res);
            char length[100];
            memset(length, 0, 100);
            strcpy(length, "Content-Length: ");
            char s[100];
            sprintf(s, "%ld", res);
            strcat(length, s);
            char *content_type = get_mime_type(props->full_path);
            if (content_type != NULL) {
                strcat(length, "\r\nContent-Type: ");
                strcat(length, content_type);
            }
            strcat(length, "\r\nConnection: close");
            strcat(length, "\r\n\r\n");
            if ((write(connect_Fd, length, sizeof(length))) < 0) {
                perror("error: write\n");
                exit(1);
            }
            char to_print;
            while ((to_print = (char) fgetc(fp)) != EOF) {
                if ((write(connect_Fd, &to_print, sizeof(to_print))) < 0) {
                    perror("error: write\n");
                    exit(1);
                }
            }

            fclose(fp);
            free(print);
        } else {
            int network_socket;
            if ((network_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
                char internal_error[512];
                memset(internal_error, 0, 512);
                strcat(internal_error, "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n");
                strcat(internal_error, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n");
                if((write(connect_Fd, internal_error, sizeof(internal_error))) < 0) {
                    perror("error: write\n");
                    exit(1);
                }
                close(connect_Fd);
                return 1;
            }

            struct hostent *hp;
            struct sockaddr_in peeraddr2;
            char *name = props->host;
            peeraddr2.sin_family = AF_INET;
            hp = gethostbyname(name);
            peeraddr2.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
            peeraddr2.sin_port = htons(80);

            FILE * filterfp = fopen (props->filter, "r");
            if(filterfp == NULL) {
                char internal_error[512];
                memset(internal_error, 0, 512);
                strcat(internal_error, "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n");
                strcat(internal_error, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n");
                if((write(connect_Fd, internal_error, sizeof(internal_error))) < 0) {
                    perror("error: write\n");
                    exit(1);
                }
                close(connect_Fd);
                return 1;
            }
            char filter_buffer[128];
            while(fgets(filter_buffer, sizeof(filter_buffer), filterfp) != NULL) {
                filter_buffer[strcspn(filter_buffer, " \r\n")] = '\0';
                if (filter_buffer[0] >= 48 && filter_buffer[1] <= 57) {
                    if (subnet(inet_ntoa(peeraddr2.sin_addr), filter_buffer)) {
                        char forbidden[MAX_SIZE];
                        memset(forbidden, 0, MAX_SIZE);
                        strcat(forbidden, "HTTP/1.0 403 Forbidden\nContent-Type: text/html\nContent-Length: 111\nConnection: close\r\n\r\n");
                        strcat(forbidden, "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H4>403 Forbidden</H4>\nAccess denied.\n</BODY></HTML>\n");
                        if((write(connect_Fd, forbidden, sizeof(forbidden))) < 0) {
                            perror("error: write\n");
                            exit(1);
                        }
                        close(connect_Fd);
                        return 1;
                    }
                }
            }

            fclose(filterfp);


            if (connect(network_socket, (struct sockaddr *) &peeraddr2, sizeof(peeraddr2)) < 0) {
                char internal_error[512];
                memset(internal_error, 0, 512);
                strcat(internal_error, "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n");
                strcat(internal_error, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n");
                if((write(connect_Fd, internal_error, sizeof(internal_error))) < 0) {
                    perror("error: write\n");
                    exit(1);
                }
                close(connect_Fd);
                return 1;
            }

            char buffer[512];
            memset(buffer, 0, 512);
            char met[100];
            memset(met, 0, 100);
            strcat(met, " ");
            strcat(met, props->protocol);
            strcat(buffer, "GET ");
            strcat(buffer, props->path);
            strcat(buffer, met);
            strcat(buffer, "\r\n");
            strcat(buffer, "Host: ");
            strcat(buffer, props->host);
            strcat(buffer, "\nConnection: close");
            strcat(buffer, "\r\n\r\n");

            printf("File is given from origin server\n");
            printf("HTTP request =\n%s\nLEN = %d\n", buffer, (int) strlen(buffer));

            if ((write(network_socket, buffer, sizeof(buffer))) < 0) {
                char internal_error[512];
                memset(internal_error, 0, 512);
                strcat(internal_error, "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n");
                strcat(internal_error, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n");
                if((write(connect_Fd, internal_error, sizeof(internal_error))) < 0) {
                    perror("error: write\n");
                    exit(1);
                }
                close(connect_Fd);
                return 1;
            }

            unsigned long size = 512;
            unsigned char *read_buf = (unsigned char *) malloc(size);
            int ok = 0;
            FILE *output;
            ssize_t r, total = 0;
            while ((r = read(network_socket, read_buf, sizeof(read_buf) - 1)) > 0) {
                total += r;
                write(connect_Fd, read_buf, r);
                if (ok == 0) {
                    ok = 1;
                    direct(props);
                    output = fopen(props->full_path, "w");
                    if (output == NULL) {
                        free(read_buf);
                        free(props);
                        perror("error: fopen\n");
                        exit(1);
                    }
                    fwrite(read_buf, 1, r, output);
                } else {
                    fwrite(read_buf, 1, r, output);
                }
                memset(read_buf, 0, size);
            }

            free(read_buf);
            printf("\nTotal response bytes: %ld\n", total);

            close(network_socket);

            if (ok == 1) {
                fclose(output);

                output = fopen(props->full_path, "r");
                if (output == NULL) {
                    char internal_error[512];
                    memset(internal_error, 0, 512);
                    strcat(internal_error, "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n");
                    strcat(internal_error, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n");
                    if((write(connect_Fd, internal_error, sizeof(internal_error))) < 0) {
                        perror("error: write\n");
                        exit(1);
                    }
                    close(connect_Fd);
                    return 1;
                }
                fseek(output, 0, SEEK_END);
                long int output_size = ftell(output);
                fseek(output, 0, SEEK_SET);
                unsigned char *output_details = (unsigned char *) malloc(output_size);
                if (output_details == NULL) {
                    char internal_error[512];
                    memset(internal_error, 0, 512);
                    strcat(internal_error, "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n");
                    strcat(internal_error, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n");
                    if((write(connect_Fd, internal_error, sizeof(internal_error))) < 0) {
                        perror("error: write\n");
                        exit(1);
                    }
                    close(connect_Fd);
                    return 1;
                }
                fread(output_details, 1, output_size, output);
                char *before_details = strstr((char *) output_details, "\r\n\r\n");
                int index = (int) ((unsigned char *) before_details - output_details) + 4;
                fclose(output);
                output = fopen(props->full_path, "w");
                if (output == NULL) {
                    char internal_error[512];
                    memset(internal_error, 0, 512);
                    strcat(internal_error, "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n");
                    strcat(internal_error, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n");
                    if((write(connect_Fd, internal_error, sizeof(internal_error))) < 0) {
                        perror("error: write\n");
                        exit(1);
                    }
                    close(connect_Fd);
                    return 1;
                }
                fwrite(output_details + index, 1, output_size - index, output);

                free(output_details);
                fclose(output);
            }

        }
    }

    close(connect_Fd);
    free(arg);
    return 0;
}

int main(int argc,char *argv[]) {

    if (argc != 5) {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        exit(1);
    }
    props = malloc(sizeof(properties));
    if (props == NULL) {
        perror("error: malloc\n");
        exit(1);
    }

    memset(props->path, 0, MAX_SIZE);
    memset(props->host, 0, MAX_SIZE);
    memset(props->protocol, 0, MAX_SIZE);
    memset(props->method, 0, MAX_SIZE);
    memset(props->full_path, 0, MAX_SIZE);

    props->port = atoi(argv[1]);
    props->pool_size = atoi(argv[2]);
    props->number_of_requests = atoi(argv[3]);
    props->filter = argv[4];
    props->browser = 0;
    if (props->port < 1024 || props->port > 10000) {
        free(props);
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        exit(1);
    }

    int socket_Fd = 0;
    struct sockaddr_in peeradr;
    char buf[512];
    memset(buf, 0, 512);

    socket_Fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_Fd <= 0) {
        free(props);
        perror("error: socket\n");
        exit(1);
    }

    int result;
    int lis;
    peeradr.sin_family = AF_INET;
    peeradr.sin_addr.s_addr = htonl(INADDR_ANY);
    peeradr.sin_port = htons(props->port);
    result = bind(socket_Fd, (struct sockaddr *) &peeradr, sizeof(peeradr));

    if (result != 0) {
        free(props);
        perror("error: bind\n");
        exit(1);
    }

    lis = listen(socket_Fd, 100);
    if (lis != 0) {
        free(props);
        perror("error: listen\n");
        exit(1);
    }
    int connect_Fd;
    socklen_t peeradr_size = sizeof(peeradr);

    threadpool *tp = create_threadpool(props->pool_size);

    int num_requests = 0;
    while (num_requests < props->number_of_requests) {
        connect_Fd = accept(socket_Fd, (struct sockaddr *) &peeradr, &peeradr_size);
        if (connect_Fd < 0) {
            free(props);
            perror("error: accept\n");
            exit(1);
        }

        num_requests++;

        int *fd = malloc(4);
        *fd = connect_Fd;
        dispatch(tp, client_handler, fd);
    }

    destroy_threadpool(tp);
    free(props);
}