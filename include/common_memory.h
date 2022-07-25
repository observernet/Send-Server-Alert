/*****************************************************
 * 프로그램ID	: common_memory.h
 * 프로그램명	: 공유 메모리를 정의한다.
 *****************************************************/
 
#ifndef _COMMON_MEMORY_H
#define _COMMON_MEMORY_H

#include <server_define.h>

/******************************************************************************
 * 서버설정정보
 ******************************************************************************/

typedef struct 
{
	int						is_debug;									/* DEBUG 여부 (0/1) */
} SERVER_CONFIG;

/******************************************************************************
 * 전체 Process 정보
 ******************************************************************************/

/**
 * Process 정보를 담고 있는 Struct
 */
typedef struct
{
	pid_t					pid;										/* Process 고유번호 */
	char					program_name[64];							/* Process 이름 */
} PROCESS_INFO;

/**                                                     			
 * Process Shared Memory                                			
 */                                                     			
typedef struct
{
	int						process_count;								/* 현재 등록된 Process 갯수 */
	int						process_end_ptr;							/* Process Memory의 제일 마지막 데이타 포인터 */
	PROCESS_INFO			process_info[MAX_PROCESS];					/* 등록된 Process 정보 */
} PROCESS_SHM;

/******************************************************************************
 * Common Shared Memory 정보
 ******************************************************************************/

typedef struct
{
	char					program_home[64];							/* 프로그램 Home */
	SERVER_CONFIG			config;										/* 서버 설정 정보 */
	PROCESS_SHM				process;									/* 전체 Process 정보 */

} COMMON_SHM;


#endif
