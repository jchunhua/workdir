
server:SntpUtil.o sntp_server.o
	gcc -o sntp_server SntpUtil.o sntp_server.o
client:SntpUtil.o sntp_client.o
	gcc -o sntp_client SntpUtil.o sntp_client.o

SntpUtil.o:SntpUtil.c 
	gcc -c -o SntpUtil.o SntpUtil.c
sntp_client:sntp_client.o
	gcc -o sntp_client.o  sntp_client.c mysntp.h
sntp_server:sntp_s.o
	gcc -o sntp_s.o  sntp_server.c mysntp.h

clean:
	rm -rf *.o sntp_client sntp_server
