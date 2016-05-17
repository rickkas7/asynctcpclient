#include "Particle.h"

#include "AsyncTCPClient.h"

// WARNING: This code appears crash an Electron. I think it's because its TCP stack doesn't
// like to be called from the non-user/loop thread, but I didn't really look into it because
// I wanted this code for the Photon, not the Electron.

// This is the stack size for the thread.
const size_t ASYNCTCPCLIENT_STACK_SIZE = 2048;

// This is how many ConnectQueueEntry objects to preallocate
const size_t ASYNCTCPCLIENT_NUM_CONNECTQUEUEENTRY = 1;

static void _threadFn(); // forward declaration
class ConnectQueueEntry; // forward declaration

// We have one of these objects for each outstanding connection attempt, though we only process
// one at a time. Normally, you'll only have one, but more than one is supported.
class ConnectQueueEntry {
public:
	ConnectQueueEntry *next = NULL; // Linked list next
	TCPClient *client = NULL; // AsyncTCPClient object making the asyncConnect request
	IPAddress ipAddr; // IPAddress (4 bytes) if using that method overload
	const char *host = NULL; // Hostname (string) if using that method overload, does DNS lookup
	int port; // port number to connect to
	AsyncTCPClientCallback callback; // Callback to call
	network_interface_t intf; // Normally 0, it's an optional parameter in TCPClient
	int result = -1; // The result from the underlying connect
};
static ConnectQueueEntry *_getQueueEntry(); // forward declaration
static void _enqueue(ConnectQueueEntry *entry); // forward declaration
static void _connect(ConnectQueueEntry *entry); // forward declaration

static Thread *_connectThread = NULL; // Thread that handles the synchronous connect calls
static Mutex _queueMutex; // Mutex used to unblock the connect queue
static ConnectQueueEntry *_connectQueueHead = NULL; // Head of pending connection linked list
static ConnectQueueEntry *_connectQueueTail = NULL; // Tail of pending connection linked list
static ConnectQueueEntry *_completionQueueHead = NULL; // Head of awaiting sending completion callback linked list
static ConnectQueueEntry *_connectQueueFree = NULL; // Head of free linked list


AsyncTCPClient::AsyncTCPClient() {

}

AsyncTCPClient::~AsyncTCPClient() {

}


// Asynchronous version of connect()
void AsyncTCPClient::asyncConnect(IPAddress ipAddr, uint16_t port, AsyncTCPClientCallback callback, network_interface_t intf /* = 0 */) {
	ConnectQueueEntry *entry = _getQueueEntry();
	entry->client = this;
	entry->ipAddr = ipAddr;
	entry->port = port;
	entry->intf = intf;
	entry->callback = callback;
	_enqueue(entry);
}

// Asynchronous version of connect()
void AsyncTCPClient::asyncConnect(const char *host, uint16_t port, AsyncTCPClientCallback callback, network_interface_t intf /* = 0 */) {
	ConnectQueueEntry *entry = _getQueueEntry();
	entry->client = this;
	entry->host = host;
	entry->port = port;
	entry->intf = intf;
	entry->callback = callback;
	_enqueue(entry);
}

// Call this out of setup()
// [static]
void AsyncTCPClient::setup() {
	if (_connectThread == NULL) {
		for(size_t ii = 0; ii < ASYNCTCPCLIENT_NUM_CONNECTQUEUEENTRY; ii++) {
			ConnectQueueEntry *entry = new ConnectQueueEntry();
			entry->next = _connectQueueFree;
			_connectQueueFree = entry;
		}
		_queueMutex.lock();

		_connectThread = new Thread("AsyncTCPClient", _threadFn, OS_THREAD_PRIORITY_DEFAULT, ASYNCTCPCLIENT_STACK_SIZE);
	}
}

// Call this out of loop().
// [static]
void AsyncTCPClient::loop() {
	if (_connectThread == NULL) {
		return;
	}
	ConnectQueueEntry *entry = NULL;

	// If there are any entries pending a call to callback(), which is always done out of loop() to vastly
	// simplify your code, handle them here
	SINGLE_THREADED_BLOCK() {
		if (_completionQueueHead != NULL) {
			entry = _completionQueueHead;
			_completionQueueHead = entry->next;
		}
	}
	if (entry != NULL) {
		// There is an entry that needs to have callback called on
		if (entry->callback != NULL) {
			entry->callback(entry->client, entry->result);
		}

		// Add the entry to the free list so we can use it for the next connect call
		SINGLE_THREADED_BLOCK() {
			entry->client = NULL;
			entry->next = _connectQueueFree;
			_connectQueueFree = entry;
		}
	}
}

// Get a free queue entry from the free list; if there isn't one, allocate a new one on the heap
static ConnectQueueEntry *_getQueueEntry() {
	ConnectQueueEntry *entry;
	SINGLE_THREADED_BLOCK() {
		entry = _connectQueueFree;
		if (entry != NULL) {
			_connectQueueFree = entry->next;
		}
		else {
			entry = new ConnectQueueEntry();
		}
	}
	return entry;
}

// Enqueue a ConnectQueueEntry to try to establish a TCPClient connection in the async connection thread
static void _enqueue(ConnectQueueEntry *entry) {
	SINGLE_THREADED_BLOCK() {
		entry->next = NULL;
		if (_connectQueueTail != NULL) {
			_connectQueueTail->next = entry;
			_connectQueueTail = entry;
		}
		else {
			_connectQueueHead = _connectQueueTail = entry;
			_queueMutex.unlock();
		}
	}
}

// Do the actual connect; called from the async connection thread. We only attempt to connect to one
// server at a time, because the underlying TCPClient.connect() is blocking.
static void _connect(ConnectQueueEntry *entry) {

	if (entry->host == NULL) {
		entry->result = entry->client->connect(entry->ipAddr, entry->port, entry->intf);
	}
	else {
		entry->result = entry->client->connect(entry->host, entry->port, entry->intf);
	}

	SINGLE_THREADED_BLOCK() {
		entry->next = _completionQueueHead;
		_completionQueueHead = entry;
	}
}

// The HAL thread function. This must never return.
static void _threadFn() {

	// Run forever - thread functions must never return.
	for(;;) {
		ConnectQueueEntry *entry = NULL;
		SINGLE_THREADED_BLOCK() {
			entry = _connectQueueHead;
			if (entry != NULL) {
				_connectQueueHead = entry->next;
				if (entry == _connectQueueTail) {
					_connectQueueTail = NULL;
				}
			}
		}
		if (entry != NULL) {
			// Handle queue entry
			_connect(entry);
		}
		if (_connectQueueHead == NULL) {
			// Block until a new request arrives in the queue
			_queueMutex.lock();
		}
	}
}
