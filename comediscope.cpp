#include <QTimer>
#include <QPainter>
#include <QApplication>
#include <QTimerEvent>
#include <QPaintEvent>

extern "C" {
#include<stdio.h>
#include<fcntl.h>
#include<stdlib.h>
#include<fcntl.h>
}

#include "comediscope.h"


#ifdef _WIN32
#include <initguid.h>
DEFINE_GUID(g_guidServiceClass, 0xb62c4e8d, 0x62cc, 0x404b, 0xbb, 0xbf, 0xbf, 0x3e, 0x3b, 0xbb, 0x13, 0x74);
#endif


ComediScope::ComediScope(Attys_scope *attys_scope_tmp,
	float f
)
	: QWidget(attys_scope_tmp) {

	int maxComediDevs = 3;

	tb_init = 1;
	tb_counter = tb_init;
	attys_scope = attys_scope_tmp;
	// erase plot
	eraseFlag = 1;

	// for ASCII
	rec_file = NULL;

	// filename
	rec_filename = new QString();

	// flag if data has been recorded and we need a new filename
	recorded = 0;

	//////////////////////////////////////////////////////////////
	
	setAttribute(Qt::WA_OpaquePaintEvent);

	dev = new SOCKET[maxComediDevs];
	attysComm = new AttysComm*[maxComediDevs];
	for (int devNo = 0; devNo < maxComediDevs; devNo++) {
		dev[devNo] = 0;
		attysComm[devNo] = nullptr;
	}

	nComediDevices = 0;

#ifdef __linux__

	inquiry_info *ii = NULL;
	int max_rsp, num_rsp;
	int dev_id, sock, len, flags;
	int i;
	char addr[19] = { 0 };
	char name[248] = { 0 };
	struct sockaddr_rc saddr;

	dev_id = hci_get_route(NULL);
	sock = hci_open_dev( dev_id );
	if (dev_id < 0 || sock < 0) {
		perror("opening socket");
		exit(1);
	}
	
	len  = 8;
	max_rsp = 255;
	flags = IREQ_CACHE_FLUSH;
	ii = new inquiry_info[max_rsp];
    
	num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
	if( num_rsp < 0 ) perror("hci_inquiry");
		

	// let's probe how many we have
	nComediDevices = 0;
	for (i = 0; i < num_rsp; i++) {
		ba2str(&(ii+i)->bdaddr, addr);
		memset(name, 0, sizeof(name));
		if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), 
					 name, 0) < 0)
			strcpy(name, "[unknown]");
		printf("%s  %s", addr, name);
		if (strstr(name,"GN-ATTYS")!=0) {
			printf("!\n");
			// allocate a socket
			int s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
			
			// set the connection parameters (who to connect to)
			saddr.rc_family = AF_BLUETOOTH;
			saddr.rc_channel = (uint8_t) 1;
			str2ba( addr, &saddr.rc_bdaddr );

			// connect to server
			int status = ::connect(s,
					       (struct sockaddr *)&saddr,
					       sizeof(saddr));
			
			if (status == 0) {
				attysComm[nComediDevices] = new AttysComm(s);
				channels_in_use = attysComm[nComediDevices]->NCHANNELS;
				nComediDevices++;
				break;
			} else {
				printf("Connect failed: %d\n",status);
				printf("Has the device been paired?\n");
			}
		} else {
			printf("\n");
		}
	}

	delete[] ii;
	
	
#elif _WIN32

	WSADATA wsd;
	WSAStartup(MAKEWORD(2, 2), &wsd);

	WSAQUERYSET wsaq;
	ZeroMemory(&wsaq, sizeof(wsaq));
	wsaq.dwSize = sizeof(wsaq);
	wsaq.dwNameSpace = NS_BTH;
	wsaq.lpcsaBuffer = NULL;
	HANDLE hLookup = nullptr;
	int iRet = WSALookupServiceBegin(&wsaq, LUP_CONTAINERS, &hLookup);
	if (0 != iRet) {
		if (WSAGetLastError() != WSASERVICE_NOT_FOUND) {
			OutputDebugStringW(L"WSALookupServiceBegin failed\n");
			exit(1);
		}
		else {
			OutputDebugStringW(L"No bluetooth devices found\n");
			exit(1);
		}
	}

	CHAR buf[4096];
	LPWSAQUERYSET pwsaResults = (LPWSAQUERYSET)buf;
	ZeroMemory(pwsaResults, sizeof(WSAQUERYSET));
	pwsaResults->dwSize = sizeof(WSAQUERYSET);
	pwsaResults->dwNameSpace = NS_BTH;
	pwsaResults->lpBlob = NULL;

	DWORD dwSize = sizeof(buf);
	while (WSALookupServiceNext(hLookup, LUP_RETURN_NAME | LUP_RETURN_ADDR, &dwSize, pwsaResults) == 0) {
		LPWSTR name = pwsaResults->lpszServiceInstanceName;
		OutputDebugStringW(name);
		if (wcsstr(name, L"GN-ATTYS1") != 0) {
			OutputDebugStringW(L" -- Found an Attys!\n");

			attys_scope->splash->showMessage("Connecting to Attys");

			for (int i = 0; i < 2; i++) {

				// allocate a socket
				SOCKET s = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);

				if (INVALID_SOCKET == s) {
					OutputDebugStringW(L"=CRITICAL= | socket() call failed.\n");
					exit(1);
				}

				PSOCKADDR_BTH btAddr = (SOCKADDR_BTH *)(pwsaResults->lpcsaBuffer->RemoteAddr.lpSockaddr);
				btAddr->addressFamily = AF_BTH;
				btAddr->serviceClassId = RFCOMM_PROTOCOL_UUID;
				btAddr->port = BT_PORT_ANY;

				int btAddrLen = pwsaResults->lpcsaBuffer->RemoteAddr.iSockaddrLength;

				// connect to server
				int status = ::connect(s, (struct sockaddr *)btAddr, btAddrLen);

				if (status == 0) {

					attysComm[nComediDevices] = new AttysComm(s);
					channels_in_use = attysComm[nComediDevices]->NCHANNELS;
					nComediDevices++;
					break;
				}
				else {
					OutputDebugStringW(L"Connect failed\n");
					OutputDebugStringW(L"Has the device been paired?\n");
					shutdown(s, SD_BOTH);
					closesocket(s);
				}
			}
		} else {
			OutputDebugStringW(L"\n");
		}
	}

	WSALookupServiceEnd(hLookup);


#else

#endif

	printf("%d rfcomm devices detected\n",nComediDevices);
	printf("%d channels in use\n",channels_in_use);
	
	// none detected
	if (nComediDevices<1) {
		OutputDebugStringW(L"No rfcomm devices detected!\n");
		attys_scope->splash->showMessage("Cound not connect and/or no devices paired.");
		Sleep(1000);
		exit(1);
	}

	assert(channels_in_use > 0);

	// initialise the graphics stuff
	ypos = new int**[nComediDevices];
	assert(ypos != NULL);
	for(int devNo=0;devNo<nComediDevices;devNo++) {
		ypos[devNo]=new int*[channels_in_use];
		assert(ypos[devNo] != NULL);
		for(int i=0;i<channels_in_use;i++) {
			ypos[devNo][i] = new int[MAX_DISP_X];
			assert( ypos[devNo][i] != NULL);
			for(int j=0;j<MAX_DISP_X;j++) {
				ypos[devNo][i][j]=0;
			}
		}
	}

	xpos=0;
	nsamples=0;

	// 50Hz or 60Hz mains notch filter
	iirnotch = new Iir::Butterworth::BandStop<IIRORDER>**[nComediDevices];
	assert( iirnotch != NULL );
	adAvgBuffer = new float*[nComediDevices];
	assert( adAvgBuffer != NULL );
	daqData = new float*[nComediDevices];
	assert( daqData != NULL );
	for(int devNo=0;devNo<nComediDevices;devNo++) {
		iirnotch[devNo] = new Iir::Butterworth::BandStop<IIRORDER>*[channels_in_use];
		assert( iirnotch[devNo] != NULL );
		// floating point buffer for plotting
		adAvgBuffer[devNo]=new float[channels_in_use];
		assert( adAvgBuffer[devNo] != NULL );
		for(int i=0;i<channels_in_use;i++) {
			adAvgBuffer[devNo][i]=0;
			iirnotch[devNo][i] = new Iir::Butterworth::BandStop<IIRORDER>;
			assert( iirnotch[devNo][i] != NULL );
		}
		// raw data buffer for saving the data
		daqData[devNo] = new float[channels_in_use];
		assert( daqData[devNo] != NULL );
		for(int i=0;i<channels_in_use;i++) {
			daqData[devNo][i]=0;
		}
	}

	setNotchFrequency(f);
}


void ComediScope::startDAQ() {
	// ready steady go!
	counter = new QTimer(this);
	assert(counter != NULL);
	connect(counter,
		SIGNAL(timeout()),
		this,
		SLOT(updateTime()));

	startTimer( 50 );		// run continuous timer
	counter->start( 500 );
	for (int i = 0; i < nComediDevices; i++) {
		if (attysComm[i])
		attysComm[i]->start();
	}
}


ComediScope::~ComediScope() {
	if (rec_file) {
		fclose(rec_file);
	}
	for(int i=0; i<nComediDevices;i++) {
		if (attysComm[i]) {
			attysComm[i]->quit();
		}
	}
}

void ComediScope::setNotchFrequency(float f) {
	if (f>attysComm[0]->getSamplingRateInHz()) {
		fprintf(stderr,
			"Error: The notch frequency %f "
			"is higher than the sampling rate of %dHz.\n",
			f, attysComm[0]->getSamplingRateInHz());
		return;
	}
	for(int j=0;j<nComediDevices;j++) {
		for(int i=0;i<channels_in_use;i++) {
			float frequency_width = f/10;
			iirnotch[j][i]->setup (IIRORDER, 
				attysComm[0]->getSamplingRateInHz(),
					       f, 
					       frequency_width);
		}
		notchFrequency = f;
	}
}



void ComediScope::updateTime() {
	QString s;
	if (!rec_file) {
		if (rec_filename->isEmpty()) {
			s.sprintf("attys_scope");
		} else {
			if (recorded) {
				s=(*rec_filename)+" --- file saved";
			} else {
				s=(*rec_filename)+" --- press REC to record ";
			}
		}
	} else {
		s = (*rec_filename) + 
			QString().sprintf("--- rec: %ldsec", nsamples/ attysComm[0]->getSamplingRateInHz());
	}
	attys_scope->setWindowTitle( s );

	char tmp[256];
	for(int n=0;n<nComediDevices;n++) {
		for(int i=0;i<channels_in_use;i++) {
			float phys = daqData[n][i];
			sprintf(tmp,VOLT_FORMAT_STRING,phys);
			attys_scope->voltageTextEdit[n][i]->setText(tmp);
		}
	}
}


void ComediScope::setFilename(QString name,int csv) {
	(*rec_filename)=name;
	recorded=0;
	if (csv) {
		separator=',';
	} else {
		separator=' ';
	}
}


void ComediScope::writeFile() {
	if (!rec_file) return;
	fprintf(rec_file, "%f", ((float)nsamples) / ((float)attysComm[0]->getSamplingRateInHz()));
	for (int n = 0; n < nComediDevices; n++) {
		for (int i = 0; i < channels_in_use; i++) {
			if (attys_scope->
				channel[n][i]->isActive()
				) {
				float phy = daqData[n][i];
				fprintf(rec_file, "%c%f", separator, phy);
			}
		}
	}
	fprintf(rec_file, "\n");
}

void ComediScope::startRec() {
	if (recorded) return;
	if (rec_filename->isEmpty()) return;
	attys_scope->disableControls();
	// counter for samples
	nsamples=0;
	// get possible comments
	QString comment = attys_scope->commentTextEdit->toPlainText();
	// ASCII
	rec_file=NULL;
	// do we have a valid filename?
	if (rec_filename)
		rec_file=fopen(rec_filename->toLocal8Bit().constData(),
			       "wt");
	// could we open it?
	if (!rec_file) {
		// could not open
		delete rec_filename;
		// print error msg
		fprintf(stderr,
			"Writing failed.\n");
	}
	// print comment
	if ((rec_file)&&(!comment.isEmpty())) {
		fprintf(rec_file,
			"# %s\n",
			comment.toLocal8Bit().constData());
	}
}


void ComediScope::stopRec() {
	if (rec_file) {
		fclose(rec_file);
		rec_file = NULL;
		recorded = 1;
	}
	// re-enabel channel switches
	attys_scope->enableControls();
	// we should have a filename, get rid of it and create an empty one
	if (rec_filename) delete rec_filename;
	rec_filename = new QString();
}



void ComediScope::paintData(float** buffer) {
	QPainter paint( this );
	QPen penData[3]={QPen(Qt::blue,1),
			 QPen(Qt::green,1),
			 QPen(Qt::red,1)};
	QPen penWhite(Qt::white,2);
	int w = width();
	int h = height();
	if (eraseFlag) {
		paint.fillRect(0,0,w,h,QColor(255,255,255));
		eraseFlag = 0;
		xpos = 0;
	}
	num_channels=0;

	// fprintf(stderr,".");
	
	for(int n=0;n<nComediDevices;n++) {
		for(int i=0;i<channels_in_use;i++) {
			if (attys_scope->channel[n][i]->isActive()) {
				num_channels++;	
			}
		}
	}
	if (!num_channels) {
		return;
	}
	int base=h/num_channels;
	if(w <= 0 || h <= 0) 
		return;
	paint.setPen(penWhite);
	int xer=xpos+5;
	if (xer>=w) {
		xer=xer-w;
	}
	paint.drawLine(xer,0,
		       xer,h);
	int act=1;
	for(int n=0;n<nComediDevices;n++) {
		for(int i=0;i<channels_in_use;i++) {
			float minV = -10;
			float maxV = 10;
			if ((i >= attysComm[n]->INDEX_Acceleration_X) && (i >= attysComm[n]->INDEX_Acceleration_Z)) {
				minV = -attysComm[n]->getAccelFullScaleRange();
				maxV = attysComm[n]->getAccelFullScaleRange();
			}
			if ((i >= attysComm[n]->INDEX_Magnetic_field_X) && (i >= attysComm[n]->INDEX_Magnetic_field_Z)) {
				minV = -attysComm[n]->getMagFullScaleRange();
				maxV = attysComm[n]->getMagFullScaleRange();
			}
			if (i == attysComm[n]->INDEX_Analogue_channel_1) {
				minV = -attysComm[n]->getADCFullScaleRange(0);
				maxV = attysComm[n]->getADCFullScaleRange(0);
			}
			if (i == attysComm[n]->INDEX_Analogue_channel_2) {
				minV = -attysComm[n]->getADCFullScaleRange(1);
				maxV = attysComm[n]->getADCFullScaleRange(1);
			}
			float dy = (float)base / (float)(maxV - minV);
			if (attys_scope->
			    channel[n][i]->
			    isActive()) {
				paint.setPen(penData[act%3]);
				float gain=attys_scope->gain[n][i]->getGain();
				float value = buffer[n][i] * gain;
				int yZero=base*act-(int)((0-minV)*dy);
				int yTmp=base*act-(int)((value-minV)*dy);
				ypos[n][i][xpos+1]=yTmp;
				paint.drawLine(xpos,ypos[n][i][xpos],
					       xpos+1,ypos[n][i][xpos+1]);
				if (xpos%2) {
					paint.drawPoint(xpos,yZero);
				}
				if ((xpos+2)==w) {
					ypos[n][i][0]=yTmp;
				}
				act++;
			}
		}
	}
	xpos++;
	if ((xpos+1)>=w) {
		xpos=0;
	}
}



//
// Handles paint events for the ComediScope widget.
// When the paint-event is triggered the averaging is done, the data is
// displayed and saved to disk.
//

void ComediScope::paintEvent(QPaintEvent *) {

	for (;;) {

		for (int n = 0; n < nComediDevices; n++) {
			int hasSample = attysComm[n]->hasSampleAvilabale();
			if (!hasSample) return;
		}

		for (int n = 0; n < nComediDevices; n++) {
			float* values = attysComm[n]->getSampleFromBuffer();
			for (int i = 0; i < channels_in_use; i++) {
				daqData[n][i] = values[i];
				if (attys_scope->channel[n][i]->isActive()) {
					// filtering
					float value = attys_scope->dcSub[n][i]->filter(values[i]);
					value = attys_scope->hp[n][i]->filter(value);
					value = attys_scope->lp[n][i]->filter(value);
					// remove 50Hz
					if (attys_scope->filterCheckbox->checkState() == Qt::Checked) {
						value = iirnotch[n][i]->filter(value);
					}
					// average response if TB is slower than sampling rate
					adAvgBuffer[n][i] = adAvgBuffer[n][i] + value;
				}
			}
		}

		// save data
		if (attys_scope->recPushButton->checkState() == Qt::Checked) {
			writeFile();
		}

		nsamples++;
		tb_counter--;

		// enough averaged?
		if (tb_counter <= 0) {
			for (int n = 0; n < nComediDevices; n++) {
				for (int i = 0; i < channels_in_use; i++) {
					adAvgBuffer[n][i] = adAvgBuffer[n][i] / tb_init;
				}
			}

			// plot the stuff
			paintData(adAvgBuffer);

			// clear buffer
			tb_counter = tb_init;
			for (int n = 0; n < nComediDevices; n++) {
				for (int i = 0; i < channels_in_use; i++) {
					adAvgBuffer[n][i] = 0;
				}
			}
		}
	}
}


void ComediScope::setTB(int us) {
	tb_init=us/(1000000/ attysComm[0]->getSamplingRateInHz());
	tb_counter=tb_init;
	for(int n=0;n<nComediDevices;n++) {
		for(int i=0;i<channels_in_use;i++) {
			adAvgBuffer[n][i]=0;
		}
	}
}

//
// Handles timer events for the ComediScope widget.
//

void ComediScope::timerEvent( QTimerEvent * )
{
	repaint();
}

void ComediScope::clearScreen()
{
	eraseFlag = 1;
	repaint();
}