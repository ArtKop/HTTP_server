#include"stdio.h"  
#include"stdlib.h"  
#include"sys/types.h"  
#include"sys/socket.h"  
#include"string.h"  
#include"netinet/in.h"  
#include"netdb.h"
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
  
#define BUF_SIZE 1024 
  
int request(char * url, int port, char * command);
int ValidIP(char * ip);
int HeaderParser(char * header);
char * FileType(char * file);

char fields[][25] = {"Date: ", "Location: ", "Content-Type: "};
char status[4] = {0, 0, 0};
char contentFileType[100];
char path[1000];

int main(int argc, char**argv) {

 FILE * fileptr;
 DIR * dirptr;  
 
 struct stat st;
 struct tm * timeinfo;
 time_t timenow;
 time (&timenow);
 timeinfo = localtime(&timenow);

 char * url, * temp, * command, * newpath, * header, * contentType, * rtr, * dir;
 char buffer[BUF_SIZE], filepath[BUF_SIZE];
 int sockfd, ret, port, i;

 //commands
 char PUT[] = "PUT";
 char GET[] = "GET";

 //handshake packets
 // char http_not_found[] = "404 Not Found\n";
 char http_ok[] = "200 OK\n"; 
 char http_bad_request[] = "400 Bad Request\n";

 if (argc != 5) { printf("Not enough arguments!\n"); return 1; }

 url = argv[1];
 dir = argv[2];
 port= atoi(argv[3]);
 command = argv[4]; 

 if ((dirptr = opendir(dir)) == NULL) { printf("Directory not found!\n"); return 1;}
 if (port > 65536 || port < 0) { printf("Invalid port number!\n"); return 1; }  //checking port validity
 if ((strcmp (GET, command) != 0) && (strcmp (PUT, command) != 0)) { printf("Invalid command!\n"); return 1; }  //checking command validity

 header = (char*)malloc(BUF_SIZE*sizeof(char));
 newpath = (char*)malloc(BUF_SIZE*sizeof(char));

 rtr = strstr(url, "/");
 strcpy(path, rtr); 
 sprintf(filepath,"%s%s", dir, path); //changing file directory type to ./files/file.[file format]
 contentType = FileType(path); //media type of the file

 if (((fileptr = fopen(filepath, "r")) == NULL ) && ((strcmp (PUT, command) == 0))) { printf("File is not found!\n"); return 1; } //checking the files to be sent
 
 sockfd = request(url, port, command);

 if (strcmp (GET, command) == 0) { //receiving the file

   printf("Requesting %s...\n", path+1);
   memset(&buffer, 0, sizeof(buffer));
   ret = recv(sockfd, buffer, BUF_SIZE, 0);  //receiving HTTP 200 OK
   if (ret < 0) { printf("Operation aborted by the Server!\n"); return 1; } 
   else {
     if ((temp = strstr(buffer, http_ok)) != NULL) {
       if (send(sockfd, http_ok, strlen(http_ok), 0)  == -1) { printf("Operation aborted by the Server!\n"); return 1;} } //sending HTTP 200 OK
     else { printf("Requested file is not found!\n"); return 1; } }
   memset(&buffer, 0, sizeof(buffer)); 
   ret = recv(sockfd, buffer, BUF_SIZE, 0); //receiving the header
   if (ret < 0) { printf("Error receiving HTTP header!\n"); return 1; } 
   else {
     if (HeaderParser(buffer) == 0) { //checking the header's fields
       if (send(sockfd, http_ok, strlen(http_ok), 0) == -1) { printf("Operation aborted by the Server!\n"); return 1;}  } //sending HTTP 200 OK
   else {
     printf("Error in HTTP header!\n");
     if (send(sockfd, http_bad_request, strlen(http_bad_request), 0) == -1) { printf("Operation aborted by the Server!\n"); return 1;}   //sending HTTP 400 Bad Request
     return 1; }
   }

   fileptr = fopen(filepath, "w");
   if (fileptr == NULL) {
     i = 0;
     printf("Error opening file!\n");
     return 1; }
   else { i = 1; }
   
   memset(&buffer, 0, sizeof(buffer));
   while (recv(sockfd, buffer, BUF_SIZE, 0) > 0) { //receiving the file
     if ((strstr(contentFileType, "text/html")) != NULL) {
       fprintf(fileptr, "%s", buffer); }
     else {
       fwrite(&buffer, sizeof(buffer), 1, fileptr); }
     memset(&buffer, 0, sizeof(buffer)); }
 
   printf("File is received.\n");
   printf("File directory is %s\n", filepath);
   printf("--------------------------------------------------------\n");
   if (i == 1)  { fclose(fileptr); } //closing the file only if it was initially opened (i = 1)
   close(sockfd); //closing the socket
   return 1;
 }

 if (strcmp (PUT, command) == 0) { //sending the file

   printf("Sending the file...\n");
   if ((fileptr = fopen(filepath, "r")) == NULL ) { printf("File is not found!\n"); return 1; }

   stat(filepath, &st); //size of the file
   printf("File size is %ld bytes.\n", st.st_size); //printing size of the file

   memset(&buffer, 0, sizeof(buffer)); 
   ret = recv(sockfd, buffer, BUF_SIZE, 0); //receiving HTTP 200 OK
   if (ret < 0) { printf("Operation aborted by the Server!\n"); return 1;}
   else {
     if ((temp = strstr(buffer, http_ok)) != NULL) {
       if (send(sockfd, http_ok, strlen(http_ok), 0) == -1)  { printf("Operation aborted by the Server!\n"); return 1;} } //sending HTTP 200 OK
     else { printf("Operation aborted by the Server!\n"); return 1; }
   }

   sprintf(header, "Date: %s\nLocation: %s\nContent-Type: %s\n\n", asctime(timeinfo), filepath, contentType); //encapsulating the header fields
   
   send(sockfd, header, strlen(header), 0); //sending the header
   if ((ret = recv(sockfd, buffer, BUF_SIZE, 0)) < 0) { printf("Operation aborted by the Server!\n"); return 1; }
   if ((temp = strstr(buffer, http_ok)) == NULL) { printf("Operation aborted by the Server!\n"); return 1; } //receiving HTTP 200 OK
   memset(&buffer, 0, sizeof(buffer));

   while (!feof(fileptr)) { //sending the file
      fread(&buffer, sizeof(buffer), 1, fileptr);
      send(sockfd, buffer, sizeof(buffer), 0);
      memset(&buffer, 0, sizeof(buffer)); }
      
   printf("File is sent.\n"); 
   printf("--------------------------------------------------------\n");
   close(sockfd); 
   fclose(fileptr);  
   }

 free(header);
 free(newpath);
 return 1;
}


int request(char * url, int port, char *command) {

 int sockfd;
 char * ptr;
 char PUT[] = "PUT";
 char GET[] = "GET";
 char getrequest[1024];
 struct sockaddr_in addr;
 
 ptr = strstr(url, "/"); 
 strcpy(path, ptr);  
 strtok(url, "/"); //extracting client_ip from client_ip/filename
   
 if (ValidIP(url) == 0) { printf("Invalid IP!"); exit(1); } //cheking IP validity
   
 if (strcmp (GET, command) == 0) { //command is GET
   sprintf(getrequest, "GET %s\n", path); }
 if (strcmp (PUT, command) == 0) { //command is PUT
   sprintf(getrequest, "PUT %s\n", path); }
 
 //creating a socket to the host
 sockfd = socket(AF_INET, SOCK_STREAM, 0);
 if (sockfd < 0) {  
   printf("Error creating socket!\n");  
   exit(1);  
 }  
 printf("Socket created...\n");

 memset(&addr, 0, sizeof(addr));  
 addr.sin_family = AF_INET;  
 addr.sin_addr.s_addr = inet_addr(url);
 addr.sin_port = htons(port);
 
 printf("Trying to connect %s...\n", url); // url is the Server's IP
 if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0 ) {
   printf("Connection error!\n");
   exit(1); }
 printf("Connection successful.\n");
 write(sockfd, getrequest, strlen(getrequest)); //sending request to the server
 return sockfd;
}


int ValidIP(char * ip) {
 struct sockaddr_in addr;
 int valid = inet_pton(AF_INET, ip, &(addr.sin_addr));
 return valid != 0;
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
    strcpy(contentFileType, temp); //coping the file type (3rd line)
  }
  line = strtok(NULL, "\n"); //"jumping" to the next line
  index ++; 
 }
 for (index = 0; index < 3; index++) { 
   if (status[index] == 0) return 1;
 }
 return 0;
}


char * FileType(char * file) {
 char * temp;
 if ((temp = strstr(file, ".html")) != NULL) { return "text/html"; } 
 else if ((temp = strstr(file, ".pdf")) != NULL) { return "application/pdf"; } 
 else if ((temp = strstr(file, ".txt")) != NULL) { return "text/html"; }
 else if ((temp = strstr(file, ".jpeg")) != NULL) { return "image/jpeg"; }
 else if ((temp = strstr(file, ".png")) != NULL) { return "image/png"; }
 else { return NULL; }
}
