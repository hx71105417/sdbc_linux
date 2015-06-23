#include <stdio.h>
#include <sccli.h>
/*  get filename from stdin,and send it to server */
//int sendfile(T_Connect *conn);
/*  get filename from remote host and send it to outfile */

#ifdef __cplusplus
extern "C" {
#endif

int getls(T_Connect *conn,T_NetHead *NetHead,char *path,FILE *outfile);
int s_getfile(T_Connect *conn,FILE *inputfile);
int m_getfile(T_Connect *conn,T_NetHead *NetHead,char *path);
char *strrtok(char *src,char *beg,char *tok);
int sh2reg(char *str);
void putlist(T_Connect *conn,char *str);
int NZ_Put_File(T_Connect *connect,char *local_file,char *remote_file);
int NZ_Put_File_Msg(T_Connect *connect,
        char *local_file,
        char *remote_file,
        void (*Msg)(int,int));
int NZ_Get_File(T_Connect *connect,char *local_file,char *remote_file);
int NZ_Get_File_Msg(T_Connect *connect,
        char *local_file,
        char *remote_file,
        void (*Msg)(int,int));

int event_catch(T_Connect *conn,int event_no);

#ifdef __cplusplus
}
#endif

