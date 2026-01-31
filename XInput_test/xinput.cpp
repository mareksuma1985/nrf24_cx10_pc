#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#include <Xinput.h>
#else
#include <unistd.h>
#endif

#include "rs232.h"

using namespace std;

int i, n,
cport_nr = 3,
bdrate = 115200;
unsigned char buf[201];
char mode[] = { '8','N','1',0 }, str[200];

#define PPM_MID 1500
#define PPM_MIN_COMMAND 1200
#define PPM_MAX_COMMAND 1700

int throttle, aileron, elevator, rudder;
int aux1 = PPM_MID;
int aux2 = PPM_MID;
int aux3 = PPM_MAX_COMMAND;
int aux4 = PPM_MID;

/* https://gitlab.com/Teuniz/RS-232/-/blob/master/demo_rx.c */

void threadTwo()
{
	while (1)
	{
		n = RS232_PollComport(cport_nr, buf, 200);

		if (n > 0)
		{
			buf[n] = 0; /* always put a "null" at the end of a string! */

			/* replace unreadable control-codes by dots */
			for (i = 0; i < n; i++)
			{
				if (buf[i] < 32)
				{
					buf[i] = '.';
				}
			}

			printf("received %i bytes: %s\n", n, (char*)buf);
		}
		Sleep(100);
	}
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		printf("Enter comport number as argument.\n");
		return -1;
	}

	/*
	int i = 0;
	while (i < argc) {
		cout << "Argument " << i + 1
			<< ": " << argv[i]
			<< endl;
		i++;
	}
	*/

	cport_nr = atoi(argv[1]);
	printf("Windows comport: COM%d\n", cport_nr);
	cport_nr -= 1;
	printf("Baud rate: %d\n", bdrate);

	/* COM1 - 0, COM2 - 1, COM3 - 2, COM4 - 3... */
	if (RS232_OpenComport(cport_nr, bdrate, mode, 0))
	{
		printf("Can not open comport\n");
		return(0);
	}

	std::thread threadOne(threadTwo);

	std::this_thread::sleep_for(1000ms);

	strcpy(str, "1500,0,0,0\n");
	while (1)
	{
		for (DWORD playerIndex = 0; playerIndex < 4; ++playerIndex)
		{
			XINPUT_STATE state;
			DWORD result = XInputGetState(playerIndex, &state);
			if (result == ERROR_SUCCESS && state.dwPacketNumber)
			{
				/* https://gitlab.com/Teuniz/RS-232/-/blob/master/demo_tx.c */

				// throttle = state.Gamepad.bRightTrigger * 3.922 + 1000;
				throttle = abs(state.Gamepad.sThumbLY) / 32.768 + 1000;
				aileron = state.Gamepad.sThumbLX / 65.536 + 1500;
				elevator = state.Gamepad.sThumbRY / 65.536 + 1500;
				rudder = state.Gamepad.sThumbRX / 65.536 + 1500;

				aux1 = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) ? PPM_MAX_COMMAND + 1 : PPM_MID; // mode 3 / headless
				aux2 = (state.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? PPM_MAX_COMMAND + 1 : PPM_MID; // flip control
				aux3 = (state.Gamepad.wButtons & XINPUT_GAMEPAD_X) ? PPM_MIN_COMMAND : PPM_MAX_COMMAND; // still camera (snapshot)
				aux4 = (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) ? PPM_MAX_COMMAND + 1 : PPM_MID; // video camera

				string message = to_string(throttle) + "," + to_string(aileron) + "," + to_string(elevator) + "," + to_string(rudder);
				if (state.Gamepad.wButtons)
				{
					message += "," + to_string(aux1) + "," + to_string(aux2) + "," + to_string(aux3) + "," + to_string(aux4);
				}
				message += '\n';
				strcpy(str, message.c_str());
				RS232_cputs(cport_nr, str);
				printf("[PC]: %s\n", str);

				/*
				printf("Player %d ", playerIndex);
				printf("LX:%6d ", state.Gamepad.sThumbLX);
				printf("LY:%6d ", state.Gamepad.sThumbLY);
				printf("RX:%6d ", state.Gamepad.sThumbRX);
				printf("RY:%6d ", state.Gamepad.sThumbRY);
				printf("LT:%3u ", state.Gamepad.bLeftTrigger);
				printf("RT:%3u ", state.Gamepad.bRightTrigger);
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_A)              printf("A ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_B)              printf("B ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_X)              printf("X ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)              printf("Y ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)        printf("up ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)      printf("down ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)      printf("left ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)     printf("right ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_START)          printf("start ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)           printf("back ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)     printf("LS ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)    printf("RS ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  printf("LB ");
				if (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) printf("RB ");
				printf("\n");
				*/
			}
		}
		Sleep(200);
	}
	threadOne.join();
	return 0;
}
