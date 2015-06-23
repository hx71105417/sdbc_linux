struct Object_U {
	long object_no;
	char server[121]; 	// server host name 
	char listenport[41];
	char listenhost[41];
	char port[21];		// server service port
	long maxconnect;
	long shmkey;
	long shmlen;
	long startted;
	long pid;
};
typedef struct Object_U T_Object;
