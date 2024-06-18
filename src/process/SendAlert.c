/*****************************************************
 * 프로그램ID	: SendAlert.c
 * 프로그램명	: 메세지를 수신해 알림을 전송한다
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>

#include <curl/curl.h>

#include <network.h>
#include <profile.h>
#include <ft_common.h>

#include <SendAlert_interface.h>
#include "SendAlert.h"

/*************************************************************************************
 * 구현 시작..
 *************************************************************************************/
 
int main(int argc, char** argv)
{
	int i, n, nread;
	
	SetProgramName(argv[0]);
	
	/* 서버를 초기화 한다. */
	InitServer();

	while ( 1 )
    {
    	/* 소켓이벤트를 체크한다. (0.01초) */
    	n = epoll_wait(epoll_fd, events, MAX_SERVER_USER, 20);
		if ( n < 0 )
		{
			if ( errno != EINTR ) Log("main: epoll_wait Error [%d]\n", errno);
			usleep(3000);
		}
		
		for ( i = 0 ; i < n ; i++ )
		{
			if ( events[i].data.fd == server_sockfd )
			{
				/* 클라이언트를 받아들인다. */
				if ( AcceptUser(server_sockfd) == -1 )
					Log("main: User를 받아 들이는데 실패하였습니다.\n");
			}
			else
			{
				ioctl(events[i].data.fd, FIONREAD, &nread);
		
				/* 클라이언트 제거 */
				if ( nread == 0 )
					RemoveUser(events[i].data.fd);
		
				/* 클라이언트 요청을 받아들인다. */
				else
					ReceiveRequest(events[i].data.fd);
			}
		}
	}
	
	interrupt(0);
	
	exit(EXIT_SUCCESS);
}

/*
 * fd를 등록한다.
 */
int add_epoll(SOCKET fd)
{
    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = fd;
    if ( epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1 )
    {
        Log("add_epoll: epoll_ctl EPOLL_CTL_ADD Failed!! errno[%d]\n", errno);
        return (-1);
    }

    return (0);
}

/*
 * fd를 해제한다.
 */
int del_epoll(SOCKET fd)
{
    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = fd;
    if ( epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) == -1 )
    {
        Log("del_epoll: epoll_ctl EPOLL_CTL_DEL Failed!! errno[%d]\n", errno);
        return (-1);
    }

    return (0);
}

/*************************************************************************************
 * 요청 데이타 처리 함수
 *************************************************************************************/

/**
 * 요청을 받아들인다
 */
int ReceiveRequest(SOCKET sockfd)
{
    int res, rcv, length;
    char rcvbuf[MAX_PACKET];

    int user_offset = GetUserOffset(sockfd);
    if ( user_offset == -1 )
    {
        Log("ReceiveRequest: 사용자 정보를 찾을수 없습니다. sockfd[%d]\n", sockfd);
        del_epoll(sockfd); CloseSocket(sockfd);
        return (-1);
    }

	/* 사이즈를 읽어온다 읽어온다. */
    memset(rcvbuf, 0x00, MAX_PACKET);
    if ( (res = ReceiveTCP(sockfd, rcvbuf, SAI_LENGTH_SIZE, &timeover)) <= 0 )
    {
        Log("ReceiveRequest: 사이즈 수신에 실패하였습니다. sockfd[%d] errno[%d]\n", sockfd, errno);
        RemoveUser(sockfd);
        return (-1);
    }
    rcv = res;

    /* 나머지 데이타를 읽어온다 */
    if ( (length = atoi(rcvbuf)) > 0 )
    {
        if ( (res = ReceiveTCP(sockfd, rcvbuf + SAI_LENGTH_SIZE, length, &timeover)) <= 0 )
        {
            Log("ReceiveRequest: 데이타 수신에 실패하였습니다. sockfd[%d] errno[%d]\n", sockfd, errno);
            RemoveUser(sockfd);
            return (-1);
        }
        rcv += res;
    }

    /* 로그를 기록한다 */
    Log("ReceiveRequest: rcvbuf[%d:%s]\n", strlen(rcvbuf), rcvbuf);
    
    /* 텔레그램에 데이타를 전송한다 */
    SendTelegram(rcvbuf+SAI_LENGTH_SIZE);

    return (0);
}

/**
 * 텔레그램에 메세지를 전달한다
 */
int SendTelegram(char* mesg)
{
    char requrl[MAX_PACKET];
    CURL *curl;
	CURLcode ret;
	CURL_DATA_INFO curlData;
	
    /* 요청할 데이타를 생성한다 */
    sprintf(requrl, "https://api.telegram.org/bot%s/sendMessage?chat_id=%s&text=%s", telegram_info.APIKey, telegram_info.ChatID, mesg);

	/* cURL을 이용해 데이타를 가져온다 */
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(curl, CURLOPT_URL, requrl);
	
	//struct curl_slist *headers = NULL;
	//headers = curl_slist_append(headers, "Content-Type: application/text");
	//curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteData);
	
	curlData.size = 0;
 	curlData.pData = (char*)calloc(1, sizeof(char *) * 1);
 	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlData);
	
	ret = curl_easy_perform(curl);
	if ( ret != CURLE_OK )
	{
		Log("RequestAirKorea: Curl Error!! [%s]\n", curl_easy_strerror(ret));
		return (-1);
	}
	curl_easy_cleanup(curl);

	if ( curlData.size <= 0 )
	{
		Log("SendTelegram: 수신데이타가 없습니다.\n");
		return (-1);
	}

	Log("SendTelegram: Recv[%s]\n", curlData.pData);

    free(curlData.pData);
	return (0);
}

// 응답 패킷 내용 쓰는 함수(콜백)
size_t CurlWriteData(void *contents, size_t size, size_t nmemb, void *userdata)
{
	size_t realsize = size * nmemb;
	CURL_DATA_INFO *mem = (CURL_DATA_INFO *)userdata;

	// 여러번 콜백될 수 있어 기존 데이터에 이어붙인다.
	mem->pData = (char *)realloc(mem->pData, mem->size + realsize + 1);
	if (mem->pData == NULL)
	{
		fprintf(stderr, "realloc failed\n");
		return 0;
	}

	memcpy(&(mem->pData[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->pData[mem->size] = 0;

	return realsize;
}

/*************************************************************************************
 * 사용자 소켓 처리사항
 *************************************************************************************/

/**
 * 사용자를 받아들인다.
 */
int AcceptUser(SOCKET sockfd)
{
	SOCKET fd;
	
	int offset;
	char ip[15];

	CLIENT user;
	
	/* 사용자소켓을 받아들인다. */
	if ( (fd = GetClientSocket(sockfd, ip)) == -1 )
	{
		Log("AcceptUser: 사용자를 받아들일 수 없습니다. [%d]\n", errno);
		return (-1);
	}

	/* 사용자 데이타 설정 */
	memset( &user, 0x00, sizeof(CLIENT) );
	user.sockfd = fd;
    strcpy(user.user_ip, ip);
	
	/* 빈공간을 찾아서 할당한다. */
	for ( offset = 0 ; offset < MAX_SERVER_USER ; offset++ )
	{
		if ( client[offset].sockfd == 0 )
			break;
	}
	if ( offset >= MAX_SERVER_USER )
	{	
		Log("AcceptUser: 허용된 최대 사용자가 초과되었습니다.\n");
		CloseSocket(fd);
		return (-1);
	}
	
	/* 사용자 접속 최종 허용 */
	memcpy( &client[offset], &user, sizeof(CLIENT) );

	add_epoll(client[offset].sockfd);
	Log("사용자 접속: offset[%d] sockfd[%d] ip[%s]\n", offset, client[offset].sockfd, client[offset].user_ip);
	
	return (offset);
}

/**
 * 사용자 연결을 종료한다.
 */
int RemoveUser(SOCKET sockfd)
{
    int offset = GetUserOffset(sockfd);
    if ( offset == -1 )
    {
        del_epoll(sockfd); CloseSocket(sockfd);
        return (0);
    }

	usleep(10000);
	del_epoll(client[offset].sockfd);
	CloseSocket(client[offset].sockfd);
	
	Log("사용자 종료: offset[%d] sockfd[%d] ip[%s]\n", offset, client[offset].sockfd, client[offset].user_ip);
	memset( &client[offset], 0x00, sizeof(CLIENT) );
	
	return (0);
}

/**
 * User Offset을 가져온다
 */
int GetUserOffset(SOCKET sockfd)
{
    int offset;

    for ( offset = 0 ; offset < MAX_SERVER_USER ; offset++ )
    {
        if ( client[offset].sockfd == sockfd )
            return (offset);
    }

    return (-1);
}

/*************************************************************************************
 * 초기화함수
 *************************************************************************************/

/**
 * 서버를 초기화한다.
 */ 
void InitServer()
{
    char conf_file_name[256];
	
	/* 공유메모리를 초기화 한다. */
	InitCommonMemory();
	
	/* 설정파일을 연다. */
	sprintf(conf_file_name, "%s/%s/%s", mdb->program_home, CONF_PATH, SERVER_CONFIG_FILE);
	if ( !OpenProfile(conf_file_name) )
	{
		Log("InitServer: 설정파일을 여는 도중 에러가 발생하였습니다. file[%s] errno[%d]\n", conf_file_name, errno);
		exit(EXIT_FAILURE);
	}
	
	/* 설정파일의 값을 가져온다. */
	server_port = GetProfileInt("SERVER_INFO", "SEND_ALERT_PORT", 0);
	if ( server_port == 0 )
	{
		Log("InitServer: 서버포트 정보가 없습니다.\n");
		exit(EXIT_FAILURE);
	}

    /* 텔레그램정보를 가져온다 */
    memset(&telegram_info, 0x00, sizeof(TELEGRAM_INFO));
    GetProfileString("TELEGRAM", "APIKEY", telegram_info.APIKey);
    GetProfileString("TELEGRAM", "CHATID", telegram_info.ChatID);
	
	/* 설정파일을 닫는다. */
	CloseProfile();

	/* epoll을 생성한다. */
    if ( (epoll_fd = epoll_create(MAX_SERVER_USER)) < 0 ) 
    { 
        Log("InitServer: epoll 생성 도중 에러가 발생하였습니다. errno[%d]\n", errno);
		exit(EXIT_FAILURE);
    }
    
	/* 서버 소켓을 생성한다. */
	server_sockfd = GetServerSocket(server_port, MAX_SERVER_USER);
	if ( server_sockfd == -1 )
	{
		Log("InitServer: 서버 소켓 생성에 실패 하였습니다. port[%d] errno[%d]\n", server_port, errno);
		exit(EXIT_FAILURE);
	}
	add_epoll(server_sockfd);
	
	/* 변수를 초기화한다. */
	memset(client, 0x00, sizeof(CLIENT)*MAX_SERVER_USER);
	timeover.tv_sec = 5; timeover.tv_usec = 0;

    /* Process를 등록한다. */
	if ( (process_id = RegistProcess(_PROC_SEND_ALERT_)) == -1 )
	{
		Log("InitServer: 프로세스 실행에 실패하였습니다.\n");
		exit(EXIT_FAILURE);
	}
	
	/* 시스널 핸들러 설정 */
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, interrupt);
    signal(SIGTERM, interrupt);
    signal(SIGQUIT, interrupt);
    
	Log("InitServer: 서버 초기화! Process Start [%d]..................................\n", process_id);
    Log("InitServer: TCP Server Port [%d]\n", server_port);
}

/**
 * 서버를 종료한다.
 */
void interrupt(int sig)
{
    int i;

	/* 연결된 사용자 모두 종료 */
	for ( i = 0 ; i < MAX_SERVER_USER ; i++ )
	{
		if ( client[i].sockfd )
			RemoveUser(client[i].sockfd);
	}
	CloseSocket(server_sockfd);
	
	/* epoll 제거 */
	close(epoll_fd);

	/* 프로세스 등록 해제 */
	RemoveProcess(_PROC_SEND_ALERT_);
	DeAttachShm((void*)mdb);
	
    exit(EXIT_SUCCESS);
}
