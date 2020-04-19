#include <iostream>
#include <string>
#include <fstream>
#include <Windows.h>

using namespace std;

const char SOH = (char)1;
const char EOT = (char)4;
const char ACK = (char)6;
const char NAK = (char)15;
const char CAN = (char)18;
const char C = (char)43;

HANDLE serialHandle;

char block[128];
char sign;
int blockNumber = 1;
char inverseBlockNumber;
char sum[2];
int mode;
unsigned long sizeSign = sizeof(sign);

bool openPort(string port);
char* calcCrc(char* ptr, int count);
char calcChecksum(char tab[]);
void receive(string nameFile);
void receiveBlock(ofstream* file);
void send(string nameFile);
void fill(char tab[]);

int main() {
	// Proponuje, zeby w programie mozna bylo wybrac czy chcesz nadawac czy odbierac
	int choice = 0;
	string port;
	string nameFile;
	
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

	cout << "Podaj nazwe pliku: " << endl;
	cin >> nameFile;

	cout << "Wybierz czy chcesz odbierac czy nadawac (1 - Odbierac, 2 - Nadawac): " << endl;
	cin >> choice;

	switch (choice) {
	case 1:
		// Jakas funkcja odbierajca

		receive(nameFile);
		break;
	case 2:
		// Jakas funkcja nadajaca
		send(nameFile);

		break;
	default:
		cout << "Nie wybrales nic!" << endl;
		return 0;
	}

	CloseHandle(serialHandle);
}

bool openPort(string port) {
	// c_str() zwraca Long Pointer to Constant STRing
	serialHandle = CreateFile(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

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
		timeout.ReadTotalTimeoutConstant = 10000;
		timeout.ReadTotalTimeoutMultiplier = 50;
		timeout.WriteTotalTimeoutConstant = 50;
		timeout.WriteTotalTimeoutMultiplier = 50;

		SetCommTimeouts(serialHandle, &timeout);

		return true;
	}

	return false;
}

char* calcCrc(char* ptr, int count) {
	int crc = 0;
	char i;

	while (--count >= 0) {
		crc = crc ^ (int)*ptr++ << 8;
		i = 8;
		do {
			if (crc & 0x8000) {
				crc = crc << 1 ^ 0x1021;
			} 
			else {
				crc = crc << 1;
			}
		} while (--i);
	}

	char tab[2];
	tab[1] = (char) (crc & 0x00FF);
	tab[0] = (char)((crc & 0xFF00) >> 8);
	return tab;
}

char calcChecksum(char tab[]) {
	char checksum = (char)26;

	for (int i = 0; i < 128; i++) {
		checksum += tab[i];
		checksum = checksum % 256;
	}

	return checksum;
}

void receive(string nameFile) {
	cout << "Wybierz tryb odbiornika:\n 1 - C (liczenie 16-bitowej sumy CRC)\n 2 - NAK (liczenie standardowej 8-bitowej sumy kontrolnej)" << endl;

	cin >> mode;

	switch (mode) {
	case 1:
		sign = C;
		break;
	case 2:
		sign = NAK;
		break;
	default:
		cout << "Nie wybrano poprawnej opcji, wybor domyslny to 2" << endl; 
		mode = 2;
		sign = NAK;
		break;
	}

	for (int i = 0; i < 6; i++) {
		WriteFile(serialHandle, &sign, 1, &sizeSign, NULL);
		cout << "Czekam" << endl;
		ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);

		if (sign == SOH) {
			cout << "Ustawiono polaczenie" << endl;
			break;
		}       
		if (5 == i) {
			cout << "Polaczenie nie zostalo nawiazane" << endl;
			exit(1);
		}
	}

	ofstream file;
	file.open(nameFile, ios::binary);

	receiveBlock(&file);

	while (true) {
		// Jeœli SOH to kontynuuje, jeœli EOT lub CAN koñczy po³¹czenie
		ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);
		if (sign == EOT) {
			WriteFile(serialHandle, &ACK, 1, &sizeSign, NULL);
			break;
		}
		if (sign == CAN) {
			break;
		}

		receiveBlock(&file);
	}
	file.close();
	system("pause");
}

void send(string nameFile) {
	bool transmition = false;

	for (int i = 0; i < 6; i++) {
		ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);

		if (C == sign) {
			cout << "C" << endl;
			mode = 1;
			break;
		}
		else if (NAK == sign) {
			cout << "NAK" << endl;
			mode = 2;
			break;
		}

		if (5 == i) {
			exit(1);
		}
	}

	ifstream file;
	file.open(nameFile, ios::binary);

	while (!file.eof()) {
		fill(block);

		for (int i = 0; i < 128 && !file.eof(); i++) {
			block[i] = file.get();
			if (file.eof()) {
				block[i] = (char)26;
			}
		}

		bool isCorrcet = false;

		while (!isCorrcet) {
			// Start of Header
			WriteFile(serialHandle, &SOH, 1, &sizeSign, NULL);

			// Packet Number
			sign = (char)blockNumber;
			WriteFile(serialHandle, &sign, 1, &sizeSign, NULL);
			//cout << "Wyslano numer bloku: " << blockNumber << endl;

			// (Packet Number) innverse block number
			inverseBlockNumber = (char)255 - blockNumber;
			sign = inverseBlockNumber;
			WriteFile(serialHandle, &sign, 1, &sizeSign, NULL);
			//cout << "Wyslano dopelnienie numeru: " << sign << endl;

			// Packet Data
			for (int i = 0; i < 128; i++) {
				WriteFile(serialHandle, &block[i], 1, &sizeSign, NULL);
			}

			// Crc
			if (1 == mode) {
//				cout << "crc: " << calcCrc(block, 128)[0];
				sum[0] = calcCrc(block, 128)[0];
				WriteFile(serialHandle, &sum[0], 1, &sizeSign, NULL);
				sum[1] = calcCrc(block, 128)[1];
				WriteFile(serialHandle, &sum[1], 1, &sizeSign, NULL);
			}
			else /* 2 == mode */ {
				char checksum = calcChecksum(block);
				WriteFile(serialHandle, &checksum, 1, &sizeSign, NULL);
			}

			while (true) {
				sign = (char)0;
				ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);

				if (ACK == sign) {
					isCorrcet = true;
					cout << "Przeslano (ACK)" << endl;
					break;
				}
				else if (NAK == sign) {
					cout << "Wysylanie pakietu ponownie (NAK)" << endl;
					break;
				}
				else if (CAN == sign) {
					cout << "Anulowano przesylanie" << endl;
					exit(1);
				}
			}
		}

		if (255 == blockNumber) {
			blockNumber = 1;
		}
		else {
			blockNumber++;
		}
	}

	do {
		sign = EOT;
		WriteFile(serialHandle, &sign, 1, &sizeSign, NULL);
		ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);
	} while (ACK != sign);
}

void fill(char tab[]) {
	for (int i = 0; i < 128; i++) {
		tab[i] = (char)26;
	}
}

void receiveBlock(ofstream* file) {
	// Odbieranie nr bloku
	ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);
	blockNumber = (int)sign;

	// Odbieranie dope³nienia do nr bloku
	ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);
	inverseBlockNumber = sign;
	//		cout << "Otrzymano dopelnienie numeru: " << (int)sign << endl;

			// Odbieranie danych
	for (int i = 0; i < 128; i++) {
		ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);
		block[i] = sign;
	}

	// Odbieranie sumy kontrolnej
	if (1 == mode) {
		ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);
		sum[0] = sign;
		ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);
		sum[1] = sign;
	}
	else /* mode == 2*/ {
		ReadFile(serialHandle, &sign, 1, &sizeSign, NULL);
		sum[0] = sign;
		sum[1] = 0;
	}

	bool isCorrect = true;

	// Sprawdzanie czy numer bloku siê zgadza
	if ((char)(255 - blockNumber) != inverseBlockNumber) {
		cout << "Niepoprawny numer bloku" << endl;
		cout << "Podany numer: " << blockNumber << ". Podane dopelnienie: " << inverseBlockNumber << endl;
		isCorrect = false;
	}

	// Sprawdzanie czy suma kontrolna siê zgadza
	if (1 == mode) {
		char sumTmp1 = calcCrc(block, 128)[0];
		char sumTmp2 = calcCrc(block, 128)[1];
		if (sum[0] != sumTmp1 || sum[1] != sumTmp2) {
			cout << "Niepoprawna suma kontrolna CRC" << endl;
			isCorrect = false;
		}
		// to do, liczenie sumy CRC
	}
	else {
		if (sum[0] != calcChecksum(block)) {
			cout << "Niepoprawna suma kontrolna" << endl;
			isCorrect = false;
		}
	}

	// Wysy³anie ACK lub NAK w zale¿noœci od poprawnoœci
	if (isCorrect) {
		WriteFile(serialHandle, &ACK, 1, &sizeSign, NULL);
		cout << "Pakiet nr " << blockNumber << " odebrany poprawnie" << endl;
		for (int i = 0; i < 128; i++) {
			if (block[i] != 26) {
				*file << block[i];
			}
		}
	}
	else {
		WriteFile(serialHandle, &NAK, 1, &sizeSign, NULL);
	}
}