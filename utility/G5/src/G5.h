/*
 * LB Dispatch - G5
 * Author  : calvin
 * Email   : calvinwillliams.c@gmail.com
 * History : 2014-03-29 v1.0.0   create
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef _H_G5_
#define _H_G5_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>

#define VERSION		"1.0.0"

/*
config file format :
rule_id	mode	client_addr -> forward_addr -> server_addr ;
	mode		G  : manage port
			MS : master/slave mode
			RR : round & robin mode
			LC : least connection mode
			RT : response Time mode
			RD : random mode
			HS : hash mode
		client_addr	format : ip1.ip2.ip3.ip4:port
				ip1~4,port allow use '*' or '?' for match
				one or more address seprating by blank
		forward_addr	format : ip1.ip2.ip3.ip4:port
				one or more address seprating by blank
		server_addr	format : ip1.ip2.ip3.ip4:port
				one or more address seprating by blank
demo :
admin	G	192.168.1.79:* - 192.168.1.54:8060 ;
webdog	MS	192.168.1.54:* 192.168.1.79:* - 192.168.1.54:8079 > 192.168.1.79:8089 192.168.1.79:8090 ;
hsbl	LB	192.168.1.*:* - 192.168.1.54:8080 > 192.168.1.79:8089 192.168.1.79:8090 192.168.1.79:8091 ;

manage port command :
	ver
	list rules
	add rule ...
	modify rule ...
	remove rule ...
	dump rule
	list forwards
	quit
demo :
	add rule webdog2 MS 1.2.3.4:1234 - 192.168.1.54:1234 > 4.3.2.1:4321 ;
	modify rule webdog2 MS 4.3.2.1:4321 - 192.168.1.54:1234 > 1.2.3.4:1234 ;
	remove rule webdog2 ;
*/

#define FOUND				9	/* 找到 */
#define NOT_FOUND			4	/* 找不到 */

#define MATCH				1	/* 匹配 */
#define NOT_MATCH			-1	/* 不匹配 */

#define RULE_ID_MAXLEN			64	/* 最长转发规则名长度 */
#define RULE_MODE_MAXLEN		2	/* 最长转发规则模式长度 */

#define FORWARD_RULE_MODE_G		"G"	/* 管理端口 */
#define FORWARD_RULE_MODE_MS		"MS"	/* 主备模式 */
#define FORWARD_RULE_MODE_RR		"RR"	/* 轮询模式 */
#define FORWARD_RULE_MODE_LC		"LC"	/* 最少连接模式 */
#define FORWARD_RULE_MODE_RT		"RT"	/* 最小响应时间模式 */
#define FORWARD_RULE_MODE_RD		"RD"	/* 随机模式 */
#define FORWARD_RULE_MODE_HS		"HS"	/* HASH模式 */

#define RULE_CLIENT_MAXCOUNT		10	/* 单条规则中最大客户端配置数量 */
#define RULE_FORWARD_MAXCOUNT		3	/* 单条规则中最大转发端配置数量 */
#define RULE_SERVER_MAXCOUNT		100	/* 单条规则中最大服务端配置数量 */

#define DEFAULT_FORWARD_RULE_MAXCOUNT	100	/* 缺省最大转发规则数量 */
#define DEFAULT_CONNECTION_MAXCOUNT	1024	/* 缺省最大连接数量 */ /* 最大转发会话数量 = 最大连接数量 * 3 */
#define DEFAULT_TRANSFER_BUFSIZE	4096	/* 缺省通讯转发缓冲区大小 */

/* 网络地址信息结构 */
struct NetAddress
{
	char			ip[ 64 + 1 ] ;
	char			port[ 10 + 1 ] ;
	struct sockaddr_in	sockaddr ;
} ;

/* 客户端信息结构 */
struct ClientNetAddress
{
	struct NetAddress	netaddr ;
	int			sock ;
} ;

/* 转发端信息结构 */
struct ForwardNetAddress
{
	struct NetAddress	netaddr ;
	int			sock ;
} ;

/* 服务端信息结构 */
struct ServerNetAddress
{
	struct NetAddress	netaddr ;
	int			sock ;
} ;

#define SERVER_UNABLE_IGNORE_COUNT	100

/* 转发规则结构 */
struct ForwardRule
{
	char				rule_id[ RULE_ID_MAXLEN + 1 ] ;
	char				rule_mode[ RULE_MODE_MAXLEN + 1 ] ;
	
	struct ClientNetAddress		client_addr[ RULE_CLIENT_MAXCOUNT ] ;
	unsigned long			client_count ;
	struct ForwardNetAddress	forward_addr[ RULE_FORWARD_MAXCOUNT ] ;
	unsigned long			forward_count ;
	struct ServerNetAddress		server_addr[ RULE_SERVER_MAXCOUNT ] ;
	unsigned long			server_count ;
	
	unsigned long			select_index ;
	unsigned long			connection_count[ RULE_SERVER_MAXCOUNT ] ;
	
	union
	{
		struct
		{
			unsigned long	server_unable ;
		} RR[ RULE_SERVER_MAXCOUNT ] ;
		struct
		{
			unsigned long	server_unable ;
		} LC[ RULE_SERVER_MAXCOUNT ] ;
		struct
		{
			unsigned long	server_unable ;
			struct timeval	tv1 ;
			struct timeval	tv2 ;
			struct timeval	dtv ;
		} RT[ RULE_SERVER_MAXCOUNT ] ;
	} status ;
} ;

#define FORWARD_SESSION_TYPE_UNUSED	0	/* 转发会话未用单元 */
#define FORWARD_SESSION_TYPE_MANAGE	1	/* 管理连接会话 */
#define FORWARD_SESSION_TYPE_LISTEN	2	/* 侦听服务会话 */
#define FORWARD_SESSION_TYPE_CLIENT	3	/* 客户端连接会话 */
#define FORWARD_SESSION_TYPE_SERVER	4	/* 服务端连接 */

#define CONNECT_STATUS_CONNECTING	0	/* 异步连接服务端中 */
#define CONNECT_STATUS_CONNECTED	1	/* 与服务端已建立连接 */

#define MANAGE_INPUT_BUFSIZE		1024	/* 管理命令输入缓冲区 */
#define MANAGE_OUTPUT_BUFSIZE		1024	/* 管理命令输出缓冲区 */

#define TRY_CONNECT_MAXCOUNT		5	/* 异步尝试连接服务端最大次数 */

/* 侦听会话结构 */
struct ListenNetAddress
{
	struct NetAddress	netaddr ;
	int			sock ;
	
	char			rule_mode[ 2 + 1 ] ;
} ;

/* 转发会话结构 */
struct ForwardSession
{
	char				forward_session_type ;
	
	struct ListenNetAddress		listen_addr ;
	char				manage_input_buffer[ MANAGE_OUTPUT_BUFSIZE + 1 ] ;
	unsigned long			manage_input_buflen ;
	
	struct ClientNetAddress		client_addr ;
	unsigned long			client_index ;
	struct ServerNetAddress		server_addr ;
	unsigned long			server_index ;
	struct ForwardRule		*p_forward_rule ;
	char				connect_status ;
	unsigned long			try_connect_count ;
} ;

/* 命令行参数结构 */
struct CommandParam
{
	char				*config_pathfilename ; /* -f */
	
	unsigned long			forward_rule_maxcount ; /* -r */
	unsigned long			forward_session_maxcount ; /* -c */
	unsigned long			transfer_bufsize ; /* -b */
	
	char				debug_flag ; /* -d */
} ;

/* 内部缓存结构 */
struct ServerCache
{
	struct timeval			tv ;
} ;

/* 服务器环境 */
struct ServerEnv
{
	struct CommandParam		cmd_para ;
	
	struct ForwardRule		*forward_rule ;
	unsigned long			forward_rule_count ;
	
	int				event_env ;
	struct ForwardSession		*forward_session ;
	unsigned long			forward_session_maxcount ;
	unsigned long			forward_session_count ;
	unsigned long			forward_session_use_offsetpos ;
	
	struct ServerCache		server_cache ;
} ;

#define WAIT_EVENTS_COUNT		1024	/* 等待事件集合数量 */

int G5( struct ServerEnv *pse );

#endif
