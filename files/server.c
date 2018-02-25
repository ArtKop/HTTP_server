#include"stdio.h"
#include"stdlib.h"
#include"sys/types.h"
#include"sys/socket.h"
#include"string.h"
#include"netinet/in.h"
#include"time.h"
#include"dirent.h"
#include"netdb.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

int createSocket(int port);
int listenForRequest(int sockfd);
char * FileType(char * file);
int HeaderParser(char * header);

char MediaType[100];
char fields[][25] = {"Date: ", "Location: ", "Content-Type: "};
char status[3] = {0, 0, 0};

int main(int argc, char **argv) { 

 DIR * dirptr;
 FILE * fileptr;
 
 struct tm * timeinfo;
 struct stat st;

 time_t timenow;
 timeinfo = localtime(&timenow);
 time (&timenow);

 char * header, * request, * path, * newpath, * dir, * temp, * contentType;
 int port, sockfd, connfd, ret, i;
 char get[3];
 char filepath[BUF_SIZE], buffer[BUF_SIZE];

 //handshake HTTP packets
 char http_not_found[] = "404 Not Found\n";
 char http_ok[] = "200 OK\n"; 
 char http_bad_request[] = "400 Bad Request\n";

 //char status_ok[] = "OK";

 //commands
 char RECEIVE[] = "GET";
 char PUT[] = "PUT";
 
 if (argc != 3) { printf("Not enough arguments!\n"); return 1;}

 header = (char*)malloc(BUF_SIZE*sizeof(char));
 request = (char*)malloc(BUF_SIZE*sizeof(char));
 path = (char*)malloc(BUF_SIZE*sizeof(char));
 newpath = (char*)malloc(BUF_SIZE*sizeof(char));

 dir = argv[1];
 port = atoi(argv[2]);

 if ((dirptr = opendir(dir)) == NULL) { printf("Directory not found!\n"); return 1;}
 
 sockfd = createSocket(port);
 
 for (;;) {
  
  connfd = listenForRequest(sockfd);
  
  recv(connfd, request, 100, 0); //gets the request from the client
  printf("Processing request...\n");
  sscanf(request, "%s %s", get, path); //parses the request (command, file name)

  if (strcmp(RECEIVE, get) == 0) {

    newpath = path + 1; //removing the first slash from the file name
    sprintf(filepath,"%s/%s", dir, newpath); //directory of the files to be sent + requested file name
    contentType = FileType(newpath);//media type of the file
    sprintf(header, "Date: %s\nLocation: %s\nContent-Type: %s\n\n", asctime(timeinfo), filepath, contentType); //encapsulating the header fields

    if ((fileptr = fopen(filepath, "r")) == NULL ) { printf("Requested file is not found!\n");
      if (send(connfd, http_not_found, strlen(http_not_found), 0) == -1) { printf("Operation aborted by the Client!\n"); continue;} } //sending HTTP 404 Not Found
    else {
      stat(filepath, &st); //size of the file
      printf("%s is requested ", newpath);
      printf("(%ld bytes).\n", st.st_size); //printing size of the file
      printf("Sending the file...\n");
      if (send(connfd, http_ok, strlen(http_ok), 0) == -1) { printf("Operation aborted by the Client!\n"); continue; } //sending HTTP 200 OK  
      if ((ret = recv(connfd, buffer, BUF_SIZE, 0)) == -1)  { printf("Operation aborted by the Client!\n"); continue; }  //receiving HTTP 200 OK  
      if ((temp = strstr(buffer, http_ok)) == NULL) { printf("Operation aborted by the Client!\n"); continue; }
      if (send(connfd, header, strlen(header), 0) == -1) { printf("Operation aborted by the Client!\n"); continue; } //sending the header
      if ((ret = recv(connfd, buffer, BUF_SIZE, 0)) == -1) { printf("Operation aborted by the Client!\n"); continue; } //receiving HTTP 200 OK  
      if ((temp = strstr(buffer, http_ok)) == NULL) { printf("Operation aborted by the Client!\n"); continue; }
      memset(&buffer, 0, sizeof(buffer));
      while (!feof(fileptr)) { //sending the file
        fread(&buffer, sizeof(buffer), 1, fileptr);
        send(connfd, buffer, sizeof(buffer), 0);
        memset(&buffer, 0, sizeof(buffer));
      }
      printf("File is sent.\n"); 
      close(connfd);
      }
  } 

  if (strcmp(PUT, get) == 0) {
   
    printf("Receiving the file...\n");
    if (send(connfd, http_ok, strlen(http_ok), 0) == -1) { printf("Operation aborted by the Client!\n"); continue; } //sending HTTP 200 OK
    if ((ret = recv(connfd, buffer, BUF_SIZE, 0)) == -1) { printf("Operation aborted by the Client!\n"); continue; } //receiving HTTP 200 OK  
    if ((temp = strstr(buffer, http_ok)) == NULL) { printf("Operation aborted by the Client!\n"); continue; }
    memset(buffer, 0, BUF_SIZE);
    ret = recv(connfd, buffer, BUF_SIZE, 0); //receiving the header
    if (ret < 0) { printf("Error receiving HTTP header!\n"); continue; } 
    else {
      if (HeaderParser(buffer) == 0) { //checking the header's fields
        if (send(connfd, http_ok, strlen(http_ok), 0) == -1) { printf("Operation aborted by the Client!\n"); continue; } } //sending HTTP 200 OK
      else {
        printf("Error in HTTP header!\n"); send(connfd, http_bad_request, strlen(http_bad_request), 0); continue; }  //sending HTTP 400 Bad Request
    }
    newpath = path + 1; //removing the first slash from the file name
    sprintf(filepath,"%s/%s", dir, newpath); //directory of the files to be sent + requested file name
    fileptr = fopen(filepath, "w");
    if (fileptr == NULL) { 
      i = 0;
      printf("Error opening file!\n");
      continue;
    }
    else {i = 1;} //file was opened successfully (i = 1)

    memset(&buffer, 0, sizeof(buffer));
    while (recv(connfd, buffer, BUF_SIZE, 0) > 0) { //receives the file
      if ((strstr(MediaType, "text/html")) != NULL) { fprintf(fileptr, "%s", buffer); }
      else { fwrite(&buffer, sizeof(buffer), 1, fileptr); }
      memset(&buffer, 0, sizeof(buffer)); 
    }
    printf("File is received.\n");
    printf("File directory is %s\n", filepath);
    close(connfd);
    if (i == 1) {fclose(fileptr);} //closing the file only if it was initially opened (i = 1)
    }
  } //closing the socket and deallocating the space
  close(sockfd);
  free(header);
  free(request);
  free(path);
  free(newpath);
  return 0;
}


int createSocket(int port) {
 int sockfd;
 struct sockaddr_in addr;

 memset(&addr, 0, sizeof(addr));
 addr.sin_family = AF_UNSPEC;
 addr.sin_addr.s_addr = INADDR_ANY;
 addr.sin_port = htons((short)port);

 sockfd = socket(AF_INET, SOCK_STREAM, 0);
 if (sockfd < 0) { printf("Error creating socket!\n"); return 1;} 
 printf("Socket created...\n");

 if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { printf("Error binding socket to port!\n"); exit(1);} 
 printf("Binding done...\n");

 return sockfd;
}


int listenForRequest(int sockfd) {
 int conn;
 char hostip[80];
 struct sockaddr_in addr;
 struct hostent * host;
 struct in_addr inAddr;
 unsigned int len;
 
 addr.sin_family = AF_UNSPEC;

 printf("--------------------------------------------------------\n");
 printf("Waiting for a connection...\n");

 listen(sockfd, 5); //maximum 5 connections
 len = sizeof(addr); 
 if ((conn = accept(sockfd, (struct sockaddr *)&addr, &len)) < 0) {
  printf("Error accepting connection!\n");
 }
 printf("Connection accepted...\n");
  
 inet_ntop(AF_INET, &(addr.sin_addr), hostip, sizeof(hostip));
 inet_pton(AF_INET, hostip, &inAddr);
 host = gethostbyaddr(&inAddr, sizeof(inAddr), AF_INET);

 printf("Connection received from: %s [IP: %s]\n", host->h_name, hostip);
 return conn;
}


char * FileType(char * file) {
 char * temp;
 if ((temp = strstr(file, ".html")) != NULL) { return "text/html"; } 
 else if ((temp = strstr(file, ".pdf")) != NULL) { return "application/pdf"; } 
 else if ((temp = strstr(file, ".txt")) != NULL) { return "text/html"; }
 else if ((temp = strstr(file, ".jpeg")) != NULL) { return "image/jpeg"; }
 else if ((temp = strstr(file, ".png")) != NULL) { return "image/png"; }
 else  {return NULL; }
}


int HeaderParser(char * header) {
 //header content is of the following type: "Date: %s\nLocation: %s\nContent-Type: %s\n\n"
 char * line, * temp;
 int index = 0;
 printf("Parsing the header...\n");
 line = strtok(header, "\n"); //copying the line of the header
 while (line != NULL) {
  if ((temp = strstr(line, fields[index])) != NULL) { //checking the field of the line
    status[index] = 1; } //line is fine
  if (index == 2) { 
    strcpy(MediaType, temp); //coping the file type (3rd line)
  }
  line = strtok(NULL, "\n"); //"jumping" to the next line
  index ++; 
 }
 for (index = 0; index < 3; index++) {
   if (status[index] == 0) return 1;
 }
 return 0;
}
