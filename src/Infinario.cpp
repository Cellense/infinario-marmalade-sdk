#include "Infinario.h"

#include "IwHTTP.h"

#include "IwHashString.h"
#include "IwRandom.h"

#include "s3eDevice.h"
#include "s3eMemory.h"
#include "s3eThread.h"
#include "s3eTimer.h"

#include <iomanip>
#include <queue>
#include <string>
#include <sstream>

Infinario::Request::Request(const std::string &uri, const std::string &body, ResponseCallback callback, void *userData)
: _uri(uri)
, _body(body)
, _callback(callback)
, _userData(userData)
{}

const uint32 Infinario::RequestManager::_bufferSize = 1024;

Infinario::RequestManager::RequestManager()
: _httpClient(new CIwHTTP())
, _externalLock(s3eThreadLockCreate())
, _internalLock(s3eThreadLockCreate())
, _emptyRequestQueueCallback(NULL)
, _emptyRequestQueueUserData(NULL)
, _isRequestBeingProcessed(false)
, _requestsQueue()
, _buffer(reinterpret_cast<char *>(s3eMalloc(RequestManager::_bufferSize + 1)))
, _accumulatedBodyLength(0)
, _accumulatedBodyContent()
{}

Infinario::RequestManager::~RequestManager()
{
	// By destroying this instance all queued callbacks have been canceled.
	delete this->_httpClient;

	bool wasQueueEmptyAtStart = this->_requestsQueue.empty();

	// Call the first queued request callback.
	if (!wasQueueEmptyAtStart) {
		const Request &currentRequest(this->_requestsQueue.front());

		if (currentRequest._callback != NULL) {
			currentRequest._callback(NULL, currentRequest._body, ResponseStatus::KilledError,
				this->_accumulatedBodyContent.str(), currentRequest._userData);
		}

		this->_requestsQueue.pop();
	}
	
	// Call the remaining queued request callbacks.
	while (!this->_requestsQueue.empty()) {
		const Request &currentRequest(this->_requestsQueue.front());

		if (currentRequest._callback != NULL) {
			currentRequest._callback(NULL, currentRequest._body, ResponseStatus::KilledError,
				std::string(), currentRequest._userData);
		}

		this->_requestsQueue.pop();
	}

	s3eFree(reinterpret_cast<void *>(this->_buffer));

	s3eThreadLockDestroy(this->_externalLock);
	s3eThreadLockDestroy(this->_internalLock);

	// Call the empty request queue function if it was supplied.
	if (!wasQueueEmptyAtStart && (this->_emptyRequestQueueCallback != NULL)) {
		this->_emptyRequestQueueCallback(this->_emptyRequestQueueUserData);
	}
}

void Infinario::RequestManager::SetProxy(const std::string &proxy)
{
	this->_httpClient->SetProxy(proxy.c_str());
}

void Infinario::RequestManager::ClearProxy()
{
	this->_httpClient->SetProxy(NULL);
}

void Infinario::RequestManager::SetEmptyRequestQueueCallback(EmptyRequestQueueCallback callback, void *userData)
{
	this->_emptyRequestQueueCallback = callback;
	this->_emptyRequestQueueUserData = userData;
}

void Infinario::RequestManager::ClearEmptyRequestQueueCallback()
{
	this->_emptyRequestQueueCallback = NULL;
	this->_emptyRequestQueueUserData = NULL;
}

void Infinario::RequestManager::Enqueue(const Request &request)
{
	s3eThreadLockAcquire(this->_externalLock);

	s3eThreadLockAcquire(this->_internalLock);
	this->_requestsQueue.push(request);
	s3eThreadLockRelease(this->_internalLock);

	// If the manager is in request processing mode the call chain will execute all queued requests.
	if (!this->_isRequestBeingProcessed) {
		// Initialize the execute call chain.
		this->Execute();
	}

	s3eThreadLockRelease(this->_externalLock);
}

// This is the callback indicating that a Post call has completed. Depending on how the server is communicating the
// content length, we may actually know the length of the content, or we may know the length of the first part of it,
// or we may know nothing. ContentExpected always returns the smallest possible size of the content, so allocate that
// much space for now if it's non-zero. If it is of zero size, the server has given no indication, we use the maximum
// size.
int32 Infinario::RequestManager::RecieveHeader(void *systenData, void *userData)
{
	// Initializing passed reference.
	RequestManager &requestManager = *(reinterpret_cast<RequestManager *>(userData));

	s3eThreadLockAcquire(requestManager._internalLock);

	const Request &currentRequest(requestManager._requestsQueue.front());

	// Test for error.
	if (requestManager._httpClient->GetStatus() == S3E_RESULT_ERROR) {
		// Call callback function if it was supplied.
		if (currentRequest._callback != NULL) {
			currentRequest._callback(requestManager._httpClient, currentRequest._body, ResponseStatus::ReceiveHeaderError,
				requestManager._accumulatedBodyContent.str(), currentRequest._userData);
		}

		// Remove current request from queue.
		requestManager._requestsQueue.pop();
		s3eThreadLockRelease(requestManager._internalLock);

		// Continue in the request execution chain.
		requestManager.Execute();
		return 0;
	}
	
	// Set estimated buffer length.
	requestManager._accumulatedBodyLength = requestManager._httpClient->ContentExpected();
	if (requestManager._accumulatedBodyLength == 0) {
		requestManager._accumulatedBodyLength = RequestManager::_bufferSize;
	}

	// Set buffer suffix and start reading recieved data to it.
	requestManager._buffer[requestManager._accumulatedBodyLength] = 0;
	requestManager._httpClient->ReadDataAsync(requestManager._buffer, requestManager._accumulatedBodyLength,
		0, RequestManager::RecieveBody, userData);

	s3eThreadLockRelease(requestManager._internalLock);
	return 0;
}

// This is the callback indicating that a ReadDataAsync call has completed. Either we've finished, or more data
// has been recieved. If the correct ammount of data was supplied initially, then this will only be called once.
// However, it may well be called several times when using chunked encoding.
int32 Infinario::RequestManager::RecieveBody(void *systenData, void *userData)
{
	// Initializing passed reference.
	RequestManager &requestManager = *(reinterpret_cast<RequestManager *>(userData));

	s3eThreadLockAcquire(requestManager._internalLock);
	
	const Request &currentRequest(requestManager._requestsQueue.front());
	
	// Test for error.
	if (requestManager._httpClient->GetStatus() == S3E_RESULT_ERROR) {
		// Call callback function if it was supplied.
		if (currentRequest._callback != NULL) {
			currentRequest._callback(requestManager._httpClient, currentRequest._body, ResponseStatus::RecieveBodyError,
				requestManager._accumulatedBodyContent.str(), currentRequest._userData);
		}

		// Remove current request from queue.
		requestManager._requestsQueue.pop();
		s3eThreadLockRelease(requestManager._internalLock);

		// Continue in the request execution chain.
		requestManager.Execute();
		return 0;
	}

	// Store recieved data buffer content.
	requestManager._accumulatedBodyContent << std::string(requestManager._buffer);

	// Test if more data was recieved.
	if (requestManager._httpClient->ContentFinished()) {
		// Call callback function if it was supplied.
		if (currentRequest._callback != NULL) {
			currentRequest._callback(requestManager._httpClient, currentRequest._body, ResponseStatus::Success,
				requestManager._accumulatedBodyContent.str(), currentRequest._userData);
		}

		// Remove current request from queue.
		requestManager._requestsQueue.pop();
		s3eThreadLockRelease(requestManager._internalLock);

		// Continue in the request execution chain.
		requestManager.Execute();
		return 0;
	}

	// Determine current recieved data size.
	uint32 bufferLength = requestManager._accumulatedBodyLength;
	if (requestManager._accumulatedBodyLength < requestManager._httpClient->ContentExpected()) {
		requestManager._accumulatedBodyLength = requestManager._httpClient->ContentExpected();
	} else {
		requestManager._accumulatedBodyLength += RequestManager::_bufferSize;
	}
	bufferLength = requestManager._accumulatedBodyLength - bufferLength;

	// Set buffer suffix and start reading newly recieved data to it.	
	requestManager._buffer[bufferLength] = 0;
	requestManager._httpClient->ReadDataAsync(requestManager._buffer,
		bufferLength, 0, RequestManager::RecieveBody, userData);

	s3eThreadLockRelease(requestManager._internalLock);
	return 0;
}

void Infinario::RequestManager::Execute()
{
	s3eThreadLockAcquire(this->_internalLock);

	// Check if a request is available for execution, if it is set the manager to request processing mode.
	this->_isRequestBeingProcessed = !this->_requestsQueue.empty();
	if (!this->_isRequestBeingProcessed) {
		s3eThreadLockRelease(this->_internalLock);

		// Call callback function if it was supplied.
		if (this->_emptyRequestQueueCallback != NULL) {
			this->_emptyRequestQueueCallback(this->_emptyRequestQueueUserData);
		}

		return;
	}

	// Reset recieved data accumulation stream.
	this->_accumulatedBodyContent.str(std::string());
	this->_accumulatedBodyContent.clear();	

	const Request &currentRequest(this->_requestsQueue.front());

	// Set request headers.
	this->_httpClient->SetRequestHeader("Content-Type", "application/json");

	// Send request.
	if (this->_httpClient->Post(currentRequest._uri.c_str(), currentRequest._body.c_str(),
		static_cast<int32>(currentRequest._body.size()), RequestManager::RecieveHeader,
		reinterpret_cast<void *>(this)) == S3E_RESULT_ERROR)
	{
		// Call callback function if it was supplied.
		if (currentRequest._callback != NULL) {
			currentRequest._callback(this->_httpClient, currentRequest._body,
				ResponseStatus::SendRequestError, this->_accumulatedBodyContent.str(), currentRequest._userData);
		}

		// Remove current request from queue.
		this->_requestsQueue.pop();
		s3eThreadLockRelease(this->_internalLock);

		// Continue in the request execution chain.
		this->Execute();
		return;
	}

	s3eThreadLockRelease(this->_internalLock);
}

const std::string Infinario::Infinario::_requestUri("http://api.infinario.com/bulk");

Infinario::Infinario::Infinario(const std::string &projectToken, const std::string &customerId)
: _requestManager()
, _projectToken(projectToken)
, _customerCookie()
, _customerId(customerId)
{
	IwRandSeed((int32)s3eTimerGetMs());

	std::stringstream sstream;
	sstream << s3eDeviceGetInt(S3E_DEVICE_PPI_LOGICAL) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_PPI) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_SUPPORTS_SUSPEND_RESUME) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_NUM_CPU_CORES) << " "
		<< s3eDeviceGetString(S3E_DEVICE_LOCALE) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_LANGUAGE) << " "
		<< s3eDeviceGetString(S3E_DEVICE_LANGUAGE) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_MEM_FREE) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_MEM_TOTAL) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_BATTERY_LEVEL) << " "
		<< s3eDeviceGetInt(S3E_DEVICE_MAINS_POWER) << " "
		<< s3eDeviceGetString(S3E_DEVICE_CHIPSET) << " "
		<< s3eDeviceGetString(S3E_DEVICE_IMSI) << " "
		<< s3eDeviceGetString(S3E_DEVICE_PHONE_NUMBER) << " "
		<< s3eDeviceGetString(S3E_DEVICE_NAME) << " "
		<< s3eDeviceGetString(S3E_DEVICE_UNIQUE_ID) << " "
		<< s3eDeviceGetString(S3E_DEVICE_ID) << " "
		<< s3eDeviceGetString(S3E_DEVICE_ARCHITECTURE) << " "
		<< s3eDeviceGetString(S3E_DEVICE_OS_VERSION) << " "
		<< s3eDeviceGetString(S3E_DEVICE_OS);

	std::stringstream hashstream;
	hashstream << (IwHashString(sstream.str().c_str()) * IwRandMinMax(1, 1000));
	this->_customerCookie = hashstream.str();
}

void Infinario::Infinario::SetProxy(const std::string &proxy)
{
	this->_requestManager.SetProxy(proxy);
}

void Infinario::Infinario::ClearProxy()
{
	this->_requestManager.ClearProxy();
}

void Infinario::Infinario::SetEmptyRequestQueueCallback(EmptyRequestQueueCallback callback, void *userData)
{
	this->_requestManager.SetEmptyRequestQueueCallback(callback, userData);
}

void Infinario::Infinario::ClearEmptyRequestQueueCallback()
{
	this->_requestManager.ClearEmptyRequestQueueCallback();
}

void Infinario::Infinario::Identify(const std::string &customerId, ResponseCallback callback, void *userData)
{
	this->_customerId = customerId;

	std::stringstream bodyStream;
	bodyStream << 
		"{ \"commands\": [{ "
			"\"name\": \"crm/customers\", "
			"\"data\": { "
				"\"ids\": {"
					" \"registered\": \"" << this->_customerId << "\","
					" \"cookie\": \"" << this->_customerCookie << "\" "
				"}, "
				"\"project_id\": \"" << this->_projectToken << "\" "
			"}"
		"}]}";

	IndentifyUserData *identifyUserData = new IndentifyUserData(*this, callback, userData);
	this->_requestManager.Enqueue(Request(Infinario::_requestUri, bodyStream.str(),
		Infinario::IdentifyCallback, reinterpret_cast<void *>(identifyUserData)));
}

void Infinario::Infinario::Update(const std::string &customerAttributes, ResponseCallback callback, void *userData)
{
	std::stringstream bodyStream;
	bodyStream << 
		"{ \"commands\": [{ "
			"\"name\": \"crm/customers\", "
			"\"data\": { "
				"\"ids\": { ";
	if (this->_customerId.empty()) {
		bodyStream << "\"cookie\": \"" << this->_customerCookie << "\" ";
	} else {
		bodyStream << "\"registered\": \"" << this->_customerId << "\" ";
	}
	bodyStream <<
				"}, "
				"\"project_id\": \"" << this->_projectToken << "\", "
				"\"properties\": " << customerAttributes <<
			"}"
		"}]}";

	this->_requestManager.Enqueue(Request(Infinario::_requestUri, bodyStream.str(), callback, userData));
}

void Infinario::Infinario::Track(const std::string &eventName, const std::string &eventAttributes,
	ResponseCallback callback, void *userData)
{
	this->Track(eventName, eventAttributes, static_cast<double>(s3eTimerGetUTC()) / 1000.0, callback, userData);
}

void Infinario::Infinario::Track(const std::string &eventName, const std::string &eventAttributes,
	const double timestamp, ResponseCallback callback, void *userData)
{
	std::stringstream bodyStream;
	bodyStream <<
		"{ \"commands\": [{ "
			"\"name\": \"crm/events\", "
			"\"data\": { "
				"\"customer_ids\": { ";
	if (this->_customerId.empty()) {
		bodyStream << "\"cookie\": \"" << this->_customerCookie << "\" ";
	} else {
		bodyStream << "\"registered\": \"" << this->_customerId << "\" ";
	}
	bodyStream <<
				" }, "
				"\"project_id\": \"" << this->_projectToken << "\", "
				"\"timestamp\": " << std::setprecision(3) << std::fixed << timestamp << ", "
				"\"type\": \"" << eventName << "\", "
				"\"properties\": " << eventAttributes <<
			"}"
		"}]}";

	this->_requestManager.Enqueue(Request(Infinario::_requestUri, bodyStream.str(), callback, userData));
}

Infinario::Infinario::IndentifyUserData::IndentifyUserData(Infinario &infinario,
	ResponseCallback callback, void *userData)
: _infinario(infinario)
, _callback(callback)
, _userData(userData)
{}

void Infinario::Infinario::IdentifyCallback(const CIwHTTP *httpClient, const std::string &requestBody,
	const ResponseStatus responseStatus, const std::string &responseBody, void *identifyUserData)
{
	IndentifyUserData *identifyData = reinterpret_cast<IndentifyUserData *>(identifyUserData);

	if ((responseStatus != ResponseStatus::Success) ||
		(responseBody.find("\"status\": \"ok\"") == std::string::npos))
	{
		identifyData->_infinario._customerId.clear();
	}
	if (identifyData->_callback != NULL) {
		identifyData->_callback(httpClient, requestBody, responseStatus, responseBody, identifyData->_userData);
	}	

	delete identifyData;
}