use DB_CENTER_NEW
go
select tjdate,unit,tabname,flg,dat1,dat2,dat3 from tjrb
where tjdate='2007.01.02' and unit='00' and tabname='A1'
go
