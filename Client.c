#include<unistd.h>
#include<sys/ipc.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<sys/sendfile.h>
#include<net/if.h>
#include<sys/ioctl.h>
#include<sys/wait.h>
#include<signal.h>
#include<sys/epoll.h>

#define BUFF_SIZE 1024
#define IF_NAME "enp0s3"
#define CLIENT_SEND_PORT 14446
#define CLIENT_RECEIVE_PORT 14447
#define SERVER_IP "59.110.53.159"
//#define SERVER_IP "127.0.0.1"
#define CLIENT_CTL_PORT 14445
#define CLIENT_CHAT_PORT 14444

socklen_t socklen = sizeof(struct sockaddr_in);

typedef struct Param{
    char *filename;
    char filepath[100];
    char receiver[50];
}Param;

struct DataReceiverPara {
    int socket_fd;
};

void safe_flush(FILE *fp)
{
    int ch;
    while ((ch = fgetc(fp)) != EOF && ch != '\n');
}

void read_input(char *buff)
{
    memset(buff,0,strlen(buff));
    scanf("%[^\n]", buff);
    // fflush(stdout);
    // memset(buff,0,buffsize);
    // while(NULL != fgets(buff,buffsize,stdin))
    // {  
    //     char *n = strchr(buff,'\n');
    //     if(n)*n = '\0';
    //     return ;
    // }
}

char* split_filename(char* filepath)
{
    if(filepath == NULL)return NULL;
    char *name_start = strchr(filepath,'/');
    if(name_start == NULL)return filepath;
    else name_start += 1;
    char *tmp = name_start;
    while((tmp = strchr(name_start,'/')) != NULL)
    {
        name_start = tmp+1;
    }
    return name_start;
}
void process_stat(int bytes,int all_bytes)
{   
    fflush(stdout);
    for(int i=1;i<=56;i++)putchar('\b');
    putchar('[');
    int num = (double)bytes/all_bytes*50;
    for(int i=1;i<=50;i++)
    if(i<=num)putchar('<');
    else putchar(' ');
    putchar(']');
    printf("%3d%%",num*2);
    fflush(stdout);
}
int split_command(char *command,char **rev)
{
    memset(rev,0,sizeof(rev));
    char *spliter = ":";
    int num = 0;
    char *tmp = NULL;
    tmp = strtok(command,spliter);
    while(tmp != NULL)
    {
        *(rev+num) = tmp;
        num++;
        tmp = strtok(NULL,spliter);
    }
    return num;
}
void out_of_connect(char *s)
{
    printf("%s\n",strcat(s,"Server out of connect"));
}
void send_file(Param *param)
{
    printf("Beginning send_file\n");
   // printf("%s %s\n",param->receiver,param->filepath);
    int ret=0,afd=0,fd=0,flag=0;
    // printf("%s\n",filename);
    struct stat fd_stat;
    //connect to server
    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(CLIENT_SEND_PORT);
    inet_pton(AF_INET,SERVER_IP,&sockaddr.sin_addr.s_addr);
    afd = socket(AF_INET,SOCK_STREAM,0);
    if(afd <0){
        perror("From sending process->Connection Error");
        return ;
    }
    ret = connect(afd,(const struct sockaddr*)&sockaddr,sizeof(sockaddr));
    if(ret < 0){
        perror("From sending process->Connection Error");
        free(param);
        return ;
    }
    ret = stat(param->filename,&fd_stat);
    if(ret < 0){
        perror("get file attribute error");
        return ;
    }
    /*
        send "Send" command to server
        command format Send:receiver:filename
    */
   // printf("before open\n");
   // printf("%s\n",param->filepath);
    fd = open(param->filepath,O_RDONLY);
   // printf("after open\n");
    if(fd < 0){
        perror("From sending process->open file error:");
        close(afd);
        return ;
    }
    //receive reply from server
   // printf("before recv\n");
    ret = recv(afd,&flag,sizeof(flag),0);
    //printf("after recv\n");
    if(ret < 0){
        perror("From sending process->Receive Reply Fail");
        close(afd);
        close(fd);
        free(param);
        return ;
    }
    else if(!ret){
        out_of_connect("From sending process->");
        close(afd);
        close(fd);
        free(param);
        return ;
    }
    //accept and sendfile
    if(!flag){
        printf("From sending process->Fail to Connect Receiver or may be you are the only one in your group!\n");
        close(afd);
        close(fd);
        free(param);
        return ;
    }
    if(flag == 3){
        printf("From sending process->No such person!\n");
        close(afd);
        close(fd);
        free(param);
        return ;
    }
    ret = send(afd,&fd_stat,sizeof(fd_stat),0);
    ssize_t sz = sendfile(afd,fd,NULL,fd_stat.st_size);//zero copy
    shutdown(afd,SHUT_WR);
    if(sz < 0)perror("From sending process->send_file Error");
    //receive reply from server in order to know whether send successfully
    ret = recv(afd,&flag,sizeof(flag),0);
   // printf("After recv\n");
    if(ret < 0){
        perror("From sending process->Receive Reply Fail");
        close(afd);
        close(fd);
        free(param);
        return ;
    }
    else if(!ret){
        out_of_connect("From sending process->");
        close(afd);
        close(fd);
        free(param);
        return ;
    }
    if(flag == 2){
        printf("From sending process->File has been sent to %s successfully!\n",param->receiver);
    }
    else{
        printf("From sending process->send_file unsuccessfully!\n");
    }
   // printf("finish sending!\n");
    close(afd);
    close(fd);
    free(param);
    return ;
}
void data_path(int afd)
{
    printf("Receiving\n");
    char buff[BUFF_SIZE],command[BUFF_SIZE],*rev[2];
    memset(command,0,sizeof(command));
    memset(buff,0,sizeof(buff));
    int recv_bytes,cur_bytes=0;
    struct stat stat_buf;
    recv_bytes = recv(afd,command,sizeof(command),0);
    recv_bytes = recv(afd,&stat_buf,sizeof(stat_buf),0);
    if(recv_bytes < 0){
        perror("From receive process->Recieve file attribute error");
        close(afd);
        return ;
    }
    else if(!recv_bytes){
        out_of_connect("From receive process->");
        close(afd);
        return ;
    }
    /*
        command format sender:filename
    */
    split_command(command,rev);
    int fd = open(rev[1],O_WRONLY|O_CREAT,0644);
    printf("From receive process->Receiveing file from %s,total %d bytes\n"
    ,rev[0],(int)stat_buf.st_size);
    while(1)
    {
        recv_bytes = recv(afd,buff,sizeof(buff),0);
        cur_bytes += recv_bytes;
        if(recv_bytes < 0){
            perror("\nFrom receive process->Recieve filie error");
            close(afd);
            close(fd);
            return ;
        }
        else if(!recv_bytes){
            if(cur_bytes == stat_buf.st_size){
                close(afd);
                close(fd);
                printf("\nFrom receive process->Receive successfully\n");
                return ;
            }
        }
        else{
            write(fd,buff,recv_bytes);
            process_stat(cur_bytes,stat_buf.st_size);
        }
    }
    close(afd);
    return ;
}
void* receive_pthread(void *argu)
{
    pthread_t tid;
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr,PTHREAD_CREATE_DETACHED);
    struct sockaddr_in sockaddr;
    sockaddr.sin_family =AF_INET;
    sockaddr.sin_port = htons(CLIENT_RECEIVE_PORT);
    inet_pton(AF_INET,SERVER_IP,&sockaddr.sin_addr.s_addr);
    int afd,ret;
    ssize_t recv_bytes = 0;
    afd = socket(AF_INET,SOCK_STREAM,0);
    ret = connect(afd,(const struct sockaddr*)&sockaddr,sizeof(sockaddr));
    if(ret < 0){
        perror("From receive process->Connection Error");
        pthread_exit(NULL);
    }
    data_path(afd);
    pthread_exit(NULL);
}

static void* data_receive_pthread(void * p){
    struct DataReceiverPara para = *((struct DataReceiverPara *) p);
    int ret = 0;
    char buf[BUFSIZ];
    for (;;)
    {
        ret = read(para.socket_fd, buf, sizeof(buf));
        write(STDOUT_FILENO, buf, ret);
    }
    pthread_exit(NULL);   //terminate calling thread!
}
void* ctl_pthread(void *p)
{
    pthread_t tid;
    pthread_attr_t pth_attr;
    pthread_attr_init(&pth_attr);
    pthread_attr_setdetachstate(&pth_attr,PTHREAD_CREATE_DETACHED);
    int *afd = (int*)p;
    char command[1024];
    while(1)
    {   
        memset(command,0,sizeof(command));
        recv(*afd,command,sizeof(command),0);
        if(command[0] == 0)pthread_exit(NULL);
        else pthread_create(&tid,&pth_attr,receive_pthread,NULL);
    }
}

void error(const char *str)
{
    perror(str);
    exit(1);
}

int main(void)
{
    struct sockaddr_in sockaddr,ctl_addr;
    sockaddr.sin_family = ctl_addr.sin_family = AF_INET;
    sockaddr.sin_port = htons(CLIENT_CHAT_PORT);
    ctl_addr.sin_port = htons(CLIENT_CTL_PORT);
    //inet_pton(AF_INET,"1.15.231.208",&sockaddr.sin_addr.s_addr);
    inet_pton(AF_INET,SERVER_IP,&sockaddr.sin_addr.s_addr);
    inet_pton(AF_INET,SERVER_IP,&ctl_addr.sin_addr.s_addr);
    int afd = socket(AF_INET,SOCK_STREAM,0);
    int ctl_fd = socket(AF_INET,SOCK_STREAM,0);
    if(afd < 0)perror("socket error");
    int ret = connect(afd,(const struct sockaddr*)&sockaddr,sizeof(sockaddr));
    ret = connect(ctl_fd,(const struct sockaddr*)&ctl_addr,sizeof(ctl_addr));
    if(ret < 0){
        perror("connect error");
        return -1;
    }
    pthread_t tid;
    pthread_attr_t pth_attr;
    pthread_attr_init(&pth_attr);
    pthread_attr_setdetachstate(&pth_attr,PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&tid,&pth_attr,ctl_pthread,(void*)(&ctl_fd));
    if(ret < 0)
    {
        perror("Create receive pthread error");
        return -1;
    }  

    struct DataReceiverPara data_receiver_para;
    data_receiver_para.socket_fd = afd;
    pthread_t receiver_thread_id;
    pthread_attr_t attr1; 
    pthread_attr_init( &attr1 ); 
    pthread_attr_setdetachstate(&attr1,1); 
    if (pthread_create(&receiver_thread_id, &attr1, data_receive_pthread, (void *)(&data_receiver_para)) == -1){
        error("thread create error\n");
    }

    char command[BUFF_SIZE];
    char* recv_buf[4]={0};
    int num,fd;
    Param *param;

    printf("-----Introduction-----\n");
    printf("1.Quit                                --- exit chatroom\n");
    printf("2.ExitGroup                           --- exit the group, enter default group\n");
    printf("3.EnterGroup:<group name>             --- enter the target chat group, then you can only receive data from the group\n");
    printf("4.SetNickname:<new nickname>          --- change nickname\n");
    printf("5.Whisper:<target nickname>:<content> --- whisper to target client with nickname\n");
    printf("6.Send:<target name>:<filename>       --- send file to group or peroson\n");
    printf("7.input other words to send message to chatroom\n");
    printf("firstly, you must input your nickname in chatroom\n");
    printf("enjoy it !\n");
    printf("-----your initial nickname in chatroom-----\n");

    char temp[50];
    char msg[100] = "Init:";
    read_input(temp);
    strcat(msg, temp);
    write(afd, msg, strlen(msg));
    safe_flush(stdin);

    char chatroom_msg[BUFF_SIZE];
    char filepath[100];
    while(1)
    {
        read_input(command);
        //printf("%s\n", command);
        //strcpy(command,"Send:ALL:a"); for test
        num = split_command(command, recv_buf);
        if(strcmp(recv_buf[0], "Send") == 0)//Send:Receiver:Filename
        {   
            if(num == 3){
                if((fd = open(recv_buf[2],O_WRONLY)) <= 0){
                    printf("No such file or directory\n");
                    continue;
                }
                else close(fd);
                param = (Param*)malloc(sizeof(Param));;
                memset(param->receiver,0,sizeof(param->receiver));
                memset(param->filepath,0,sizeof(param->filepath));
                memset(filepath,0,sizeof(filepath));
                strcpy(param->receiver,recv_buf[1]);
                strcpy(param->filepath,recv_buf[2]);
                strcpy(filepath,recv_buf[2]);
                param->filename = split_filename(filepath);
                memset(command,0,sizeof(command));
                sprintf(command,"%s:%s:%s","Send",param->receiver,param->filename);
                //printf("%s\n",command);
                send(afd,command,sizeof(command),0);
                //printf("%s %s %s\n",param->filepath,param->filename,param->receiver);
                send_file(param);
            }
            else if(num == 2)printf("Please Input Filename\n");
            else if(num == 1)printf("Please Input Receiver and Filename\n");
        }
        else{
            memset(chatroom_msg, 0, sizeof(chatroom_msg));
            if (strcmp(recv_buf[0], "quit") == 0 || strcmp(recv_buf[0], "Quit") == 0)
            {
                strcpy(chatroom_msg, "Quit");
                write(afd, chatroom_msg, strlen(chatroom_msg));
                break;
            }
            strcpy(chatroom_msg, "");
            for (int i = 0; i < num; i++){
                strcat(chatroom_msg, recv_buf[i]);
                strcat(chatroom_msg, ":");
            }
            write(afd, chatroom_msg, strlen(chatroom_msg));
            //safe_flush(stdin);
        }
        safe_flush(stdin);
    }
}