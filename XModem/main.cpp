#include <iostream>
#include <string>
#include <Windows.h>

using namespace std;

bool openPort(string port);

int main() {
	// Proponuje, zeby w programie mozna bylo wybrac czy chcesz nadawac czy odbierac
	int choice = 0;
	string port;

	cout << "Wybierz port do transmisji danych (1 - COM1, 2 - COM2, 3 - COM3): " << endl;
	cin >> choice;

	switch (choice) {
	case 1:
		port = "COM1";
		break;
	case 2:
		port = "COM2";
		break;
	case 3:
		port = "COM3";
		break;
	default:
		port = "COM1";
		break;
	}

	if (openPort(port)) {
		cout << "Port is open" << endl;
	}
	else {
		cout << "Port is closed" << endl;
		return 0;
	}

	cout << "Wybierz czy chcesz odbierac czy nadawac (1 - Odbierac, 2 - Nadawac): " << endl;
	cin >> choice;

	switch (choice) {
	case 1:
		// Jakas funkcja nadajaca
		break;
	case 2:
		// Jakas funkcja odbierajaca 
		break;
	default:
		cout << "Nie wybrales nic!" << endl;
		return 0;
	}
}


// https://stackoverflow.com/questions/15794422/serial-port-rs-232-connection-in-c
bool openPort(string port) {
	// c_str() zwraca Long Pointer to Constant STRing
	HANDLE serialHandle = CreateFile(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (INVALID_HANDLE_VALUE != serialHandle) {
		DCB serialParams = { 0 };
		serialParams.DCBlength = sizeof(serialParams);

		GetCommState(serialHandle, &serialParams);
		serialParams.BaudRate = CBR_9600;
		serialParams.ByteSize = 8;
		serialParams.StopBits = ONESTOPBIT;
		serialParams.Parity = NOPARITY;
		SetCommState(serialHandle, &serialParams);

		COMMTIMEOUTS timeout = { 0 };
		timeout.ReadIntervalTimeout = 50;
		timeout.ReadTotalTimeoutConstant = 50;
		timeout.ReadTotalTimeoutMultiplier = 50;
		timeout.WriteTotalTimeoutConstant = 50;
		timeout.WriteTotalTimeoutMultiplier = 10;

		SetCommTimeouts(serialHandle, &timeout);

		return true;
	}

	return false;
}