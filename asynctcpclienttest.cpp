#include "Particle.h"

#include "AsyncTCPClient.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(AUTOMATIC);

const unsigned long timeWarnMs = 25;

enum State { CONNECT_STATE, CONNECT_WAIT, TEST_WAIT };

void handleConnect(); // forward declaration
void connectCallback(TCPClient *client, int status); // forward declaration

unsigned long lastLoopExit = 0;
unsigned long stateStart = 0;
unsigned long stateTime = 10000;
unsigned long connectEnter = 0;
int testNum = 0;
enum State state = TEST_WAIT;
AsyncTCPClient client;


void setup() {
	Serial.begin(9600);
	AsyncTCPClient::setup();
}

void loop() {
	unsigned long loopEnter = millis();

	if (lastLoopExit != 0 && loopEnter - lastLoopExit >= timeWarnMs) {
		Serial.printlnf("* time outside loop %lu ms", loopEnter - lastLoopExit);
	}


	switch(state) {
	case CONNECT_STATE:
		connectEnter = millis();
		Serial.println("attempting connection");
		state = CONNECT_WAIT;
		client.asyncConnect("notavalidhost.example.com", 80, connectCallback);
		break;

	case CONNECT_WAIT:
		break;

	case TEST_WAIT:
		if (millis() - stateStart >= stateTime) {
			state = CONNECT_STATE;
		}
		break;

	}

	AsyncTCPClient::loop();

	if (millis() - loopEnter >= timeWarnMs) {
		Serial.printlnf("* time inside loop %lu ms", millis() - loopEnter);
	}

	lastLoopExit = millis();
}

void connectCallback(TCPClient *client, int status) {
	unsigned long elapsed = millis() - connectEnter;
	if (status == 1) {
		Serial.printlnf("connect succeeded %lu", elapsed);
		client->stop();
	}
	else {
		Serial.printlnf("connect failed %lu", elapsed);
	}

	// Wait before repeating the test
	state = TEST_WAIT;
	stateStart = millis();
	stateTime = 30000;

}
