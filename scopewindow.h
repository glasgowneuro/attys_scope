#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#endif

class ScopeWindow;
#ifndef COMEDISCOPE_H
#define COMEDISCOPE_H

#define MAXBUFFER 65536

#include <QWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLayout> 
#include <QPaintEvent>
#include <QTimerEvent>
#include <QUdpSocket>

#ifdef __linux__ 
#include<sys/ioctl.h>
#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>
#include<termios.h>
#include <sys/socket.h>
#elif _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <ws2bth.h>
#include <BluetoothAPIs.h>
#else

#endif

#include <fcntl.h>

#include <stdio.h>

#include <string>

#include "attys-scope.h"

#include "AttysComm.h"
#include "AttysScan.h"

#define MAX_DISP_X 4096 // max screen width

#define VOLT_FORMAT_STRING "%+.3f"

#define DUMMY_SAMPLE_CONSTANT 0

class ScopeWindow : public QWidget
{
	Q_OBJECT
public:
	/**
	 * Constructor:
	 **/
	ScopeWindow(Attys_scope* attys_scope_tmp);
	/**
	 * Destructor: close the file if necessary
	 **/
	~ScopeWindow();


private:
	void btprintf(int s, const char *message, int checkOK);

protected:
	/**
	 * Overloads the empty paint-function: draws the functions and saves the data
	 **/
	void	paintEvent(QPaintEvent *);


	void resizeEvent(QResizeEvent *event) {
		QWidget::resizeEvent(event);
		eraseFlag = 1;
	}

public:
	/**
	 * Clears the screen
	 **/
	void        clearScreen();

private:
	/**
	 * Paints the data on the screen, is callsed by paintEvent
	 **/
	void        paintData(float** buffer);

	/**
	 * Is called by the timer of the window. This causes the drawing and saving
	 * of all not yet displayed/saved data.
	 **/
	void	timerEvent(QTimerEvent *);

private slots:

	/**
	 * Updates the time-code and also the voltages of the channels
	 **/
	void	updateTime();

private:
	/**
	 * Saves data which has arrived from the AD-converter
	 **/
	void        writeFile();

	/**
	 * y-positions of the data
	 **/
	int         ***ypos;

	/**
	 * current x-pos
	 **/
	int         xpos = 0;

	/**
	 * elapsed msec
	 **/
	long int         nsamples;

	/**
	* flag if data is displayed
	**/
	int display_data;

public:
	/**
	 * sets the filename for the data-file
	 **/
	void        setFilename(QString name, int csv);

public:
	/**
	 * sets the time between the samples
	 **/
	void        setTB(int us);

private:
	/**
	 * pointer to the parent widget which contains all the controls
	 **/
	Attys_scope* attys_scope = nullptr;


private:
	/**
	 * the filename of the data-file as entered by the user
	 **/
	QString     rec_filename;

    /**
	 * The filename of the date-file as actually used to save the data
	 **/
	QString      finalFilename;

	/**
	 * the file descriptor to the data-file
	 **/
	FILE*       rec_file = nullptr;

	/**
	 * number of channels switched on
	 **/
	int         num_channels = 0;

	/**
	* time when recording starts
	**/
	unsigned long start_time = 0;

	/**
	* reconnecting flag
	**/
	int reconnectFlag = 0;

private:
	/**
	 * buffer which adds up the data for averaging
	 **/
	float**   adAvgBuffer;

private:
	/**
	 * init value for the averaging counter
	 **/
	int         tb_init;

	int eraseFlag;

private:
	/**
	 * counter for the tb. If zero the average is
	 * taken from adAvgBuffer and saved into actualAD.
	 **/
	int         tb_counter;

public:
	/**
	 * Starts recording
	 **/
	void startRec();

	/**
	 * Ends recording
	 **/
	void stopRec();

	/**
	* Opens file
	**/
	void openFile();

private:
	/**
	 * Frequency for the notch filter in Hz
	 **/
	float notchFrequency = 50;

private:
	/**
	 * flag if data has been recorded. Prevents overwrite.
	 **/
	int recorded = 0;

	/**
	 * comma separated?
	 **/
	char separator = '\t';

	/**
	 * Timer for printing the voltages in the textfields
	 **/
	QTimer* counter = NULL;

	/**
	 * Raw daq data from the A/D converter which is saved to a file
	 **/
	float** unfiltDAQData = NULL;

	/**
	* Filtered daq data from the A/D converter which is saved to a file
	**/
	float** filtDAQData = NULL;

public:
	/**
	 * Start the DAQ board(s)
	 **/
	void startDAQ();

public:
	int getActualSamplingRate() { return attysScan.attysComm[0]->getSamplingRateInHz(); }


private:
	float **minV;
	float **maxV;

private:
	void writeUDP();
	QPointer<QUdpSocket> udpSocket = NULL;
	int udpPort = 65000;
	int udpStatus = 0;

public:
	void startUDP(int port);
	void stopUDP();

/////////////////////////////////////////////////////////////////////////////////////////////
// mechanisms for re-connect

	void attysHasReconnected();
	void clearAllRingbuffers();

private:
	class AttysScopeCommMessage : public AttysCommMessage {
		// pointer to the Scopewindow
	public:
		ScopeWindow* scopeWindow = NULL;
		virtual void hasMessage(int msg, const char*) {
			if ( (msg == AttysComm::MESSAGE_RECONNECTED) && scopeWindow ) {
				scopeWindow->attysHasReconnected();
			}
			if ((msg == AttysComm::MESSAGE_RECEIVING_DATA) && scopeWindow) {
				scopeWindow->clearAllRingbuffers();
			}
		}
	};

	AttysScopeCommMessage attysScopeCommMessage;

	float dummySample[AttysComm::NCHANNELS];

};



#endif
