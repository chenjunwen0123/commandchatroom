#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<pthread.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<errno.h>
#include<assert.h>
#include<netinet/in.h>
#include<string.h>
#include<sys/stat.h>

#define PORT 14444
#define BUFFER_LENGTH 1024
#define MAX_CONN_LIMIT 512
#define CLIENT_SEND_PORT 14446
#define CLIENT_RECEIVE_PORT 14447
#define CLIENT_CTL_PORT 14445

enum Color {
      WHITE = 1, RED, GREEN, BLUE, YELLOW
};

typedef struct ClientNode{
    int socket_fd;
    int send_socket;
    int receive_socket;
    int ctl_socket;
    char nickname[50];
    char group_name[50];
    pthread_t thread_id;
    char Client_ip[512];
    struct ClientNode *next;
    struct ClientNode *root;
}Client;
typedef struct Param{
    char command[4][50];
    Client *p;
}Param;

//pthread pool
typedef struct tpool_work_t{
    void*(*work)(void*);
    void *argu;
    struct tpool_work_t* next;
}tpool_work_t;
typedef struct tpool{
    int shutdown;
    int max_pthread;
    pthread_t *tid;
    tpool_work_t* work_head;
    pthread_cond_t queue_ready;
    pthread_mutex_t queue_lock;
}tpool_t;
tpool_t* tpool;
void* work_routine(void *argu)
{
    tpool_t *tpool = (tpool_t*)argu;
    tpool_work_t *job = NULL;
    while(1)
    {
        pthread_mutex_lock(&(tpool->queue_lock));
        while(!(tpool->work_head) && !(tpool->shutdown)){
            pthread_cond_wait(&(tpool->queue_ready),&(tpool->queue_lock));
        }
        if(tpool->shutdown){
            pthread_mutex_unlock(&(tpool->queue_lock));
            pthread_exit(NULL);
        }
        job = tpool->work_head;
        tpool->work_head = (tpool_work_t*)tpool->work_head->next;
        pthread_mutex_unlock(&(tpool->queue_lock));

        job->work(job->argu);
        free(job);
    }
    pthread_exit(NULL);
}
tpool_t* create_pool(int max_pthread)
{
    tpool_t *tpool = (tpool_t*)malloc(sizeof(tpool_t));
    tpool->shutdown = 0;
    tpool->max_pthread = max_pthread;
    tpool->tid = (pthread_t*)malloc(sizeof(pthread_t)*max_pthread);
    tpool->work_head = NULL;
    pthread_mutex_init(&(tpool->queue_lock),NULL);
    pthread_cond_init(&(tpool->queue_ready),NULL);
    for(int i=0;i<max_pthread;i++)
    pthread_create(&(tpool->tid[i]),NULL,work_routine,(void*)tpool);
    return tpool;
}
int add_task(tpool_t *tpool,void*(routine)(void*),void* argu)
{
    if(routine == NULL || tpool == NULL)return -1;
    tpool_work_t* tpool_work = (tpool_work_t*)malloc(sizeof(tpool_work_t));
    tpool_work->work = routine;
    tpool_work->argu = argu;
    tpool_work->next = NULL;
    pthread_mutex_lock(&(tpool->queue_lock));
    if(tpool->work_head == NULL)tpool->work_head = tpool_work;
    else{
        tpool_work_t *p = tpool->work_head;
        while(p->next != NULL)p = p->next;
         p->next = tpool_work;
    }
    pthread_cond_signal(&(tpool->queue_ready));
    pthread_mutex_unlock(&(tpool->queue_lock));
    return 1;
}
void destory_tpool(tpool_t *tpool)
{
    tpool_work_t *tmp;
    if(tpool->shutdown == 1)return ;
    tpool->shutdown = 1;
    pthread_mutex_lock(&(tpool->queue_lock));
    pthread_cond_broadcast(&(tpool->queue_ready));
    pthread_mutex_unlock(&(tpool->queue_lock));
    for(int i=0;i<tpool->max_pthread;i++)
    pthread_join(tpool->tid[i],NULL);
    free(tpool-> tid);
    while(tpool->work_head)
    {
        tmp = tpool->work_head;
        tpool->work_head = tpool->work_head->next;
        free(tmp);
    }
    pthread_mutex_destroy(&(tpool->queue_lock));
    pthread_cond_destroy(&(tpool->queue_ready));
    free(tpool);
}

void format_color_string(char* string, char* out, enum Color color){
    strcpy(out, "");
    switch (color)
    {
    case WHITE:
        strcat(out, "\033[37m");
        strcat(out, string);
        strcat(out, "\033[0m");
        break;
    case RED:
        strcat(out, "\033[31m");
        strcat(out, string);
        strcat(out, "\033[0m");
        break;
    case GREEN:
        strcat(out, "\033[32m");
        strcat(out, string);
        strcat(out, "\033[0m");
        break;
    case BLUE:
        strcat(out, "\033[34m");
        strcat(out, string);
        strcat(out, "\033[0m");
        break;
    case YELLOW:
        strcat(out, "\033[33m");
        strcat(out, string);
        strcat(out, "\033[0m");
        break;
    
    default:
        break;
    }
}

Client * init_client(){
    Client * p=(Client*)malloc(sizeof(Client));
    return p;
}
//p为原链表，elem表示新数据元素，add表示新元素要插入的位置
Client * insert_elem(Client * p, Client* elem, int add) {
    Client * temp = p;//创建临时结点temp
    //首先找到要插入位置的上一个结点
    for (int i = 1; i < add; i++) {
        temp = temp->next;
        if (temp == NULL) {
            printf("插入位置无效\n");
            return p;
        }
    }
    //向链表中插入结点
    elem->next = temp->next;
    temp->next = elem;
    return p;
}
//p为原链表，node为要删除元素的值
Client * del_elem(Client * p, Client node) {
    Client * temp = p;
    //由于头节点的存在，因此while中的判断为t->next
    while (temp->next != NULL) {
        if (temp->next->socket_fd == node.socket_fd) break;
        temp=temp->next;
    }
    Client * del = temp->next;//单独设置一个指针指向被删除结点，以防丢失
    temp->next = temp->next->next;//删除某个结点的方法就是更改前一个结点的指针域
    free(del);//手动释放该结点，防止内存泄漏
    return p;
}
int get_length(Client * p){
//新建一个指针t，初始化为头指针 p
    Client * t=p;
    int i=1;
    //由于头节点的存在，因此while中的判断为t->next
    while (t->next != NULL) {
        t=t->next;
        i++;
    }
    //程序执行至此处，表示查找失败
    return i;
}
static void* client_handler(void * p);
void split(char *src,const char *separator,char **dest,int *num);

void err(const char* str){
    perror(str);
    exit(1);
}
void* file_transport(void *argu)
{
    Param* param = (Param*)argu;
    struct sockaddr_in sockaddr;
    socklen_t socklen;
    int send_feedback = accept(param->p->send_socket,(struct sockaddr *)&sockaddr,&socklen);
    //printf("after accpet");
    if(send_feedback < 0){
        perror("accpet error");
        free(param);
        return NULL;
    }
    int receive_fd[MAX_CONN_LIMIT],num=0,flag;
    int afd;
    if(strcmp(param->command[1],"Group") == 0)
    {
        Client *p = param->p->root->next;
        while(p != NULL)
        {
            if(strcmp(param->p->group_name,p->group_name) == 0 && strcmp(param->p->nickname,p->nickname) != 0)
            {
                send(p->ctl_socket,"Receive",8,0);
                afd = accept(p->receive_socket,(struct sockaddr*)&sockaddr,&socklen);
                if(afd > 0)receive_fd[num++] = afd;
            }
            p = p->next;
        }
    }
    else{
        Client *p = param->p->root->next;
        while(p!= NULL && strcmp(p->nickname,param->command[1]) != 0)
        p = p->next;
        if(p == NULL){
            printf("No such person.\n");
            flag = 3;
            send(send_feedback,&flag,sizeof(flag),0);
            free(param);
            return NULL;
        }
        else{
            send(p->ctl_socket,"Receive",8,0);
            afd = accept(p->receive_socket,(struct sockaddr*)&sockaddr,&socklen);
            if(afd >0)receive_fd[num++] = afd;
        }
    }
   // printf("%d\n",num);
    if(!num){
        flag = 0;
        send(send_feedback,&flag,sizeof(flag),0);
        shutdown(send_feedback,SHUT_RDWR);
        free(param);
        printf("Send unsuccessfully!\n");
        return NULL;
    }
    flag = 1;
    char buff[BUFFER_LENGTH],command[BUFFER_LENGTH];
    memset(buff,0,sizeof(buff));
    memset(command,0,sizeof(command));
   // printf("before send\n");
    ssize_t bytes = send(send_feedback,&flag,sizeof(flag),0);
    if(bytes < 0){
        perror("send error");
    }
    //printf("%d\n",bytes);
    struct stat stat_buf;
    int cur_bytes = 0,ret;
    bytes = recv(send_feedback,&stat_buf,sizeof(stat_buf),0);
    sprintf(command,"%s:%s",param->p->nickname,param->command[2]);
    // printf("%s\n",command);
    for(int i=0;i<num;i++)
    send(receive_fd[i],&command,sizeof(command),0);
    for(int i=0;i<num;i++)
    send(receive_fd[i],&stat_buf,sizeof(stat_buf),0);
    while(1)
    {
        bytes = recv(send_feedback,buff,sizeof(buff),0);
        cur_bytes += bytes;
        if(bytes < 0){
            perror("recv error");
            flag = 3;
            for(int i=0;i<num;i++)shutdown(receive_fd[i],SHUT_WR);
            send(send_feedback,&flag,sizeof(flag),0);
            free(param);
            return NULL;
        }
        else if(!bytes){
            if(cur_bytes == stat_buf.st_size){
                printf("Transport Finish!\n");
                flag = 2;
                send(send_feedback,&flag,sizeof(flag),0);
                for(int i=0;i<num;i++)shutdown(receive_fd[i],SHUT_WR);
                free(param);
                return NULL;
            }
            else{
                printf("Transport Interupted!\n");
            }
                flag = 3;
                send(param->p->socket_fd,&flag,sizeof(flag),0);
                for(int i=0;i<num;i++)shutdown(receive_fd[i],SHUT_WR);
                free(param);
                return NULL;
        }
        else{
            for(int i=0;i<num;i++)
            {
                ret = send(receive_fd[i],buff,bytes,0);
                if(ret < 0){
                    printf("Somebody fail to receive\n");
                    flag = 3;
                    send(send_feedback,&flag,sizeof(flag),0);
                    for(int i=0;i<num;i++)shutdown(receive_fd[i],SHUT_WR);
                    free(param);
                    return NULL;
                }
            }
        }
    }
}

int main(){
    
    int total_connection_count = 0;

    int sockfd_server, accept_feedback, result, sockfd_send, sockfd_receive, sockfd_ctl;
    int send_feedback, receive_feedback,ctl_feedback;
    Client *clients = init_client();
    clients->next = NULL;

    char buf[BUFSIZ], client_IP[1024];
    memset(client_IP,0,sizeof(client_IP));
    
    struct sockaddr_in server_address, client_address;
    struct sockaddr_in send_address, receive_address, ctl_address;
    socklen_t clt_addr_len;

    ctl_address.sin_family = AF_INET;
    ctl_address.sin_port = htons(CLIENT_CTL_PORT);
    ctl_address.sin_addr.s_addr = htonl(INADDR_ANY);


    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    sockfd_server = socket(AF_INET,SOCK_STREAM,0);
    sockfd_send = socket(AF_INET,SOCK_STREAM,0);
    sockfd_receive = socket(AF_INET,SOCK_STREAM,0);
    sockfd_ctl = socket(AF_INET,SOCK_STREAM,0);

    if(sockfd_server < 0) err("server_socket error\n");
    if(sockfd_send < 0) err("send_socket error\n");;
    if(sockfd_receive < 0) err("receive_socket error\n");
    if(sockfd_ctl < 0) err("ctl_socket error\n");
    result = bind(sockfd_server,(struct sockaddr*)&server_address,sizeof(server_address));
    if(result < 0) err("server bind error\n");
    result = bind(sockfd_ctl,(struct sockaddr*)&ctl_address,sizeof(ctl_address));
    send_address.sin_family = AF_INET;
    send_address.sin_port = htons(CLIENT_SEND_PORT);
    send_address.sin_addr.s_addr = htonl(INADDR_ANY);
    result = bind(sockfd_send,(struct sockaddr*)&send_address,sizeof(send_address));
    if(result < 0) err("send bind error\n");

    receive_address.sin_family = AF_INET;
    receive_address.sin_port = htons(CLIENT_RECEIVE_PORT);
    receive_address.sin_addr.s_addr = htonl(INADDR_ANY);
    result = bind(sockfd_receive,(struct sockaddr*)&receive_address,sizeof(receive_address));
    if(result < 0) err("receive bind error\n");

    result = listen(sockfd_server,128);
    result = listen(sockfd_send,128);
    result = listen(sockfd_receive,128);
    result = listen(sockfd_ctl,128);
    if(result < 0) err("listen error\n");

    tpool = create_pool(5);
    while (1) {
        pthread_t thread_id;
        clt_addr_len = sizeof(client_address);
        accept_feedback = accept(sockfd_server, (struct sockaddr*) &client_address, &clt_addr_len);
        if(accept_feedback < 0) err("accept error\n");
        // send_feedback = accept(sockfd_send,NULL,NULL);
        // if(send_feedback < 0) err("send accept error\n");
        ctl_feedback = accept(sockfd_ctl,(struct sockaddr*)&receive_address,&clt_addr_len);
        if(receive_feedback < 0) err("receive accept error\n");
        printf("client IP: %s ,port:%d\n",
            inet_ntop(AF_INET, &client_address.sin_addr.s_addr, client_IP, sizeof(client_IP)),
            ntohs(client_address.sin_port)
        );

        pthread_attr_t attr; 
        pthread_attr_init( &attr ); 
        pthread_attr_setdetachstate(&attr,1); 
        Client* new_client = (Client*)malloc(sizeof(Client));
        new_client->socket_fd = accept_feedback;
        new_client->send_socket = sockfd_send;
        new_client->receive_socket = sockfd_receive;
        new_client->root = clients;
        new_client->ctl_socket = ctl_feedback;
        strcpy(new_client->Client_ip,client_IP);
        strcpy(new_client->group_name, "DefaultGroup");
        strcpy(new_client->nickname, "DefaultNickName");
        

        if (pthread_create(&thread_id, &attr, client_handler, (void *)(new_client)) == -1){
            err("thread create error\n");
            break;
        }

        insert_elem(clients, new_client, get_length(clients));

        new_client->thread_id = thread_id;

        total_connection_count += 1;

        printf("total_connection_count:%d\n", total_connection_count);

    }

    result = shutdown(sockfd_server, SHUT_WR);
    assert (result != -1);

    printf("server shut down!\n");
    destory_tpool(tpool);
    return 0;
}

void get_chatroom_status(Client* client, char* out){
    Client* t = client->root;
    sprintf(out, "-----ChatRoom Status-----\nclients count:%d\n-----clients-----\n", get_length(t) - 1);
    while (t->next != NULL) {
        t = t->next;
        strcat(out, "[");
        strcat(out, t->group_name);
        strcat(out, "]");
        strcat(out, t->nickname);
        strcat(out, "\n");
    }
}

static void* client_handler(void * p){
    Client* client = ((Client *) p);
    int i_reveive_bytes;
    char data_recv[BUFFER_LENGTH];         //存 客户端发送的信息
    char data_send[BUFFER_LENGTH];         //存 要发送给客户端的信息
    char data_chatroom_send[BUFFER_LENGTH];//存 服务器需要广播通知的信息
    
    pthread_t tid;
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr,PTHREAD_CREATE_DETACHED);
    
    while(1)
    {
        //printf("waiting for request...\n");
        //Reset data.
        memset(data_recv, 0, BUFFER_LENGTH);
        memset(data_send, 0, BUFFER_LENGTH);
        memset(data_chatroom_send, 0, BUFFER_LENGTH);

        i_reveive_bytes = read(client->socket_fd, data_recv, BUFFER_LENGTH);  //不断读取客户端的输入请求
        if(i_reveive_bytes == 0)                     //没有读取到客户端输入
        {
            printf("maybe the client has closed\n");
            break;
        }
        if(i_reveive_bytes == -1)                  //读取失败
        {
            fprintf(stderr, "read error!\n");
            break;
        }

        //如果读取到了输入
        char* revbuf[4] = {0};     //存放分割后的子字符串 
	
        //分割后子字符串的个数
        int num = 0;
        split(data_recv, ":", revbuf, &num); 
        //调用函数对data_recv按冒号分割，分割后的子字符串存入revbuf，num为子字符串数量

        if (num > 0){   //说明读取到客户端输入
            if(strcmp(revbuf[0], "Quit") == 0)   //客户端输入Quit
            {
                printf("quit command!\n");
                strcpy(data_send, "[ChatRoom]");
                strcat(data_send, client->nickname);
                strcat(data_send, ":");
                strcat(data_send, "left the chatroom!");
                strcat(data_send, "\n");
                format_color_string(data_send, data_chatroom_send, BLUE);    //将data_send的字体转成蓝色存入data_chatroom_send
                Client* t = client->root;
                while (t != NULL) {  //广播这个用户退出的信息
                    write(t -> socket_fd, data_chatroom_send, strlen(data_chatroom_send));
                    t = t->next;
                }
                break;                           //Break the while loop.
            }
        }

        if(num == 3 && strcmp(revbuf[0],"Send") == 0){     //如果用户发起文件传输
            Param *param = (Param*)malloc(sizeof(Param));
            memset(param->command,0,sizeof(param->command));
            for(int i=0;i<num;i++)strcpy(param->command[i],revbuf[i]);
            param->p = client;
            add_task(tpool,file_transport,(void*)param);
        }

        else if (num == 3){       
            if(strcmp(revbuf[0], "Whisper") == 0){     //如果用户发起私聊
                char* target_nickname = revbuf[1];     //目标用户
                char* content = revbuf[2];             //私聊内容
                Client* t = client->root;
                while (t != NULL) {
                    if (strcmp(t->nickname, target_nickname) == 0){       //找到目标用户
                        strcpy(data_send, "[ChatRoom]Whisper from ");
                        strcat(data_send, client->nickname);
                        strcat(data_send, ":");
                        strcat(data_send, content);
                        strcat(data_send, "\n");
                        format_color_string(data_send, data_chatroom_send, YELLOW);    //格式化私聊的内容，转为蓝色字体
                        write(t -> socket_fd, data_chatroom_send, strlen(data_chatroom_send));//将格式化后的内容发送给目标客户端
                    }
                    t = t->next;
                }
            }
        }
        else if (num == 2){
            if (strcmp(revbuf[0], "EnterGroup") == 0){    //如果用户想加入群组，群组的名字在revbuf[1]
                strcpy(client->group_name, revbuf[1]);    //填补客户端的群组信息
                strcpy(data_send, "[ChatRoom]you have entered group:");
                strcat(data_send, client->group_name);
                strcat(data_send, "\n");
                format_color_string(data_send, data_chatroom_send, RED);  //格式化客户端加入群组信息
                write(client->socket_fd, data_chatroom_send, strlen(data_chatroom_send)); //用红色字体告知用户成功加入群组
            }
            if (strcmp(revbuf[0], "Init") == 0){         //用户初始化，输入Init:Nickname，以Nickname加入聊天室
                strcpy(client->nickname, revbuf[1]);
                Client* t = client->root;                
                while (t != NULL) {                      //
                    strcpy(data_send, "[ChatRoom]");
                    strcat(data_send, client->nickname);
                    strcat(data_send, " entered chatroom!\n");
                    format_color_string(data_send, data_chatroom_send, BLUE);
                    write(t -> socket_fd, data_chatroom_send, strlen(data_chatroom_send));
                    t = t->next;
                }
            }
            if(strcmp(revbuf[0], "SetNickname") == 0)    //改昵称
            {
                strcpy(client->nickname, revbuf[1]);
                strcpy(data_send, "[ChatRoom]you have changed nickname:");
                strcat(data_send, client->nickname);
                strcat(data_send, "\n");
                format_color_string(data_send, data_chatroom_send, RED);
                write(client->socket_fd, data_chatroom_send, strlen(data_chatroom_send));   //红色字体通知用户更改成功
            }
        }
        else if (num == 1){
            if (strcmp(revbuf[0], "ExitGroup") == 0){        //退出当前群组，则加入默认组
                strcpy(client->group_name, "DefaultGroup");
                strcpy(data_send, "[ChatRoom]you have exited group, and entered default group\n");
                format_color_string(data_send, data_chatroom_send, RED);
                write(client->socket_fd, data_chatroom_send, strlen(data_chatroom_send));
            }
            if (strcmp(revbuf[0], "Status") == 0){          //返回当前聊天室在线用户列表 

                get_chatroom_status(client, data_send);
                format_color_string(data_send, data_chatroom_send, RED);
                write(client->socket_fd, data_chatroom_send, strlen(data_chatroom_send));
            }
            else{                               //无特殊命令的聊天
                Client* t = client->root;
                //由于头节点的存在，因此while中的判断为t->next
                while (t != NULL) {
                    strcpy(data_send, "[ChatRoom]");
                    if (strcmp(client->group_name, "DefaultGroup") == 0) strcat(data_send, "");  //如果用户是默认群组，则不做操作
                    else {
                        strcat(data_send, "[");
                        strcat(data_send, client->group_name);
                        strcat(data_send, "]");
                    }
                    if (strcmp(client->group_name, t->group_name) == 0){   //找到同组的用户
                        if (t->socket_fd != client->socket_fd){       
                            strcat(data_send, client->nickname);
                        }
                        else{
                            strcat(data_send, client->nickname);
                            strcat(data_send, "(you)");   //因为是广播，广播的时候发送信息的用户会有一个(you)标识
                        }
                        strcat(data_send, ":");
                        strcat(data_send, revbuf[0]);    //revbuf[0] 即要发送的信息内容
                        strcat(data_send, "\n");         
                        format_color_string(data_send, data_chatroom_send, GREEN);  
                        write(t -> socket_fd, data_chatroom_send, strlen(data_chatroom_send));  //聊天信息以绿色字体广播给同群组的成员客户端
                    }
                    t = t->next;
                }
            }
        }


        printf("---read from client---\nclient_id : %d\nnickname : %s\n", client->socket_fd, client->nickname);
        for(int i=0;i<num;i++)
        printf("content%d:%s\n",i+1,revbuf[i]);
    }
    //假如用户输入Quit退出聊天室，就会跳出while循环执行下面的代码
    del_elem(client->root, *client);     //将用户从用户列表中删除

    //Clear
    printf("terminating current client_connection...\n");
    close(client->socket_fd);            //将该用户的socket套接字关闭.
    pthread_exit(NULL);   //terminate calling thread!
}



void split(char *src,const char *separator,char **dest,int *num) {
	/*
		src 源字符串的首地址(buf的地址) 
		separator 指定的分割字符
		dest 接收子字符串的数组
		num 分割后子字符串的个数
	*/
     char *pNext;
     int count = 0;
     if (src == NULL || strlen(src) == 0) //如果传入的地址为空或长度为0，直接终止 
        return;
     if (separator == NULL || strlen(separator) == 0) //如未指定分割的字符串，直接终止 
        return;
     pNext = (char *)strtok(src,separator); //必须使用(char *)进行强制类型转换(虽然不写有的编译器中不会出现指针错误)
     while(pNext != NULL) {
          *dest++ = pNext;
          ++count;
         pNext = (char *)strtok(NULL,separator);  //必须使用(char *)进行强制类型转换
    }  
    *num = count;
} 