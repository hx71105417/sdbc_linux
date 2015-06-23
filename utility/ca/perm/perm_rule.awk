#/usr/bin/awk -f 
#
#Oracle table priv make script @1.1
#BY YangRuiLin May 2003
#Privilege definition
#
BEGIN {
        PRIV_SET="ABCDEFGHIJKLMNO"
	PRIV[1]="delete"
	PRIV[2]="update"
	PRIV[3]="update,delete"
	PRIV[4]="insert"
	PRIV[5]="insert,delete"
	PRIV[6]="insert,update"
	PRIV[7]="insert,update,delete"
	PRIV[8]="select"
	PRIV[9]="select,delete"
	PRIV[10]="select,update"
	PRIV[11]="select,update,delete"
	PRIV[12]="select,insert"
	PRIV[13]="select,insert,delete"
	PRIV[14]="select,insert,update"
	PRIV[15]="select,insert,update,delete"
	print "Now begin Process ......"
        print " "
}
/^!/{
}
/^%/{
	for ( i = 2 ; i < 5 ; i++ ) {
print $i
		USER[i]=$i
	}
}
!/^#/{
	if ( $2 > "" ) printf "revoke all on %s from public;\n",$2 >"perm.sql"
	for ( i = 3 ; i < NF-1 ; i++ ) {
                l=0
		if(substr($1,0,1) != "%") 		
       		printf "revoke all on %s from %s;\n",$2,USER[i-1] >"perm.sql"
                if ( $i > "") l = index(PRIV_SET,$i)
       		if ( l > 0 )  printf "grant %s on %s to %s;\n",PRIV[l],$2,USER[i-1] >"perm.sql"
	}
}
END {
	print ""
	print "End ... ..."
}
