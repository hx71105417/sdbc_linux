/*
 * LB Dispatch - G5
 * Author  : calvin
 * Email   : calvinwillliams.c@gmail.com
 * History : 2014-03-29 v1.0.0   create
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "G5.h"

/* 日志输出 */
static void DebugOutput( struct ServerEnv *pse , char *format , ... )
{
	va_list		valist ;
	
	if( pse->cmd_para.debug_flag == 0 )
		return;
	
	va_start( valist , format );
	vfprintf( stdout , format , valist );
	va_end( valist );
	
	return;
}

static void InfoOutput( struct ServerEnv *pse , char *format , ... )
{
	va_list		valist ;
	
	va_start( valist , format );
	vfprintf( stdout , format , valist );
	va_end( valist );
	
	return;
}

static void ErrorOutput( struct ServerEnv *pse , char *format , ... )
{
	va_list		valist ;
	
	va_start( valist , format );
	vfprintf( stderr , format , valist );
	va_end( valist );
	
	return;
}

/* 取随机数工具函数 */
static int FetchRand( int min, int max )
{
	return ( rand() % ( max - min + 1 ) ) + min ;
}

/* 计算字符串HASH工具函数 */
static unsigned long CalcHash( char *str )
{
	unsigned long	hashval ;
	unsigned char	*puc = NULL ;
	
	hashval = 19791007 ;
	for( puc = (unsigned char *)str ; *puc ; puc++ )
	{
		hashval = hashval * 19830923 + (*puc) ;
	}
	
	return hashval;
}

/* 设置sock重用选项 */
static int SetReuseAddr( int sock )
{
	int	on ;
	
	on = 1 ;
	setsockopt( sock , SOL_SOCKET , SO_REUSEADDR , (void *) & on, sizeof(on) );
	
	return 0;
}

/* 设置sock非堵塞选项 */
static int SetNonBlocking( int sock )
{
	int	opts;
	
	opts = fcntl( sock , F_GETFL ) ;
	if( opts < 0 )
	{
		return -1;
	}
	
	opts = opts | O_NONBLOCK;
	if( fcntl( sock , F_SETFL , opts ) < 0 )
	{
		return -2;
	}
	
	return 0;
}

/* 从epoll连接池取一个未用单元 */
static int GetForwardSessionUnusedUnit( struct ServerEnv *pse , struct ForwardSession **pp_forward_session )
{
	unsigned long		index ;
	unsigned long		count ;
	struct ForwardSession	*p_forward_session = NULL ;
	
	for( count = 0 , index = pse->forward_session_use_offsetpos , p_forward_session = & (pse->forward_session[pse->forward_session_use_offsetpos])
		; count < pse->forward_session_maxcount
		; count++ , index++ , p_forward_session++ )
	{
		if( index >= pse->forward_session_maxcount )
		{
			index = 0 ;
			p_forward_session = & (pse->forward_session[0]) ;
		}
		
		if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_UNUSED )
		{
			memset( p_forward_session , 0x00 , sizeof(struct ForwardSession) );
			(*pp_forward_session) = p_forward_session ;
			pse->forward_session_use_offsetpos = ( ( pse->forward_session_use_offsetpos + 1 ) % pse->forward_session_maxcount ) ;
			return FOUND;
		}
	}
	
	return NOT_FOUND;
}

/* 把一个epoll连接池单元设置为未用状态 */
static int SetForwardSessionUnitUnused( struct ForwardSession *p_forward_session )
{
	memset( p_forward_session , 0x00 , sizeof(struct ForwardSession) );
	return 0;
}

/* 查询转发规则 */
static int QueryForwardRule( struct ServerEnv *pse , char *rule_id , struct ForwardRule **pp_forward_rule , unsigned long *p_index )
{
	unsigned long		index ;
	struct ForwardRule	*p_forward_rule = NULL ;
	
	for( index = 0 , p_forward_rule = & (pse->forward_rule[0]) ; index < pse->forward_rule_count ; index++ , p_forward_rule++ )
	{
		if( strcmp( p_forward_rule->rule_id , rule_id ) == 0 )
		{
			if( pp_forward_rule )
				(*pp_forward_rule) = p_forward_rule ;
			if( p_index )
				(*p_index) = index ;
			return FOUND;
		}
	}
	
	return NOT_FOUND;
}

/* 按转发规则强制断开所有相关网络连接 */
static int CloseSocketWithRuleForcely( struct ServerEnv *pse , struct ForwardRule *p_forward_rule )
{
	unsigned long		index ;
	struct ForwardSession	*p_forward_session = NULL ;
	
	for( index = 0 , p_forward_session = & (pse->forward_session[0])
		; index < pse->forward_session_maxcount
		; index++ , p_forward_session++ )
	{
		if( p_forward_session->p_forward_rule == p_forward_rule )
		{
			if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_CLIENT )
			{
				InfoOutput( pse , "close #%d# , forcely\n" , p_forward_session->client_addr.sock );
				epoll_ctl( pse->event_env , EPOLL_CTL_DEL , p_forward_session->client_addr.sock , NULL );
				close( p_forward_session->client_addr.sock );
				SetForwardSessionUnitUnused( p_forward_session );
			}
			else if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_SERVER )
			{
				InfoOutput( pse , "close #%d# , forcely\n" , p_forward_session->server_addr.sock );
				epoll_ctl( pse->event_env , EPOLL_CTL_DEL , p_forward_session->server_addr.sock , NULL );
				close( p_forward_session->server_addr.sock );
				SetForwardSessionUnitUnused( p_forward_session );
			}
		}
	}
	
	return 0;
}

/* 如果没有绑定侦听端口，绑定之，并登记到epoll池 */
static int BinListenSocket( struct ServerEnv *pse , struct ForwardRule *p_forward_rule , struct ForwardNetAddress *p_forward_addr )
{
	unsigned long		forward_session_index ;
	struct ForwardSession	*p_forward_session = NULL ;
	struct epoll_event	event ;
	
	int			nret = 0 ;
	
	/* 判断是否太多转发规则 */
	if( pse->forward_session_count >= pse->forward_session_maxcount )
	{
		ErrorOutput( pse , "too many listen addr\n" );
		return -91;
	}
	
	/* 判断是否有重复转发规则 */
	for( forward_session_index = 0 , p_forward_session = & ( pse->forward_session[0] )
		; forward_session_index < pse->forward_session_maxcount
		; forward_session_index++ , p_forward_session++ )
	{
		if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_LISTEN )
		{
			if(	strcmp( p_forward_session->listen_addr.netaddr.ip , p_forward_addr->netaddr.ip ) == 0
				&&
				p_forward_session->listen_addr.netaddr.port == p_forward_addr->netaddr.port )
			{
				return 1;
			}
		}
	}
	
	/* 创建侦听端口，登记转发会话，登记epoll池 */
	nret = GetForwardSessionUnusedUnit( pse , & p_forward_session ) ;
	if( nret != FOUND )
	{
		ErrorOutput( pse , "too many listen addr\n" );
		return -92;
	}
	
	strcpy( p_forward_session->listen_addr.rule_mode , p_forward_rule->rule_mode );
	strcpy( p_forward_session->listen_addr.netaddr.ip , p_forward_addr->netaddr.ip );
	strcpy( p_forward_session->listen_addr.netaddr.port , p_forward_addr->netaddr.port );
	
	p_forward_session->listen_addr.sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP );
	if( p_forward_session->listen_addr.sock < 0 )
	{
		ErrorOutput( pse , "socket failed[%d]errno[%d]\n" , p_forward_session->listen_addr.sock , errno );
		return -93;
	}
	
	SetReuseAddr( p_forward_session->listen_addr.sock );
	SetNonBlocking( p_forward_session->listen_addr.sock );
	
	memset( & (p_forward_session->listen_addr.netaddr.sockaddr) , 0x00 , sizeof(p_forward_session->listen_addr.netaddr.sockaddr) );
	p_forward_session->listen_addr.netaddr.sockaddr.sin_family = AF_INET ;
	inet_aton( p_forward_session->listen_addr.netaddr.ip , & (p_forward_session->listen_addr.netaddr.sockaddr.sin_addr) );
	p_forward_session->listen_addr.netaddr.sockaddr.sin_port = htons( (unsigned short)atoi(p_forward_session->listen_addr.netaddr.port) );
	
	bind( p_forward_session->listen_addr.sock , (struct sockaddr *) & (p_forward_session->listen_addr.netaddr.sockaddr) , sizeof(struct sockaddr) );
	listen( p_forward_session->listen_addr.sock , 1024 );
	
	p_forward_session->forward_session_type = FORWARD_SESSION_TYPE_LISTEN ;
	
	memset( & event , 0x00 , sizeof(event) );
	event.data.ptr = p_forward_session ;
	event.events = EPOLLIN | EPOLLET ;
	epoll_ctl( pse->event_env , EPOLL_CTL_ADD , p_forward_session->listen_addr.sock , & event );
	
	pse->forward_session_count++;
	
	p_forward_addr->sock = p_forward_session->listen_addr.sock ;
	
	return 0;
}

/* 新增一条转发规则 */
static int AddForwardRule( struct ServerEnv *pse , struct ForwardRule *p_forward_rule )
{
	int		nret = 0 ;
	
	if( pse->forward_rule_count >= pse->cmd_para.forward_rule_maxcount )
	{
		ErrorOutput( pse , "too many forward rule\n" );
		return -1;
	}
	
	nret = QueryForwardRule( pse , p_forward_rule->rule_id , NULL , NULL ) ;
	if( nret == FOUND )
	{
		ErrorOutput( pse , "forward rule rule_id[%s] found\n" , p_forward_rule->rule_id );
		return -2;
	}
	
	if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_G ) == 0 )
	{
		nret = BinListenSocket( pse , p_forward_rule , p_forward_rule->forward_addr ) ;
		if( nret < 0 )
		{
			ErrorOutput( pse , "BinListenSocket failed[%d]\n" , nret );
			return -3;
		}
	}
	
	memcpy( & (pse->forward_rule[pse->forward_rule_count]) , p_forward_rule , sizeof(struct ForwardRule) );
	pse->forward_rule_count++;
	
	return 0;
}

/* 修改一条转发规则 */
static int ModifyForwardRule( struct ServerEnv *pse , struct ForwardRule *p_forward_rule )
{
	struct ForwardRule	*p = NULL ;
	
	int			nret = 0 ;
	
	nret = QueryForwardRule( pse , p_forward_rule->rule_id , & p , NULL ) ;
	if( nret == NOT_FOUND )
	{
		ErrorOutput( pse , "forward rule rule_id[%s] not found\n" , p_forward_rule->rule_id );
		return -1;
	}
	
	if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_G ) == 0 )
	{
		nret = BinListenSocket( pse , p_forward_rule , p_forward_rule->forward_addr ) ;
		if( nret < 0 )
		{
			ErrorOutput( pse , "BinListenSocket failed[%d]\n" , nret );
			return -2;
		}
		else if( nret == 0 )
		{
			InfoOutput( pse , "LISTEN #%d#\n" , p_forward_rule->forward_addr->sock );
		}
	}
	
	CloseSocketWithRuleForcely( pse , p );
	
	memcpy( p , p_forward_rule , sizeof(struct ForwardRule) );
	
	return 0;
}

/* 删除一条转发规则 */
static int RemoveForwardRule( struct ServerEnv *pse , char *rule_id )
{
	struct ForwardRule	*p_forward_rule = NULL ;
	unsigned long		index ;
	
	int			nret = 0 ;
	
	if( pse->forward_rule_count == 0 )
	{
		ErrorOutput( pse , "no forward rule exist\n" );
		return -1;
	}
	
	nret = QueryForwardRule( pse , rule_id , & p_forward_rule , & index ) ;
	if( nret == NOT_FOUND )
	{
		ErrorOutput( pse , "forward rule rule_id[%s] not found\n" , rule_id );
		return -2;
	}
	
	CloseSocketWithRuleForcely( pse , p_forward_rule );
	
	memmove( & (pse->forward_rule[index]) , & (pse->forward_rule[index+1]) , sizeof(struct ForwardRule) * (pse->forward_rule_count-index-1) );
	memset( & (pse->forward_rule[pse->forward_rule_count-1]) , 0x00 , sizeof(struct ForwardRule) );
	pse->forward_rule_count--;
	
	return 0;
}

/* 从配置段中解析网络地址 */
static int ParseIpAndPort( char *ip_and_port , struct NetAddress *paddr )
{
	char		*p_colon = NULL ;
	
	p_colon = strchr( ip_and_port , ':' ) ;
	if( p_colon == NULL )
		return -1;
	
	strncpy( paddr->ip , ip_and_port , p_colon - ip_and_port );
	strcpy( paddr->port , p_colon + 1 );
	
	return 0;
}

/* 装载单条转发配置 */
static int LoadForwardConfig( struct ServerEnv *pse , char *buffer , struct ForwardRule *p_forward_rule )
{
	char				*p_remark = NULL ;
	
	char				*p_begin = NULL ;
	
	unsigned long			client_index ;
	struct ClientNetAddress		*p_client_addr = NULL ;
	unsigned long			forward_index ;
	struct ForwardNetAddress	*p_forward_addr = NULL ;
	unsigned long			server_index ;
	struct ServerNetAddress		*p_server_addr = NULL ;
	
	int				nret = 0 ;
	
	p_remark = strchr( buffer , '#' ) ;
	if( p_remark )
	{
		(*p_remark) = '\0' ;
	}
	
	memset( p_forward_rule , 0x00 , sizeof(struct ForwardRule) );
	
	p_begin = strtok( buffer , " \t\r\n" ) ;
	if( p_begin == NULL )
	{
		return 11;
	}
	if( strlen(p_begin) > RULE_ID_MAXLEN )
	{
		ErrorOutput( pse , "rule rule_id too long\n" );
		return -12;
	}
	strcpy( p_forward_rule->rule_id , p_begin );
	
	p_begin = strtok( NULL , " \t\r\n" ) ;
	if( p_begin == NULL )
	{
		ErrorOutput( pse , "expect rule rule_mode\n" );
		return -21;
	}
	if( strlen(p_begin) > RULE_MODE_MAXLEN )
	{
		ErrorOutput( pse , "rule rule_mode too long\n" );
		return -22;
	}
	strcpy( p_forward_rule->rule_mode , p_begin );
	
	if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_G )
		&& strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_MS )
		&& strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_RR )
		&& strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_LC )
		&& strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_RT )
		&& strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_RD )
		&& strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_HS ) )
	{
		ErrorOutput( pse , "rule rule_mode [%s] invalid\n" , p_forward_rule->rule_mode );
		return -23;
	}
	
	InfoOutput( pse , "%s %s" , p_forward_rule->rule_id , p_forward_rule->rule_mode );
	
	for( client_index = 0 , p_client_addr = & (p_forward_rule->client_addr[0]) ; client_index < RULE_CLIENT_MAXCOUNT ; client_index++ , p_client_addr++ , p_forward_rule->client_count++ )
	{
		p_begin = strtok( NULL , " \t\r\n" ) ;
		if( p_begin == NULL )
		{
			ErrorOutput( pse , "expect client addr\n" );
			return -31;
		}
		
		if( strcmp( p_begin , "-" ) == 0 || strcmp( p_begin , ";" ) == 0 )
			break;
		
		nret = ParseIpAndPort( p_begin , & (p_client_addr->netaddr) ) ;
		if( nret )
		{
			ErrorOutput( pse , "client addr invalid[%d]\n" , nret );
			return -32;
		}
		
		InfoOutput( pse , " %s:%s" , p_client_addr->netaddr.ip , p_client_addr->netaddr.port );
	}
	
	if( strcmp( p_begin , ";" ) != 0 )
	{
		printf( " -" );
		
		for( forward_index = 0 , p_forward_addr = & (p_forward_rule->forward_addr[0]) ; forward_index < RULE_CLIENT_MAXCOUNT ; forward_index++ , p_forward_addr++ , p_forward_rule->forward_count++ )
		{
			p_begin = strtok( NULL , " \t\r\n" ) ;
			if( p_begin == NULL )
			{
				ErrorOutput( pse , "expect forward addr\n" );
				return -41;
			}
			
			if( strcmp( p_begin , ">" ) == 0 || strcmp( p_begin , ";" ) == 0 )
				break;
			
			nret = ParseIpAndPort( p_begin , & (p_forward_addr->netaddr) ) ;
			if( nret )
			{
				ErrorOutput( pse , "forward addr invalid[%d]\n" , nret );
				return -42;
			}
			
			InfoOutput( pse , " %s:%s" , p_forward_addr->netaddr.ip , p_forward_addr->netaddr.port );
			
			nret = BinListenSocket( pse , p_forward_rule , p_forward_addr ) ;
			if( nret < 0 )
			{
				return nret;
			}
			else if( nret == 0 )
			{
				InfoOutput( pse , "(LISTEN)#%d#" , p_forward_addr->sock );
			}
		}
		
		if( strcmp( p_begin , ";" ) != 0 )
		{
			printf( " >" );
			
			for( server_index = 0 , p_server_addr = & (p_forward_rule->server_addr[0]) ; server_index < RULE_CLIENT_MAXCOUNT ; server_index++ , p_server_addr++ , p_forward_rule->server_count++ )
			{
				p_begin = strtok( NULL , " \t\r\n" ) ;
				if( p_begin == NULL )
				{
					fprintf( stderr , "expect server addr\n" );
					return -51;
				}
				
				if( strcmp( p_begin , ";" ) == 0 )
					break;
				
				nret = ParseIpAndPort( p_begin , & (p_server_addr->netaddr) ) ;
				if( nret )
				{
					fprintf( stderr , "server addr invalid[%d]\n" , nret );
					return -52;
				}
				
				InfoOutput( pse , " %s:%s" , p_server_addr->netaddr.ip , p_server_addr->netaddr.port );
			}
		}
	}
	
	InfoOutput( pse , " ;\n" );
	
	if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_G ) == 0 )
	{
		if( p_forward_rule->server_count > 0 )
		{
			fprintf( stderr , "rule rule_mode [%s] unexpect server_addr\n" , p_forward_rule->rule_mode );
			return -61;
		}
	}
	else
	{
		if( p_forward_rule->forward_count == 0 )
		{
			ErrorOutput( pse , "rule rule_mode [%s] expect forward_addr\n" , p_forward_rule->rule_mode );
			return -62;
		}
		if( p_forward_rule->server_count == 0 )
		{
			ErrorOutput( pse , "rule rule_mode [%s] expect server_addr\n" , p_forward_rule->rule_mode );
			return -63;
		}
	}
	
	return 0;
}

/* 装载所有配置 */
static int LoadConfig( struct ServerEnv *pse )
{
	FILE				*fp = NULL ;
	char				buffer[ 1024 + 1 ] ;
	
	struct ForwardRule		forward_rule ;
	
	int				nret = 0 ;
	
	fp = fopen( pse->cmd_para.config_pathfilename , "r" ) ;
	if( fp == NULL )
	{
		ErrorOutput( pse , "can't open config file [%s]\n" , pse->cmd_para.config_pathfilename );
		return -1;
	}
	
	while( fgets( buffer , sizeof(buffer)-1 , fp ) )
	{
		nret = LoadForwardConfig( pse , buffer , & forward_rule ) ;
		if( nret > 0 )
		{
			continue;
		}
		else if( nret < 0 )
		{
			ErrorOutput( pse , "LoadForwardConfig failed[%d]\n" , nret );
			fclose(fp);
			return nret;
		}
		else
		{
			nret = AddForwardRule( pse , & forward_rule ) ;
			if( nret )
			{
				fclose(fp);
				return nret;
			}
		}
	}
	
	fclose(fp);
	
	return 0;
}

/* 判断字符串匹配性 */
static int IsMatchString(char *pcMatchString, char *pcObjectString, char cMatchMuchCharacters, char cMatchOneCharacters)
{
	int el=strlen(pcMatchString);
	int sl=strlen(pcObjectString);
	char cs,ce;

	int is,ie;
	int last_xing_pos=-1;

	for(is=0,ie=0;is<sl && ie<el;){
		cs=pcObjectString[is];
		ce=pcMatchString[ie];

		if(cs!=ce){
			if(ce==cMatchMuchCharacters){
				last_xing_pos=ie;
				ie++;
			}else if(ce==cMatchOneCharacters){
				is++;
				ie++;
			}else if(last_xing_pos>=0){
				while(ie>last_xing_pos){
					ce=pcMatchString[ie];
					if(ce==cs)
						break;
					ie--;
				}

				if(ie==last_xing_pos)
					is++;
			}else
				return -1;
		}else{
			is++;
			ie++;
		}
	}

	if(pcObjectString[is]==0 && pcMatchString[ie]==0)
		return 0;

	if(pcMatchString[ie]==0)
		ie--;

	if(ie>=0){
		while(pcMatchString[ie])
			if(pcMatchString[ie++]!=cMatchMuchCharacters)
				return -2;
	} 

	return 0;
}

/* 判断客户端网络地址是否匹配 */
static int MatchClientAddr( struct ClientNetAddress *p_client_addr , struct ForwardRule *p_forward_rule )
{
	unsigned long			match_addr_index ;
	struct ClientNetAddress		*p_match_addr = NULL ;
	
	for( match_addr_index = 0 , p_match_addr = & (p_forward_rule->client_addr[0])
		; match_addr_index < p_forward_rule->client_count
		; match_addr_index++ , p_match_addr++ )
	{
		if(	IsMatchString( p_match_addr->netaddr.ip , p_client_addr->netaddr.ip , '*' , '?' ) == 0
			&&
			IsMatchString( p_match_addr->netaddr.port , p_client_addr->netaddr.port , '*' , '?' ) == 0
		)
		{
			return MATCH;
		}
	}
	
	return NOT_MATCH;
}

/* 判断本地侦听端网络地址是否匹配 */
static int MatchForwardAddr( struct ListenNetAddress *p_listen_addr , struct ForwardRule *p_forward_rule )
{
	unsigned long			match_addr_index ;
	struct ForwardNetAddress	*p_match_addr = NULL ;
	
	for( match_addr_index = 0 , p_match_addr = & (p_forward_rule->forward_addr[0])
		; match_addr_index < p_forward_rule->forward_count
		; match_addr_index++ , p_match_addr++ )
	{
		if(	IsMatchString( p_match_addr->netaddr.ip , p_listen_addr->netaddr.ip , '*' , '?' ) == 0
			&&
			IsMatchString( p_match_addr->netaddr.port , p_listen_addr->netaddr.port , '*' , '?' ) == 0
		)
		{
			return MATCH;
		}
	}
	
	return NOT_MATCH;
}

/* 判断转发规则是否匹配 */
static int MatchForwardRule( struct ServerEnv *pse , struct ClientNetAddress *p_client_addr , struct ListenNetAddress *p_listen_addr , struct ForwardRule **pp_forward_rule )
{
	unsigned long		forward_no ;
	struct ForwardRule	*p_forward_rule = NULL ;
	
	for( forward_no = 0 , p_forward_rule = & (pse->forward_rule[0]) ; forward_no < pse->forward_rule_count ; forward_no++ , p_forward_rule++ )
	{
		if( MatchForwardAddr( p_listen_addr , p_forward_rule ) == MATCH && MatchClientAddr( p_client_addr , p_forward_rule ) == MATCH )
		{
			(*pp_forward_rule) = p_forward_rule ;
			return FOUND;
		}
	}
	
	return NOT_FOUND;
}

/* 从目标网络地址中根据不同算法选择一个目标网络地址 */
static int SelectServerAddress( struct ServerEnv *pse , struct ClientNetAddress *p_client_addr , struct ForwardRule *p_forward_rule , char *ip , char *port )
{
	if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_MS ) == 0 )
	{
		strcpy( ip , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.ip );
		strcpy( port , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.port );
	}
	else if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_RR ) == 0 )
	{
		while(1)
		{
			if( p_forward_rule->status.RR[p_forward_rule->select_index].server_unable <= 0 )
			{
				strcpy( ip , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.ip );
				strcpy( port , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.port );
				
				p_forward_rule->select_index = ( (p_forward_rule->select_index+1) % p_forward_rule->server_count ) ;
				
				return 0;
			}
			else
			{
				p_forward_rule->status.RR[p_forward_rule->select_index].server_unable--;
			}
			
			p_forward_rule->select_index++;
			if( p_forward_rule->select_index >= p_forward_rule->server_count )
				p_forward_rule->select_index = 0 ;
		}
	}
	else if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_LC ) == 0 )
	{
		unsigned long		index ;
		unsigned long		connection_count ;
		
		p_forward_rule->select_index = -1 ;
		connection_count = ULONG_MAX ;
		while( p_forward_rule->select_index == -1 )
		{
			for( index = 0 ; index < p_forward_rule->server_count ; index++ )
			{
				if( p_forward_rule->status.LC[index].server_unable <= 0 )
				{
					if( p_forward_rule->connection_count[index] <= connection_count )
					{
						p_forward_rule->select_index = index ;
						connection_count = p_forward_rule->connection_count[index] ;
					}
				}
				else 
				{
					p_forward_rule->status.LC[index].server_unable--;
				}
			}
		}
		
		strcpy( ip , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.ip );
		strcpy( port , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.port );
	}
	else if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_RT ) == 0 )
	{
		unsigned long		dtt ;
		unsigned long		index ;
		unsigned long		dt ;
		
		p_forward_rule->select_index = -1 ;
		dtt = ULONG_MAX ;
		while( p_forward_rule->select_index == -1 )
		{
			for( index = 0 ; index < p_forward_rule->server_count ; index++ )
			{
				if( p_forward_rule->status.RT[index].server_unable <= 0 )
				{
					if( p_forward_rule->status.RT[index].tv1.tv_sec == 0 || p_forward_rule->status.RT[index].tv2.tv_sec == 0 )
					{
						p_forward_rule->select_index = index ;
						break;
					}
					
					p_forward_rule->status.RT[index].dtv.tv_sec = abs( p_forward_rule->status.RT[index].tv1.tv_sec - p_forward_rule->status.RT[index].tv2.tv_sec ) ;
					p_forward_rule->status.RT[index].dtv.tv_usec = abs( p_forward_rule->status.RT[index].tv1.tv_usec - p_forward_rule->status.RT[index].tv2.tv_usec ) ;
					dt = p_forward_rule->status.RT[index].dtv.tv_sec * 1000000 + p_forward_rule->status.RT[index].dtv.tv_usec ;
					if( dt <= dtt )
					{
						p_forward_rule->select_index = index ;
						dtt = dt ;
					}
				}
				else
				{
					p_forward_rule->status.RT[index].server_unable--;
				}
			}
		}
		
		strcpy( ip , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.ip );
		strcpy( port , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.port );
	}
	else if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_RD ) == 0 )
	{
		p_forward_rule->select_index = FetchRand( 0 , p_forward_rule->server_count - 1 ) ;
		
		strcpy( ip , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.ip );
		strcpy( port , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.port );
	}
	else if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_HS ) == 0 )
	{
		p_forward_rule->select_index = CalcHash( p_client_addr->netaddr.ip ) % p_forward_rule->server_count ;
		
		strcpy( ip , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.ip );
		strcpy( port , p_forward_rule->server_addr[p_forward_rule->select_index].netaddr.port );
	}
	else
	{
		ErrorOutput( pse , "'rule_mode'[%s] invalid\n" , p_forward_rule->rule_mode );
		return -1;
	}
	
	return 0;
}

/* 当目标网络地址不可用时，根据不同算法做相应处理 */
static int OnServerUnable( struct ServerEnv *pse , struct ForwardRule *p_forward_rule )
{
	if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_MS ) == 0 )
	{
		p_forward_rule->select_index = ( (p_forward_rule->select_index+1) % p_forward_rule->server_count ) ;
	}
	else if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_RR ) == 0 )
	{
		p_forward_rule->status.RR[p_forward_rule->select_index].server_unable = SERVER_UNABLE_IGNORE_COUNT ;
	}
	else if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_LC ) == 0 )
	{
		p_forward_rule->status.LC[p_forward_rule->select_index].server_unable = SERVER_UNABLE_IGNORE_COUNT ;
	}
	else if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_RT ) == 0 )
	{
		p_forward_rule->status.RT[p_forward_rule->select_index].server_unable = SERVER_UNABLE_IGNORE_COUNT ;
	}
	else if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_RD ) == 0 )
	{
	}
	else if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_HS ) == 0 )
	{
	}
	else
	{
		ErrorOutput( pse , "'rule_mode'[%s] invalid\n" , p_forward_rule->rule_mode );
		return -1;
	}
	
	return 0;
}

/* 连接到目标网络地址 */
static int ConnectToRemote( struct ServerEnv *pse , struct epoll_event *p_event , struct ForwardSession *p_forward_session , struct ForwardRule *p_forward_rule , struct ClientNetAddress *p_client_addr , unsigned long try_connect_count )
{
	socklen_t		addr_len = sizeof(struct sockaddr_in) ;
	
	struct epoll_event	client_event ;
	
	struct ServerNetAddress	server_addr ;
	struct epoll_event	server_event ;
	
	struct ForwardSession	*p_forward_session_client = NULL ;
	struct ForwardSession	*p_forward_session_server = NULL ;
	
	int			nret = 0 ;
	
	/* 创建客户端sock */
	server_addr.sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP );
	if( server_addr.sock < 0 )
	{
		ErrorOutput( pse , "socket failed[%d]errno[%d]\n" , server_addr.sock , errno );
		return 1;
	}
	
	SetNonBlocking( server_addr.sock );
	SetReuseAddr( server_addr.sock );
	
	/* 根据转发规则，选择目标网络地址 */
	nret = SelectServerAddress( pse , p_client_addr , p_forward_rule , server_addr.netaddr.ip , server_addr.netaddr.port ) ;
	if( nret )
	{
		return nret;
	}
	
	memset( & (server_addr.netaddr.sockaddr) , 0x00 , sizeof(server_addr.netaddr.sockaddr) );
	server_addr.netaddr.sockaddr.sin_family = AF_INET ;
	inet_aton( server_addr.netaddr.ip , & (server_addr.netaddr.sockaddr.sin_addr) );
	server_addr.netaddr.sockaddr.sin_port = htons( (unsigned short)atoi(server_addr.netaddr.port) );
	
	/* 连接目标网络地址 */
	nret = connect( server_addr.sock , ( struct sockaddr *) & (server_addr.netaddr.sockaddr) , addr_len );
	if( nret < 0 )
	{
		if( errno != EINPROGRESS )
		{
			ErrorOutput( pse , "connect to [%s:%s] failed[%d]errno[%d]\n" , server_addr.netaddr.ip , server_addr.netaddr.port , nret , errno );
			close( server_addr.sock );
			return 1;
		}
		
		/* 登记服务端转发会话，登记epoll池 */
		nret = GetForwardSessionUnusedUnit( pse , & p_forward_session_server ) ;
		if( nret != FOUND )
		{
			ErrorOutput( pse , "GetForwardSessionUnusedUnit failed[%d]\n" , nret );
			close( server_addr.sock );
			return 1;
		}
		
		p_forward_session_server->forward_session_type = FORWARD_SESSION_TYPE_SERVER ;
		
		strcpy( p_forward_session_server->client_addr.netaddr.ip , p_client_addr->netaddr.ip );
		strcpy( p_forward_session_server->client_addr.netaddr.port , p_client_addr->netaddr.port );
		p_forward_session_server->client_addr.sock = p_client_addr->sock ;
		memcpy( & (p_forward_session_server->client_addr.netaddr) , & (p_client_addr->netaddr) , sizeof(struct sockaddr_in) );
		p_forward_session_server->client_index = 0 ;
		
		strcpy( p_forward_session_server->listen_addr.netaddr.ip , p_forward_session->listen_addr.netaddr.ip );
		strcpy( p_forward_session_server->listen_addr.netaddr.port , p_forward_session->listen_addr.netaddr.port );
		p_forward_session_server->listen_addr.sock = p_forward_session->listen_addr.sock ;
		memcpy( & (p_forward_session_server->listen_addr.netaddr) , & (p_forward_session->listen_addr.netaddr) , sizeof(struct sockaddr_in) );
		strcpy( p_forward_session_server->listen_addr.rule_mode , p_forward_rule->rule_mode );
		
		strcpy( p_forward_session_server->server_addr.netaddr.ip , server_addr.netaddr.ip );
		strcpy( p_forward_session_server->server_addr.netaddr.port , server_addr.netaddr.port );
		p_forward_session_server->server_addr.sock = server_addr.sock ;
		memcpy( & (p_forward_session_server->server_addr.netaddr) , & (server_addr.netaddr) , sizeof(struct sockaddr_in) );
		p_forward_session_server->server_index = p_forward_session_server - & (pse->forward_session[0]) ;
		
		p_forward_session_server->p_forward_rule = p_forward_rule ;
		p_forward_session_server->connect_status = CONNECT_STATUS_CONNECTING ;
		p_forward_session_server->try_connect_count = try_connect_count ;
		
		memset( & (server_event) , 0x00 , sizeof(server_event) );
		server_event.data.ptr = p_forward_session_server ;
		server_event.events = EPOLLOUT | EPOLLERR | EPOLLET ;
		epoll_ctl( pse->event_env , EPOLL_CTL_ADD , p_forward_session_server->server_addr.sock , & server_event );
		
		p_forward_session_server->p_forward_rule->connection_count[p_forward_session_server->p_forward_rule->select_index]++;
	}
	else
	{
		/* 登记客户端转发会话，登记epoll池 */
		nret = GetForwardSessionUnusedUnit( pse , & p_forward_session_client ) ;
		if( nret != FOUND )
		{
			ErrorOutput( pse , "GetForwardSessionUnusedUnit failed[%d]\n" , nret );
			close( server_addr.sock );
			return 1;
		}
		
		p_forward_session_client->forward_session_type = FORWARD_SESSION_TYPE_CLIENT ;
		
		strcpy( p_forward_session_client->client_addr.netaddr.ip , p_client_addr->netaddr.ip );
		strcpy( p_forward_session_client->client_addr.netaddr.port , p_client_addr->netaddr.port );
		p_forward_session_client->client_addr.sock = p_client_addr->sock ;
		memcpy( & (p_forward_session_client->client_addr.netaddr) , & (p_client_addr->netaddr) , sizeof(struct sockaddr_in) );
		p_forward_session_client->client_index = p_forward_session_client - & (pse->forward_session[0]) ;
		
		strcpy( p_forward_session_client->listen_addr.netaddr.ip , p_forward_session->listen_addr.netaddr.ip );
		strcpy( p_forward_session_client->listen_addr.netaddr.port , p_forward_session->listen_addr.netaddr.port );
		p_forward_session_client->listen_addr.sock = p_forward_session->listen_addr.sock ;
		memcpy( & (p_forward_session_client->listen_addr.netaddr) , & (p_forward_session->listen_addr.netaddr) , sizeof(struct sockaddr_in) );
		strcpy( p_forward_session_client->listen_addr.rule_mode , p_forward_rule->rule_mode );
		
		strcpy( p_forward_session_client->server_addr.netaddr.ip , server_addr.netaddr.ip );
		strcpy( p_forward_session_client->server_addr.netaddr.port , server_addr.netaddr.port );
		p_forward_session_client->server_addr.sock = server_addr.sock ;
		memcpy( & (p_forward_session_client->server_addr.netaddr) , & (server_addr.netaddr) , sizeof(struct sockaddr_in) );
		p_forward_session_client->server_index = p_forward_session_server - & (pse->forward_session[0]) ;
		
		p_forward_session_client->p_forward_rule = p_forward_rule ;
		p_forward_session_client->connect_status = CONNECT_STATUS_CONNECTED ;
		
		memset( & (client_event) , 0x00 , sizeof(client_event) );
		client_event.data.ptr = p_forward_session_client ;
		client_event.events = EPOLLIN | EPOLLERR | EPOLLET ;
		epoll_ctl( pse->event_env , EPOLL_CTL_ADD , p_forward_session_client->client_addr.sock , & client_event );
		
		/* 登记服务端转发会话，登记epoll池 */
		nret = GetForwardSessionUnusedUnit( pse , & p_forward_session_server ) ;
		if( nret != FOUND )
		{
			ErrorOutput( pse , "GetForwardSessionUnusedUnit failed[%d]\n" , nret );
			close( server_addr.sock );
			return 1;
		}
		
		p_forward_session_server->forward_session_type = FORWARD_SESSION_TYPE_SERVER ;
		
		strcpy( p_forward_session_server->client_addr.netaddr.ip , p_client_addr->netaddr.ip );
		strcpy( p_forward_session_server->client_addr.netaddr.port , p_client_addr->netaddr.port );
		p_forward_session_server->client_addr.sock = p_client_addr->sock ;
		memcpy( & (p_forward_session_server->client_addr.netaddr) , & (p_client_addr->netaddr) , sizeof(struct sockaddr_in) );
		p_forward_session_server->client_index = p_forward_session_server - & (pse->forward_session[0]) ;
		
		strcpy( p_forward_session_server->server_addr.netaddr.ip , server_addr.netaddr.ip );
		strcpy( p_forward_session_server->server_addr.netaddr.port , server_addr.netaddr.port );
		p_forward_session_server->server_addr.sock = server_addr.sock ;
		memcpy( & (p_forward_session_server->server_addr.netaddr) , & (server_addr.netaddr) , sizeof(struct sockaddr_in) );
		p_forward_session_server->server_index = p_forward_session_server - & (pse->forward_session[0]) ;
		
		p_forward_session_server->p_forward_rule = p_forward_rule ;
		p_forward_session_server->connect_status = CONNECT_STATUS_CONNECTED ;
		
		memset( & (server_event) , 0x00 , sizeof(server_event) );
		server_event.data.ptr = p_forward_session_server ;
		server_event.events = EPOLLIN | EPOLLERR | EPOLLET ;
		epoll_ctl( pse->event_env , EPOLL_CTL_ADD , p_forward_session_server->server_addr.sock , & server_event );
		
		p_forward_session_server->p_forward_rule->connection_count[p_forward_session_server->p_forward_rule->select_index]++;
		
		DebugOutput( pse , "accept [%s:%s]#%d# - [%s:%s]#%d# > [%s:%s]#%d#\n"
			, p_forward_session_client->client_addr.netaddr.ip , p_forward_session_client->client_addr.netaddr.port , p_forward_session_client->client_addr.sock
			, p_forward_session->listen_addr.netaddr.ip , p_forward_session->listen_addr.netaddr.port , p_forward_session->listen_addr.sock
			, p_forward_session_client->server_addr.netaddr.ip , p_forward_session_client->server_addr.netaddr.port , p_forward_session_client->server_addr.sock );
	}
	
	return 0;
}

/* 接受管理端口连接 */
static int AcceptManageSocket( struct ServerEnv *pse , struct epoll_event *p_event , struct ForwardSession *p_forward_session )
{
	socklen_t		addr_len = sizeof(struct sockaddr_in) ;
	
	struct ClientNetAddress	client_addr ;
	struct epoll_event	client_event ;
	
	struct ForwardSession	*p_forward_session_client = NULL ;
	
	int			nret = 0 ;
	
	/* 循环接受管理端口连接 */
	while(1)
	{
		/* 接受管理端口连接 */
		client_addr.sock = accept( p_forward_session->listen_addr.sock , (struct sockaddr *) & (client_addr.netaddr.sockaddr) , & addr_len ) ;
		if( client_addr.sock < 0 )
		{
			if( errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED || errno == EPROTO )
				break;
			
			ErrorOutput( pse , "accept[%d] failed[%d]errno[%d]\n" , p_forward_session->listen_addr.sock , client_addr.sock  , errno );
			return 1;
		}
		
		SetNonBlocking( client_addr.sock );
		SetReuseAddr( client_addr.sock );
		
		strcpy( client_addr.netaddr.ip , inet_ntoa( client_addr.netaddr.sockaddr.sin_addr ) );
		sprintf( client_addr.netaddr.port , "%ld" , (unsigned long)ntohs( client_addr.netaddr.sockaddr.sin_port ) );
		
		/* 登记转发会话、登记epoll池 */
		nret = GetForwardSessionUnusedUnit( pse , & p_forward_session_client ) ;
		if( nret != FOUND )
		{
			ErrorOutput( pse , "GetForwardSessionUnusedUnit failed[%d]\n" , nret );
			close( client_addr.sock );
			return 1;
		}
		
		p_forward_session_client->forward_session_type = FORWARD_SESSION_TYPE_MANAGE ;
		
		strcpy( p_forward_session_client->client_addr.netaddr.ip , client_addr.netaddr.ip );
		strcpy( p_forward_session_client->client_addr.netaddr.port , client_addr.netaddr.port );
		p_forward_session_client->client_addr.sock = client_addr.sock ;
		memcpy( & (p_forward_session_client->client_addr.netaddr) , & (client_addr.netaddr) , sizeof(struct sockaddr_in) );
		p_forward_session_client->client_index = p_forward_session_client - & (pse->forward_session[0]) ;
		
		strcpy( p_forward_session_client->listen_addr.netaddr.ip , p_forward_session->listen_addr.netaddr.ip );
		strcpy( p_forward_session_client->listen_addr.netaddr.port , p_forward_session->listen_addr.netaddr.port );
		p_forward_session_client->listen_addr.sock = p_forward_session->listen_addr.sock ;
		memcpy( & (p_forward_session_client->listen_addr.netaddr) , & (p_forward_session->listen_addr.netaddr) , sizeof(struct sockaddr_in) );
		strcpy( p_forward_session_client->listen_addr.rule_mode , p_forward_session->listen_addr.rule_mode );
		
		memset( & (client_event) , 0x00 , sizeof(client_event) );
		client_event.data.ptr = p_forward_session_client ;
		client_event.events = EPOLLIN | EPOLLERR | EPOLLET ;
		epoll_ctl( pse->event_env , EPOLL_CTL_ADD , p_forward_session_client->client_addr.sock , & client_event );
		
		DebugOutput( pse , "accept [%s:%s]#%d# - [%s:%s]#%d# manage\n"
			, p_forward_session_client->client_addr.netaddr.ip , p_forward_session_client->client_addr.netaddr.port , p_forward_session_client->client_addr.sock
			, p_forward_session->listen_addr.netaddr.ip , p_forward_session->listen_addr.netaddr.port , p_forward_session->listen_addr.sock );
		
		send( p_forward_session_client->client_addr.sock , "> " , 2 , 0 );
	}
	
	return 0;
}

/* 接受转发端口连接 */
static int AcceptForwardSocket( struct ServerEnv *pse , struct epoll_event *p_event , struct ForwardSession *p_forward_session )
{
	socklen_t		addr_len = sizeof(struct sockaddr_in) ;
	
	struct ClientNetAddress	client_addr ;
	
	struct ForwardRule	*p_forward_rule = NULL ;
	
	int			nret = 0 ;
	
	/* 循环接受转发端口连接 */
	while(1)
	{
		/* 接受转发端口连接 */
		client_addr.sock = accept( p_forward_session->listen_addr.sock , (struct sockaddr *) & (client_addr.netaddr.sockaddr) , & addr_len ) ;
		if( client_addr.sock < 0 )
		{
			if( errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED || errno == EPROTO )
				break;
			
			ErrorOutput( pse , "accept[%d] failed[%d]errno[%d]\n" , p_forward_session->listen_addr.sock , client_addr.sock  , errno );
			return 1;
		}
		
		SetNonBlocking( client_addr.sock );
		SetReuseAddr( client_addr.sock );
		
		strcpy( client_addr.netaddr.ip , inet_ntoa( client_addr.netaddr.sockaddr.sin_addr ) );
		sprintf( client_addr.netaddr.port , "%ld" , (unsigned long)ntohs( client_addr.netaddr.sockaddr.sin_port ) );
		
		/* 匹配转发规则 */
		nret = MatchForwardRule( pse , & client_addr , & (p_forward_session->listen_addr) , & p_forward_rule ) ;
		if( nret != FOUND )
		{
			ErrorOutput( pse , "match forward rule [%s:%s] - [%s:%s] failed[%d]\n" , client_addr.netaddr.ip , client_addr.netaddr.port , p_forward_session->listen_addr.netaddr.ip , p_forward_session->listen_addr.netaddr.port , nret );
			close( client_addr.sock );
			return 1;
		}
		
		/* 连接目标网络地址 */
		nret = ConnectToRemote( pse , p_event , p_forward_session , p_forward_rule , & client_addr , TRY_CONNECT_MAXCOUNT ) ;
		if( nret )
		{
			close( client_addr.sock );
			return nret;
		}
	}
	
	return 0;
}

/* 转发通讯数据 */
static int TransferSocketData( struct ServerEnv *pse , struct epoll_event *p_event , struct ForwardSession *p_forward_session )
{
	int			in_sock ;
	int			out_sock ;
	
	static char		*buf = NULL ;
	char			*p = NULL ;
	ssize_t			recv_len ;
	ssize_t			send_len ;
	ssize_t			len ;
	
	unsigned long		client_index ;
	unsigned long		server_index ;
	
	if( buf == NULL )
	{
		/* 初始化通讯数据转发缓冲区 */
		buf = (char*)malloc( pse->cmd_para.transfer_bufsize + 1 ) ;
		if( buf == NULL )
		{
			ErrorOutput( pse , "alloc transfer_bufsize failed , errno[%d]\n" , errno );
			exit(1);
		}
		memset( buf , 0x00 , pse->cmd_para.transfer_bufsize + 1 );
	}
	
	if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_CLIENT )
	{
		in_sock = p_forward_session->client_addr.sock ;
		out_sock = p_forward_session->server_addr.sock ;
	}
	else
	{
		in_sock = p_forward_session->server_addr.sock ;
		out_sock = p_forward_session->client_addr.sock ;
	}
	client_index = p_forward_session->client_index ;
	server_index = p_forward_session->server_index ;
	
	while(1)
	{
		/* 接收通讯数据 */
		memset( buf , 0x00 , pse->cmd_para.transfer_bufsize );
		recv_len = recv( in_sock , buf , pse->cmd_para.transfer_bufsize , 0 ) ;
		if( recv_len < 0 )
		{
			if( errno == EAGAIN )
			{
				break;
			}
			else if( errno == ECONNRESET )
			{
				pse->forward_session[server_index].p_forward_rule->connection_count[p_forward_session->p_forward_rule->select_index]--;
				epoll_ctl( pse->event_env , EPOLL_CTL_DEL , in_sock , NULL );
				epoll_ctl( pse->event_env , EPOLL_CTL_DEL , out_sock , NULL );
				ErrorOutput( pse , "close #%d# reset , close #%d# passivity\n" , in_sock , out_sock );
				close( in_sock );
				close( out_sock );
				SetForwardSessionUnitUnused( & (pse->forward_session[client_index]) );
				SetForwardSessionUnitUnused( & (pse->forward_session[server_index]) );
				return 0;
			}
			else
			{
				pse->forward_session[server_index].p_forward_rule->connection_count[p_forward_session->p_forward_rule->select_index]--;
				epoll_ctl( pse->event_env , EPOLL_CTL_DEL , in_sock , NULL );
				epoll_ctl( pse->event_env , EPOLL_CTL_DEL , out_sock , NULL );
				ErrorOutput( pse , "close #%d# recv error errno[%d] , close #%d# passivity\n" , in_sock , errno , out_sock );
				close( in_sock );
				close( out_sock );
				SetForwardSessionUnitUnused( & (pse->forward_session[client_index]) );
				SetForwardSessionUnitUnused( & (pse->forward_session[server_index]) );
				return 0;
			}
		}
		else if( recv_len == 0 )
		{
			/* 通讯连接断开处理 */
			pse->forward_session[server_index].p_forward_rule->connection_count[p_forward_session->p_forward_rule->select_index]--;
			epoll_ctl( pse->event_env , EPOLL_CTL_DEL , in_sock , NULL );
			epoll_ctl( pse->event_env , EPOLL_CTL_DEL , out_sock , NULL );
			DebugOutput( pse , "close #%d# recv 0 , close #%d# passivity\n" , in_sock , out_sock );
			close( in_sock );
			close( out_sock );
			SetForwardSessionUnitUnused( & (pse->forward_session[client_index]) );
			SetForwardSessionUnitUnused( & (pse->forward_session[server_index]) );
			return 0;
		}
		
		/* RT模式额外处理 */
		if( strcmp( pse->forward_session[server_index].p_forward_rule->rule_mode , FORWARD_RULE_MODE_RT ) == 0 )
		{
			if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_CLIENT )
			{
				pse->forward_session[server_index].p_forward_rule->status.RT[pse->forward_session[server_index].p_forward_rule->select_index].tv1 = pse->server_cache.tv ;
			}
			else
			{
				pse->forward_session[server_index].p_forward_rule->status.RT[pse->forward_session[server_index].p_forward_rule->select_index].tv2 = pse->server_cache.tv ;
			}
		}
		
		/* 发送通讯数据 */
		p = buf ;
		send_len = recv_len ;
		while( send_len )
		{
			len = send( out_sock , p , send_len , 0 ) ;
			if( len < 0 )
			{
				if( errno == EAGAIN )
				{
					continue;
				}
				else
				{
					pse->forward_session[server_index].p_forward_rule->connection_count[p_forward_session->p_forward_rule->select_index]--;
					epoll_ctl( pse->event_env , EPOLL_CTL_DEL , in_sock , NULL );
					epoll_ctl( pse->event_env , EPOLL_CTL_DEL , out_sock , NULL );
					ErrorOutput( pse , "close #%d# send error , close #%d# passivity\n" , in_sock , out_sock );
					close( in_sock );
					close( out_sock );
					SetForwardSessionUnitUnused( & (pse->forward_session[client_index]) );
					SetForwardSessionUnitUnused( & (pse->forward_session[server_index]) );
					return 0;
				}
			}
			
			p += len ;
			send_len -= len ;
		}
		
		DebugOutput( pse , "transfer #%d# [%d]bytes to #%d#\n" , in_sock , recv_len , out_sock );
	}
	
	return 0;
}

/* 处理管理命令 */
static int ProcessManageCommand( struct ServerEnv *pse , int out_sock , struct ForwardSession *p_forward_session )
{
	char		buf[ MANAGE_OUTPUT_BUFSIZE + 1 ] ;
	
	char		cmd1[ 64 + 1 ] ;
	char		cmd2[ 64 + 1 ] ;
	char		cmd3[ 64 + 1 ] ;
	
	memset( cmd1 , 0x00 , sizeof(cmd1) );
	memset( cmd2 , 0x00 , sizeof(cmd2) );
	sscanf( p_forward_session->manage_input_buffer , "%64s %64s %64s" , cmd1 , cmd2 , cmd3 );
	
	if( strcmp( cmd1 , "ver" ) == 0 )
	{
		/* 显示版本 */
		memset( buf , 0x00 , sizeof(buf) );
		snprintf( buf , sizeof(buf)-1 , "version v%s build %s %s\n" , VERSION , __DATE__ , __TIME__ );
		send( out_sock , buf , strlen(buf) , 0 );
	}
	else if( strcmp( cmd1 , "list" ) == 0 && strcmp( cmd2 , "rules" ) == 0 )
	{
		unsigned long			forward_rule_index ;
		struct ForwardRule		*p_forward_rule = NULL ;
		
		unsigned long			client_index ;
		struct ClientNetAddress		*p_client_addr = NULL ;
		unsigned long			forward_index ;
		struct ForwardNetAddress	*p_forward_addr = NULL ;
		unsigned long			server_index ;
		struct ServerNetAddress		*p_server_addr = NULL ;
		
		/* 列表所有转发规则 */
		for( forward_rule_index = 0 , p_forward_rule = & (pse->forward_rule[0]) ; forward_rule_index < pse->forward_rule_count ; forward_rule_index++ , p_forward_rule++ )
		{
			snprintf( buf , sizeof(buf)-1 , "%6ld : %s %s" , forward_rule_index+1 , p_forward_rule->rule_id , p_forward_rule->rule_mode );
			send( out_sock , buf , strlen(buf) , 0 );
			
			for( client_index = 0 , p_client_addr = & (p_forward_rule->client_addr[0]) ; client_index < p_forward_rule->client_count ; client_index++ , p_client_addr++ )
			{
				snprintf( buf , sizeof(buf)-1 , " %s:%s" , p_client_addr->netaddr.ip , p_client_addr->netaddr.port );
				send( out_sock , buf , strlen(buf) , 0 );
			}
			
			snprintf( buf , sizeof(buf)-1 , " -" );
			send( out_sock , buf , strlen(buf) , 0 );
			
			for( forward_index = 0 , p_forward_addr = & (p_forward_rule->forward_addr[0]) ; forward_index < p_forward_rule->forward_count ; forward_index++ , p_forward_addr++ )
			{
				snprintf( buf , sizeof(buf)-1 , " %s:%s" , p_forward_addr->netaddr.ip , p_forward_addr->netaddr.port );
				send( out_sock , buf , strlen(buf) , 0 );
			}
			
			if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_G ) != 0 )
			{
				snprintf( buf , sizeof(buf)-1 , " >" );
				send( out_sock , buf , strlen(buf) , 0 );
				
				for( server_index = 0 , p_server_addr = & (p_forward_rule->server_addr[0]) ; server_index < p_forward_rule->server_count ; server_index++ , p_server_addr++ )
				{
					snprintf( buf , sizeof(buf)-1 , " %s:%s" , p_server_addr->netaddr.ip , p_server_addr->netaddr.port );
					send( out_sock , buf , strlen(buf) , 0 );
				}
			}
			
			snprintf( buf , sizeof(buf)-1 , " ;\n" );
			send( out_sock , buf , strlen(buf) , 0 );
		}
	}
	else if( strcmp( cmd1 , "add" ) == 0 && strcmp( cmd2 , "rule" ) == 0 )
	{
		char				*p_buffer = NULL ;
		struct ForwardRule		forward_rule ;
		
		int				nret = 0 ;
		
		/* 新增转发规则 */
		p_buffer = strtok( p_forward_session->manage_input_buffer , " \t" ) ;
		p_buffer = strtok( NULL , " \t" ) ;
		p_buffer = strtok( NULL , "" ) ;
		nret = LoadForwardConfig( pse , p_buffer , & forward_rule ) ;
		if( nret > 0 )
		{
		}
		else if( nret < 0 )
		{
			snprintf( buf , sizeof(buf)-1 , "parse forward rule failed[%d]\n" , nret );
			send( out_sock , buf , strlen(buf) , 0 );
		}
		else
		{
			nret = AddForwardRule( pse , & forward_rule ) ;
			if( nret )
			{
				snprintf( buf , sizeof(buf)-1 , "add forward rule failed[%d]\n" , nret );
				send( out_sock , buf , strlen(buf) , 0 );
			}
			else
			{
				snprintf( buf , sizeof(buf)-1 , "add forward rule ok\n" );
				send( out_sock , buf , strlen(buf) , 0 );
			}
		}
	}
	else if( strcmp( cmd1 , "modify" ) == 0 && strcmp( cmd2 , "rule" ) == 0 )
	{
		char				*p_buffer = NULL ;
		struct ForwardRule		forward_rule ;
		
		int				nret = 0 ;
		
		/* 修改转发规则 */
		p_buffer = strtok( p_forward_session->manage_input_buffer , " \t" ) ;
		p_buffer = strtok( NULL , " \t" ) ;
		p_buffer = strtok( NULL , "" ) ;
		nret = LoadForwardConfig( pse , p_buffer , & forward_rule ) ;
		if( nret > 0 )
		{
		}
		else if( nret < 0 )
		{
			snprintf( buf , sizeof(buf)-1 , "parse forward rule failed[%d]\n" , nret );
			send( out_sock , buf , strlen(buf) , 0 );
		}
		else
		{
			nret = ModifyForwardRule( pse , & forward_rule ) ;
			if( nret )
			{
				snprintf( buf , sizeof(buf)-1 , "modify forward rule failed[%d]\n" , nret );
				send( out_sock , buf , strlen(buf) , 0 );
			}
			else
			{
				snprintf( buf , sizeof(buf)-1 , "modify forward rule ok\n" );
				send( out_sock , buf , strlen(buf) , 0 );
			}
		}
	}
	else if( strcmp( cmd1 , "remove" ) == 0 && strcmp( cmd2 , "rule" ) == 0 )
	{
		int				nret = 0 ;
		
		/* 删除转发规则 */
		nret = RemoveForwardRule( pse , cmd3 ) ;
		if( nret )
		{
			snprintf( buf , sizeof(buf)-1 , "remove forward rule failed[%d]\n" , nret );
			send( out_sock , buf , strlen(buf) , 0 );
		}
		else
		{
			snprintf( buf , sizeof(buf)-1 , "remove forward rule ok\n" );
			send( out_sock , buf , strlen(buf) , 0 );
		}
	}
	else if( strcmp( cmd1 , "dump" ) == 0 && strcmp( cmd2 , "rules" ) == 0 )
	{
		FILE				*fp = NULL ;
		
		unsigned long			forward_rule_index ;
		struct ForwardRule		*p_forward_rule = NULL ;
		
		unsigned long			client_index ;
		struct ClientNetAddress		*p_client_addr = NULL ;
		unsigned long			forward_index ;
		struct ForwardNetAddress	*p_forward_addr = NULL ;
		unsigned long			server_index ;
		struct ServerNetAddress		*p_server_addr = NULL ;
		
		/* 保存规则到配置文件 */
		fp = fopen( pse->cmd_para.config_pathfilename , "w" ) ;
		if( fp == NULL )
		{
			snprintf( buf , sizeof(buf)-1 , "can't open config file for writing\n" );
			send( out_sock , buf , strlen(buf) , 0 );
			return 0;
		}
		
		for( forward_rule_index = 0 , p_forward_rule = & (pse->forward_rule[0]) ; forward_rule_index < pse->forward_rule_count ; forward_rule_index++ , p_forward_rule++ )
		{
			fprintf( fp , "%ld : %s %s" , forward_rule_index+1 , p_forward_rule->rule_id , p_forward_rule->rule_mode );
			
			for( client_index = 0 , p_client_addr = & (p_forward_rule->client_addr[0]) ; client_index < p_forward_rule->client_count ; client_index++ , p_client_addr++ )
			{
				fprintf( fp , " %s:%s" , p_client_addr->netaddr.ip , p_client_addr->netaddr.port );
			}
			
			fprintf( fp , " -" );
			
			for( forward_index = 0 , p_forward_addr = & (p_forward_rule->forward_addr[0]) ; forward_index < p_forward_rule->forward_count ; forward_index++ , p_forward_addr++ )
			{
				fprintf( fp , " %s:%s" , p_forward_addr->netaddr.ip , p_forward_addr->netaddr.port );
			}
			
			if( strcmp( p_forward_rule->rule_mode , FORWARD_RULE_MODE_G ) != 0 )
			{
				fprintf( fp , " >" );
				
				for( server_index = 0 , p_server_addr = & (p_forward_rule->server_addr[0]) ; server_index < p_forward_rule->server_count ; server_index++ , p_server_addr++ )
				{
					fprintf( fp , " %s:%s" , p_server_addr->netaddr.ip , p_server_addr->netaddr.port );
				}
			}
			
			fprintf( fp , " ;\n" );
		}
		
		fclose(fp);
		
		snprintf( buf , sizeof(buf)-1 , "dump all forward rules ok\n" );
		send( out_sock , buf , strlen(buf) , 0 );
	}
	else if( strcmp( cmd1 , "list" ) == 0 && strcmp( cmd2 , "forwards" ) == 0 )
	{
		unsigned long		index ;
		struct ForwardSession	*p_forward_session = NULL ;
		
		/* 列表转发会话 */
		for( index = 0 , p_forward_session = & (pse->forward_session[0]) ; index < pse->forward_session_maxcount ; index++ , p_forward_session++ )
		{
			memset( buf , 0x00 , sizeof(buf) );
			
			if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_MANAGE )
			{
				snprintf( buf , sizeof(buf)-1 , "%6ld : CLIENT [%s:%s]#%d# - MANAGE [%s:%s]#%d#\n"
					, index+1
					, p_forward_session->client_addr.netaddr.ip , p_forward_session->client_addr.netaddr.port , p_forward_session->client_addr.sock
					, p_forward_session->listen_addr.netaddr.ip , p_forward_session->listen_addr.netaddr.port , p_forward_session->listen_addr.sock );
			}
			else if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_LISTEN )
			{
				snprintf( buf , sizeof(buf)-1 , "%6ld : LISTEN [%s:%s]#%d#\n"
					, index+1
					, p_forward_session->listen_addr.netaddr.ip , p_forward_session->listen_addr.netaddr.port , p_forward_session->listen_addr.sock );
			}
			else if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_CLIENT )
			{
				snprintf( buf , sizeof(buf)-1 , "%6ld : CLIENT [%s:%s]#%d# - LISTEN [%s:%s]#%d# > SERVER [%s:%s]#%d# %s\n"
					, index+1
					, p_forward_session->client_addr.netaddr.ip , p_forward_session->client_addr.netaddr.port , p_forward_session->client_addr.sock
					, p_forward_session->listen_addr.netaddr.ip , p_forward_session->listen_addr.netaddr.port , p_forward_session->listen_addr.sock
					, p_forward_session->server_addr.netaddr.ip , p_forward_session->server_addr.netaddr.port , p_forward_session->server_addr.sock
					, ( p_forward_session->connect_status == CONNECT_STATUS_CONNECTING ? "connecting" : "connected" ) );
			}
			else if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_SERVER )
			{
				snprintf( buf , sizeof(buf)-1 , "%6ld : CLIENT [%s:%s]#%d# < LISTEN [%s:%s]#%d# - SERVER [%s:%s]#%d# %s\n"
					, index+1
					, p_forward_session->client_addr.netaddr.ip , p_forward_session->client_addr.netaddr.port , p_forward_session->client_addr.sock
					, p_forward_session->listen_addr.netaddr.ip , p_forward_session->listen_addr.netaddr.port , p_forward_session->listen_addr.sock
					, p_forward_session->server_addr.netaddr.ip , p_forward_session->server_addr.netaddr.port , p_forward_session->server_addr.sock
					, ( p_forward_session->connect_status == CONNECT_STATUS_CONNECTING ? "connecting" : "connected" ) );
			}
			
			send( out_sock , buf , strlen(buf) , 0 );
		}
	}
	else if( strcmp( cmd1 , "quit" ) == 0 )
	{
		/* 断开管理端口 */
		epoll_ctl( pse->event_env , EPOLL_CTL_DEL , out_sock , NULL );
		ErrorOutput( pse , "close #%d# initiative\n" , out_sock );
		close( out_sock );
		SetForwardSessionUnitUnused( p_forward_session );
		return 1;
	}
	else
	{
		memset( buf , 0x00 , sizeof(buf) );
		snprintf( buf , sizeof(buf)-1 , "command invalid [%s]\n" , p_forward_session->manage_input_buffer );
		send( out_sock , buf , strlen(buf) , 0 );
	}
	
	return 0;
}

/* 接收管理命令，或并处理之 */
static int ReceiveOrProcessManageData( struct ServerEnv *pse , struct epoll_event *p_event , struct ForwardSession *p_forward_session )
{
	int		in_sock ;
	int		out_sock ;
	
	char		*p_manage_buffer_offset = NULL ;
	ssize_t		manage_input_remain_bufsize ;
	ssize_t		recv_len ;
	char		*p = NULL ;
	
	int		nret = 0 ;
	
	in_sock = p_forward_session->client_addr.sock ;
	out_sock = in_sock ;
	
	while(1)
	{
		/* 接收管理端口数据 */
		p_manage_buffer_offset = p_forward_session->manage_input_buffer + p_forward_session->manage_input_buflen ;
		manage_input_remain_bufsize = MANAGE_INPUT_BUFSIZE-1 - p_forward_session->manage_input_buflen ;
		if( manage_input_remain_bufsize == 0 )
		{
			epoll_ctl( pse->event_env , EPOLL_CTL_DEL , in_sock , NULL );
			ErrorOutput( pse , "close #%d# too many data\n" , in_sock );
			close( in_sock );
			SetForwardSessionUnitUnused( p_forward_session );
			return 0;
		}
		recv_len = recv( in_sock , p_manage_buffer_offset , manage_input_remain_bufsize , 0 ) ;
		if( recv_len < 0 )
		{
			if( errno == EAGAIN )
			{
				break;
			}
			else if( errno == ECONNRESET )
			{
				epoll_ctl( pse->event_env , EPOLL_CTL_DEL , in_sock , NULL );
				ErrorOutput( pse , "close #%d# , reset\n" , in_sock );
				close( in_sock );
				SetForwardSessionUnitUnused( p_forward_session );
				return 0;
			}
			else
			{
				epoll_ctl( pse->event_env , EPOLL_CTL_DEL , in_sock , NULL );
				ErrorOutput( pse , "close #%d# , recv error\n" , in_sock );
				close( in_sock );
				SetForwardSessionUnitUnused( p_forward_session );
				return 0;
			}
		}
		else if( recv_len == 0 )
		{
			/* 接受到断开事件 */
			epoll_ctl( pse->event_env , EPOLL_CTL_DEL , in_sock , NULL );
			ErrorOutput( pse , "close #%d# recv 0\n" , in_sock );
			close( in_sock );
			SetForwardSessionUnitUnused( p_forward_session );
			return 0;
		}
		
		/* 判断是否形成完整命令数据 */
		p = strchr( p_manage_buffer_offset , '\n' ) ;
		if( p )
		{
			/* 已经形成完整命令数据，处理之，并保留未成型命令 */
			(*p) = '\0' ;
			if( p - p_manage_buffer_offset > 0 && *(p-1) == '\r' )
				*(p-1) = '\0' ;
			
			if( p_forward_session->manage_input_buffer[0] != '\0' )
			{
				nret = ProcessManageCommand( pse , out_sock , p_forward_session ) ;
				if( nret > 0 )
				{
					break;
				}
				else if( nret < 0 )
				{
					epoll_ctl( pse->event_env , EPOLL_CTL_DEL , in_sock , NULL );
					ErrorOutput( pse , "close #%d# proc error\n" , in_sock );
					close( in_sock );
					SetForwardSessionUnitUnused( p_forward_session );
					return -1;
				}
			}
			
			p_forward_session->manage_input_buflen = strlen(p+1) ;
			memmove( p_forward_session->manage_input_buffer , p+1 , strlen(p+1)+1 );
			memset( p_forward_session->manage_input_buffer , 0x00 , MANAGE_INPUT_BUFSIZE - p_forward_session->manage_input_buflen );
			
			send( out_sock , "> " , 2 , 0 );
		}
		else
		{
			/* 未形成完整命令，继续累加 */
			p_forward_session->manage_input_buflen += recv_len ;
		}
	}
	
	return 0;
}

/* 异步连接目标网络地址后回调，连接成功后登记到epoll池 */
static int SetSocketConnected( struct ServerEnv *pse , struct epoll_event *p_event , struct ForwardSession *p_forward_session_server )
{
	struct ForwardSession	*p_forward_session_client = NULL ;
	struct epoll_event	client_event ;
	struct epoll_event	server_event ;
	
	int			nret = 0 ;
	
	/* 查询epoll池未用单元 */
	nret = GetForwardSessionUnusedUnit( pse , & p_forward_session_client ) ;
	if( nret != FOUND )
	{
		ErrorOutput( pse , "GetForwardSessionUnusedUnit failed[%d]\n" , nret );
		close( p_forward_session_server->client_addr.sock );
		close( p_forward_session_server->server_addr.sock );
		SetForwardSessionUnitUnused( p_forward_session_server );
		return 1;
	}
	
	/* 登记客户端信息转发会话、epoll池，更新服务端信息 */
	p_forward_session_client->forward_session_type = FORWARD_SESSION_TYPE_CLIENT ;
	
	memcpy( & (p_forward_session_client->client_addr) , & (p_forward_session_server->client_addr) , sizeof(struct ClientNetAddress) );
	p_forward_session_client->client_index = p_forward_session_client - & (pse->forward_session[0]) ;
	memcpy( & (p_forward_session_client->listen_addr) , & (p_forward_session_server->listen_addr) , sizeof(struct ClientNetAddress) );
	memcpy( & (p_forward_session_client->server_addr) , & (p_forward_session_server->server_addr) , sizeof(struct ServerNetAddress) );
	p_forward_session_client->server_index = p_forward_session_server->server_index ;
	
	p_forward_session_client->p_forward_rule = p_forward_session_server->p_forward_rule ;
	p_forward_session_client->connect_status = CONNECT_STATUS_CONNECTED ;
	
	memset( & (client_event) , 0x00 , sizeof(client_event) );
	client_event.data.ptr = p_forward_session_client ;
	client_event.events = EPOLLIN | EPOLLERR | EPOLLET ;
	epoll_ctl( pse->event_env , EPOLL_CTL_ADD , p_forward_session_client->client_addr.sock , & client_event );
	
	p_forward_session_server->client_index = p_forward_session_client - & (pse->forward_session[0]) ;
	p_forward_session_server->connect_status = CONNECT_STATUS_CONNECTED ;
	
	memset( & (server_event) , 0x00 , sizeof(server_event) );
	server_event.data.ptr = p_forward_session_server ;
	server_event.events = EPOLLIN | EPOLLERR | EPOLLET ;
	epoll_ctl( pse->event_env , EPOLL_CTL_MOD , p_forward_session_server->server_addr.sock , & server_event );
	
	DebugOutput( pse , "forward2 [%s:%s]#%d# - [%s:%s]#%d# > [%s:%s]#%d#\n"
		, p_forward_session_client->client_addr.netaddr.ip , p_forward_session_client->client_addr.netaddr.port , p_forward_session_client->client_addr.sock
		, p_forward_session_server->listen_addr.netaddr.ip , p_forward_session_server->listen_addr.netaddr.port , p_forward_session_server->listen_addr.sock
		, p_forward_session_server->server_addr.netaddr.ip , p_forward_session_server->server_addr.netaddr.port , p_forward_session_server->server_addr.sock );
	
	return 0;
}

/* 解决sock错误处理 */
static int ResolveSocketError( struct ServerEnv *pse , struct epoll_event *p_event , struct ForwardSession *p_forward_session )
{
	int		nret = 0 ;
	
	/* 如果是异步连接错误事件 */
	if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_SERVER && p_forward_session->connect_status == CONNECT_STATUS_CONNECTING )
	{
		ErrorOutput( pse , "connect2 to [%s:%s] failed\n" , p_forward_session->server_addr.netaddr.ip , p_forward_session->server_addr.netaddr.port );
		
		/* 处理目标网络地址不可用错误 */
		nret = OnServerUnable( pse , p_forward_session->p_forward_rule ) ;
		if( nret )
			goto _CLOSE_PAIR;
		
		if( p_forward_session->try_connect_count <= 0 )
		{
			goto _CLOSE_PAIR;
		}
		
		/* 连接其它目标网络地址 */
		nret = ConnectToRemote( pse , p_event , p_forward_session , p_forward_session->p_forward_rule , & (p_forward_session->client_addr) , --p_forward_session->try_connect_count ) ;
		
		/* 从epoll池中删除客户端sock */
		p_forward_session->p_forward_rule->connection_count[p_forward_session->p_forward_rule->select_index]--;
		epoll_ctl( pse->event_env , EPOLL_CTL_DEL , p_forward_session->server_addr.sock , NULL );
		close( p_forward_session->server_addr.sock );
		SetForwardSessionUnitUnused( p_forward_session );
		
		if( nret )
		{
			goto _CLOSE_PAIR;
		}
	}
	/* 如果是转发错误事件 */
	else
	{
_CLOSE_PAIR :
	{
		int			in_sock ;
		int			out_sock ;
		unsigned long		client_index ;
		unsigned long		server_index ;
		
		/* 从epoll池中删除转发两端信息、删除转发会话 */
		if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_CLIENT )
		{
			in_sock = p_forward_session->client_addr.sock ;
			out_sock = p_forward_session->server_addr.sock ;
		}
		else
		{
			in_sock = p_forward_session->server_addr.sock ;
			out_sock = p_forward_session->client_addr.sock ;
		}
		client_index = p_forward_session->client_index ;
		server_index = p_forward_session->server_index ;
		
		pse->forward_session[server_index].p_forward_rule->connection_count[p_forward_session->server_index]--;
		epoll_ctl( pse->event_env , EPOLL_CTL_DEL , in_sock , NULL );
		epoll_ctl( pse->event_env , EPOLL_CTL_DEL , out_sock , NULL );
		ErrorOutput( pse , "close #%d# - #%d# EPOLLERR\n" , in_sock , out_sock );
		close( in_sock );
		close( out_sock );
		SetForwardSessionUnitUnused( & (pse->forward_session[client_index]) );
		SetForwardSessionUnitUnused( & (pse->forward_session[server_index]) );
	}
	}
	
	return 0;
}

/* 服务器主工作循环 */
static int ServerLoop( struct ServerEnv *pse )
{
	struct epoll_event	events[ WAIT_EVENTS_COUNT ] , *p_event = NULL ;
	int			sock_count ;
	int			sock_index ;
	
	struct ForwardSession	*p_forward_session = NULL ;
	
	int			nret = 0 ;
	
	while(1)
	{
		/* 批量等待epoll事件 */
		sock_count = epoll_wait( pse->event_env , events , WAIT_EVENTS_COUNT , -1 ) ;
		
		gettimeofday( & (pse->server_cache.tv) , NULL );
		
		for( sock_index = 0 , p_event = & (events[0]) ; sock_index < sock_count ; sock_index++ , p_event++ )
		{
			p_forward_session = p_event->data.ptr ;
			
			/* 如果是侦听端口事件 */
			if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_LISTEN )
			{
				/* 如果是管理端口事件 */
				if( strcmp( p_forward_session->listen_addr.rule_mode , FORWARD_RULE_MODE_G ) == 0 )
				{
					nret = AcceptManageSocket( pse , p_event , p_forward_session ) ;
					if( nret > 0 )
					{
						continue;
					}
					else if( nret < 0 )
					{
						ErrorOutput( pse , "AcceptManageSocket failed[%d]\n" , nret );
						return nret;
					}
				}
				/* 如果是转发端口事件 */
				else
				{
					nret = AcceptForwardSocket( pse , p_event , p_forward_session ) ;
					if( nret > 0 )
					{
						continue;
					}
					else if( nret < 0 )
					{
						ErrorOutput( pse , "AcceptForwardSocket failed[%d]\n" , nret );
						return nret;
					}
				}
			}
			/* 如果是输入事件 */
			else if( p_event->events & EPOLLIN )
			{
				/* 如果是管理端口输入事件 */
				if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_MANAGE )
				{
					nret = ReceiveOrProcessManageData( pse , p_event , p_forward_session ) ;
					if( nret > 0 )
					{
						continue;
					}
					else if( nret < 0 )
					{
						ErrorOutput( pse , "ReceiveOrProcessManageData failed[%d]\n" , nret );
						return nret;
					}
				}
				/* 如果是转发端口输入事件 */
				else if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_CLIENT
					||
					p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_SERVER
				)
				{
					nret = TransferSocketData( pse , p_event , p_forward_session ) ;
					if( nret > 0 )
					{
						continue;
					}
					else if( nret < 0 )
					{
						ErrorOutput( pse , "TransferSocketData failed[%d]\n" , nret );
						return nret;
					}
				}
			}
			/* 如果是输出事件 */
			else if( p_event->events & EPOLLOUT )
			{
				if( p_forward_session->forward_session_type == FORWARD_SESSION_TYPE_SERVER )
				{
					nret = SetSocketConnected( pse , p_event , p_forward_session ) ;
					if( nret > 0 )
					{
						continue;
					}
					else if( nret < 0 )
					{
						ErrorOutput( pse , "SetSocketConnected failed[%d]\n" , nret );
						return nret;
					}
				}
			}
			/* 如果是错误事件 */
			else if( p_event->events & EPOLLERR )
			{
				nret = ResolveSocketError( pse , p_event , p_forward_session ) ;
				if( nret > 0 )
				{
					continue;
				}
				else if( nret < 0 )
				{
					ErrorOutput( pse , "ResolveSocketError failed[%d]\n" , nret );
					return nret;
				}
			}
		}
	}
	
	return 0;
}

/* G5入口函数 */
int G5( struct ServerEnv *pse )
{
	int			nret ;
	
	/* 创建epoll池 */
	pse->event_env = epoll_create( pse->forward_session_maxcount ) ;
	if( pse->event_env < 0 )
	{
		ErrorOutput( pse , "epoll_create failed[%d]errno[%d]\n" , pse->event_env , errno );
		return -1;
	}
	
	printf( "epoll_create ok #%d#\n" , pse->event_env );
	
	/* 装载配置 */
	nret = LoadConfig( pse ) ;
	if( nret )
	{
		ErrorOutput( pse , "load config failed[%d]\n" , nret );
		return nret;
	}
	
	/* 服务器主工作循环 */
	nret = ServerLoop( pse ) ;
	if( nret )
	{
		ErrorOutput( pse , "server loop failed[%d]\n" , nret );
		return nret;
	}
	
	/* 销毁epoll池 */
	close( pse->event_env );
	
	return 0;
}

/* 软件版本及命令行参数说明 */
static void usage()
{
	printf( "G5 - tcp LB dispatch\n" );
	printf( "v%s build %s %s WITH %d:%d:%d,%d:%d:%d,%d\n" , VERSION , __DATE__ , __TIME__
		, DEFAULT_FORWARD_RULE_MAXCOUNT , DEFAULT_CONNECTION_MAXCOUNT , DEFAULT_TRANSFER_BUFSIZE
		, RULE_CLIENT_MAXCOUNT , RULE_FORWARD_MAXCOUNT , RULE_SERVER_MAXCOUNT
		, RULE_ID_MAXLEN );
	printf( "Copyright by calvin 2014\n" );
	printf( "Email : calvinwilliams.c@gmail.com\n" );
	printf( "\n" );
	printf( "USAGE : G5 -f config_pathfilename [ -r forward_rule_maxcount ] [ -s forward_session_maxcount ] [ -b transfer_bufsize ] [ -d ]\n" );
	printf( "\n" );
	return;
}

int main( int argc , char *argv[] )
{
	struct ServerEnv	se , *pse = & se ;
	
	int			ch ;
	
	/* 设置标准输出无缓冲 */
	setbuf( stdout , NULL );
	
	/* 设置随机数种子 */
	srand( (unsigned)time(NULL) );
	
	if( argc > 1 )
	{
		/* 初始化服务器环境 */
		memset( pse , 0x00 , sizeof(struct ServerEnv) );
		
		/* 初始化命令行参数 */
		pse->cmd_para.forward_rule_maxcount = DEFAULT_FORWARD_RULE_MAXCOUNT ;
		pse->cmd_para.forward_session_maxcount = DEFAULT_CONNECTION_MAXCOUNT ;
		pse->cmd_para.transfer_bufsize = DEFAULT_TRANSFER_BUFSIZE ;
		
		/* 分析命令行参数 */
		while( ( ch = getopt( argc , argv , "f:r:s:b:d" ) ) != -1 )
		{
			switch( ch )
			{
				case 'f' :
					pse->cmd_para.config_pathfilename = optarg ;
					break;
				case 'r' :
					pse->cmd_para.forward_rule_maxcount = atol(optarg) ;
					break;
				case 's' :
					pse->cmd_para.forward_session_maxcount = atol(optarg) ;
					break;
				case 'b' :
					pse->cmd_para.transfer_bufsize = atol(optarg) ;
					break;
				case 'd' :
					pse->cmd_para.debug_flag = 1 ;
					break;
				default :
					fprintf( stderr , "invalid opt[%c]\n" , ch );
					usage();
					return 7;
			}
		}
		if( pse->cmd_para.config_pathfilename == NULL )
		{
			usage();
			return 7;
		}
		
		/* 申请服务器环境内部内存 */
		pse->forward_rule = (struct ForwardRule *)malloc( sizeof(struct ForwardRule) * pse->cmd_para.forward_rule_maxcount ) ;
		if( pse->forward_rule == NULL )
		{
			fprintf( stderr , "alloc failed , errno[%d]\n" , errno );
			return 7;
		}
		memset( pse->forward_rule , 0x00 , sizeof(struct ForwardRule) * pse->cmd_para.forward_rule_maxcount );
		
		pse->forward_session_maxcount = pse->cmd_para.forward_session_maxcount * 3 ;
		pse->forward_session = (struct ForwardSession *)malloc( sizeof(struct ForwardSession) * pse->forward_session_maxcount ) ;
		if( pse->forward_session == NULL )
		{
			fprintf( stderr , "alloc failed , errno[%d]\n" , errno );
			return 7;
		}
		memset( pse->forward_session , 0x00 , sizeof(struct ForwardSession) * pse->forward_session_maxcount );
		
		printf( "forward_rule_maxcount    [%ld]\n" , pse->cmd_para.forward_rule_maxcount );
		printf( "forward_session_maxcount [%ld]\n" , pse->cmd_para.forward_session_maxcount );
		printf( "transfer_bufsize         [%ld]bytes\n" , pse->cmd_para.transfer_bufsize );
		
		/* 调用G5入口函数 */
		return -G5( pse );
	}
	else
	{
		usage();
		return 1;
	}
}
