T_PkgType login_tpl[]={
        {CH_CHAR,17,"DEVID",0,-1},
        {CH_CHAR,256,"LABEL"},
        {CH_CHAR,81,"CA"},
        {CH_CHAR,17,"UID"},
        {CH_CHAR,14,"PWD"},
};

T_PkgType log_tpl[]={
        {CH_CHAR,17,"DEVID",0,-1},
        {CH_CHAR,17,"UID"},
        {CH_CHAR,81,"DBUSER"},
        {CH_CHAR,81,"DBOWN"},
        {CH_DATE,YEAR_TO_SEC_LEN,"Logtime",YEAR_TO_SEC},
        {-1,0,0}
};
