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

Infinario::JsonPost::JsonPost(const std::string &uri, const std::string &body,
	ResponseCallback callback, void *userData)
: _uri(uri)
, _body(body)
, _callback(callback)
, _userData(userData)
{}

const uint32 Infinario::RequestManager::_bufferSize = 1024;

Infinario::RequestManager::RequestManager()
	: _httpClient()
	, _externalLock(s3eThreadLockCreate())
	, _internalLock(s3eThreadLockCreate())
	, _isRequestBeingProcessed(false)
	, _jsonPosts()
	, _buffer(reinterpret_cast<char *>(s3eMalloc(RequestManager::_bufferSize + 1)))
	, _accumulatedBodyLength(0)
	, _accumulatedBodyContent()
{}

Infinario::RequestManager::~RequestManager()
{
	s3eFree(reinterpret_cast<void *>(this->_buffer));

	s3eThreadLockDestroy(this->_internalLock);
	s3eThreadLockDestroy(this->_externalLock);
}

void Infinario::RequestManager::enqueueJsonPost(const JsonPost &jsonPost)
{
	s3eThreadLockAcquire(this->_externalLock);
	
	s3eThreadLockAcquire(this->_internalLock);
	this->_jsonPosts.push(jsonPost);
	s3eThreadLockRelease(this->_internalLock);

	if (!this->_isRequestBeingProcessed) {
		this->executeJsonPost();
	}

	s3eThreadLockRelease(this->_externalLock);
}

int32 Infinario::RequestManager::recieveHeader(void *systenData, void *userData)
{
	RequestManager &requestManager = *(reinterpret_cast<RequestManager *>(userData));
	JsonPost &lastJsonPost(requestManager._jsonPosts.front());

	if (requestManager._httpClient.GetStatus() == S3E_RESULT_ERROR) {
		if (lastJsonPost._callback != NULL) {
			lastJsonPost._callback(requestManager._httpClient, ResponseStatus::ReceiveHeaderError,
				requestManager._accumulatedBodyContent.str(), lastJsonPost._userData);
		}

		s3eThreadLockAcquire(requestManager._internalLock);
		requestManager._jsonPosts.pop();
		s3eThreadLockRelease(requestManager._internalLock);

		requestManager.executeJsonPost();
		return 0;
	}

	// Depending on how the server is communicating the content
	// length, we may actually know the length of the content, or
	// we may know the length of the first part of it, or we may
	// know nothing. ContentExpected always returns the smallest
	// possible size of the content, so allocate that much space
	// for now if it's non-zero. If it is of zero size, the server
	// has given no indication, so we need to guess. We'll guess at 1k.
	requestManager._accumulatedBodyLength = requestManager._httpClient.ContentExpected();
	if (requestManager._accumulatedBodyLength == 0) {
		requestManager._accumulatedBodyLength = RequestManager::_bufferSize;
	}

	requestManager._buffer[requestManager._accumulatedBodyLength] = 0;
	requestManager._httpClient.ReadDataAsync(requestManager._buffer, requestManager._accumulatedBodyLength,
		0, RequestManager::recieveBody, userData);

	return 0;
}

int32 Infinario::RequestManager::recieveBody(void *systenData, void *userData)
{
	RequestManager &requestManager = *(reinterpret_cast<RequestManager *>(userData));
	JsonPost &lastJsonPost(requestManager._jsonPosts.front());

	// This is the callback indicating that a ReadDataAsync call has
	// completed.  Either we've finished, or a bigger buffer is
	// needed.  If the correct ammount of data was supplied initially,
	// then this will only be called once. However, it may well be
	// called several times when using chunked encoding.

	// Firstly see if there's an error condition.
	if (requestManager._httpClient.GetStatus() == S3E_RESULT_ERROR) {
		if (lastJsonPost._callback != NULL) {
			lastJsonPost._callback(requestManager._httpClient, ResponseStatus::RecieveBodyError,
				requestManager._accumulatedBodyContent.str(), lastJsonPost._userData);
		}
		
		s3eThreadLockAcquire(requestManager._internalLock);
		requestManager._jsonPosts.pop();
		s3eThreadLockRelease(requestManager._internalLock);

		requestManager.executeJsonPost();
		return 0;
	}

	requestManager._accumulatedBodyContent << std::string(requestManager._buffer);

	if (requestManager._httpClient.ContentFinished()) {
		if (lastJsonPost._callback != NULL) {
			lastJsonPost._callback(requestManager._httpClient, ResponseStatus::Success,
				requestManager._accumulatedBodyContent.str(), lastJsonPost._userData);
		}
		
		s3eThreadLockAcquire(requestManager._internalLock);
		requestManager._jsonPosts.pop();
		s3eThreadLockRelease(requestManager._internalLock);

		requestManager.executeJsonPost();
		return 0;
	}

	// We have some data but not all of it. We need more space.
	uint32 oldReadSize = requestManager._accumulatedBodyLength;
	// If iwhttp has a guess how big the next bit of data is (this
	// basically means chunked encoding is being used), allocate
	// that much space. Otherwise guess.
	if (requestManager._accumulatedBodyLength < requestManager._httpClient.ContentExpected()) {
		requestManager._accumulatedBodyLength = requestManager._httpClient.ContentExpected();
	} else {
		requestManager._accumulatedBodyLength += RequestManager::_bufferSize;
	}

	requestManager._buffer[requestManager._accumulatedBodyLength] = 0;
	requestManager._httpClient.ReadDataAsync(requestManager._buffer,
		requestManager._accumulatedBodyLength - oldReadSize, 0, RequestManager::recieveBody, userData);

	return 0;
}

void Infinario::RequestManager::executeJsonPost()
{
	s3eThreadLockAcquire(this->_internalLock);
	this->_isRequestBeingProcessed = !this->_jsonPosts.empty();
	if (!this->_isRequestBeingProcessed) {
		return;
	}
	s3eThreadLockRelease(this->_internalLock);

	this->_accumulatedBodyContent.str(std::string());
	this->_accumulatedBodyContent.clear();

	JsonPost &lastJsonPost(this->_jsonPosts.front());

	this->_httpClient.SetRequestHeader("Content-Type", "application/json");
	if (this->_httpClient.Post(lastJsonPost._uri.c_str(), lastJsonPost._body.c_str(),
		static_cast<int32>(lastJsonPost._body.size()), RequestManager::recieveHeader,
		reinterpret_cast<void *>(this)) == S3E_RESULT_ERROR)
	{
		if (lastJsonPost._callback != NULL) {
			lastJsonPost._callback(this->_httpClient, ResponseStatus::SendRequestError,
				this->_accumulatedBodyContent.str(), lastJsonPost._userData);
		}
		
		s3eThreadLockAcquire(this->_internalLock);
		this->_jsonPosts.pop();
		s3eThreadLockRelease(this->_internalLock);

		this->executeJsonPost();
	}
}

const std::string Infinario::Infinario::_requestUri("http://api.infinario.com/bulk");

Infinario::RequestManager *Infinario::Infinario::_requestManager = NULL;

void Infinario::Infinario::initialize()
{
	Infinario::_requestManager = new RequestManager();
}

void Infinario::Infinario::terminate()
{
	delete Infinario::_requestManager;
}

Infinario::Infinario::Infinario(const std::string &projectToken, const std::string &customerId)
: _projectToken(projectToken)
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

void Infinario::Infinario::identify(const std::string &customerId, ResponseCallback callback, void *userData)
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
	Infinario::_requestManager->enqueueJsonPost(JsonPost(Infinario::_requestUri, bodyStream.str(),
		Infinario::identifyCallback, reinterpret_cast<void *>(identifyUserData)));
}

void Infinario::Infinario::update(const std::string &customerAttributes, ResponseCallback callback, void *userData)
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

	Infinario::_requestManager->enqueueJsonPost(JsonPost(Infinario::_requestUri, bodyStream.str(), callback, userData));
}

void Infinario::Infinario::track(const std::string &eventName, const std::string &eventAttributes,
	ResponseCallback callback, void *userData)
{
	this->track(eventName, eventAttributes, static_cast<double>(s3eTimerGetUTC()) / 1000.0, callback);
}

void Infinario::Infinario::track(const std::string &eventName, const std::string &eventAttributes,
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

	Infinario::_requestManager->enqueueJsonPost(JsonPost(Infinario::_requestUri, bodyStream.str(), callback, userData));
}

Infinario::Infinario::IndentifyUserData::IndentifyUserData(Infinario &infinario,
	ResponseCallback callback, void *userData)
: _infinario(infinario)
, _callback(callback)
, _userData(userData)
{}

void Infinario::Infinario::identifyCallback(const CIwHTTP &httpClient, const ResponseStatus &responseStatus,
	const std::string &responseBody, void *identifyUserData)
{
	IndentifyUserData *identifyData = reinterpret_cast<IndentifyUserData *>(identifyUserData);

	if ((responseStatus != ResponseStatus::Success) ||
		(responseBody.find("\"status\": \"ok\"") == std::string::npos))
	{
		identifyData->_infinario._customerId.clear();
	}
	if (identifyData->_callback != NULL) {
		identifyData->_callback(httpClient, responseStatus, responseBody, identifyData->_userData);
	}	

	delete identifyData;
}