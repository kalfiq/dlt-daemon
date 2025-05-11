#include <iostream>
#include <ctype.h>      /* for isprint() */
#include <stdlib.h>     /* for atoi() */
#include <sys/stat.h>   /* for S_IRUSR, S_IWUSR, S_IRGRP, S_IROTH */
#include <fcntl.h>      /* for open() */
#include <sys/uio.h>    /* for writev() */
#include <errno.h>
#include <string.h>
#include <glob.h>
#include <syslog.h>
#include <signal.h>
#include <sys/socket.h>
#ifdef __linux__
#   include <linux/limits.h>
#else
#   include <limits.h>
#endif
#include <inttypes.h>
#include "dlt_log.h"
#include "dlt_client.h"
#include "dlt-control-common.h"
#include <map>
#include <functional>

#include <Poco/FileChannel.h>
#include <Poco/Logger.h>

#define DLT_RECEIVE_ECU_ID "RECV"

Poco::Logger& Logger(Poco::Logger::get("Logger"));

DltClient dltclient;
static bool sig_close_recv = false;

typedef struct {
    int aflag;
    int sflag;
    int xflag;
    int mflag;
    int vflag;
    int yflag;
    int uflag;
    int rflag;
    char *ovalue;
    char *ovaluebase; /* ovalue without ".dlt" */
    char *fvalue;       /* filename for space separated filter file (<AppID> <ContextID>) */
    char *jvalue;       /* filename for json filter file */
    char *evalue;
    int bvalue;
    int rvalue;
    int sendSerialHeaderFlag;
    int resyncSerialHeaderFlag;
    int64_t climit;
    char ecuid[4];
    int ohandle;
    int64_t totalbytes; /* bytes written so far into the output file, used to check the file size limit */
    int part_num;    /* number of current output file if limit was exceeded */
    DltFile file;
    DltFilter filter;
    int port;
    char *ifaddr;
} DltReceiveData;

void signal_handler(int signal)
{
    switch (signal) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
        /* stop main loop */
        sig_close_recv = true;
        shutdown(dltclient.receiver.fd, SHUT_RD);
        break;
    default:
        /* This case should never happen! */
        break;
    } /* switch */

}

void log(char* msg, char** buff) {
    std::string payloadBuff[4];
    std::string payload = "";

    payloadBuff[0] = std::string(buff[0]) + std::string(" ") + std::string(buff[1]);
    payloadBuff[1] = std::string("[") + std::string(buff[5]) + std::string("]");
    payloadBuff[2] = std::string("[") + std::string(buff[6]) + std::string("]");
    payloadBuff[3] = std::string("[") + std::string(buff[8]) + std::string("]");

    for (int i = 0; i < 4; ++i) {
        payload += payloadBuff[i] + std::string(" ");
    }

    payload += std::string(msg);

    Poco::AutoPtr<Poco::FileChannel> channel = new Poco::FileChannel;

    channel->setProperty("path", "test.log");
    Logger.setChannel(channel);

    Logger.information(payload);
}

int dlt_receive_message_callback(DltMessage *message, void *data)
{
    DltReceiveData *dltdata;
    static char text[DLT_RECEIVE_BUFSIZE];
    static char header[DLT_RECEIVE_BUFSIZE];

    if ((message == 0) || (data == 0))
        return -1;

    dltdata = (DltReceiveData *)data;

    /* Prepare storage header */
    if (DLT_IS_HTYP_WEID(message->standardheader->htyp))
        dlt_set_storageheader(message->storageheader, message->headerextra.ecu);
    else
        dlt_set_storageheader(message->storageheader, dltdata->ecuid);

	/* If no filter set or filter is matching display message */
    if (((dltdata->fvalue || dltdata->jvalue) == 0) ||
        (dlt_message_filter_check(message, &(dltdata->filter), dltdata->vflag) == DLT_RETURN_TRUE)) {
        if (dltdata->aflag)
        {
            char* header_ptr = NULL;
            char* buff[20];
            bool foundFlag = false;
            uint8_t counter = 0;

            dlt_message_header(message, header, DLT_RECEIVE_BUFSIZE, dltdata->vflag);

            header_ptr = header;

            for (char* token = strtok(header_ptr, " "); token != NULL; token = strtok(header_ptr, " ")) {
                if (std::string(token) == "TAPP") {
                    foundFlag = true;
                    dlt_message_payload(message, text, DLT_RECEIVE_BUFSIZE, DLT_OUTPUT_ASCII, dltdata->vflag);
                }
                buff[counter++] = token;

                header_ptr = NULL;
            }

            if (foundFlag) {
                log(text, &buff[0]);
            }
        }
    }

    return 0;
}

/**
 * Main function of tool.
 */
int main()
{
    DltReceiveData dltdata;
    memset(&dltdata, 0, sizeof(dltdata));

    /* Initialize dltdata */
    dltdata.climit = -1; /* default: -1 = unlimited */
    dltdata.ohandle = -1;
    dltdata.part_num = -1;
    dltdata.port = 3490;

    /* Config signal handler */
    struct sigaction act;

    /* Initialize signal handler struct */
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP, &act, 0);
    sigaction(SIGTERM, &act, 0);
    sigaction(SIGINT, &act, 0);
    sigaction(SIGQUIT, &act, 0);

    /* Initialize DLT Client */
    dlt_client_init(&dltclient, dltdata.vflag);

    /* Register callback to be called when message was received */
    dlt_client_register_message_callback(dlt_receive_message_callback);

    /* Setup DLT Client structure */
    if(dltdata.uflag) {
        dltclient.mode = DLT_CLIENT_MODE_UDP_MULTICAST;
    }
    else {
        dltclient.mode = (DltClientMode)dltdata.yflag;
    }

		// Hard-coded way to assign the hostname
	char hostname[] = "localhost";
	if (dlt_client_set_server_ip(&dltclient, hostname) == -1) {
		fprintf(stderr, "Failed to set server ip\n");
		return -1;
	}

	dltdata.aflag = 1;
	dltclient.port = dltdata.port;

	if (dltclient.servIP == 0) {
		/* no hostname selected, show usage and terminate */
		fprintf(stderr, "ERROR: No hostname selected\n");
		// usage();
		dlt_client_cleanup(&dltclient, dltdata.vflag);
		return -1;
	}

	if (dltdata.ifaddr != 0) {
		if (dlt_client_set_host_if_address(&dltclient, dltdata.ifaddr) != DLT_RETURN_OK) {
			fprintf(stderr, "set host interface address didn't succeed\n");
			return -1;
		}
	}

    /* initialise structure to use DLT file */
    dlt_file_init(&(dltdata.file), dltdata.vflag);

    /* first parse filter file if filter parameter is used */
    dlt_filter_init(&(dltdata.filter), dltdata.vflag);

    if (dltdata.fvalue) {
        if (dlt_filter_load(&(dltdata.filter), dltdata.fvalue, dltdata.vflag) < DLT_RETURN_OK) {
            dlt_file_free(&(dltdata.file), dltdata.vflag);
            return -1;
        }

        dlt_file_set_filter(&(dltdata.file), &(dltdata.filter), dltdata.vflag);
    }

    while (true) {
        /* Attempt to connect to TCP socket or open serial device */
        if (dlt_client_connect(&dltclient, dltdata.vflag) != DLT_RETURN_ERROR) {

            /* Dlt Client Main Loop */
            dlt_client_main_loop(&dltclient, &dltdata, dltdata.vflag);

            if (dltdata.rflag == 1 && sig_close_recv == false) {
                dlt_vlog(LOG_INFO, "Reconnect to server with %d milli seconds specified\n", dltdata.rvalue);
                sleep(dltdata.rvalue / 1000);
            } else {
                /* Dlt Client Cleanup */
                dlt_client_cleanup(&dltclient, dltdata.vflag);
                break;
            }
        } else {
            break;
        }
    }

    free(dltdata.ovaluebase);
    dlt_file_free(&(dltdata.file), dltdata.vflag);
    dlt_filter_free(&(dltdata.filter), dltdata.vflag);

    return 0;
}
