/* OS416: Assignment 2 - Char() Device using ioctl() 
 * By: Zac Brown, Pintu Patel, Priya
 * November 3, 2013
 */


#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include "cryptctl.h"

/*our project assumes every message,key,etc to always be lowercase*/
void lowercase(char *message){
    int x = strlen(message);
    int i;
    for(i=0;i<x;i++){
	message[i] = tolower(message[i]);
    }

}
/*gets the number of bytes in a file*/
int byteCount(const char *path){
    /*pointer to the system call*/
    FILE *fp;
    int i,line_count;
    /*buffer for the results of the system call*/
    char buffer[100];
    /*This is the linux command to get the linecount. we need to append the file name*/
    char oldCommand[] = "wc -c ";
    /*create a string sizeof(oldcommand)+sizeof(file_path)*/
    char *command = (char*)malloc(sizeof(char)*(strlen(path)+strlen(oldCommand)+1));
    /*keep a pointer to the start of the new command*/
    char *command_start = command;
    
    /*create the new command*/
    for(i=0;i<(int)strlen(oldCommand);i++){
	*(command)=oldCommand[i];
	command++;
    }
    for(i=0;i<(int)strlen(path);i++){
	*(command)=path[i];
	command++;
    }
    //printf("System call: %s\n",command_start);
    
    /*call the command and store result in buffer*/
    fp = popen(command_start,"r");
    if(fp<0)
	return -1;

    /*WHY CANT I JUST DO FSCANF()???*/
    fgets(buffer,100,fp);
    /*scan for the line count and return*/
    sscanf(buffer,"%d",&line_count);
    /*clean up*/
    free(command_start);
    fclose(fp);
    return line_count;

} 

/*set the cryptctl key for encryption/decryption*/
void set_key(int fd,char *message){
    int x =strlen(message)+1;

    /*first do an ioctl() to send the size of the key*/
    if (ioctl(fd, QUERY_SET_SIZE, &x) == -1)
    {
        perror("query_apps ioctl get");
    }
    /*next do an ioctl() to send the key*/
    if (ioctl(fd, QUERY_SET_KEY, message) == -1)
    {
        perror("query_apps ioctl get");
    }
    else
    {
	printf("set key as %s\n",message);
    }

}

/*get the key from control device
 *just to show we save it and in case user forgets what they stored the key as
 */
int get_key(int fd){
    int x=0;
    char *key;
    /*first i need to get the size so I know how much to malloc*/
    if (ioctl(fd, QUERY_GET_SIZE, &x) == -1)
    {
        printf("query_apps ioctl get error\n");
	return 0;
    }
    /*check if there is a key*/
    if(x==0){
	printf("Get Error:\nMaybe you forgot to set the key?\n");
	return 0;
    }
    key = (char*)malloc(x);
    /*now i can receive the key*/
    if(ioctl(fd,QUERY_GET_KEY,key) == -1){
	printf("query_apps ioctl get error");
	return;
    }
    printf("Key:\n%s\n",key);
    free(key);
    return x;
}


/*for now this is the function we use to create our subdrivers*/ 
void create_pair(int fd,char *message)
{
 
    /*call ioctl on cryptctl to create a new device*/
    if (ioctl(fd, QUERY_CREATE_PAIR) == -1)
    {
        perror("ioctl() create pair FAILED");
    }
}
/*for now this is a function the READS AND DELETES a subdriver
 *prints the encrypted message of hello_testA ONLY
 *deletes all subdrivers
 */
void destory_pair(int fd){

    /*call ioctl on cryptctl to delete all subdrivers*/
    if (ioctl(fd, QUERY_DESTORY_PAIR) == -1)
    {
        perror("ioctl() destroy pair FAILED");
    }
    printf("\n");
}
/*for now this is the function to WRITE to subdrivers
 *writes message to driver device
 *DONT FORGET TO ADD PERROR STUFF TO HANDLE BAD FILES
 */
void encrypt_message(int fd,char *message,char *device){
    
    int fild;
    printf("Before encryption: %s\n",message);
    FILE *fp = fopen(device,"w");
    if(fp==NULL){
	printf("ERROR: could not open file\n");
	return;
    }
    /*check if there is a key*/
    if(get_key(fd)==0){
	return;
    }
    fwrite(message,1,strlen(message)+1,fp);
    printf("finished writing to %s\n",device);

    fclose(fp);

}

/*for now this is the function to READ from subdrivers
 *it reads from driver device
 */
void read_message(int fd, char *device){
    char *message;
    struct stat fileStat;
    int fild,ret;
    int bytecount;
    if(get_key(fd)==0){
	return;
    }
    /*open the char driver*/
    fild = open(device,O_RDONLY);
    if(fild<0){
	perror( "Error opening file" );
        printf( "Error opening file: %s\n", strerror( errno ) );
	return;
    }
    /*get the number of bytes written to device*/
    bytecount = byteCount(device);
    printf("size of file is %d\n",bytecount);
    message = (char *)malloc(bytecount);
    /*read the encrypted file to message*/
    ret=read(fild,message,bytecount);
    if(ret<0){
	perror( "Error reading file" );
        printf( "Error reading file: %s\n", strerror( errno ) );
	return;
    }
    printf("Message: %s\n",message);
}
 
int main(int argc, char *argv[])
{
    char *file_name = "/dev/cryptctl";
    int fd;
    enum
    {
        e_create,
        e_destroy,
        e_write,
	e_read,
	e_kset,
	e_kget
    } option;
 
     if (argc == 1)
    {
        printf("\nEncrypt/Decrypt App\n");
	printf("<flags>:\n\t"
		"-c: create new device pair\n"
		"\t    ex)./app -c\n\t"
		"-w: write <message> to device <device_name>\n"
		"\t    ex)./app -w <message <device_name>\n\t"
		"-d: delete all device pairs\n"
		"\t    ex)./app -d\n\t"
		"-r: read from device <device_name>\n"
		"\t    ex)./app -r <device_name>\n\t"
		"-ks: set main device's key to <key>\n"
		"\t    ex)./app -ks <key>\n\t"
		"-kg: get the main device's key\n"
		"\t    ex)./app -kg\n\n");
	printf("<message>:\n\tthe message you want written to device <device_name>\n");
	printf("<device_name>:\n\tthe name of the device you want to use\n");
	printf("<key>:\n\tthe key you want to use for the control device\n\n");
	return;
    }
    /*set option to run the appropriate command*/
    else
    {
        if (strcmp(argv[1], "-c") == 0)
        {
            option = e_create;
        }
        else if (strcmp(argv[1], "-d") == 0)
        {
            option = e_destroy;
        }
        else if (strcmp(argv[1], "-w") == 0)
        {
	    if(argc !=4){
		printf("ERROR: please provide only a message and a device_name\nex)./app -s <text>" 				
                      " <device_name>\n");
		exit(-1);
	    }
	    lowercase(argv[2]);
            option = e_write;

        }
	else if (strcmp(argv[1], "-r")==0){
	    if(argc != 3){
		printf("ERROR: please provide only a device to read from\nex)./app -r <device_name>\n");
		exit(-1);
	    }
	    option = e_read;
	}
	else if (strcmp(argv[1],"-ks")==0){
	    if(argc != 3){
		printf("ERROR: please provide only a key\nex)./app -ks <key>\n");
		exit(-1);
	    }
	    lowercase(argv[2]);
	    option = e_kset;
	}
	else if(strcmp(argv[1],"-kg")==0){
	    option = e_kget;
	}
        else
        {
            fprintf(stderr, "Error: Incorrect flag\n./app for usage\n");
            return 1;
        }
    }
    /*open the cryptctl and run ioctl()*/
    fd = open(file_name, O_RDWR);
    if (fd == -1)
    {
        perror("query_apps open");
        return 2;
    }
 
    switch (option)
    {
        case e_create:
            create_pair(fd,"test");
            break;
        case e_destroy:
            destory_pair(fd);
            break;
        case e_write:
            encrypt_message(fd,argv[2],argv[3]);
            break;
	case e_read:
	    read_message(fd,argv[2]);
	    break;
        case e_kset:
	    set_key(fd,argv[2]);
	    break;
	case e_kget:
	    get_key(fd);
	    break;
        default:
            break;
    }
 
    close (fd);
 
    return 0;
}
