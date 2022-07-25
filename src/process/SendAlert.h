#ifndef _SEND_ALERT_H
#define _SEND_ALERT_H

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************
 * Defines.....
 *************************************************************************************/

// Telegram
// https://api.telegram.org/bot%APIKEY%/getUpdates
// https://api.telegram.org/bot%APIKEY%/sendMessage?chat_id=%CHATID%&text=%MESSAGE%

typedef struct
{
    char            APIKey[128];
    char            ChatID[64];
} TELEGRAM_INFO;

typedef struct
{
    SOCKET          sockfd;
    char            user_ip[16];
} CLIENT;

/* CURL 데이타 */
typedef struct 
{
	char*           pData;
	size_t          size;
} CURL_DATA_INFO;

/*************************************************************************************
 * Global 변수 정의
 *************************************************************************************/
 
char				program_name[64];
int 				process_id;

int					epoll_fd;
struct epoll_event	events[MAX_SERVER_USER];
struct timeval		timeover;

int				    server_port;
SOCKET			    server_sockfd;
CLIENT              client[MAX_SERVER_USER];

TELEGRAM_INFO       telegram_info;

/*************************************************************************************
 * 함수 정의
 *************************************************************************************/

int     add_epoll(SOCKET fd);
int     del_epoll(SOCKET fd);

int     ReceiveRequest(SOCKET sockfd);
int     SendTelegram(char* mesg);
size_t  CurlWriteData(void *contents, size_t size, size_t nmemb, void *userdata);

int     AcceptUser(SOCKET sockfd);
int     RemoveUser(SOCKET sockfd);

void    InitServer();
void    interrupt(int);

#ifdef __cplusplus
}
#endif

#endif
